#include "cnf.h"
#include <string>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <cassert>
#include <algorithm>
#include <unistd.h>

// Resets the cnf to its state before edit stack
void undo_edits(Cnf &cnf, Queue &edit_stack) {
    while (edit_stack.count > 0) {
        FormulaEdit *recent_ptr = (FormulaEdit *)(edit_stack.pop_from_front());
        FormulaEdit recent = *recent_ptr;
        switch (recent.edit_type) {
            case 'v': {
                int var_id = recent.edit_id;
                if (cnf.assigned_true[var_id]) {
                    cnf.assigned_true[var_id] = false;
                } else {
                    cnf.assigned_false[var_id] = false;
                }
                break;
            } case 'c': {
                int clause_id = recent.edit_id;
                cnf.clauses.re_add_value(clause_id);
                break;
            } default: {
                int clause_id = recent.edit_id;
                int size_before = (int)recent.size_before;
                int size_after = (int)recent.size_after;
                cnf.clauses.change_size_of_value(
                    clause_id, size_after, size_before);
                break;
            }
        }
        free(recent_ptr);
    }
}

bool solve(Cnf &cnf, int &conflict_id, int var_id, bool var_value) {
    cnf.depth++;
    cnf.depth_str.append("\t");
    Queue edit_stack;
    if (var_id != -1) {
        if (PRINT_LEVEL > 0) printf("%sPID %d trying var %d |= %d\n", cnf.depth_str.c_str(), 0, var_id, (int)var_value);
        // Propagate choice
        if (!cnf.propagate_assignment(
            var_id, var_value, conflict_id, edit_stack)) {
            // Conflict clause found
            if (PRINT_LEVEL > 0) printf("%sPID %d assignment propagation of var %d = %d failed (conflict = %d)\n", cnf.depth_str.c_str(), 0, var_id, (int)var_value, conflict_id);
            undo_edits(cnf, edit_stack);
            cnf.depth_str = cnf.depth_str.substr(1);
            cnf.depth--;
            return false;
        }
    }
    if (PRINT_LEVEL > 0) cnf.print_cnf(0, "Current CNF", cnf.depth_str, (PRINT_LEVEL == 2));
    // Pick a new variable
    if (cnf.clauses.get_linked_list_size() == 0) {
        if (PRINT_LEVEL > 0) printf("%sPID %d base can success with var %d |= %d\n", cnf.depth_str.c_str(), 0, var_id, (int)var_value);
        cnf.depth_str = cnf.depth_str.substr(1);
        cnf.depth--;
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
        bool result = solve(cnf, conflict_id, new_var_id, new_var_sign);
        if (!result) {
            // Conflict clause found
            if (PRINT_LEVEL > 0) printf("%sPID %d only child trying var %d |= %d failed (conflict = %d)\n", cnf.depth_str.c_str(), 0, new_var_id, (int)new_var_sign, conflict_id);
            undo_edits(cnf, edit_stack);
            cnf.depth_str = cnf.depth_str.substr(1);
            cnf.depth--;
            return false;
        }
        edit_stack.free_data();
        if (PRINT_LEVEL > 0) printf("%sPID %d recursive success with var %d |= %d\n", cnf.depth_str.c_str(), 0, var_id, (int)var_value);
        cnf.depth_str = cnf.depth_str.substr(1);
        cnf.depth--;
        return true;
    }
    // Two recursive calls
    bool first_choice = get_first_pick(new_var_sign);
    bool left_result = solve(cnf, conflict_id, new_var_id, first_choice);
    if (!left_result) {
        if (PRINT_LEVEL > 0) printf("%sPID %d left child trying var %d |= %d failed (conflict = %d)\n", cnf.depth_str.c_str(), 0, new_var_id, (int)new_var_sign, conflict_id);
        bool right_result = solve(cnf, conflict_id, new_var_id, !first_choice);
        if (!right_result) {
            if (PRINT_LEVEL > 0) printf("%sPID %d right child trying var %d |= %d failed (conflict = %d)\n", cnf.depth_str.c_str(), 0, new_var_id, (int)(!new_var_sign), conflict_id);
            undo_edits(cnf, edit_stack);
            cnf.depth_str = cnf.depth_str.substr(1);
            cnf.depth--;
            return false;
        }
    }
    if (PRINT_LEVEL > 0) printf("%sPID %d recursive success with var %d |= %d\n", cnf.depth_str.c_str(), 0, var_id, (int)var_value);
    edit_stack.free_data();
    cnf.depth_str = cnf.depth_str.substr(1);
    cnf.depth--;
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

    Cnf cnf(constraints, n, sqrt_n, num_constraints);

    cnf.print_cnf(0, "CNF", "", true);

    int conflict_id;
    bool result = solve(cnf, conflict_id, -1, true);
    if (!result) {
        raise_error("Couldn't solve");
    }
    bool *assignment = cnf.assigned_true;
    // Convert to a sudoku board
}

void run_example_1() {
    int conflict_id;
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
    Clause C3;
    Clause C4 = make_small_clause(4, 5, false, true);
    Clause C5 = make_small_clause(4, 6, true, true);
    Clause C6;

    int *C3_vars = (int *)malloc(sizeof(int) * 3);
    bool *C3_signs = (bool *)calloc(sizeof(int), 3);
    int *C6_vars = (int *)malloc(sizeof(int) * 3);
    bool *C6_signs = (bool *)calloc(sizeof(int), 3);

    C3.num_literals = 3;
    C6.num_literals = 3;

    C3_vars[0] = 5;
    C3_vars[1] = 4;
    C3_vars[2] = 1;
    C6_vars[0] = 0;
    C6_vars[1] = 4;
    C6_vars[2] = 6;
    C6_signs[1] = true;

    C3.literal_variable_ids = C3_vars;
    C3.literal_signs = C3_signs;
    C6.literal_variable_ids = C6_vars;
    C6.literal_signs = C6_signs;

    add_clause(C1, input_clauses, input_variables);
    add_clause(C2, input_clauses, input_variables);
    add_clause(C3, input_clauses, input_variables);
    add_clause(C4, input_clauses, input_variables);
    add_clause(C5, input_clauses, input_variables);
    add_clause(C6, input_clauses, input_variables);

    
    Cnf cnf(input_clauses, input_variables, num_variables);

    bool result = solve(cnf, conflict_id, -1, true);

    bool *assignment = cnf.get_assignment();
    printf("\nAssignment = [");
    for (int i = 0; i < num_variables; i++) {
        if (assignment[i]) {
            printf("T");
        } else {
            printf("F");
        }
        if (i != num_variables - 1) {
            printf(", ");
        }
    }
    printf("]\n\n");
}


int main(int argc, char *argv[]) {
    run_example_1();
}