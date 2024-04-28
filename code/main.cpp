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
#include <cmath> 

void run_filename(
        int pid,
        int nproc,
        std::string input_filename, 
        short branching_factor, 
        short assignment_method, 
        bool use_smart_prop, 
        int reduction_method) 
    {
    const auto init_start = std::chrono::steady_clock::now();

    int n;
    int sqrt_n;
    int num_constraints;
    int num_assignments;
    GridAssignment *assignments;
    int **constraints = read_puzzle_file(
        input_filename, &n, &sqrt_n, &num_constraints, 
        &num_assignments, assignments);

    Cnf cnf(pid, nproc, constraints, n, 
        sqrt_n, num_constraints, num_assignments, 
        reduction_method, assignments);
    Deque task_stack;
    Interconnect interconnect(pid, nproc, cnf.work_ints * 4);
    State state(pid, nproc, branching_factor, 
        assignment_method, use_smart_prop);

    if (pid == 0) {
        const double init_time = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - init_start).count();
        std::cout << "Initialization time (sec): " << std::fixed << std::setprecision(10) << init_time << '\n';
    }
    const auto compute_start = std::chrono::steady_clock::now();

    bool result = state.solve(cnf, task_stack, interconnect);

    printf("\tPID %d: Solve called %llu times\n", pid, state.calls_to_solve);
    
    const double compute_time = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - compute_start).count();
    std::cout << "Computation time (sec): " << std::fixed << std::setprecision(10) << compute_time << '\n';

    MPI_Finalize();

    if (state.was_explicit_abort) {
        // A solution was found
        if (!result) {
            // someone else has the solution
            return;
        }
        printf("Solution found by PID %d\n", pid);
        std::cout << "Solution (PID %d) computation time (sec): " << std::fixed << std::setprecision(10) << compute_time << '\n';
    } else {
        // No solution was found
        raise_error("No solution was found");
    }
    
    bool *assignment = cnf.get_assignment();
    if (PRINT_LEVEL > 0) {
        print_assignment((short)pid, "", "", assignment, cnf.num_variables);
    }
    short **board = cnf.sudoku_board;
    print_board(board, cnf.n);
    for (int i = 0; i < n; i++) {
        free(board[i]);
    }
    free(board);
}

void run_tests(
        int pid,
        int nproc,
        int num_tests,
        short test_length,
        short branching_factor, 
        short assignment_method, 
        bool use_smart_prop, 
        int reduction_method) 
    {
    //uses hard_16s.txt
    int n = 16;
    int sqrt_n = 4;
    int num_constraints = 0;

    std::ifstream fin("inputs/16_hards.txt");

    if (!fin) {
    std::cerr << "Unable to open inputs/16_hards.txt\n";
    exit(EXIT_FAILURE);
    }

    std::string str;
    for (int i = 0; i < test_length; i++) {
        std::getline(fin, str);
    }
        
    int num_assignments = 0;
    for (int k =0;k<256;k++) {
        if (str[k] != '.') num_assignments++;
    }

    int **constraints = (int **)calloc(sizeof(int *), 0);
    GridAssignment* assignments = (GridAssignment *)malloc(
        sizeof(GridAssignment) * num_assignments); 

    int j = 0;
    for (int k = 0; k < 256; k++) {
        if (str[k] == '.') continue;
        GridAssignment assignment;
        assignment.row = k % 16;
        assignment.col = k / 16;
        int value = (str[k] >= 'A') ? (str[k] - 'A' + 10) : (str[k] - '0');
        assignment.value = value - 1; //needs to be 0-indexed
        assignments[j] = assignment;
        j++;
    }

    Cnf cnf(pid, nproc, constraints, n, 
        sqrt_n, num_constraints, num_assignments, 
        reduction_method, assignments);
    Deque task_stack;
    Interconnect interconnect(pid, nproc, cnf.work_ints * 4);
    State state(pid, nproc, branching_factor, 
        assignment_method, use_smart_prop);

    const auto compute_start = std::chrono::steady_clock::now();

    bool result = state.solve(cnf, task_stack, interconnect);

    printf("\tPID %d: Solve called %llu times\n", pid, state.calls_to_solve);
    
    const double compute_time = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - compute_start).count();
    std::cout << "Computation time (sec): " << std::fixed << std::setprecision(10) << compute_time << '\n';

    if (state.was_explicit_abort) {
        // A solution was found
        if (!result) {
            // someone else has the solution
            return;
        }
        printf("Solution found by PID %d\n", pid);
        std::cout << "Solution (PID %d) computation time (sec): " << std::fixed << std::setprecision(10) << compute_time << '\n';
    } else {
        // No solution was found
        raise_error("No solution was found");
    }

    bool *assignment = cnf.get_assignment();
    if (PRINT_LEVEL > 0) {
        print_assignment((short)pid, "", "", assignment, cnf.num_variables);
    }
    short **board = cnf.sudoku_board;
    print_board(board, cnf.n);

    MPI_Finalize();
    for (int i = 0; i < n; i++) {
        free(board[i]);
    }
    free(board);
    fin.close();
}

void run_example_1(
        int pid,
        int nproc,
        std::string input_filename, 
        short branching_factor, 
        short assignment_method, 
        bool use_smart_prop, 
        int reduction_method) 
    {
    const auto init_start = std::chrono::steady_clock::now();

    int num_variables = 7;
    VariableLocations *input_variables = (VariableLocations *)malloc(
        sizeof(VariableLocations) * num_variables);
    Clauses input_clauses(6, 20);

    for (int i = 0; i < num_variables; i++) {
        VariableLocations current;
        current.variable_id = i;
        current.variable_row = i;
        current.variable_col = i;
        current.variable_k = i;
        current.clauses_containing;
        input_variables[i] = current;
    }
    Clause C1 = make_small_clause(0, 1, false, true);
    Clause C2 = make_small_clause(2, 3, false, true);
    Clause C3 = make_small_clause(4, 5, false, true);
    Clause C4 = make_small_clause(4, 6, true, true);
    Clause C5 = make_triple_clause(1, 4, 5, false, false, false);
    Clause C6 = make_triple_clause(0, 4, 6, false, true, false);

    add_clause(C1, input_clauses, input_variables);
    add_clause(C2, input_clauses, input_variables);
    add_clause(C3, input_clauses, input_variables);
    add_clause(C4, input_clauses, input_variables);
    add_clause(C5, input_clauses, input_variables);
    add_clause(C6, input_clauses, input_variables);

    Cnf cnf(pid, 1, input_clauses, input_variables, num_variables);

    Deque task_stack;
    Interconnect interconnect(pid, nproc, cnf.work_ints * 4);
    State state(pid, nproc, branching_factor, 
        assignment_method, use_smart_prop);

    if (pid == 0) {
        const double init_time = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - init_start).count();
        std::cout << "Initialization time (sec): " << std::fixed << std::setprecision(10) << init_time << '\n';
    }
    const auto compute_start = std::chrono::steady_clock::now();

    bool result = state.solve(cnf, task_stack, interconnect);

    if (PRINT_LEVEL > 1) printf("\tPID %d: Solve called %llu times\n", pid, state.calls_to_solve);
    
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
        print_assignment((short)pid, "", "", assignment, cnf.num_variables, true);
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

int n_choose_2(int n) {
    return (n * (n - 1)) / 2;
}

unsigned long long get_literal_count(int n) {
    unsigned long long count = 0;
    // For each position, there is at most one k value true
    // Exactly n^3(1/2)(n-1) clauses
    count += (n * n * n_choose_2(n) * 2);
    
    // For each position, there is at leadt one k value true
    // Exactly n^2 clauses
    count += (n * n * n);
    
    // For each row and k, there is at most one value with this k
    // Exactly n^3(1/2)(n - 1) clauses
    count += (n * n * n_choose_2(n) * 2);
    
    // For each col and k, there is at most one value with this k
    // Exactly n^3(1/2)(n - 1) clauses
    count += (n * n * n_choose_2(n) * 2);

    // For each chunk and k, there is at most one value with this k
    // Less than n^3(1/2)(n - 1) clauses
    count += (n * n * n_choose_2(n) * 2);
    return count;
}

void print_memory_stats() {
    for (int sqrt_n = 2; sqrt_n <= 12; sqrt_n++) {
        int n = sqrt_n * sqrt_n;
        printf("\nn = %d\n", n);
        int n_squ = n * n;
        unsigned long long int v = std::pow(n,3) + n*n*sqrt_n*comm_num_vars(sqrt_n, false) + 2*n*n*(comm_num_vars(sqrt_n) + comm_num_vars(n));
        unsigned long long int c = n_squ*sqrt_n * comm_num_clauses(sqrt_n+1) + 2*n_squ*comm_num_clauses(n) + 2*n_squ*comm_num_clauses(sqrt_n) ;
        printf("\tnum_variables =         %llu\n", v);
        printf("\tnum_clauses =           %llu\n", c);
        unsigned long long int max_num_edits = v + c;
        unsigned long long int edit_stack_count_element_size = sizeof(IntDoublyLinkedList); // Doubly Linked list
        unsigned long long int edit_stack_element_size = sizeof(DoublyLinkedList) + sizeof(FormulaEdit); // Doubly Linked list
        unsigned long long int max_edit_stack_size = (edit_stack_element_size * (v + c)) + (edit_stack_count_element_size * v);
        std::string suffix = "";
        truncate_size(max_edit_stack_size, suffix);
        printf("\tedit_stack_size =       %llu %s\n", max_edit_stack_size, suffix.c_str());
        unsigned long long int work_queue_element_size = sizeof(DoublyLinkedList) + sizeof(Task); // Doubly linked list
        unsigned long long int max_work_queue_size = (3 * v) * work_queue_element_size;
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
        unsigned long long num_literals = get_literal_count(n);
        unsigned long long literal_cost = 5 * num_literals;
        unsigned long long clauses_cost = (sizeof(Clause) + sizeof(DoublyLinkedList)) * c;
        unsigned long long formula_cost = literal_cost + clauses_cost;
        suffix = "";
        truncate_size(literal_cost, suffix);
        printf("\tcost of literals =      %llu %s\n", literal_cost, suffix.c_str());
        suffix = "";
        truncate_size(clauses_cost, suffix);
        printf("\tcost of clauses =       %llu %s\n", clauses_cost, suffix.c_str());
        suffix = "";
        truncate_size(formula_cost, suffix);
        printf("\tcost of formula =       %llu %s\n", formula_cost, suffix.c_str());
    }
    printf("\n");
}

int main(int argc, char *argv[]) {
    int pid;
    int nproc;
    MPI_Init(&argc, &argv); // Initialize MPI
    MPI_Comm_rank(MPI_COMM_WORLD, &pid); // Get process rank
    MPI_Comm_size(MPI_COMM_WORLD, &nproc);

    // Read command line arguments
    std::string command = "runfile";
    std::string input_filename;
    int num_tests = 1;
    short test_length;
    int opt;
    short branching_factor = 2;
    short assignment_method = 1;
    bool use_smart_prop = false;
    int reduction_method = 0;
    while ((opt = getopt(argc, argv, "c:f:t:l:b:m:s:r:")) != -1) {
        switch (opt) {
            case 'c':
                command = optarg;
                break;
            case 'f':
                input_filename = optarg;
                break;
            case 't':
                num_tests = (int)atoi(optarg);
                break;
            case 'l':
                test_length = (int)atoi(optarg);
                break;
            case 'b':
                branching_factor = (short)atoi(optarg);
                break;
            case 'm':
                assignment_method = (short)atoi(optarg);
                break;
            case 's':
                use_smart_prop = (bool)atoi(optarg);
                break;
            case 'r':
                reduction_method = (int)atoi(optarg);
                break;
            default:
                std::cerr << "Incorrect command line arguments\n";  
                MPI_Finalize();    
                exit(EXIT_FAILURE);
        }
    }
    if (command == "runfile") {
        run_filename(
            pid,
            nproc,
            input_filename, 
            branching_factor, 
            assignment_method, 
            use_smart_prop, 
            reduction_method);
    } else if (command == "runtests") {
        run_tests(
            pid,
            nproc,
            num_tests,
            test_length,
            branching_factor, 
            assignment_method, 
            use_smart_prop, 
            reduction_method);
    } else if (command == "example1") {
        run_example_1(
            pid,
            nproc,
            input_filename, 
            branching_factor, 
            assignment_method, 
            use_smart_prop, 
            reduction_method);
    } else {
        if (pid == 0) {
            print_memory_stats();
        }
        MPI_Finalize();
    }
}