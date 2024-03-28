#include "solver.h"
#include "cnf.h"
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>

//----------------------------------------------------------------
// BEGIN IMPLEMENTATION
//----------------------------------------------------------------

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

// Makes CNF formula from inputs
Cnf::Cnf(int **constraints, int n, int sqrt_n, int num_constraints) {
    int n_squ = n * n;
    Cnf::num_variables = (9 * n_squ);
    IndexableDLL clauses(27 * n * n_squ);
    Cnf::clauses = clauses;
    Cnf::variables = (VariableLocations *)malloc(
        sizeof(VariableLocations) * Cnf::num_variables);
    Cnf::assigned_true = (bool *)calloc(sizeof(bool), Cnf::num_variables);
    Cnf::assigned_false = (bool *)calloc(sizeof(bool), Cnf::num_variables);
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
                Cnf::variables[variable_id] = current_variable;
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
                    add_clause(restriction);
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
                    add_clause(restriction);  
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
                    add_clause(restriction);    
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
                    add_clause(restriction); 
                }
            }
        }
    }
    printf("%d clauses added\n", Cnf::clauses.count);
    // TODO: Add sum restrictions

    Cnf::ints_needed_for_clauses = ceil_div(
        Cnf::clauses.count, (sizeof(int) * 8));
    Cnf::ints_needed_for_vars = ceil_div(
        Cnf::num_variables, (sizeof(int) * 8));
}

// Default constructor
Cnf::Cnf() {
    Cnf::num_variables = 0;
    Cnf::ints_needed_for_clauses = 0;
    Cnf::ints_needed_for_vars = 0;
}

// Debug print a full cnf structure
void Cnf::print_cnf(
        int caller_pid,
        std::string prefix_string, 
        std::string tab_string) 
    {
    Cnf::clauses.set_to_head();
    std::string data_string = std::to_string(Cnf::clauses.count);
    data_string.append(" clauses:\n");
    int clause_index = 0;
    // printf("Head = %s\n", clause_to_string(*((Clause *)((*(Cnf::clauses.head)).value))).c_str());
    // printf("Tail = %s\n", clause_to_string(*((Clause *)((*(Cnf::clauses.tail)).value))).c_str());
    while (!(Cnf::clauses.iterator_finished)) {
        Clause *clause_ptr = (Clause *)(Cnf::clauses.get_current_value());
        Clause clause = *clause_ptr;
        data_string.append(tab_string);
        data_string.append("\t");
        data_string.append(std::to_string(clause_index));
        data_string.append(": ");
        data_string.append(clause_to_string(clause));
        data_string.append("\n");
        clause_index++;
        Cnf::clauses.advance_iterator();
    }
    data_string.append("]");
    printf("%s PID %d %s %s\n", tab_string.c_str(), caller_pid, prefix_string.c_str(), data_string.c_str());
    Cnf::clauses.set_to_head();
}

// Adds a clause to the cnf
void Cnf::add_clause(Clause new_clause) {
    Clause *clause_ptr = (Clause *)malloc(sizeof(Clause));
    *clause_ptr = new_clause;
    int *clause_id_ptr = (int *)malloc(sizeof(int));
    *clause_id_ptr = Cnf::clauses.count;
    for (int lit = 0; lit < new_clause.num_literals; lit++) {
        int variable_id = new_clause.literal_variable_ids[lit];
        ((Cnf::variables[variable_id]).clauses_containing).add_to_back(
            (void *)clause_id_ptr);
    }
    // printf("Clause = %s\n", clause_to_string(*(clause_ptr)).c_str());
    Cnf::clauses.add_value((void *)clause_ptr, Cnf::clauses.count);
}

// Updates formula with given assignment.
// Returns false on failure and populates conflict id.
bool Cnf::propagate_assignment(
        int var_id, 
        bool value, 
        int *conflict_id,
        Queue &edit_stack) {
    edit_stack.add_to_front(variable_edit(var_id));
    VariableLocations locations = (Cnf::variables)[var_id];
    if (value) {
        Cnf::assigned_true[var_id] = true;
    } else {
        Cnf::assigned_false[var_id] = true;
    }
    int clauses_to_check = locations.clauses_containing.count;
    while (clauses_to_check > 0) {
        int *current = (int *)locations.clauses_containing.pop_from_front();
        int clause_id = *current;
        // Try to drop it
        if (Cnf::clauses_dropped[clause_id]) {
            // Implies other value is not yet satisfied
            Clause clause = *((Clause *)((Cnf::clauses).get_value(clause_id)));
            // ASSUMES CLAUSES WITH ONLY TWO LITERALS!
            int our_index = 0;
            int other_index = 1;
            if (var_id != (clause.literal_variable_ids[0])) {
                our_index = 1;
                other_index = 0;
            }
            if (value == (clause.literal_signs[our_index])) {
                // This literal is satisfied
                Cnf::clauses_dropped[clause_id] = true;
                Cnf::clauses.strip_value(clause_id);
                edit_stack.add_to_front(clause_edit(clause_id));
            } else {
                int other_id = clause.literal_variable_ids[other_index];
                if (Cnf::assigned_true[other_id] 
                    || Cnf::assigned_true[other_id]) {
                    // Conflict clause found
                    *conflict_id = clause_id;
                    locations.clauses_containing.add_to_back((void *)current);
                    return false;
                }
            }
        }
        locations.clauses_containing.add_to_back((void *)current);
        clauses_to_check--;
    }
    return true;
}

// Converts current formula to integer representation
// Requires variable assignment arrays as well
unsigned int *Cnf::to_int_rep(bool *assigned_true, bool *assigned_false) {
    unsigned int *compressed = (unsigned *)calloc(
        sizeof(unsigned int), 
        (Cnf::ints_needed_for_clauses + (2 * Cnf::ints_needed_for_vars)));
    bool *bit_addr = Cnf::clauses_dropped;
    for (int i = 0; i < Cnf::ints_needed_for_clauses; i++) {
        unsigned int current = bits_to_int(bit_addr);
        compressed[i] = current;
        bit_addr = bit_addr + 32;
    }
    bit_addr = Cnf::assigned_true;
    for (int i = 0; i < Cnf::ints_needed_for_vars; i++) {
        unsigned int current = bits_to_int(bit_addr);
        compressed[i + Cnf::ints_needed_for_clauses] = current;
        bit_addr = bit_addr + 32;
    }
    bit_addr = Cnf::assigned_false;
    for (int i = 0; i < Cnf::ints_needed_for_vars; i++) {
        unsigned int current = bits_to_int(bit_addr);
        compressed[
            i + Cnf::ints_needed_for_clauses + Cnf::ints_needed_for_vars
            ] = current;
        bit_addr = bit_addr + 32;
    }
    return compressed;
}

// Recursively frees the Cnf formula data structure
void Cnf::free_cnf() {
    // TODO: implement this
    return;
}

//----------------------------------------------------------------
// END IMPLEMENTATION
//----------------------------------------------------------------