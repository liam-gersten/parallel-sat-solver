#ifndef CNF_H
#define CNF_H

#include "helpers.h"

struct VariableLocations {
    int variable_id; // id of variable
    int variable_row;
    int variable_col;
    int variable_k;
    // Will have dynamic allocation size
    Queue *clauses_containing; // LL saving pointers to clause structures
};

// Adds a clause to data structures
void add_clause(
    Clause new_clause, 
    IndexableDLL &clauses, 
    VariableLocations *variables);

class Cnf {
    public:
        IndexableDLL clauses; // dynamic number
        VariableLocations *variables; // static number
        Deque *edit_stack;
        Deque *edit_counts_stack;
        int local_edit_count;
        int conflict_id;
        int num_variables;
        int ints_needed_for_clauses;
        int ints_needed_for_vars;
        int work_ints;
        bool *clauses_dropped;
        bool *assigned_true;
        bool *assigned_false;
        int n;
        int depth;
        std::string depth_str;

        // Makes CNF formula from inputs
        Cnf(int **constraints, int n, int sqrt_n, int num_constraints);
        // Makes CNF formula from premade data structures
        Cnf(
            IndexableDLL input_clauses, 
            VariableLocations *input_variables, 
            int num_variables);
        // Default constructor
        Cnf();

        // Gets string representation of edited clause
        std::string clause_to_string_current(Clause clause, bool elimination);

        // Debug print a full cnf structure
        void print_cnf(
            int caller_pid,
            std::string prefix_string, 
            std::string tab_string,
            bool elimination);

        // Prints out the task stack
        void print_task_stack(
            int caller_pid,
            std::string prefix_string, 
            Deque &task_stack);

        // Prints out the task stack
        void print_edit_stack(
            int caller_pid,
            std::string prefix_string, 
            Deque &edit_stack);

        // Picks unassigned variable from the clause, returns the number of unsats
        int pick_from_clause(Clause clause, int *var_id, bool *var_sign);

        // Gets the status of a clause, 's', 'u', or 'n'.
        char check_clause(Clause clause, int *num_unsat);

        // Updates formula with given assignment.
        // Returns false on failure and populates conflict id.
        bool propagate_assignment(int var_id, bool value);

        // Returns the assignment of variables
        bool *get_assignment();

        // Creates and writes to sudoku board from formula
        short **get_sudoku_board();

        // Resets the cnf to its state before edit stack
        void undo_local_edits();
        
        // Updates internal variables based on a recursive backtrack
        void backtrack();
        
        // Updates internal variables based on a recursive call
        void recurse();

        // Returns the task embedded in the work received
        Task extract_task_from_work(void *work);
        
        // Reconstructs one's own formula (state) from an integer representation
        void reconstruct_state(void *work, Deque &task_stack);

        // Converts task + state into work message
        void *convert_to_work_message(unsigned int *compressed, Task task);
        
        // Converts current formula to integer representation
        unsigned int *to_int_rep();

        // Frees the Cnf formula data structure
        void free_cnf();
};

#endif