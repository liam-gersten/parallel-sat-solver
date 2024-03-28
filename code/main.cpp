#include "cnf.h"
#include "solver.h"
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
            } default: {
                int clause_id = recent.edit_id;
                cnf.clauses.re_add_value(clause_id);
                break;
            }
        }
        free(recent_ptr);
    }
}

bool solve(Cnf &cnf, int &conflict_id) {
    Queue edit_stack;
    while (cnf.clauses.count > 0) {
        cnf.clauses.set_to_head();
        Clause arbitrary_clause = *((Clause *)(cnf.clauses.get_current_value()));
        int var0 = arbitrary_clause.literal_variable_ids[0];
        int var1 = arbitrary_clause.literal_variable_ids[1];
        if (cnf.assigned_true[var0] || cnf.assigned_false[var0]) {
            // Know var1's value
            if (!cnf.propagate_assignment(
                var1, arbitrary_clause.literal_signs[1], 
                &conflict_id, edit_stack)) {
                // Conflict clause found
                undo_edits(cnf, edit_stack);
                return false;
            }
        } else if (cnf.assigned_true[var1] || cnf.assigned_false[var1]) {
            // Know var0's value
            if (!cnf.propagate_assignment(
                var0, arbitrary_clause.literal_signs[0], 
                &conflict_id, edit_stack)) {
                // Conflict clause found
                undo_edits(cnf, edit_stack);
                return false;
            }
        } else {
            Queue tmp_stack;
            // Recurse on var0 = T
            if (cnf.propagate_assignment(
                var0, true, &conflict_id, tmp_stack)) {
                if (solve(cnf, conflict_id)) {
                    edit_stack.free_data();
                    return true;
                }
            }
            undo_edits(cnf, tmp_stack);
            // Recurse on var0 = F
            if (cnf.propagate_assignment(
                var0, false, &conflict_id, tmp_stack)) {
                if (solve(cnf, conflict_id)) {
                    edit_stack.free_data();
                    return true;
                }
            }
            // Unsat
            undo_edits(cnf, tmp_stack);
            undo_edits(cnf, edit_stack);
            return false;
        }
    }
    edit_stack.free_data();
    return true;
}


int main(int argc, char *argv[]) {
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

    printf("n = %d, sqrt_n = %d, num_constraints = %d\n", n, sqrt_n, num_constraints);
    
    Cnf cnf(constraints, n, sqrt_n, num_constraints);

    cnf.print_cnf(0, "CNF", "");
    


    // test_solve_rec(cnf, 0);
    // Solver solver(solver_formula);
    // bool *assignment = (bool *)calloc(sizeof(bool), solver_formula.num_variables);
    // bool is_satisfiable = solver.solve(assignment);
    // if (is_satisfiable) {
    //     print_assignment(0, "satisfying assignment:", "", assignment, 
    //     solver_formula.num_variables);
    // } else {
    //     printf("Not satisfyable\n");
    // }
}