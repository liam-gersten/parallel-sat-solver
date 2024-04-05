#include "cnf.h"
#include "interconnect.h"
#include "state.h"
#include <string>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <cassert>
#include <chrono>
#include <algorithm>
#include <unistd.h>

void run_filename(int argc, char *argv[]) {
    const auto init_start = std::chrono::steady_clock::now();
    int pid;
    int nproc;

    // Initialize MPI
    MPI_Init(&argc, &argv);
    // Get process rank
    MPI_Comm_rank(MPI_COMM_WORLD, &pid);
    // Get total number of processes specificed at start of run
    MPI_Comm_size(MPI_COMM_WORLD, &nproc);


    std::string input_filename;
    // Read command line arguments
    int opt;
    short branching_factor = 2;
    while ((opt = getopt(argc, argv, "f:b:")) != -1) {
        switch (opt) {
        case 'f':
            input_filename = optarg;
            break;
        case 'b':
            branching_factor = (short)atoi(optarg);
            break;
        default:
            std::cerr << "Usage: " << argv[0] << " -f input_filename\n";  
            MPI_Finalize();    
            exit(EXIT_FAILURE);
        }
    }

    int n;
    int sqrt_n;
    int num_constraints;
    int **constraints = read_puzzle_file(
        input_filename, &n, &sqrt_n, &num_constraints);

    Cnf cnf(pid, constraints, n, sqrt_n, num_constraints);
    Deque task_stack;
    Interconnect interconnect(pid, nproc, cnf.work_ints * 8);
    State state(pid, nproc, branching_factor);

    if (pid == 0) {
        const double init_time = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - init_start).count();
        std::cout << "Initialization time (sec): " << std::fixed << std::setprecision(10) << init_time << '\n';
    }
    const auto compute_start = std::chrono::steady_clock::now();

    bool result = state.solve(cnf, task_stack, interconnect);
    
    const double compute_time = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - compute_start).count();
    std::cout << "Computation time (sec): " << std::fixed << std::setprecision(10) << compute_time << '\n';

    MPI_Finalize();

    if (state.was_explicit_abort) {
        // A solution was found
        if (!result) {
            // somone else has the solution
            return;
        }
    } else {
        // No solution was found
        raise_error("No solution was found");
    }
    
    bool *assignment = cnf.get_assignment();
    if (PRINT_LEVEL > 0) {
        print_assignment((short)pid, "", "", assignment, cnf.num_variables);
    }
    short **board = cnf.get_sudoku_board();
    print_board(board, cnf.n);
}

void run_example_1() {
    int num_variables = 7;
    VariableLocations *input_variables = (VariableLocations *)malloc(
        sizeof(VariableLocations) * num_variables);
    IndexableDLL input_clauses(30);

    for (int i = 0; i < num_variables; i++) {
        VariableLocations current;
        current.variable_id = i;
        current.variable_row = i;
        current.variable_col = i;
        current.variable_k = i;
        Queue variable_clauses;
        Queue *variable_clauses_ptr = (Queue *)malloc(sizeof(Queue));
        *variable_clauses_ptr = variable_clauses;
        current.clauses_containing = variable_clauses_ptr;
        input_variables[i] = current;
    }
    
    Clause C1 = make_small_clause(0, 1, false, true);
    Clause C2 = make_small_clause(2, 3, false, true);
    Clause C3 = make_triple_clause(5, 4, 1, false, false, false);
    Clause C4 = make_small_clause(4, 5, false, true);
    Clause C5 = make_small_clause(4, 6, true, true);
    Clause C6 = make_triple_clause(0, 4, 6, false, true, false);

    add_clause(C1, input_clauses, input_variables);
    add_clause(C2, input_clauses, input_variables);
    add_clause(C3, input_clauses, input_variables);
    add_clause(C4, input_clauses, input_variables);
    add_clause(C5, input_clauses, input_variables);
    add_clause(C6, input_clauses, input_variables);

    Cnf cnf(0, input_clauses, input_variables, num_variables);
    Deque task_stack;
    Interconnect interconnect(0, 1, cnf.work_ints * 8);
    State state(0, 1, 2);

    bool result = state.solve(cnf, task_stack, interconnect);
    
    if (state.was_explicit_abort) {
        // A solution was found
        if (!result) {
            // somone else has the solution
            return;
        }
    } else {
        // No solution was found
        raise_error("No solution was found");
    }

    bool *assignment = cnf.get_assignment();
    if (PRINT_LEVEL > 0) {
        print_assignment(0, "", "", assignment, cnf.num_variables);
    }
}

void truncate_size(unsigned long long int &input_size, std::string &suffix) {
    if (input_size > (unsigned long long int)(1000000000000)) {
        suffix.append("TB(s)");
        input_size /= (unsigned long long int)(1000000000000);
    } else if (input_size > 1000000000) {
        suffix.append("GB(s)");
        input_size /= (1000000000);
    } else if (input_size > 1000000) {
        suffix.append("MB(s)");
        input_size /= 1000000;
    } else if (input_size > 1000) {
        suffix.append("KB(s)");
        input_size /= 1000;
    } else {
        suffix.append("B(s)");
    }
}

void print_memory_stats() {
    for (int sqrt_n = 2; sqrt_n <= 8; sqrt_n++) {
        int n = sqrt_n * sqrt_n;
        printf("\nn = %d\n", n);
        int n_squ = n * n;
        unsigned long long int v = (n * n_squ) + (2 * n_squ);
        unsigned long long int c = (2 * n_squ * n_squ) - (2 * n_squ * n) + n_squ - ((n_squ * n) * (sqrt_n - 1));
        printf("\tnum_variables =         %llu\n", v);
        printf("\tnum_clauses =           %llu\n", c);
        unsigned long long int max_num_edits = v + c;
        unsigned long long int edit_stack_count_element_size = 20; // Doubly Linked list
        unsigned long long int edit_stack_element_size = 20; // Doubly Linked list
        unsigned long long int max_edit_stack_size = (edit_stack_element_size * (v + c)) + (edit_stack_count_element_size * v);
        std::string suffix = "";
        truncate_size(max_edit_stack_size, suffix);
        printf("\tedit_stack_size =       %llu %s\n", max_edit_stack_size, suffix.c_str());
        unsigned long long int work_queue_element_size = 20; // Doubly linked list
        unsigned long long int max_work_queue_size = (2 * v) * work_queue_element_size;
        suffix = "";
        truncate_size(max_work_queue_size, suffix);
        printf("\twork_queue_size =       %llu %s\n", max_work_queue_size, suffix.c_str());
        unsigned long long int compressed_vars = ceil_div(c, (sizeof(int) * 8)) + ceil_div(v, (sizeof(int) * 8));
        printf("\tcompressed_vars =       %llu\n", compressed_vars);
        unsigned long long compressed_clauses_size = ceil_div(
            c, (sizeof(int) * 8)) * sizeof(int);
        unsigned long long compressed_variables_size = ceil_div(
            v, (sizeof(int) * 8)) * sizeof(int);
        unsigned long long compressed_cnf_size = compressed_clauses_size + compressed_variables_size;
        unsigned long long max_compressed_stack_size = (compressed_cnf_size + 16) * v;
        suffix = "";
        truncate_size(compressed_cnf_size, suffix);
        printf("\tcompressed_size =       %llu %s\n", compressed_cnf_size, suffix.c_str());
        suffix = "";
        truncate_size(max_compressed_stack_size, suffix);
        printf("\tcompressed_stack_size = %llu %s\n", max_compressed_stack_size, suffix.c_str());
    }
    printf("\n");
}

int main(int argc, char *argv[]) {
    // run_example_1();
    // print_memory_stats();
    run_filename(argc, argv);
    // print_memory_stats();
    
}