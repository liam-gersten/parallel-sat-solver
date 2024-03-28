#include "cnf.h"
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>
#include <cassert>

//----------------------------------------------------------------
// BEGIN IMPLEMENTATION
//----------------------------------------------------------------

// Adds a clause to data structures
void add_clause(
    Clause new_clause, 
    IndexableDLL &clauses, 
    VariableLocations *variables) 
    {
    Clause *clause_ptr = (Clause *)malloc(sizeof(Clause));
    *clause_ptr = new_clause;
    int new_clause_id = clauses.num_indexed;
    int *clause_id_ptr = (int *)malloc(sizeof(int));
    *clause_id_ptr = new_clause_id;
    for (int lit = 0; lit < new_clause.num_literals; lit++) {
        int variable_id = new_clause.literal_variable_ids[lit];
        (*(((variables[variable_id])).clauses_containing)).add_to_back(
            (void *)clause_id_ptr);
    }
    clauses.add_value(
        (void *)clause_ptr, new_clause_id, new_clause.num_literals);
}

// Makes CNF formula from inputs
Cnf::Cnf(int **constraints, int n, int sqrt_n, int num_constraints) {
    // This is broken
    int n_squ = n * n;
    Cnf::n = n;
    Cnf::num_variables = (n * n_squ);
    IndexableDLL clauses((2 * (n_squ * n_squ)) + n_squ);
    Cnf::clauses = clauses;
    Cnf::variables = (VariableLocations *)malloc(
        sizeof(VariableLocations) * Cnf::num_variables);
    Cnf::assigned_true = (bool *)calloc(sizeof(bool), Cnf::num_variables);
    Cnf::assigned_false = (bool *)calloc(sizeof(bool), Cnf::num_variables);
    int variable_id = 0;
    for (int k = 0; k < n; k++) {
        for (int row = 0; row < n; row++) {
            for (int col = 0; col < n; col++) {
                VariableLocations current_variable;
                current_variable.variable_id = variable_id;
                current_variable.variable_row = row;
                current_variable.variable_col = col;
                current_variable.variable_k = k;
                Queue clauses_containing;
                Queue *clauses_containing_ptr = (Queue *)malloc(sizeof(Queue));
                *clauses_containing_ptr = clauses_containing;
                current_variable.clauses_containing = clauses_containing_ptr;
                Cnf::variables[variable_id] = current_variable;
                variable_id++;
            }
        }
    }
    int var1;
    int var2;
    int variable_num = 0;
    // For each position, there is at most one k value true
    // Exactly n^3(1/2)(n-1) clauses
    for (int row = 0; row < n; row++) {
        for (int col = 0; col < n; col++) {
            for (int k1 = 0; k1 < (n - 1); k1++) {
                for (int k2 = k1 + 1; k2 < n; k2++) {
                    var1 = ((n_squ) * k1) + variable_num;
                    var2 = ((n_squ) * k2) + variable_num;
                    Clause restriction = make_small_clause(
                        var1, var2, false, false);
                    add_clause(restriction, Cnf::clauses, Cnf::variables);
                }
            }
            variable_num++;
        }
    }
    // For each position, there is at leadt one k value true
    // Exactly n^2 clauses
    for (int row = 0; row < n; row++) {
        for (int col = 0; col < n; col++) {
            int variable_offset = (row * n) + col;
            Clause at_least_one;
            at_least_one.num_literals = n;
            at_least_one.literal_variable_ids = (int *)malloc(sizeof(int) * n);
            at_least_one.literal_signs = (bool *)malloc(sizeof(int) * n);
            for (int k = 0; k < n; k++) {
                at_least_one.literal_variable_ids[k] = 
                    variable_offset + (k * n_squ);
                at_least_one.literal_signs[k] = true;
            }
            add_clause(at_least_one, Cnf::clauses, Cnf::variables);
        }
    }
    // For each row and k, there is at most one value with this k
    // Exactly n^3(1/2)(n - 1) clauses
    for (int k = 0; k < n; k++) {
        for (int row = 0; row < n; row++) {
            for (int col1 = 0; col1 < n - 1; col1++) {
                for (int col2 = col1 + 1; col2 < n; col2++) {
                    int var1 = (k * n_squ) + (row * n) + col1;
                    int var2 = (k * n_squ) + (row * n) + col2;
                    Clause restriction = make_small_clause(
                        var1, var2, false, false);
                    add_clause(restriction, Cnf::clauses, Cnf::variables);
                }
            }
        }
    }
    // For each col and k, there is at most one value with this k
    // Exactly n^3(1/2)(n - 1) clauses
    for (int k = 0; k < n; k++) {
        for (int col = 0; col < n; col++) {
            for (int row1 = 0; row1 < n - 1; row1++) {
                for (int row2 = row1 + 1; row2 < n; row2++) {
                    int var1 = (k * n_squ) + (row1 * n) + col;
                    int var2 = (k * n_squ) + (row2 * n) + col;
                    Clause restriction = make_small_clause(
                        var1, var2, false, false);
                    add_clause(restriction, Cnf::clauses, Cnf::variables);
                }
            }
        }
    }
    // For each chunk and k, there is at most one value with this k
    // Less than n^3(1/2)(n - 1) clauses
    for (int k = 0; k < n; k++) {
        for (int block_row = 0; block_row < sqrt_n; block_row++) {
            for (int block_col = 0; block_col < sqrt_n; block_col++) {
                for (int local1 = 0; local1 < n - 1; local1++) {
                    for (int local2 = local1+1; local2 < n; local2++) {
                        int local_row1 = local1 / sqrt_n;
                        int local_col1 = local1 % sqrt_n;
                        int local_row2 = local2 / sqrt_n;
                        int local_col2 = local2 % sqrt_n;
                        int row1 = (block_row * sqrt_n) + local_row1;
                        int col1 = (block_col * sqrt_n) + local_col1;
                        int row2 = (block_row * sqrt_n) + local_row2;
                        int col2 = (block_col * sqrt_n) + local_col2;
                        if (row1 == row2 || col1 == col2) {
                            continue;
                        }
                        int var1 = (k * n_squ) + (row1 * n) + col1;
                        int var2 = (k * n_squ) + (row2 * n) + col2;
                        Clause restriction = make_small_clause(
                            var1, var2, false, false);
                        add_clause(restriction, Cnf::clauses, Cnf::variables);
                    }
                }
            }
        }
    }
    printf("%d clauses added\n", Cnf::clauses.num_indexed);
    // TODO: Add sum restrictions for killer Sudoku

    Cnf::clauses_dropped = (bool *)calloc(
        sizeof(bool), Cnf::clauses.max_indexable);
    Cnf::ints_needed_for_clauses = ceil_div(
        Cnf::clauses.max_indexable, (sizeof(int) * 8));
    Cnf::ints_needed_for_vars = ceil_div(
        Cnf::num_variables, (sizeof(int) * 8));

    Cnf::depth = 0;
    Cnf::depth_str = "";
    if (PRINT_LEVEL > 0) print_cnf(0, "Current CNF", Cnf::depth_str, true);
}

// Makes CNF formula from premade data structures
Cnf::Cnf(
        IndexableDLL input_clauses, 
        VariableLocations *input_variables, 
        int num_variables) 
    {
    Cnf::n = 0;
    Cnf::clauses = input_clauses;
    Cnf::variables = input_variables;
    Cnf::num_variables = num_variables;
    Cnf::ints_needed_for_clauses = ceil_div(
        Cnf::clauses.num_indexed, (sizeof(int) * 8));
    Cnf::ints_needed_for_vars = ceil_div(
        Cnf::num_variables, (sizeof(int) * 8));
    Cnf::clauses_dropped = (bool *)calloc(
        sizeof(bool), Cnf::clauses.num_indexed);
    Cnf::assigned_true = (bool *)calloc(sizeof(bool), Cnf::num_variables);
    Cnf::assigned_false = (bool *)calloc(sizeof(bool), Cnf::num_variables);
    Cnf::depth = 0;
    Cnf::depth_str = "";
}

// Default constructor
Cnf::Cnf() {
    Cnf::n = 0;
    Cnf::num_variables = 0;
    Cnf::ints_needed_for_clauses = 0;
    Cnf::ints_needed_for_vars = 0;
    Cnf::depth = 0;
    Cnf::depth_str = "";
}

// Gets string representation of clause
std::string Cnf::clause_to_string_current(Clause clause, bool elimination) {
    std::string clause_string = "(";
    int num_displayed = 0;
    for (int lit = 0; lit < clause.num_literals; lit++) {
        int var_id = clause.literal_variable_ids[lit];
        if ((Cnf::assigned_true[var_id] || Cnf::assigned_false[var_id]) 
            && elimination) {
            continue;
        }
        if (num_displayed != 0) {
            clause_string.append(" \\/ ");
        }
        if (!(clause.literal_signs[lit])) {
            clause_string.append("!");
        }
        if (Cnf::assigned_true[var_id]) {
            clause_string.append("T");
        } else if (Cnf::assigned_false[var_id]) {
            clause_string.append("F");
        } else {
            clause_string.append(std::to_string(var_id));
        }
        num_displayed++;
    }
    clause_string.append(")");
    return clause_string;
}

// Debug print a full cnf structure
void Cnf::print_cnf(
        int caller_pid,
        std::string prefix_string, 
        std::string tab_string,
        bool elimination) 
    {
    Cnf::clauses.reset_iterator();
    std::string data_string = std::to_string(
        Cnf::clauses.get_linked_list_size());
    data_string.append(" clauses: ");

    if (!CONCISE_FORMULA) data_string.append("\n");

    int num_seen = 0;
    if (Cnf::clauses.iterator_is_finished()) {
        data_string.append("T");
    }
    while (!(Cnf::clauses.iterator_is_finished())) {
        Clause *clause_ptr = (Clause *)(Cnf::clauses.get_current_value());
        Clause clause = *clause_ptr;
        int clause_id = Cnf::clauses.get_current_index();
        Cnf::clauses.advance_iterator();
        if (CONCISE_FORMULA) {
            if (num_seen > 0) {
                data_string.append(" /\\ ");
            }
            data_string.append(clause_to_string_current(clause, elimination));
            num_seen++;
            continue;
        }
        data_string.append(tab_string);
        data_string.append("\t");
        data_string.append(std::to_string(clause_id));
        data_string.append(": ");
        data_string.append(clause_to_string_current(clause, elimination));
        data_string.append("\n");
    }
    if (!CONCISE_FORMULA) {
        data_string.append("  ");
        data_string.append(std::to_string(Cnf::num_variables));
        data_string.append(" variables:\n");
        for (int var_id = 0; var_id < Cnf::num_variables; var_id++) {
            if (elimination && 
                (Cnf::assigned_true[var_id] || Cnf::assigned_false[var_id])) {
                continue;
            }
            data_string.append(tab_string);
            data_string.append("\t");
            data_string.append(std::to_string(var_id));
            data_string.append(": contained in {");
            VariableLocations current_location = Cnf::variables[var_id];
            Queue tmp_stack;
            Queue containment_queue = *(current_location.clauses_containing);
            while (containment_queue.count > 0) {
                void *clause_ptr = containment_queue.pop_from_front();
                tmp_stack.add_to_front(clause_ptr);
                int clause_id = (*((int *)clause_ptr));
                if (Cnf::clauses_dropped[clause_id] && elimination) {
                    continue;
                }
                data_string.append("C");
                data_string.append(std::to_string(clause_id));
                if (containment_queue.count != 0) {
                    data_string.append(", ");
                }
            }
            data_string.append("}\n");
            while (tmp_stack.count > 0) {
                containment_queue.add_to_front(tmp_stack.pop_from_front());
            }
        }
    }
    if (PRINT_LEVEL >= 6) {
        data_string.append("\n");
        data_string.append(tab_string);
        data_string.append(std::to_string(Cnf::clauses.num_indexed));
        data_string.append(" indexed clauses: ");
        if (!CONCISE_FORMULA) data_string.append("\n");
        for (int clause_id = 0; clause_id < Cnf::clauses.num_indexed; clause_id++) {
            DoublyLinkedList *element_ptr = Cnf::clauses.element_ptrs[clause_id];
            DoublyLinkedList element = *element_ptr;
            Clause *clause_ptr = (Clause *)(element.value);
            Clause clause = *clause_ptr;
            if (clause_id > 0) {
                data_string.append(" /\\ ");
            }
            data_string.append(clause_to_string_current(clause, elimination));
        }
    }
    printf("%sPID %d %s %s\n", tab_string.c_str(), caller_pid, prefix_string.c_str(), data_string.c_str());
    Cnf::clauses.reset_iterator();
}

// Picks unassigned variable from the clause, returns the number of unsats
int Cnf::pick_from_clause(Clause clause, int *var_id, bool *var_sign) {
    int num_unsat = 0;
    bool picked_already = false;
    for (int i = 0; i < clause.num_literals; i++) {
        int current_var_id = clause.literal_variable_ids[i];
        bool current_var_sign = clause.literal_signs[i];
        if (Cnf::assigned_true[current_var_id] 
            || Cnf::assigned_false[current_var_id]) {
            continue;
        } else {
            if (!picked_already) {
                *var_id = current_var_id;
                *var_sign = current_var_sign;
                picked_already = true;
            }
            num_unsat++;
        }
    }
    assert(0 < num_unsat && num_unsat <= clause.num_literals);
    return num_unsat;
}

// Gets the status of a clause, 's', 'u', or 'n'.
char Cnf::check_clause(Clause clause, int *num_unsat) {
    int unsat_count = 0;
    for (int i = 0; i < clause.num_literals; i++) {
        int var_id = clause.literal_variable_ids[i];
        if (Cnf::assigned_true[var_id]) {
            if (clause.literal_signs[i]) {
                *num_unsat = 0;
                return 's';
            }
        } else if (Cnf::assigned_false[var_id]) {
            if (!(clause.literal_signs[i])) {
                *num_unsat = 0;
                return 's';
            }
        } else {
            unsat_count++;
        }
    }
    *num_unsat = unsat_count;
    if (unsat_count == 0) {
        return 'u';
    }
    return 'n';
}

// Updates formula with given assignment.
// Returns false on failure and populates conflict id.
bool Cnf::propagate_assignment(
        int var_id, 
        bool value, 
        int &conflict_id,
        Queue &edit_stack) {
    edit_stack.add_to_front(variable_edit(var_id));
    VariableLocations locations = (Cnf::variables)[var_id];
    if (value) {
        Cnf::assigned_true[var_id] = true;
    } else {
        Cnf::assigned_false[var_id] = true;
    }
    int clauses_to_check = (*(locations.clauses_containing)).count;
    if (PRINT_LEVEL >= 3) printf("%sPID %d assigned var %d |= %d, checking %d clauses\n", Cnf::depth_str.c_str(), 0, var_id, (int)value, clauses_to_check);    
    while (clauses_to_check > 0) {
        int *current = (int *)((*locations.clauses_containing).pop_from_front());
        int clause_id = *current;
        // Try to drop it
        if (!Cnf::clauses_dropped[clause_id]) {
            if (PRINT_LEVEL >= 6) printf("%sPID %d checking clause %d\n", Cnf::depth_str.c_str(), 0, clause_id);
            Clause clause = *((Clause *)((Cnf::clauses).get_value(clause_id)));
            int num_unsat;
            char new_clause_status = check_clause(clause, &num_unsat);
            switch (new_clause_status) {
                case 's': {
                    // Satisfied, can now drop
                    if (PRINT_LEVEL >= 3) printf("%sPID %d dropping clause %d %s\n", Cnf::depth_str.c_str(), 0, clause_id, clause_to_string_current(clause, false).c_str());
                    Cnf::clauses_dropped[clause_id] = true;
                    Cnf::clauses.strip_value(clause_id);
                    edit_stack.add_to_front(clause_edit(clause_id));
                    break;
                } case 'u': {
                    if (PRINT_LEVEL >= 3) printf("%sPID %d clause %d %s contains conflict\n", Cnf::depth_str.c_str(), 0, clause_id, clause_to_string_current(clause, false).c_str());
                    conflict_id = clause_id;
                    (*locations.clauses_containing).add_to_back(
                        (void *)current);
                    return false;
                } default: {
                    // At least the size changed
                    if (PRINT_LEVEL >= 3) printf("%sPID %d decreasing clause %d %s size (%d -> %d)\n", Cnf::depth_str.c_str(), 0, clause_id, clause_to_string_current(clause, false).c_str(), num_unsat + 1, num_unsat);
                    Cnf::clauses.change_size_of_value(
                        clause_id, num_unsat + 1, num_unsat);
                    edit_stack.add_to_front(size_change_edit(
                        clause_id, num_unsat + 1, num_unsat));
                    break;
                }
            }
        }
        (*locations.clauses_containing).add_to_back((void *)current);
        clauses_to_check--;
    }
    if (PRINT_LEVEL >= 4) printf("%sPID %d propagated var %d |= %d\n", Cnf::depth_str.c_str(), 0, var_id, (int)value);
    return true;
}

// Returns the assignment of variables
bool *Cnf::get_assignment() {
    return Cnf::assigned_true;
}

// Creates and writes to sudoku board from formula
short **Cnf::get_sudoku_board() {
    short **board = (short **)calloc(sizeof(short *), Cnf::n);
    for (int i = 0; i < Cnf::n; i++) {
        board[i] = (short *)calloc(sizeof(short), Cnf::n);
    }
    for (int var_id = 0; var_id < Cnf::num_variables; var_id++) {
        if (Cnf::assigned_true[var_id]) {
            VariableLocations current_location = Cnf::variables[var_id];
            int row = current_location.variable_row;
            int col = current_location.variable_col;
            int k = current_location.variable_k;
            board[row][col] = k + 1;
        }
    }
    return board;
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