#ifndef SOLVER_H
#define SOLVER_H

#include "helpers.h"

// Will have fixed allocation size
struct Clause {
    int *literal_variable_ids; // variable ids for each literal
    bool *literal_signs; // each literal
    int num_literals; // size of the two pointers
};

struct VariableLocations {
    int variable_id; // id of variable
    int variable_row;
    int variable_col;
    int variable_k;
    // Will have dynamic allocation size
    Queue clauses_containing; // LL saving pointers to clause structures
};

struct Cnf {
    Queue clauses; // dynamic number
    VariableLocations *variables; // static number
    int num_variables;
};

// Gets string representation of clause
std::string clause_to_string(Clause clause);

// Debug print a full cnf structure
void print_cnf(
    int caller_pid,
    std::string prefix_string, 
    std::string tab_string,
    Cnf cnf);

// Makes a clause of just two variables
Clause make_small_clause(int var1, int var2, bool sign1, bool sign2);

// Adds a clause to the cnf
void add_clause_to_cnf(Cnf &cnf, Clause new_clause, bool front);

// Makes CNF formula from inputs
Cnf make_cnf(int **constraints, int n, int sqrt_n, int num_constraints);

// Recursively frees the Cnf formula data structure
void free_cnf_formula(Cnf formula);

class Solver {
    public:
        Cnf cnf_formula;

        // Initialized with input formula
        Solver(Cnf formula);

        // Solver, returns true and populates assignment if satisfiable
        bool solve(bool *assignment);

        // Sets variable's value in cnf formula to given value 
        void assign_variable_value(int variable_id, bool value);

        // Effectively undoes assign_variable_value, useful for recursive
        // backtracking
        void reset_variable_value(int variable_id);

        // Gets the truth value of a clause given variable assignments
        bool evaluate_clause(bool *assignment);

        // Gets the truth value of a cnf given variable assignments
        bool evaluate_cnf(bool *assignment);
};

#endif