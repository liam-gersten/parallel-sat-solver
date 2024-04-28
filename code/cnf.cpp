#include "cnf.h"
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <iomanip> 
#include <string>
#include <vector>
#include <cassert>
#include <cstring>
#include <cmath> 

#include <bits/stdc++.h>
#include <queue>
#include <map>

//----------------------------------------------------------------
// BEGIN IMPLEMENTATION
//----------------------------------------------------------------

// Adds a clause to data structures
void add_clause(
    Clause new_clause, 
    Clauses &clauses, 
    VariableLocations *variables) 
    {
    assert(clause_is_sorted(new_clause));
    int new_clause_id = clauses.num_indexed;
    new_clause.id = new_clause_id;
    for (int lit = 0; lit < new_clause.num_literals; lit++) {
        int variable_id = new_clause.literal_variable_ids[lit];
        (((variables[variable_id])).clauses_containing).add_to_back(
            new_clause_id);
    }
    clauses.add_regular_clause(new_clause);
}

/**
 * @brief 
 * 
 * @param vars Assumes all to be positive
 * @param length 
 * @param var_id 
 * @param beginning
 * @param newComm 
 * @param newCommSign 
 * @return std::tuple<int, bool> 
 */
std::tuple<int, bool> Cnf::oneOfClause(int* vars, int length, int &var_id, bool beginning, int newComm, bool newCommSign) {
    if (length == 1) {
        return {vars[0], true};
    } else if (length == 2 || (length == 3 && !beginning)) {
        // define new commander variable if newComm=0, or use newComm
        // if use newComm, don't increment var_id
        VariableLocations comm;
        if (newComm == -1) {
            comm.variable_id = var_id;
            // comm.variable_row = row;
            // comm.variable_col = col;
            // comm.variable_k = k;
            Cnf::true_assignment_statuses[var_id] = 'u';
            Cnf::false_assignment_statuses[var_id] = 'u';
            comm.clauses_containing;
            Cnf::variables[var_id] = comm;
            var_id++;
        } else {
            comm = Cnf::variables[newComm];
        }
        int comm_id = comm.variable_id;
        bool comm_sign = newComm == -1 ? true : newCommSign;

        // pairwise distinct
        for (int i = 0; i < length; i++) {
            for (int j = 0; j < i; j++) {
                Clause unequal = make_small_clause(vars[j], vars[i], false, false);
                add_clause(unequal, Cnf::clauses, Cnf::variables);
            }
        }
        // comm => OR vars
        Clause pos_comm;
        pos_comm.num_literals = 1+length;
        pos_comm.literal_variable_ids = (int *)malloc(sizeof(int) * pos_comm.num_literals);
        pos_comm.literal_signs = (bool *)malloc(sizeof(int) * pos_comm.num_literals);

        for (int i = 0; i < length; i++) {
            pos_comm.literal_variable_ids[i] = vars[i];
            pos_comm.literal_signs[i] = true;
        }
        pos_comm.literal_variable_ids[length] = comm_id;
        pos_comm.literal_signs[length] = true; //should be !comm_sign, but assign after sort

        std::sort(pos_comm.literal_variable_ids, pos_comm.literal_variable_ids + length+1);
        for (int i = 0; i < length+1; i++) {
            if (pos_comm.literal_variable_ids[i] == comm_id) {
                pos_comm.literal_signs[i] = !comm_sign;
            }
        }

        add_clause(pos_comm, Cnf::clauses, Cnf::variables);
        // not comm => AND not vars
        for (int i = 0; i < length; i++) {
            Clause neg_comm = make_small_clause(vars[i], comm_id, false, comm_sign);
            add_clause(neg_comm,  Cnf::clauses, Cnf::variables);
        }

        return {comm_id, comm_sign};
    } else {
        // if length odd, odd partition
        if (length % 2 == 1) {
            if (beginning) {
                auto [comm1, comm1sign] = oneOfClause(vars + 2, length-2, var_id, beginning=false);
                
                add_clause(make_small_clause(vars[0], comm1, false, !comm1sign),  Cnf::clauses, Cnf::variables);
                add_clause(make_small_clause(vars[1], comm1, false, !comm1sign),  Cnf::clauses, Cnf::variables);
                add_clause(make_small_clause(vars[0], vars[1], false, false),  Cnf::clauses, Cnf::variables);
                add_clause(make_triple_clause(vars[0], vars[1], comm1, true, true, comm1sign), Cnf::clauses, Cnf::variables);
            } else {
                auto [comm1, comm1sign] = oneOfClause(vars + 2, length-2, var_id, beginning=false);

                int children[3] = {vars[0], vars[1], comm1};
                return oneOfClause(children, 3, var_id, beginning=false, newComm=newComm, newCommSign=newCommSign);
            }
        } else {
            // even length: 2-partition into odd numbers
            if (beginning) {
                oneOfClause(vars+1, length-1, var_id, beginning=false, newComm=vars[0], newCommSign=false);
            } else {
                auto [comm2, comm2sign] = oneOfClause(vars+1, length-1, var_id, beginning=false);
                int children[2] = {vars[0], comm2};
                return oneOfClause(children, 2, var_id, beginning=false);
            }
        }
    }
    return {-1, false};
}

/**
 * @brief Get id from [0,n**3)
 * 
 * @param i row
 * @param j col
 * @param k digit
 * @param n 
 * @return int 
 */
int getRegularVariable(int i, int j, int k, int n) {
    return k*n*n + i*n + j;
}

int getSubcolID(
        int col, 
        int xbox, 
        int ybox, 
        int k, 
        int sqrt_n, 
        int start, 
        int spacing) {
    int raw = k*(sqrt_n*sqrt_n*sqrt_n) + col * sqrt_n*sqrt_n + xbox * sqrt_n + ybox;
    return start + spacing*raw;
}

int comm_num_clauses(int n) {
    if (n == 2) {return 1;}
    return ceil(7*(n/2)-1);
}

int comm_num_vars(int n, bool begin) {
    int ans = begin ? std::ceil(n/2.) - 2 : std::floor(n/2.);
    return ans;
}

// Makes CNF formula from inputs
Cnf::Cnf(
        short pid,
        short nprocs,
        int **constraints, 
        int n, 
        int sqrt_n, 
        int num_constraints,
        int num_assignments,
        int reduction_method,
        GridAssignment *assignments) 
    {
    Cnf::n = n;
    Cnf::num_conflict_to_hold = n * n * n * n;
    Cnf::edit_stack;
    Cnf::edit_counts_stack;
    Cnf::pid = pid;
    Cnf::nprocs = nprocs;
    Cnf::local_edit_count = 0;
    Cnf::num_vars_assigned = 0;
    Cnf::reduction_method = reduction_method;

    switch (reduction_method) {
        case (0): {
            reduce_puzzle_original(n, sqrt_n, num_assignments, assignments);
            break;
        } case (1): {
            reduce_puzzle_clauses_truncated(n, sqrt_n, num_assignments, assignments);
            break;
        }
    }

    if (pid == 0) {
        printf("%d clauses added\n", Cnf::clauses.num_indexed);
        printf("%d variables added\n", Cnf::num_variables);
    }
    Cnf::depth = 0;
    Cnf::depth_str = "";
    init_compression();
    if (PRINT_LEVEL > 1) print_cnf("Current CNF", Cnf::depth_str, true);
    for (int i = 0; i < num_constraints; i++) {
        free(constraints[i]);
    }
    free(constraints);
    free(assignments);
}

// Makes CNF formula from premade data structures
Cnf::Cnf(
        short pid,
        short nprocs,
        Clauses input_clauses, 
        VariableLocations *input_variables, 
        int num_variables) 
    {
    Cnf::pid = pid;
    Cnf::nprocs = nprocs;
    Cnf::n = 0;
    Cnf::clauses = input_clauses;
    Cnf::variables = input_variables;
    Cnf::num_variables = num_variables;
    Cnf::num_conflict_to_hold = 0;
    Cnf::edit_stack;
    Cnf::edit_counts_stack;
    Cnf::local_edit_count = 0;
    Cnf::depth = 0;
    Cnf::depth_str = "";
    Cnf::num_vars_assigned = 0;
    Cnf::reduction_method = 0;
    Task recently_undone_assignment;
    Cnf::true_assignment_statuses = (char *)calloc(
        sizeof(char), Cnf::num_variables);
    Cnf::false_assignment_statuses = (char *)calloc(
        sizeof(char), Cnf::num_variables);
    memset(Cnf::true_assignment_statuses, 'u', Cnf::num_variables);
    memset(Cnf::false_assignment_statuses, 'u', Cnf::num_variables);
    init_compression();
}

// Default constructor
Cnf::Cnf() {
    Cnf::n = 0;
    Cnf::num_variables = 0;
    Cnf::num_conflict_to_hold = 0;
    Cnf::ints_needed_for_clauses = 0;
    Cnf::ints_needed_for_vars = 0;
    Cnf::work_ints = 0;
    Cnf::local_edit_count = 0;
    Cnf::depth = 0;
    Cnf::depth_str = "";
    Cnf::num_vars_assigned = 0;
    Cnf::reduction_method = 0;
    Task recently_undone_assignment;
}

void Cnf::reduce_puzzle_clauses_truncated(int n, int sqrt_n, int num_assignments, GridAssignment *assignments) {
    int n_squ = n * n;
    Cnf::num_variables = std::pow(n,3) + n*n*sqrt_n*comm_num_vars(sqrt_n, false) + 2*n*n*(comm_num_vars(sqrt_n) + comm_num_vars(n));
    Cnf::true_assignment_statuses = (char *)calloc(
        sizeof(char), Cnf::num_variables);
    Cnf::false_assignment_statuses = (char *)calloc(
        sizeof(char), Cnf::num_variables);
    // Exact figure
    int num_clauses = n_squ*sqrt_n * comm_num_clauses(sqrt_n+1) + 2*n_squ*comm_num_clauses(n) + 2*n_squ*comm_num_clauses(sqrt_n) + num_assignments;
    Clauses clauses(num_clauses, Cnf::num_conflict_to_hold);
    Cnf::clauses = clauses;
    Cnf::variables = (VariableLocations *)malloc(
        sizeof(VariableLocations) * Cnf::num_variables);
    
    int variable_id = 0;
    for (int k = 0; k < n; k++) {
        for (int col = 0; col < n; col++) {
            for (int row = 0; row < n; row++) {
                VariableLocations current_variable;
                current_variable.variable_id = variable_id;
                current_variable.variable_row = row;
                current_variable.variable_col = col;
                current_variable.variable_k = k;
                Cnf::true_assignment_statuses[variable_id] = 'u';
                Cnf::false_assignment_statuses[variable_id] = 'u';
                current_variable.clauses_containing;
                Cnf::variables[variable_id] = current_variable;
                variable_id++;
            }
        }
    }
    int var1;
    int var2;
    int vars[n];
    int variable_num = 0;

    //probably works: tested for n<=5
    int subcol_id_spacing = floor(sqrt_n / 2.);
    int start = n*n*n + subcol_id_spacing - 1;

    // subcols
    for (int k = 0; k < n; k++) { 
        for (int col = 0; col < sqrt_n; col++) {
            for (int xbox = 0; xbox < sqrt_n; xbox++) {
                for (int ybox = 0; ybox < sqrt_n; ybox++) {  
                    for (int j = 0; j < sqrt_n; j++) {
                        vars[j] = getRegularVariable(xbox*sqrt_n + col, ybox*sqrt_n + j, k, n);
                    }
                    auto [subcol_id, _] = oneOfClause(vars, sqrt_n, variable_id, false);
                    // printf("%d, %d, %d, %d: %d vs %d\n", k, col, xbox, ybox, subcol_id, getSubcolID(col, xbox, ybox, k, sqrt_n, start, subcol_id_spacing));
                    assert(subcol_id == getSubcolID(col, xbox, ybox, k, sqrt_n, start, subcol_id_spacing));
                }
            }
        }
    }
    if (PRINT_LEVEL > 4) printf("done w subcols, %d\n", variable_id);
    // Each cell has one digit
    for (int row = 0; row < n; row++) {
        for (int col = 0; col < n; col++) {
            for (int k = 0; k < n; k++) {
                vars[k] = getRegularVariable(col, row, k, n);
            }
            oneOfClause(vars, n, variable_id);
        }
    }
    if (PRINT_LEVEL > 4) printf("cell uniq, %d\n", variable_id);
    // Each row has exactly one of each digit
    for (int k = 0; k < n; k++) {
        for (int row = 0; row < n; row++) {
            for (int col = 0; col < n; col++) {
                vars[col] = getRegularVariable(col, row, k, n);
            }
            oneOfClause(vars, n, variable_id);
        }
    }
    if (PRINT_LEVEL > 4) printf("row uniq, %d\n", variable_id);
    // cols
    for (int k = 0; k < n; k++) {
        for (int col = 0; col < sqrt_n; col++) {
            for (int xbox = 0; xbox < sqrt_n; xbox++) {
                for (int ybox = 0; ybox < sqrt_n; ybox++) {
                    vars[ybox] = getSubcolID(col, xbox, ybox, k, sqrt_n, start, subcol_id_spacing);
                }
                oneOfClause(vars, sqrt_n, variable_id);
            }
        }
    }
    if (PRINT_LEVEL > 4) printf("col uniq, %d\n", variable_id);
    // For each chunk and k, there is at most one value with this k
    for (int k = 0; k < n; k++) {
        for (int xbox = 0; xbox < sqrt_n; xbox++) {
            for (int ybox = 0; ybox < sqrt_n; ybox++) {
                for (int col = 0; col < sqrt_n; col++) {
                    vars[col] = getSubcolID(col, xbox, ybox, k, sqrt_n, start, subcol_id_spacing);
                }
                oneOfClause(vars, sqrt_n, variable_id);
            }
        }
    }
    if (PRINT_LEVEL > 4) printf("done, %d\n", variable_id);

    for (int i = 0; i < num_assignments; i++) {
        Clause c;
        c.num_literals = 1;
        c.literal_variable_ids = (int *)malloc(sizeof(int));
        c.literal_signs = (bool *)malloc(sizeof(int));
        c.literal_variable_ids[0] = getRegularVariable(assignments[i].row, assignments[i].col, assignments[i].value, n);
        c.literal_signs[0] = true;
        add_clause(c, Cnf::clauses, Cnf::variables);
    }
    if (PRINT_LEVEL > 4) printf("done assignments\n");
}

// Original version with lowest variable count
void Cnf::reduce_puzzle_original(
        int n, 
        int sqrt_n, 
        int num_assignments,
        GridAssignment *assignments) 
    {
    int n_squ = n * n;
    Cnf::num_variables = (n * n_squ);
    Cnf::true_assignment_statuses = (char *)calloc(
        sizeof(char), Cnf::num_variables);
    Cnf::false_assignment_statuses = (char *)calloc(
        sizeof(char), Cnf::num_variables);
    // Exact figure
    int num_clauses = (2 * n_squ * n_squ) - (2 * n_squ * n) + n_squ - ((n_squ * n) * (sqrt_n - 1)) + num_assignments;
    Clauses clauses(num_clauses, Cnf::num_conflict_to_hold);
    Cnf::clauses = clauses;
    Cnf::variables = (VariableLocations *)malloc(
        sizeof(VariableLocations) * Cnf::num_variables);
    int variable_id = 0;
    for (int k = 0; k < n; k++) {
        for (int row = 0; row < n; row++) {
            for (int col = 0; col < n; col++) {
                VariableLocations current_variable;
                current_variable.variable_id = variable_id;
                current_variable.variable_row = row;
                current_variable.variable_col = col;
                current_variable.variable_k = k;
                Cnf::true_assignment_statuses[variable_id] = 'u';
                Cnf::false_assignment_statuses[variable_id] = 'u';
                current_variable.clauses_containing;
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

    for (int i = 0; i < num_assignments; i++) {
        Clause c;
        c.num_literals = 1;
        c.literal_variable_ids = (int *)malloc(sizeof(int));
        c.literal_signs = (bool *)malloc(sizeof(int));
        c.literal_variable_ids[0] = getRegularVariable(assignments[i].row, assignments[i].col, assignments[i].value, n);
        c.literal_signs[0] = true;
        add_clause(c, Cnf::clauses, Cnf::variables);
    }
}

// Initializes CNF compression
void Cnf::init_compression() {
    Cnf::ints_needed_for_clauses = ceil_div(
        Cnf::clauses.max_indexable, (sizeof(int) * 8));
    Cnf::ints_needed_for_conflict_clauses = ceil_div(
        Cnf::clauses.max_conflict_indexable, (sizeof(int) * 8));
    Cnf::ints_needed_for_vars = ceil_div(
        Cnf::num_variables, (sizeof(int) * 8));
    int ints_per_state = (2 * Cnf::ints_needed_for_vars) + Cnf::ints_needed_for_clauses;
    Cnf::work_ints = 2 + ints_per_state;
    int start_clause_id = 0;
    for (int comp_index = 0; comp_index < Cnf::ints_needed_for_clauses; comp_index++) {
        unsigned int running_addition = 1;
        for (int clause_id = start_clause_id; clause_id < start_clause_id + 32; clause_id++) {
            if (clause_id >= Cnf::clauses.num_indexed) {
                break;
            }
            Clause *clause_ptr = Cnf::clauses.get_clause_ptr(clause_id);
            Clause clause = *clause_ptr;
            clause.clause_addition = running_addition;
            clause.clause_addition_index = (unsigned int)(comp_index + 2);
            *clause_ptr = clause;
            running_addition = running_addition << 1;
        }
        start_clause_id += 32;
    }
    int start_variable_id = 0;
    for (int comp_index = 0; comp_index < Cnf::ints_needed_for_vars; comp_index++) {
        unsigned int running_addition = 1;
        for (int var_id = start_variable_id; var_id < start_variable_id + 32; var_id++) {
            if (var_id >= Cnf::num_variables) {
                break;
            }
            VariableLocations locations = Cnf::variables[var_id];
            locations.variable_addition = running_addition;
            locations.variable_true_addition_index = 
                (unsigned int)(Cnf::ints_needed_for_clauses + comp_index + 2);
            locations.variable_false_addition_index = 
                (unsigned int)(Cnf::ints_needed_for_clauses 
                + Cnf::ints_needed_for_vars + comp_index + 2);
            Cnf::variables[var_id] = locations;
            running_addition = running_addition << 1;
        }
        start_variable_id += 32;
    }
    Cnf::assigned_true = (bool *)calloc(
        sizeof(int), Cnf::ints_needed_for_vars * 8);
    Cnf::assigned_false = (bool *)calloc(
        sizeof(int), Cnf::ints_needed_for_vars * 8);
    Cnf::assignment_times = (int *)calloc(sizeof(int), Cnf::num_variables);
    Cnf::assignment_depths = (int *)calloc(sizeof(int), Cnf::num_variables);
    Cnf::current_time = 0;
    Cnf::oldest_compressed = to_int_rep();
    if (PRINT_LEVEL > 2) print_compressed(Cnf::pid, "", "", Cnf::oldest_compressed, Cnf::work_ints);
}

// Testing method only
bool Cnf::clauses_equal(Clause a, Clause b) {
    // Assumes sorted order of variable ids in both clauses
    if (a.id == b.id) {
        return true;
    } else if (a.num_literals != b.num_literals) {
        return false;
    }
    for (int i = 0; i < a.num_literals; i++) {
        if (a.literal_variable_ids[i] != b.literal_variable_ids[i]) {
            return false;
        } else if (a.literal_signs[i] != b.literal_signs[i]) {
            return false;
        }
    }
    return true;
}

// Returns whethere every variable has at most one truth value
bool Cnf::valid_truth_assignments() {
    for (int var_id = 0; var_id < Cnf::num_variables; var_id++) {
        bool true_status = Cnf::assigned_true[var_id];
        bool false_status = Cnf::assigned_false[var_id];
        if (true_status && false_status) {
            return false;
        }
    }
    return true;
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
        clause_string.append(std::to_string(var_id));
        num_displayed++;
        if (num_displayed >= 100) {
            clause_string.append(" ... ");
            break;
        }
    }
    clause_string.append(")");
    if (clause.num_literals > 15) {
        clause_string.append(" len = ");
        clause_string.append(std::to_string(clause.num_literals));
    }
    return clause_string;
}

// Debug print a full cnf structure
void Cnf::print_cnf(
        std::string prefix_string, 
        std::string tab_string,
        bool elimination) 
    {
    Cnf::clauses.reset_iterator();
    std::string data_string = "CNF ";
    data_string.append(std::to_string(
        Cnf::clauses.get_linked_list_size()));
    data_string.append(" clauses: ");

    if (!PRINT_CONCISE_FORMULA) data_string.append("\n");

    int num_seen = 0;
    if (Cnf::clauses.iterator_is_finished()) {
        data_string.append("T");
    }
    while (!(Cnf::clauses.iterator_is_finished())) {
        if (num_seen > 10) {
            data_string.append(" ... ");
            break;
        }
        Clause clause = Cnf::clauses.get_current_clause();
        int clause_id = clause.id;
        Cnf::clauses.advance_iterator();
        if (PRINT_CONCISE_FORMULA) {
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
    if (!PRINT_CONCISE_FORMULA) {
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
            IntDeque tmp_stack;
            IntDeque containment_queue = current_location.clauses_containing;
            while (containment_queue.count > 0) {
                int clause_id = containment_queue.pop_from_front();
                tmp_stack.add_to_front(clause_id);
                if (Cnf::clauses.clause_is_dropped(clause_id) && elimination) {
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
            tmp_stack.free_deque();
        }
    }
    if (PRINT_LEVEL >= 6) {
        data_string.append("\n");
        data_string.append(tab_string);
        data_string.append(std::to_string(Cnf::clauses.num_indexed));
        data_string.append(" indexed clauses: ");
        if (!PRINT_CONCISE_FORMULA) data_string.append("\n");
        for (int clause_id = 0; clause_id < Cnf::clauses.num_indexed; clause_id++) {
            Clause clause = Cnf::clauses.get_clause(clause_id);
            if (clause_id > 0) {
                data_string.append(" /\\ ");
            }
            data_string.append(clause_to_string_current(clause, elimination));
        }
    }
    printf("%sPID %d: %s %s\n", tab_string.c_str(), Cnf::pid, prefix_string.c_str(), data_string.c_str());
    Cnf::clauses.reset_iterator();
}

// Prints out the task stack
void Cnf::print_task_stack(
        std::string prefix_string, 
        Deque &task_stack, 
        int upto) 
    {
    std::string data_string = std::to_string(task_stack.count);
    data_string.append(" tasks: [");
    Queue tmp_stack;
    int task_count = 0;
    while (task_stack.count > 0) {
        if (task_count > upto) {
            data_string.append(" ... ");
            break;
        }
        void *task_ptr = task_stack.pop_from_front();
        tmp_stack.add_to_front(task_ptr);
        Task task = *((Task *)task_ptr);
        data_string.append("(");
        if (task.is_backtrack) {
            data_string.append("B");
            data_string.append(std::to_string(task.var_id));
        } else {
            data_string.append(std::to_string(task.var_id));
            data_string.append("=");
            if (task.assignment) {
                data_string.append("T");
            } else {
                data_string.append("F");
            }
        }
        data_string.append(")");
        if (task_stack.count != 0) {
            data_string.append(", ");
        }
        task_count++;
    }
    data_string.append("]");
    while (tmp_stack.count > 0) {
        task_stack.add_to_front(tmp_stack.pop_from_front());
    }
    printf("%sPID %d: %s %s\n", Cnf::depth_str.c_str(), Cnf::pid, prefix_string.c_str(), data_string.c_str());
}

// Prints out the task stack
void Cnf::print_edit_stack(
        std::string prefix_string, 
        Deque &edit_stack, 
        int upto) 
    {
    std::string data_string = std::to_string(edit_stack.count);
    data_string.append(" edits: [");
    Queue tmp_stack;
    int current_count = 0;
    while (edit_stack.count > 0) {
        if (current_count > upto) {
            data_string.append(" ... ");
            break;
        }
        FormulaEdit *edit_ptr = (FormulaEdit *)edit_stack.pop_from_front();
        tmp_stack.add_to_front(edit_ptr);
        FormulaEdit edit = *edit_ptr;
        data_string.append("(");
        if (edit.edit_type == 'v') {
            data_string.append("Set ");
            data_string.append(std::to_string(edit.edit_id));
        } else if (edit.edit_type == 'c') {
            data_string.append("Drop ");
            data_string.append(std::to_string(edit.edit_id));
        } else {
            data_string.append("Decr ");
            data_string.append(std::to_string(edit.edit_id));
        }
        data_string.append(")");
        if (edit_stack.count != 0) {
            data_string.append(", ");
        }
        current_count++;
    }
    data_string.append("]");
    while (tmp_stack.count > 0) {
        edit_stack.add_to_front(tmp_stack.pop_from_front());
    }
    printf("%sPID %d: %s %s\n", Cnf::depth_str.c_str(), Cnf::pid, prefix_string.c_str(), data_string.c_str());
}

// Returns whether a there is already a decided variable set edit.
// Populates passed var_id with the decided one found if so.
bool Cnf::decided_variable_in_local_edits(int *var_id) {
    void **edit_list = Cnf::edit_stack.as_list();
    for (int i = 0; i < Cnf::local_edit_count; i++) {
        FormulaEdit edit = *((FormulaEdit *)(edit_list[i]));
        if (edit.edit_type != 'v') {
            continue;
        }
        if (Cnf::variables[edit.edit_id].implying_clause_id == -1) {
            *var_id = edit.edit_id;
            return true;
        }
    }
    return false;
}

// Adds edit edit to edit stack, checks assertion
void Cnf::add_to_edit_stack(void *raw_edit) {
    FormulaEdit edit = *((FormulaEdit *)raw_edit);
    if (edit.edit_type == 'v') {
        if (Cnf::variables[edit.edit_id].implying_clause_id == -1) {
            int existing_var_id; 
            assert(!decided_variable_in_local_edits(&existing_var_id));  
        }
    }
    Cnf::edit_stack.add_to_front(raw_edit);
    Cnf::local_edit_count++;
}

// Gets the status of a variable decision (task)
char Cnf::get_decision_status(Assignment decision) {
    if (decision.value) {
        return Cnf::true_assignment_statuses[decision.var_id];
    } else {
        return Cnf::false_assignment_statuses[decision.var_id];
    }
}

// Gets the status of a variable decision (task)
char Cnf::get_decision_status(Task decision) {
    if (decision.assignment) {
        return Cnf::true_assignment_statuses[decision.var_id];
    } else {
        return Cnf::false_assignment_statuses[decision.var_id];
    }
}

// Returns whether we have a valid lowest bad decision
bool Cnf::valid_lbd(
        Deque decided_conflict_literals, 
        Task lowest_bad_decision) 
    {
    int decision_var_id = lowest_bad_decision.var_id;
    int decision_time = Cnf::assignment_times[decision_var_id];
    void **conflict_literal_list = decided_conflict_literals.as_list();
    for (int i = 0; i < decided_conflict_literals.count; i++) {
        void *decision_ptr = conflict_literal_list[i];
        Assignment good_decision = *((Assignment *)decision_ptr);
        int var_id = good_decision.var_id;
        if (var_id == decision_var_id) {
            continue;
        }
        int other_time = Cnf::assignment_times[var_id];
        if (other_time >= decision_time) {
            printf("Alleged lowest bad decision is variable %d with assignment time %d\n", decision_var_id, decision_time);
            printf("Actual lowest bad decision is variable %d with assignment time %d\n", var_id, other_time);
            return false;
        }
    }
    return true;
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
    assert(num_unsat <= clause.num_literals);
    return num_unsat;
}

// Just returns the number of unsatisfied literals in a clause
int Cnf::get_num_unsat(Clause clause) {
    int dummy_1;
    bool dummy_2;
    return pick_from_clause(clause, &dummy_1, &dummy_2);
}

// Gets the status of a clause, 's', 'u', or 'n'.
char Cnf::check_clause(Clause clause, int *num_unsat, bool from_propagate) {
    // if from_propagate, how many relevant variables are we expecting?
    int cid = clause.id;
    int old_num_unsat;
    if (cid < Cnf::clauses.num_indexed) {
        old_num_unsat = Cnf::clauses.normal_clauses.element_counts[cid];
    } else {
        cid -= Cnf::clauses.num_indexed;
        old_num_unsat = Cnf::clauses.conflict_clauses.element_counts[cid];
    }

    int unsat_count = 0; //0 if clause is true; otherwise, number of undetermined
    for (int i = 0; i < clause.num_literals; i++) {
        int var_id = clause.literal_variable_ids[i];
        if (Cnf::assigned_true[var_id]) {
            assert(!Cnf::assigned_false[var_id]);
            if (clause.literal_signs[i]) {
                *num_unsat = 0;
                return 's';
            }
        } else if (Cnf::assigned_false[var_id]) {
            assert(!Cnf::assigned_true[var_id]);
            if (!(clause.literal_signs[i])) {
                *num_unsat = 0;
                return 's';
            }
        } else {
            unsat_count++;
        }
    }
    *num_unsat = unsat_count;
    if (!(unsat_count <= old_num_unsat || old_num_unsat == 0)) {
        printf("AAH: %d, %d\n", unsat_count, old_num_unsat);
    }
    if (unsat_count == 0) {
        return 'u';
    }
    return 'n';
}

// A shortened version of a conflict clause containing only the literals
// (Assignment structs) that are decided, not unit propagated.
Deque Cnf::get_decided_conflict_literals(Clause conflict_clause) {
    Deque result;
    for (int i = 0; i < conflict_clause.num_literals; i++) {
        int var_id = conflict_clause.literal_variable_ids[i];
        if (Cnf::variables[var_id].implying_clause_id == -1) {
            bool assignment_value = conflict_clause.literal_signs[i];
            void *assignment = make_assignment(var_id, assignment_value);
            result.add_to_front(assignment);
        }
    }
    return result;
}

// Returns whether a clause already exists
bool Cnf::clause_exists_already(Clause new_clause) {
    // NICE: make this faster
    // Check it is not equal to an existing conflict clause
    for (int clause_id = Cnf::clauses.max_indexable; clause_id < Cnf::clauses.max_indexable + Cnf::clauses.num_conflict_indexed; clause_id++) {
        Clause other_clause = Cnf::clauses.get_clause(clause_id);
        if (clauses_equal(new_clause, other_clause)) {
            // Why would we get the same conflict clause twice?
            printf("Clause equal to %d\n", clause_id);
            assert(!Cnf::clauses.clause_is_dropped(clause_id));
            assert(Cnf::nprocs > 1);
            return true;
        }
    }
    // Check it is not equal to an existing normal clause
    for (int clause_id = 0; clause_id < Cnf::clauses.num_indexed; clause_id++) {
        Clause other_clause = Cnf::clauses.get_clause(clause_id);
        if (clauses_equal(new_clause, other_clause)) {
            return true;
        }
    }
    return false;
}

// Returns whether a clause is fully assigned, populates the result w eval
bool Cnf::clause_satisfied(Clause clause, bool *result) {
    bool running_value = true;
    for (int i = 0; i < clause.num_literals; i++) {
        int var_id = clause.literal_variable_ids[i];
        bool expected_value = clause.literal_signs[i];
        bool actual_value;
        if (Cnf::assigned_true[var_id]) {
            actual_value = true;
        } else if (Cnf::assigned_false[var_id]) {
            actual_value = false;
        } else {
            return false;
        }
        if (expected_value == actual_value) {
            continue;
        } else {
            running_value = false;
        }
    }
    *result = running_value;
    return true;
}

// Returns whether a clause id is for a conflict clause
bool Cnf::is_conflict_clause(int clause_id) {
    return (clause_id >= Cnf::clauses.max_indexable);
}

// Resolves two clauses, returns the resulting clause
Clause Cnf::resolve_clauses(Clause A, Clause B, int variable) {
    assert(clause_is_sorted(A));
    assert(clause_is_sorted(B));
    assert(A.num_literals + B.num_literals >= 3);
    if (PRINT_LEVEL > 1) printf("%s\tPID %d: resolving clauses\n\t\t%sC%d %s\n%s\t\t\twith %d\n\t\t%sC%d %s\n", Cnf::depth_str.c_str(), Cnf::pid, Cnf::depth_str.c_str(), A.id, clause_to_string_current(A, false).c_str(), Cnf::depth_str.c_str(), variable, Cnf::depth_str.c_str(), B.id, clause_to_string_current(B, false).c_str());  

    int num = A.num_literals + B.num_literals - 2;
    int *ids = (int *)malloc(sizeof(int) * num);
    bool *signs = (bool *)malloc(sizeof(bool) * num);
    short i = 0;
        
    short a_index = 0;
    short b_index = 0;
    while (a_index < A.num_literals && b_index < B.num_literals) {
        int var_id_a = A.literal_variable_ids[a_index];
        int var_id_b = B.literal_variable_ids[b_index];
        if (var_id_a == variable && var_id_b == variable) {
            a_index++;
            b_index++;
            continue;
        } else if (var_id_a == variable) {
            a_index++;
            continue;
        } else if (var_id_b == variable) {
            b_index++;
            continue;
        }
        bool var_sign_a = A.literal_signs[a_index];
        bool var_sign_b = B.literal_signs[b_index];
        int var_id;
        bool var_sign;
        if (var_id_a < var_id_b) {
            var_id = var_id_a;
            var_sign = var_sign_a;
            a_index++;
        } else if (var_id_b < var_id_a) {
            var_id = var_id_b;
            var_sign = var_sign_b;
            b_index++;
        } else {
            var_id = var_id_a;
            var_sign = var_sign_a;
            a_index++;
            b_index++;
        }
        ids[i] = var_id;
        signs[i] = var_sign;
        i++;
    }
    while (a_index < A.num_literals) {
        int var_id_a = A.literal_variable_ids[a_index];
        if (var_id_a == variable) {
            a_index++;
            continue;
        }
        bool var_sign_a = A.literal_signs[a_index];
        ids[i] = var_id_a;
        signs[i] = var_sign_a;
        a_index++;
        i++;
    }
    while (b_index < B.num_literals) {
        int var_id_b = B.literal_variable_ids[b_index];
        if (var_id_b == variable) {
            b_index++;
            continue;
        }
        bool var_sign_b = B.literal_signs[b_index];
        ids[i] = var_id_b;
        signs[i] = var_sign_b;
        b_index++;
        i++;
    }

    Clause result;
    result.id = -1;
    // resize if necessary
    if (i != num) {
        assert(i < num);

        result.num_literals = i;
        result.literal_variable_ids = (int *)malloc(sizeof(int) * i);
        result.literal_signs = (bool *)malloc(sizeof(bool) * i);
        memcpy(result.literal_variable_ids, ids, i*sizeof(int));
        memcpy(result.literal_signs, signs, i*sizeof(bool));
        free(ids);
        free(signs);
    } else {
        result.num_literals = num;
        result.literal_variable_ids = ids;
        result.literal_signs = signs;
    }

    if (PRINT_LEVEL > 2) printf("\t%sPID %d: resolved clause = %s\n", Cnf::depth_str.c_str(), Cnf::pid, clause_to_string_current(result, false).c_str());
    assert(clause_is_sorted(result));
    return result;
}

// Sorts (in place) the variables by decreasing assignment time
void Cnf::sort_variables_by_assignment_time(int *variables, int num_vars) {
    // NICE: quicksort
    IntDeque tmp;
    for (int i = 0; i < num_vars; i++) {
        tmp.add_to_back(variables[i]);
    }
    for (int i = 0; i < num_vars; i++) {
        IntDoublyLinkedList *current = (*tmp.head).next;
        IntDoublyLinkedList *max_ptr = current;
        int max_var_id = (*current).value;
        int max_value = Cnf::assignment_times[max_var_id];
        current = (*current).next;
        for (int j = 1; j < tmp.count; j++) {
            int var_id = (*current).value;
            int value = Cnf::assignment_times[var_id];
            if (value > max_value) {
                max_var_id = var_id;
                max_value = value;
                max_ptr = current;
            }
            current = (*current).next;
        }
        IntDoublyLinkedList *prev = (*max_ptr).prev;
        IntDoublyLinkedList *next = (*max_ptr).next;
        (*prev).next = next;
        (*next).prev = prev;
        tmp.count--;
        variables[i] = max_var_id;
    }
}

// Performs resoltion, populating the result clause.
// Returns whether a result could be generated.
bool Cnf::conflict_resolution(int culprit_id, Clause &result) {
    Clause conflict_clause = Cnf::clauses.get_clause(culprit_id);
    if (PRINT_LEVEL > 1) { 
        std::string data_string = "(";
        for (int i = 0; i < conflict_clause.num_literals; i++) {
            int resolve_variable_id = conflict_clause.literal_variable_ids[i];
            VariableLocations locations = Cnf::variables[resolve_variable_id];
            if (i > 0) {
                data_string.append(", ");
            }
            data_string.append(std::to_string(locations.implying_clause_id));
        }
        data_string.append(")");
        printf("%sPID %d: resolving conflict clause %d %s, implying = %s\n", Cnf::depth_str.c_str(), Cnf::pid, culprit_id, clause_to_string_current(conflict_clause, false).c_str(), data_string.c_str());
    }
    result = copy_clause(conflict_clause);
    int *propagated_variables = (int *)malloc(
        sizeof(int) * conflict_clause.num_literals);
    int num_propagated_variables = 0;
    for (int i = 0; i < conflict_clause.num_literals; i++) {
        int var_id = conflict_clause.literal_variable_ids[i];
        VariableLocations locations = Cnf::variables[var_id];
        if (locations.implying_clause_id != -1) {
            propagated_variables[num_propagated_variables] = var_id;
            num_propagated_variables++;
        }
    }
    if (num_propagated_variables == 0) {
        free(propagated_variables);
        return false;
    }
    sort_variables_by_assignment_time(
        propagated_variables, num_propagated_variables);
    for (int i = 0; i < num_propagated_variables; i++) {
        int var_id = propagated_variables[i];
        VariableLocations locations = Cnf::variables[var_id];
        Clause implying_clause = Cnf::clauses.get_clause(
            locations.implying_clause_id);
        Clause new_result = resolve_clauses(result, implying_clause, var_id);
        free_clause(result);
        result = new_result;
    }
    if (PRINT_LEVEL > 0) printf("%sPID %d: generated conflict clause = %s\n", Cnf::depth_str.c_str(), Cnf::pid, clause_to_string_current(result, false).c_str());
    free(propagated_variables);
    return true;
}

// Populates result clause with 1UID conflict clause
// Returns whether a result could be generated.
bool Cnf::conflict_resolution_uid_old(int culprit_id, Clause &result, int decided_var_id) {
    Clause conflict_clause = Cnf::clauses.get_clause(culprit_id);
    assert(clause_is_sorted(conflict_clause));
    if (PRINT_LEVEL > 1) { 
        std::string data_string = "(";
        for (int i = 0; i < conflict_clause.num_literals; i++) {
            int resolve_variable_id = conflict_clause.literal_variable_ids[i];
            VariableLocations locations = Cnf::variables[resolve_variable_id];
            if (i > 0) {
                data_string.append(", ");
            }
            data_string.append(std::to_string(locations.implying_clause_id));
        }
        data_string.append(")");
        printf("%sPID %d: resolving conflict clause %d %s, implying = %s\n", Cnf::depth_str.c_str(), Cnf::pid, culprit_id, clause_to_string_current(conflict_clause, false).c_str(), data_string.c_str());
    }

    std::map<int, int> lit_to_time; // in ascending order of assignment time

    int last_decided_depth = Cnf::assignment_depths[decided_var_id];
    int current_cycle_variables = 0;
    for (int i = 0; i < conflict_clause.num_literals; i++) {
        int lit = conflict_clause.literal_variable_ids[i];
        int time = Cnf::assignment_times[lit];
        int depth = Cnf::assignment_depths[lit];
        lit_to_time.insert({time, lit});
        if (depth >= last_decided_depth) {
            current_cycle_variables++;
        }
    }

    result = copy_clause(conflict_clause);
    assert(clause_is_sorted(result));
    if (current_cycle_variables < 1) {
        printf("bad cl: %s", clause_to_string_current(result, false).c_str());
        printf("depths: %d, %d:: %d\n", Cnf::assignment_depths[451], Cnf::assignment_depths[7794], last_decided_depth);
        printf("\n");
    }
    assert(current_cycle_variables >= 1);
    while (current_cycle_variables > 1) {
        // replace latest u-propped guy with rest of clause [must be u-propped]
        auto iter = lit_to_time.rbegin();
        int u = iter->second; //take latest literal
        int u_time = iter->first;
        lit_to_time.erase(u_time);
        assert(Cnf::assignment_depths[u] == last_decided_depth);
        current_cycle_variables--;
        
        VariableLocations locations = Cnf::variables[u];
        Clause implying_clause = Cnf::clauses.get_clause(
            locations.implying_clause_id);
        assert(clause_is_sorted(implying_clause));
        assert(clause_is_sorted(result));
        Clause new_result = resolve_clauses(result, implying_clause, u);
        free_clause(result);
        result = new_result;

        // add relevant vars from implying_clause
        for (int i = 0; i < implying_clause.num_literals; i++) {
            if (implying_clause.literal_variable_ids[i] == u) continue;
            int lit = implying_clause.literal_variable_ids[i];
            int depth = Cnf::assignment_depths[lit];
            int time = Cnf::assignment_times[lit];
            
            auto [_, success] = lit_to_time.insert({time, lit});
            if (success && depth >= last_decided_depth) {
                current_cycle_variables++;
            }
        }
    }
    // TODO: when should we keep/discard conflict clause?
    if (result.num_literals > CONFLICT_CLAUSE_SIZE*Cnf::n 
    || Cnf::clauses.num_conflict_indexed - 1 == Cnf::clauses.max_conflict_indexable) {
        free_clause(result);
        return false;
    }
    return true;
}

bool cmp_assignments(Assignment a, Assignment b) {
    return a.var_id < b.var_id;
}
// Populates result clause with 1UID conflict clause
// Returns whether a result could be generated.
bool Cnf::conflict_resolution_uid(int culprit_id, Clause &result, int decided_var_id) {
    Clause conflict_clause = Cnf::clauses.get_clause(culprit_id);
    assert(clause_is_sorted(conflict_clause));
    if (PRINT_LEVEL > 1) { 
        std::string data_string = "(";
        for (int i = 0; i < conflict_clause.num_literals; i++) {
            int resolve_variable_id = conflict_clause.literal_variable_ids[i];
            VariableLocations locations = Cnf::variables[resolve_variable_id];
            if (i > 0) {
                data_string.append(", ");
            }
            data_string.append(std::to_string(locations.implying_clause_id));
        }
        data_string.append(")");
        printf("%sPID %d: resolving conflict clause %d %s, implying = %s\n", Cnf::depth_str.c_str(), Cnf::pid, culprit_id, clause_to_string_current(conflict_clause, false).c_str(), data_string.c_str());
    }

    std::map<int, Assignment> lit_to_time; // in ascending order of assignment time

    int last_decided_depth = Cnf::assignment_depths[decided_var_id];
    int current_cycle_variables = 0;
    for (int i = 0; i < conflict_clause.num_literals; i++) {
        Assignment lit;
        lit.var_id = conflict_clause.literal_variable_ids[i];
        lit.value = conflict_clause.literal_signs[i];

        int time = Cnf::assignment_times[lit.var_id];
        int depth = Cnf::assignment_depths[lit.var_id];
        lit_to_time.insert({time, lit});
        if (depth >= last_decided_depth) {
            current_cycle_variables++;
        }
    }

    if (current_cycle_variables < 1) {
        printf("bad cl: %s", clause_to_string_current(result, false).c_str());
        printf("depths: %d, %d:: %d\n", Cnf::assignment_depths[451], Cnf::assignment_depths[7794], last_decided_depth);
        printf("\n");
    }
    assert(current_cycle_variables >= 1);
    while (current_cycle_variables > 1) {
        // replace latest u-propped guy with rest of clause [must be u-propped]
        auto iter = lit_to_time.rbegin();
        Assignment u = iter->second; //take latest literal
        int u_time = iter->first;
        lit_to_time.erase(u_time);
        assert(Cnf::assignment_depths[u.var_id] == last_decided_depth);
        current_cycle_variables--;
        
        VariableLocations locations = Cnf::variables[u.var_id];
        Clause implying_clause = Cnf::clauses.get_clause(
            locations.implying_clause_id);

        // add relevant vars from implying_clause
        for (int i = 0; i < implying_clause.num_literals; i++) {
            if (implying_clause.literal_variable_ids[i] == u.var_id) continue;
            Assignment lit;
            lit.var_id = implying_clause.literal_variable_ids[i];
            lit.value = implying_clause.literal_signs[i];
            int depth = Cnf::assignment_depths[lit.var_id];
            int time = Cnf::assignment_times[lit.var_id];
            
            auto [_, success] = lit_to_time.insert({time, lit});
            if (success && depth >= last_decided_depth) {
                current_cycle_variables++;
            }
        }

        // TODO: optimization if it gets too big
        if (lit_to_time.size() > CONFLICT_CLAUSE_SIZE*Cnf::n) {
            return false;
        }
    }
    result.id = -1;
    result.num_literals = lit_to_time.size();
    result.literal_variable_ids = (int *)malloc(sizeof(int) * result.num_literals);
    result.literal_signs = (bool *)malloc(sizeof(bool) * result.num_literals);
    // make result clause from lit_to_time
    Assignment lits[result.num_literals];
    int i = 0;
    for (auto iter = lit_to_time.begin(); iter != lit_to_time.end(); ++iter) {
        lits[i] = iter->second;
        i++;
    }
    std::sort(lits, lits+result.num_literals, cmp_assignments);

    for (i = 0; i < result.num_literals; i++) {
        result.literal_variable_ids[i] = lits[i].var_id;
        result.literal_signs[i] = lits[i].value;
    }

    // TODO: when should we keep/discard conflict clause?
    if (result.num_literals > CONFLICT_CLAUSE_SIZE*Cnf::n 
    || Cnf::clauses.num_conflict_indexed - 1 == Cnf::clauses.max_conflict_indexable) {
        free_clause(result);
        return false;
    }
    return true;
}

// Updates formula with given assignment.
// Returns false on failure and populates Conflict clause.
bool Cnf::propagate_assignment(
        int var_id, 
        bool value, 
        int implier, 
        int *conflict_id) 
    {
    if (PRINT_LEVEL > 1) printf("%sPID %d: trying var %d |= %d\n", Cnf::depth_str.c_str(), Cnf::pid, var_id, (int)value);
    VariableLocations locations = (Cnf::variables)[var_id];
    int old_implier = locations.implying_clause_id;
    if (value) {
        assert(!Cnf::assigned_false[var_id]);
        Cnf::assigned_true[var_id] = true;
        Cnf::true_assignment_statuses[var_id] = 'l';
    } else {
        assert(!Cnf::assigned_true[var_id]);
        Cnf::assigned_false[var_id] = true;
        Cnf::false_assignment_statuses[var_id] = 'l';
    }
    Cnf::current_time++;
    Cnf::assignment_times[var_id] = Cnf::current_time;
    Cnf::assignment_depths[var_id] = Cnf::depth;
    Cnf::variables[var_id].implying_clause_id = implier;
    add_to_edit_stack(variable_edit(var_id, old_implier));
    Cnf::num_vars_assigned++;
    int num_to_check = locations.clauses_containing.count;
    int *clauses_to_check = locations.clauses_containing.as_list();
    if (PRINT_LEVEL > 3) printf("%sPID %d: assigned var %d |= %d (edits = %d), checking %d clauses\n", Cnf::depth_str.c_str(), Cnf::pid, var_id, (int)value, (Cnf::edit_stack).count, num_to_check);    
    for (int i = 0; i < num_to_check; i++) {
        int clause_id = clauses_to_check[i];
        // Try to drop it
        if (!Cnf::clauses.clause_is_dropped(clause_id)) {
            if (PRINT_LEVEL >= 6) printf("%sPID %d: checking clause %d\n", Cnf::depth_str.c_str(), Cnf::pid, clause_id);
            Clause clause = Cnf::clauses.get_clause(clause_id);
            int num_unsat;
            char new_clause_status = check_clause(clause, &num_unsat, true);            
            switch (new_clause_status) {
                case 's': {
                    // Satisfied, can now drop
                    if (PRINT_LEVEL > 3) printf("%sPID %d: dropping clause %d %s (%d edits)\n", Cnf::depth_str.c_str(), Cnf::pid, clause_id, clause_to_string_current(clause, false).c_str(), (Cnf::edit_stack).count);
                    Cnf::clauses.drop_clause(clause_id);
                    add_to_edit_stack(clause_edit(clause_id));
                    break;
                } case 'u': {
                    if (PRINT_LEVEL >= 3) printf("%sPID %d: clause %d %s contains conflict (%d local edits)\n", Cnf::depth_str.c_str(), Cnf::pid, clause_id, clause_to_string_current(clause, false).c_str(), Cnf::local_edit_count);
                    *conflict_id = clause_id;
                    free(clauses_to_check);
                    if (PRINT_LEVEL > 1) printf("%sPID %d: assignment propagation of var %d = %d failed (conflict = %d)\n", Cnf::depth_str.c_str(), Cnf::pid, var_id, (int)value, clause_id);
                    return false;
                } default: {
                    // At least the size changed
                    if (PRINT_LEVEL > 3) printf("%sPID %d: decreasing clause %d %s size (%d -> %d) (%d edits)\n", Cnf::depth_str.c_str(), Cnf::pid, clause_id, clause_to_string_current(clause, false).c_str(), num_unsat + 1, num_unsat, (Cnf::edit_stack).count);
                    if (num_unsat < 3) {
                        Cnf::clauses.change_clause_size(
                            clause_id, num_unsat);
                        add_to_edit_stack(size_change_edit(
                            clause_id, num_unsat + 1, num_unsat));
                    }
                    break;
                }
            }
        }
    }
    free(clauses_to_check);
    if (PRINT_LEVEL >= 3) printf("%sPID %d: propagated var %d |= %d (%d local edits)\n", Cnf::depth_str.c_str(), Cnf::pid, var_id, (int)value, Cnf::local_edit_count);
    if (PRINT_LEVEL > 3) print_cnf("Post prop CNF", Cnf::depth_str, (PRINT_LEVEL >= 2));
    return true;
}

// Uses Sudoku context with variable indexing to immidiately assign
// other variables
bool Cnf::smart_propagate_assignment(
        int var_id, 
        bool value, 
        int implier,
        int *conflict_id) {
    if (PRINT_LEVEL > 1) printf("%sPID %d: smart propagating var %d |= %d (%d edits)\n", Cnf::depth_str.c_str(), Cnf::pid, var_id, (int)value, (Cnf::edit_stack).count);
    if (!value) {
        // Not very useful to us
        return true;
    }
    VariableLocations locations = Cnf::variables[var_id];
    int i = locations.variable_row;
    int j = locations.variable_col;
    int k = locations.variable_k;
    int n_squ = Cnf::n * Cnf::n;
    int k_n_squ = k * n_squ;
    int coords_offset = (Cnf::n * i) + j;
    int extras_assigned = 0;
    // All others at same coordinate, different k
    for (int other_k = 0; other_k < Cnf::n; other_k++) {
        if (other_k == k) {
            continue;
        }
        unsigned int other_var_id = (other_k * n_squ) + coords_offset;
        if (Cnf::assigned_true[other_var_id] 
            || Cnf::assigned_false[other_var_id]) {
            continue;
        }
        bool iterative_result = propagate_assignment(
            other_var_id, false, -1, conflict_id);
        extras_assigned++;
        if (!iterative_result) {
            return false;
        }
    }
    // No others on the same row can have value k
    for (int other_j = 0; other_j < Cnf::n; other_j++) {
        if (other_j == j) {
            continue;
        }
        unsigned int other_var_id = (k_n_squ) + (i * Cnf::n) + other_j;
        if (Cnf::assigned_true[other_var_id] 
            || Cnf::assigned_false[other_var_id]) {
            continue;
        }
        bool iterative_result = propagate_assignment(
            other_var_id, false, -1, conflict_id);
        extras_assigned++;
        if (!iterative_result) {
            return false;
        }
    }
    // No others on the same column can have value k
    for (int other_i = 0; other_i < Cnf::n; other_i++) {
        if (other_i == i) {
            continue;
        }
        unsigned int other_var_id = (k_n_squ) + (other_i * Cnf::n) + j;
        if (Cnf::assigned_true[other_var_id] 
            || Cnf::assigned_false[other_var_id]) {
            continue;
        }
        bool iterative_result = propagate_assignment(
            other_var_id, false, -1, conflict_id);
        extras_assigned++;
        if (!iterative_result) {
            return false;
        }
    }
    // TODO: same thing within blocks
    if (PRINT_LEVEL > 1 && extras_assigned > 0) printf("%sPID %d: smart propagating var %d |= %d done (%d edits) (extras = %d)\n", Cnf::depth_str.c_str(), Cnf::pid, var_id, (int)value, (Cnf::edit_stack).count, extras_assigned);
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
    for (int var_id = 0; var_id < Cnf::n*Cnf::n*Cnf::n; var_id++) {
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

// On success, finishes arbitrarily assigning remaining variables
void Cnf::assign_remaining() {
    for (int i = 0; i < Cnf::num_variables; i++) {
        if ((!Cnf::assigned_true[i]) && (!Cnf::assigned_false[i])) {
            Cnf::num_vars_assigned++;
            Cnf::assigned_true[i] = true;
        }
    }
}

// Resets the cnf to its state before edit stack
void Cnf::undo_local_edits() {
    if (PRINT_LEVEL > 1) printf("%sPID %d: undoing %d local edits\n", Cnf::depth_str.c_str(), Cnf::pid, Cnf::local_edit_count);
    bool found_decided_variable_assignment = false;
    for (int i = 0; i < Cnf::local_edit_count; i++) {
        FormulaEdit *recent_ptr = (FormulaEdit *)((Cnf::edit_stack).pop_from_front());
        FormulaEdit recent = *recent_ptr;
        switch (recent.edit_type) {
            case 'v': {
                int var_id = recent.edit_id;
                int current_implier = Cnf::variables[var_id].implying_clause_id;
                int prev_implier = recent.implier;
                Cnf::num_vars_assigned--;
                if (Cnf::assigned_true[var_id]) {
                    assert(!Cnf::assigned_false[var_id]);
                    Cnf::assigned_true[var_id] = false;
                    Cnf::true_assignment_statuses[var_id] = 'u';
                    if (PRINT_LEVEL >= 3) printf("%sPID %d: undoing variable edit %d = T implied by %d\n", Cnf::depth_str.c_str(), Cnf::pid, var_id, current_implier);
                } else {
                    assert(Cnf::assigned_false[var_id]);
                    assert(!Cnf::assigned_true[var_id]);
                    Cnf::assigned_false[var_id] = false;
                    Cnf::false_assignment_statuses[var_id] = 'u';
                    if (PRINT_LEVEL >= 3) printf("%sPID %d: undoing variable edit %d = F implied by %d\n", Cnf::depth_str.c_str(), Cnf::pid, var_id, current_implier);
                }
                assert(Cnf::current_time == Cnf::assignment_times[var_id]);
                if (current_implier == -1) {
                    assert(!found_decided_variable_assignment);
                    found_decided_variable_assignment = true;
                }
                Cnf::current_time--;
                Cnf::variables[var_id].implying_clause_id = prev_implier;
                break;
            } case 'c': {
                int clause_id = recent.edit_id;
                Cnf::clauses.re_add_clause(clause_id);
                break;
            } default: {
                assert(recent.edit_type == 's');
                int clause_id = recent.edit_id;
                int size_before = (int)recent.size_before;
                int size_after = (int)recent.size_after;
                Cnf::clauses.change_clause_size(clause_id, size_before);
                break;
            }
        }
        free(recent_ptr);
    }
    if (Cnf::depth == 0) {
        Cnf::local_edit_count = 0;
    } else {
        Cnf::local_edit_count = (Cnf::edit_counts_stack).pop_from_front();
    }
    if (PRINT_LEVEL > 2) printf("%sPID %d: new local edit count = %d\n", Cnf::depth_str.c_str(), Cnf::pid, Cnf::local_edit_count);
}

// Updates internal variables based on a recursive backtrack
void Cnf::backtrack() {
    if (PRINT_LEVEL > 1) printf("%sPID %d: backtracking\n", Cnf::depth_str.c_str(), Cnf::pid);
    undo_local_edits();
    if (PRINT_LEVEL > 1) print_cnf("Restored CNF", Cnf::depth_str, (PRINT_LEVEL >= 2));
    if (Cnf::depth > 0) {
        if (PRINT_INDENT) {
            Cnf::depth_str = Cnf::depth_str.substr(1);
        }
        Cnf::depth--;
    }
    if (PRINT_LEVEL > 5) printf("%sPID %d: backtracking success\n", Cnf::depth_str.c_str(), Cnf::pid);
}

// Updates internal variables based on a recursive call
void Cnf::recurse() {
    if (PRINT_LEVEL > 1) printf("%s  PID %d: recurse, stashing %d edits\n", Cnf::depth_str.c_str(), Cnf::pid, Cnf::local_edit_count);
    if (Cnf::depth != 0) {
        assert(Cnf::local_edit_count > 0); 
    }
    (Cnf::edit_counts_stack).add_to_front(Cnf::local_edit_count);
    Cnf::local_edit_count = 0;
    Cnf::depth++;
    if (PRINT_INDENT) {
        Cnf::depth_str.append(" ");
    }
}

// Returns the task embedded in the work received
Task Cnf::extract_task_from_work(void *work) {
    Task task;
    int offset = Cnf::ints_needed_for_clauses + (2 * Cnf::ints_needed_for_vars);
    task.var_id = ((unsigned int *)work)[offset] & ((unsigned int)(1 << 31) - 1);
    task.implier = (int)(((unsigned int *)work)[offset + 1]);
    task.assignment = (bool)(((unsigned int *)work)[offset] & ((unsigned int)(1 << 31)));
    task.is_backtrack = false;
    return task;
}

// Reconstructs one's own formula (state) from an integer representation
void Cnf::reconstruct_state(void *work, Deque &task_stack) {
    Cnf::clauses.reset();
    unsigned int *compressed = (unsigned int *)work;
    Cnf::oldest_compressed = compressed;
    int clause_group_offset = 0;
    // Drop normal clauses
    for (int clause_group = 0; clause_group < Cnf::ints_needed_for_clauses; clause_group++) {
        unsigned int compressed_group = compressed[clause_group];
        bool *clause_bits = int_to_bits(compressed_group);
        for (int bit = 0; bit < 32; bit++) {
            int clause_id = bit + clause_group_offset;
            if (clause_id >= Cnf::clauses.num_indexed) break;
            bool should_be_dropped = clause_bits[bit];
            if (should_be_dropped) {
                Cnf::clauses.drop_clause(clause_id);
            } else {
                Clause conflict_clause = Cnf::clauses.get_clause(clause_id);
                int num_unsat;
                char clause_status = check_clause(conflict_clause, &num_unsat);
                assert(clause_status == 'n');
                if (num_unsat != conflict_clause.num_literals) {
                    Cnf::clauses.change_clause_size(clause_id, num_unsat);
                }
            }
        }
        free(clause_bits);
        clause_group_offset += 32;
    }
    // Set values
    memset(Cnf::true_assignment_statuses, 'u', Cnf::num_variables * sizeof(char));
    memset(Cnf::false_assignment_statuses, 'u', Cnf::num_variables * sizeof(char));
    int value_group_offset = 0;
    for (int value_group = 0; value_group < Cnf::ints_needed_for_vars; value_group++) {
        unsigned int compressed_group_true = compressed[
            Cnf::ints_needed_for_clauses + value_group];
        unsigned int compressed_group_false = compressed[
            Cnf::ints_needed_for_clauses + Cnf::ints_needed_for_vars + value_group];
        bool *value_bits_true = int_to_bits(compressed_group_true);
        bool *value_bits_false = int_to_bits(compressed_group_false);
        memcpy(Cnf::assigned_true + value_group_offset, value_bits_true, 32);
        memcpy(Cnf::assigned_false + value_group_offset, value_bits_false, 32);
        Cnf::num_vars_assigned = 0;
        for (unsigned int var_id = 0; var_id < Cnf::num_variables; var_id++) {
            if (Cnf::assigned_true[var_id]) {
                Cnf::true_assignment_statuses[var_id] = 'r';
                Cnf::false_assignment_statuses[var_id] = 'u';
                Cnf::num_vars_assigned++;
            } else if (Cnf::assigned_false[var_id]) {
                Cnf::true_assignment_statuses[var_id] = 'u';
                Cnf::false_assignment_statuses[var_id] = 'r';
                Cnf::num_vars_assigned++;
            } else {
                Cnf::true_assignment_statuses[var_id] = 'u';
                Cnf::false_assignment_statuses[var_id] = 'u';
            }
        }
        free(value_bits_true);
        free(value_bits_false);
    }
    // Re-evaluate conflict clauses
    for (int i = 0; i < Cnf::clauses.num_conflict_indexed; i++) {
        int conflict_id = Cnf::clauses.max_indexable + i;
        Clause conflict_clause = Cnf::clauses.get_clause(conflict_id);
        int num_unsat;
        char clause_status = check_clause(conflict_clause, &num_unsat);
        // Should never be given a false formula
        assert(clause_status != 'u');
        if (clause_status == 's') {
            Cnf::clauses.drop_clause(conflict_id);
        } else {
            if (num_unsat != conflict_clause.num_literals) {
                Cnf::clauses.change_clause_size(conflict_id, num_unsat);
            }
        }
    }
    memset(Cnf::assignment_times, -1, Cnf::num_variables * sizeof(int));
    memset(Cnf::assignment_depths, -1, Cnf::num_variables * sizeof(int));
    task_stack.free_data();
    (Cnf::edit_stack).free_data();
    (Cnf::edit_counts_stack).free_data();
    Task first_task = extract_task_from_work(work);
    task_stack.add_to_front(
        make_task(first_task.var_id, first_task.implier, first_task.assignment));
    if (first_task.assignment) {
        Cnf::true_assignment_statuses[first_task.var_id] = 'q';
    } else {
        Cnf::false_assignment_statuses[first_task.var_id] = 'q';
    }
    Cnf::depth = 0;
    Cnf::depth_str = "";
    Cnf::local_edit_count = 0;
    Cnf::current_time = 0;
    return; // Cnf and task stack are now ready for a new call to solve
}

// Converts task + state into work message, returns a COPY of the data
void *Cnf::convert_to_work_message(unsigned int *compressed, Task task) {
    assert(task.var_id >= 0);
    unsigned int *work = (unsigned int *)malloc(sizeof(unsigned int) * work_ints);
    memcpy(work, compressed, sizeof(unsigned int) * work_ints);
    int offset = Cnf::ints_needed_for_clauses + (2 * Cnf::ints_needed_for_vars);
    assert(task.var_id >= 0);
    work[offset] = ((unsigned int)task.var_id);
    if (task.assignment) {
        work[offset] += (1 << 31);
    }
    work[offset + 1] = (unsigned int)task.implier;
    return (void *)work;
}

// Converts current formula to integer representation
unsigned int *Cnf::to_int_rep() {
    unsigned int *compressed = (unsigned *)calloc(
        sizeof(unsigned int), work_ints);
    bool *bit_addr = Cnf::clauses.normal_clauses.elements_dropped;
    for (int i = 0; i < Cnf::ints_needed_for_clauses; i++) {
        unsigned int current = bits_to_int(bit_addr);
        // printf("current = %u\n", current);
        compressed[i + 2] = current;
        bit_addr = bit_addr + 32;
    }
    bit_addr = Cnf::assigned_true;
    for (int i = 0; i < Cnf::ints_needed_for_vars; i++) {
        unsigned int current = bits_to_int(bit_addr);
        compressed[i + 2 + Cnf::ints_needed_for_clauses] = current;
        bit_addr = bit_addr + 32;
    }
    bit_addr = Cnf::assigned_false;
    for (int i = 0; i < Cnf::ints_needed_for_vars; i++) {
        unsigned int current = bits_to_int(bit_addr);
        compressed[
            i + 2 + Cnf::ints_needed_for_clauses + Cnf::ints_needed_for_vars
            ] = current;
        bit_addr = bit_addr + 32;
    }
    return compressed;
}

// Frees the Cnf formula data structure
void Cnf::free_cnf() {
    // TODO: implement this
    Cnf::clauses.free_data();
    for (int var_id = 0; var_id < Cnf::num_variables; var_id++) {
        VariableLocations locations = Cnf::variables[var_id];
        locations.clauses_containing.free_deque();
    }
    free(Cnf::variables);
    Cnf::edit_stack.free_deque();
    Cnf::edit_counts_stack.free_deque();
    free(Cnf::oldest_compressed);
    free(Cnf::assigned_true);
    free(Cnf::assigned_false);
    free(Cnf::true_assignment_statuses);
    free(Cnf::false_assignment_statuses);
    free(Cnf::assignment_times);
    free(Cnf::assignment_depths);
    return;
}

//----------------------------------------------------------------
// END IMPLEMENTATION
//----------------------------------------------------------------