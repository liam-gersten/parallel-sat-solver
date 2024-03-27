#include "solver.h"
#include "helpers.h"
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>

//----------------------------------------------------------------
// BEGIN IMPLEMENTATION
//----------------------------------------------------------------

// Gets string representation of clause
std::string clause_to_string(Clause clause) {
    std::string clause_string = "(";
    for (int lit = 0; lit < clause.num_literals; lit++) {
        if (!(clause.literal_signs[lit])) {
            clause_string.append("!");
        }
        clause_string.append(
            std::to_string(clause.literal_variable_ids[lit]));
        if (lit != clause.num_literals - 1) {
            clause_string.append(" U ");
        }
    }
    clause_string.append(")");
    return clause_string;
}

// Debug print a full cnf structure
void print_cnf(
        int caller_pid,
        std::string prefix_string, 
        std::string tab_string,
        Cnf cnf) 
    {
    Queue tmp;
    std::string data_string = std::to_string(cnf.clauses.count);
    data_string.append(" clauses:\n");
    int clause_index = 0;
    while (cnf.clauses.count > 0) {
        Clause *clause_ptr = (Clause *)(cnf.clauses.pop_from_front());
        Clause clause = *clause_ptr;
        data_string.append(tab_string);
        data_string.append("\t");
        data_string.append(std::to_string(clause_index));
        data_string.append(": ");
        data_string.append(clause_to_string(clause));
        data_string.append("\n");
        clause_index++;
        tmp.add_to_front((void *)clause_ptr);
    }
    while (tmp.count > 0) {
        cnf.clauses.add_to_front(tmp.pop_from_front());
    }
    data_string.append("]");
    printf("%s PID %d %s %s\n", tab_string.c_str(), caller_pid, prefix_string.c_str(), data_string.c_str());
}

// Makes a clause of just two variables
Clause make_small_clause(int var1, int var2, bool sign1, bool sign2) {
    Clause current;
    current.literal_variable_ids = (int *)malloc(sizeof(int) * 2);
    current.literal_signs = (bool *)malloc(sizeof(bool) * 2);
    (current.literal_variable_ids)[0] = var1;
    (current.literal_variable_ids)[1] = var2;
    (current.literal_signs)[0] = sign1;
    (current.literal_signs)[1] = sign2;
    current.num_literals = 2;
    return current;
}

// Adds a clause to the cnf
void add_clause_to_cnf(Cnf &cnf, Clause new_clause, bool front) {
    Clause *clause_ptr = (Clause *)malloc(sizeof(Clause));
    *clause_ptr = new_clause;
    for (int lit = 0; lit < new_clause.num_literals; lit++) {
        int variable_id = new_clause.literal_variable_ids[lit];
        ((cnf.variables[variable_id]).clauses_containing).add_to_back(
            (void *)clause_ptr);
    }
    if (front) {
        cnf.clauses.add_to_front((void *)clause_ptr);
    } else {
        cnf.clauses.add_to_back((void *)clause_ptr);
    }
    printf("%d clauses added\n", cnf.clauses.count);
}

// Makes CNF formula from inputs
Cnf make_cnf(int **constraints, int n, int sqrt_n, int num_constraints) {
    int n_squ = n * n;
    Cnf current;
    current.num_variables = (9 * n_squ);
    Queue clauses;
    current.clauses = clauses;
    current.variables = (VariableLocations *)malloc(
        sizeof(VariableLocations) * current.num_variables);
    int variable_id = 0;
    for (int k = 0; k < 9; k++) {
        for (int row = 0; row < n; row++) {
            for (int col = 0; col < n; col++) {
                VariableLocations current_variable;
                current_variable.variable_id = variable_id;
                current_variable.variable_row = row;
                current_variable.variable_col = col;
                current_variable.variable_k = k;
                Queue clauses_containing;
                current_variable.clauses_containing = clauses_containing;
                current.variables[variable_id] = current_variable;
                variable_id++;
            }
        }
    }
    int var1;
    int var2;
    int variable_num = 0;
    for (int row = 0; row < n; row++) {
        for (int col = 0; col < n; col++) {
            for (int k1 = 0; k1 < 8; k1++) {
                for (int k2 = k1 + 1; k2 < 9; k2++) {
                    var1 = ((n_squ) * k1) + variable_num;
                    var2 = ((n_squ) * k2) + variable_num;
                    Clause restriction = make_small_clause(
                        var1, var2, false, false);
                    add_clause_to_cnf(current, restriction, false);
                }   
            }
            variable_num++;
        }
    }
    int semi_final_column = (n * (n - 1));
    for (int k = 0; k < n; k++) {
        int k_offset = ((n_squ) * k);
        int row_offset = k_offset;
        for (int group = 0; group < n; group++) {
            // Row restriction
            for (int first = 0; first < n - 1; first++) {
                for (int second = first + 1; second < n; second++) {
                    var1 = row_offset + first;
                    var2 = row_offset + second;
                    Clause restriction = make_small_clause(
                        var1, var2, false, false);
                    add_clause_to_cnf(current, restriction, false);    
                }
            }
            row_offset += n;
            // Col restriction
            for (int first = 0; first < semi_final_column; first += n) {
                for (int second = first + n; second < n_squ; second += n) {
                    var1 = k_offset + first;
                    var2 = k_offset + second;
                    Clause restriction = make_small_clause(
                        var1, var2, false, false);
                    add_clause_to_cnf(current, restriction, false);    
                }
            }
            // // Block restriction (expensive)
            int block_row = group / sqrt_n;
            int block_col = group & sqrt_n;
            for (int first = 0; first < n - 1; first++) {
                for (int second = first + 1; second < n; second++) {
                    int first_row = block_row + (first / sqrt_n);
                    int first_col = block_col + (first % sqrt_n);
                    int second_row = block_row + (second / sqrt_n);
                    int second_col = block_col + (second % sqrt_n);
                    if ((first_row == second_row) 
                        || (first_col == second_col)) {
                        continue;
                    }
                    var1 = k_offset + ((first_row * n) + first_col);
                    var2 = k_offset + ((second_row * n) + second_col);
                    Clause restriction = make_small_clause(
                        var1, var2, false, false);
                    add_clause_to_cnf(current, restriction, false); 
                }
            }
        }
    }
    printf("%d clauses added\n", current.clauses.count);
    // TODO: Add sum restrictions
    return current;
}

// Recursively frees the Cnf formula data structure
void free_cnf_formula(Cnf formula) {
    // TODO: implement this
    return;
}

// Initialized with input formula
Solver::Solver(Cnf formula) {
    // TODO: implement this
    return;
}

// Solver, returns true and populates assignment if satisfiable
bool Solver::solve(bool *assignment) {
    // TODO: implement this
    return false;
}

// Sets variable's value in cnf formula to given value 
void Solver::assign_variable_value(int variable_id, bool value) {
    // TODO: implement this
    return;
}

// Effectively undoes assign_variable_value, useful for recursive
// backtracking
void Solver::reset_variable_value(int variable_id) {
    // TODO: implement this
    return;
}

// Gets the truth value of a clause given variable assignments
bool Solver::evaluate_clause(bool *assignment) {
    // TODO: implement this
    return false;
}

// Gets the truth value of a cnf given variable assignments
bool Solver::evaluate_cnf(bool *assignment) {
    // TODO: implement this
    return false;
}

//----------------------------------------------------------------
// END IMPLEMENTATION
//----------------------------------------------------------------