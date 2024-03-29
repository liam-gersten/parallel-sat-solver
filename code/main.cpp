#include "cnf.h"
#include <string>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <cassert>
#include <chrono>
#include <algorithm>
#include <unistd.h>

bool solve_recursive(Cnf &cnf, int var_id, bool var_value) {
    cnf.recurse();
    if (var_id != -1) { // Propagate choice
        if (!cnf.propagate_assignment(var_id, var_value)) { // Conflict clause found
            cnf.undo_local_edits();
            cnf.backtrack();
            return false;
        }
    }
    // Pick a new variable
    if (cnf.clauses.get_linked_list_size() == 0) {
        if (PRINT_LEVEL > 0) printf("%sPID %d base case success with var %d |= %d\n", cnf.depth_str.c_str(), 0, var_id, (int)var_value);
        cnf.backtrack();
        printf("Edit stack size = %d\n", (*cnf.edit_stack).count);
        return true;
    }
    cnf.clauses.reset_iterator();
    Clause current_clause = *((Clause *)(cnf.clauses.get_current_value()));
    int current_clause_id = cnf.clauses.get_current_index();
    int new_var_id;
    bool new_var_sign;
    int num_unsat = cnf.pick_from_clause(
        current_clause, &new_var_id, &new_var_sign);
    if (PRINT_LEVEL > 0) printf("%sPID %d picked new var %d from clause %d %s\n", cnf.depth_str.c_str(), 0, new_var_id, current_clause_id, cnf.clause_to_string_current(current_clause, false).c_str());
    if (num_unsat == 1) {
        // Already know its correct value, one recursive call
        bool result = solve_recursive(cnf, new_var_id, new_var_sign);
        if (!result) {
            // Conflict clause found
            if (PRINT_LEVEL > 0) printf("%sPID %d only child trying var %d |= %d failed (conflict = %d)\n", cnf.depth_str.c_str(), 0, new_var_id, (int)new_var_sign, cnf.conflict_id);
            cnf.undo_local_edits();
            cnf.backtrack();
            return false;
        }
        if (PRINT_LEVEL > 0) printf("%sPID %d recursive success with var %d |= %d\n", cnf.depth_str.c_str(), 0, var_id, (int)var_value);
        cnf.backtrack();
        return true;
    }
    // Two recursive calls
    bool first_choice = get_first_pick(new_var_sign);
    bool left_result = solve_recursive(cnf, new_var_id, first_choice);
    if (!left_result) {
        if (PRINT_LEVEL > 0) printf("%sPID %d left child trying var %d |= %d failed (conflict = %d)\n", cnf.depth_str.c_str(), 0, new_var_id, (int)new_var_sign, cnf.conflict_id);
        bool right_result = solve_recursive(cnf, new_var_id, !first_choice);
        if (!right_result) {
            if (PRINT_LEVEL > 0) printf("%sPID %d right child trying var %d |= %d failed (conflict = %d)\n", cnf.depth_str.c_str(), 0, new_var_id, (int)(!new_var_sign), cnf.conflict_id);
            cnf.undo_local_edits();
            cnf.backtrack();
            return false;
        }
    }
    if (PRINT_LEVEL > 0) printf("%sPID %d recursive success with var %d |= %d\n", cnf.depth_str.c_str(), 0, var_id, (int)var_value);
    cnf.backtrack();
    return true;
}

// Adds one or two variable assignment tasks to task stack
void add_tasks_from_formula(Cnf &cnf, Queue &task_stack) {
    cnf.clauses.reset_iterator();
    Clause current_clause = *((Clause *)(cnf.clauses.get_current_value()));
    int current_clause_id = cnf.clauses.get_current_index();
    int new_var_id;
    bool new_var_sign;
    int num_unsat = cnf.pick_from_clause(
        current_clause, &new_var_id, &new_var_sign);
    if (PRINT_LEVEL > 0) printf("%sPID %d picked new var %d from clause %d %s\n", cnf.depth_str.c_str(), 0, new_var_id, current_clause_id, cnf.clause_to_string_current(current_clause, false).c_str());
    // Add an undo task to do last
    void *undo_task = make_task(-1, true);
    task_stack.add_to_front(undo_task);
    if (num_unsat == 1) {
        void *only_task = make_task(new_var_id, new_var_sign);
        task_stack.add_to_front(only_task);
    } else {
        bool first_choice = get_first_pick(new_var_sign);
        void *important_task = make_task(new_var_id, first_choice);
        void *other_task = make_task(new_var_id, !first_choice);
        task_stack.add_to_front(other_task);
        task_stack.add_to_front(important_task);
    }   
}

bool solve(Cnf &cnf) {
    if (PRINT_LEVEL > 0) cnf.print_cnf(0, "Init CNF", cnf.depth_str, (PRINT_LEVEL >= 2));
    Queue task_stack;
    add_tasks_from_formula(cnf, task_stack);
    while (task_stack.count > 0) {
        if (PRINT_LEVEL > 1) cnf.print_task_stack(0, "Loop start", task_stack);
        if (PRINT_LEVEL > 1) cnf.print_cnf(0, "Loop start CNF", cnf.depth_str, (PRINT_LEVEL >= 2));
        if (cnf.clauses.get_linked_list_size() == 0) {
            if (PRINT_LEVEL > 0) printf("%sPID %d base case success\n", cnf.depth_str.c_str(), 0);
            if (PRINT_LEVEL > 0) printf("Edit stack size = %d\n", (*cnf.edit_stack).count);
            return true;
        }
        Task task = get_task(task_stack);
        int var_id = task.var_id;
        int assignment = task.assignment;
        if (var_id == -1) {
            // Children backtracked, need to backtrack ourselves
            if (PRINT_LEVEL > 1) cnf.print_task_stack(0, "Children backtrack", task_stack);
            cnf.backtrack();
            continue;
        }
        cnf.recurse();
        // Propagate choice
        bool propagate_result = cnf.propagate_assignment(var_id, assignment);
        if (!propagate_result) { // Conflict clause found
            if (PRINT_LEVEL > 1) cnf.print_task_stack(0, "Prop fail", task_stack);
            cnf.backtrack();
            continue;
        }
        if (cnf.clauses.get_linked_list_size() == 0) {
            if (PRINT_LEVEL > 0) printf("%sPID %d base case success\n", cnf.depth_str.c_str(), 0);
            if (PRINT_LEVEL > 0) printf("Edit stack size = %d\n", (*cnf.edit_stack).count);
            return true;
        }
        add_tasks_from_formula(cnf, task_stack);
        if (PRINT_LEVEL > 1) cnf.print_task_stack(0, "Loop end", task_stack);
    }
    if (PRINT_LEVEL > 0) printf("%sPID %d recursive success\n", cnf.depth_str.c_str(), 0);
    return true;
}

void run_filename(int argc, char *argv[]) {
    std::string input_filename;
    // Read command line arguments
    int opt;
    while ((opt = getopt(argc, argv, "f:")) != -1) {
        switch (opt) {
        case 'f':
            input_filename = optarg;
            break;
        default:
            std::cerr << "Usage: " << argv[0] << " -f input_filename\n";      
            exit(EXIT_FAILURE);
        }
    }
    printf("filename = %s\n", input_filename.c_str());

    int n;
    int sqrt_n;
    int num_constraints;
    int **constraints = read_puzzle_file(
        input_filename, &n, &sqrt_n, &num_constraints);

    const auto compute_start = std::chrono::steady_clock::now();

    Cnf cnf(constraints, n, sqrt_n, num_constraints);

    bool result = solve(cnf);
    const double compute_time = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - compute_start).count();
    std::cout << "Computation time (sec): " << std::fixed << std::setprecision(10) << compute_time << '\n';

    if (!result) {
        raise_error("Couldn't solve");
    }
    bool *assignment = cnf.get_assignment();
    if (PRINT_LEVEL > 0) {
        print_assignment(0, "", "", assignment, cnf.num_variables);
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

    Cnf cnf(input_clauses, input_variables, num_variables);

    bool result = solve(cnf);
    assert(result);

    bool *assignment = cnf.get_assignment();
    if (PRINT_LEVEL > 0) {
        print_assignment(0, "", "", assignment, cnf.num_variables);
    }
}


int main(int argc, char *argv[]) {
    // run_example_1();
    run_filename(argc, argv);
}