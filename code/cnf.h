#ifndef CNF_H
#define CNF_H

#include "helpers.h"

struct VariableLocations {
    int variable_id; // id of variable
    int variable_row;
    int variable_col;
    int variable_k;
    // Will have dynamic allocation size
    Queue clauses_containing; // LL saving pointers to clause structures
};

// Makes a clause of just two variables
Clause make_small_clause(int var1, int var2, bool sign1, bool sign2);

class Cnf {
    public:
        IndexableDLL clauses; // dynamic number
        VariableLocations *variables; // static number
        int num_variables;
        int ints_needed_for_clauses;
        int ints_needed_for_vars;
        bool *clauses_dropped;
        bool *assigned_true;
        bool *assigned_false;

        // Makes CNF formula from inputs
        Cnf(int **constraints, int n, int sqrt_n, int num_constraints);
        // Default constructor
        Cnf();

        // Debug print a full cnf structure
        void print_cnf(
            int caller_pid,
            std::string prefix_string, 
            std::string tab_string);

        // Adds a clause to the cnf
        void add_clause(Clause new_clause);

        // Updates formula with given assignment.
        // Returns false on failure and populates conflict id.
        bool propagate_assignment(
            int var_id,
            bool value, 
            int *conflict_id,
            Queue &edit_stack);

        // Converts current formula to integer representation
        unsigned int *to_int_rep(bool *assigned_true, bool *assigned_false);

        // Recursively frees the Cnf formula data structure
        void free_cnf();
};

#endif