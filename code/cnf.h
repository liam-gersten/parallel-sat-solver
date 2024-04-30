#ifndef CNF_H
#define CNF_H

#include "helpers.h"

struct VariableLocations {
    int variable_id; // id of variable
    // Used to determine who caused a conflict
    int implying_clause_id;
    // Used to quickly updates the compressed version of the CNF
    unsigned int variable_addition;
    unsigned int variable_true_addition_index;
    unsigned int variable_false_addition_index;
    // Will have dynamic allocation size
    IntDeque clauses_containing; // LL of clause ids

    int variable_row;
    int variable_col;
    int variable_k;
};

// Adds a clause to data structures
void add_clause(
    Clause new_clause, 
    Clauses &clauses, 
    VariableLocations *variables);

int getRegularVariable(int i, int j, int k, int n);

int getSubcolID(
    int col, 
    int xbox, 
    int ybox, 
    int k, 
    int sqrt_n, 
    int start, 
    int spacing);

int comm_num_clauses(int n);

int comm_num_vars(int n, bool begin=true);

class Cnf {
    public:
        Clauses clauses; // dynamic number
        VariableLocations *variables; // static number
        Deque eedit_stack;
        unsigned int *oldest_compressed;
        short pid;
        short nprocs;
        int num_variables;
        int num_conflict_to_hold;
        int ints_needed_for_clauses;
        int ints_needed_for_conflict_clauses;
        int ints_needed_for_vars;
        int work_ints;
        bool *assigned_true;
        bool *assigned_false;
        // Used to determine who needs a conflict clause
        // 'u': unassigned
        // 'l': assigned locally
        // 'q': queued locally
        // 'r': assigned remotely (by parent)
        // 's': stolen another thread
        char *true_assignment_statuses;
        char *false_assignment_statuses;
        int *assignment_times;
        int *assignment_depths;
        int current_time;
        unsigned int num_vars_assigned;
        int n;
        int depth;
        int reduction_method;
        std::string depth_str;
        short **sudoku_board;
        bool is_sudoku;

        bool *ans;

        // Makes CNF formula from inputs
        Cnf(
            short pid,
            short nprocs,
            int **constraints, 
            int n, 
            int sqrt_n, 
            int num_constraints,
            int num_assignments,
            int reduction_method,
            GridAssignment *assignments);
        // Makes CNF formula from premade data structures
        Cnf(
            short pid,
            short nprocs,
            Clauses input_clauses, 
            VariableLocations *input_variables, 
            int num_variables);
        // Default constructor
        Cnf();

        std::tuple<int, bool> oneOfClause(int* vars, int length, int &var_id, bool beginning=true, int newComm=-1, bool newCommSign=false);

        // Working version that reduces the number of clauses needed
        void reduce_puzzle_clauses_truncated(int n, int sqrt_n, int num_assignments, GridAssignment* assignments);

        // Original version with lowest variable count
        void reduce_puzzle_original(
            int n, 
            int sqrt_n, 
            int num_assignments,
            GridAssignment *assignments);
        
        // Initializes CNF compression
        void init_compression();

        // Testing method only
        bool clauses_equal(Clause a, Clause b);

        // Returns whethere every variable has at most one truth value
        bool valid_truth_assignments();

        // Gets string representation of edited clause
        std::string clause_to_string_current(Clause clause, bool elimination);

        // Debug print a full cnf structure
        void print_cnf(
            std::string prefix_string, 
            std::string tab_string,
            bool elimination);

        // Prints out the task stack
        void print_task_stack(
            std::string prefix_string, 
            Deque &task_stack,
            int upto = 20);

        // Prints out the task stack
        void print_edit_stack(
            std::string prefix_string, 
            Deque &edit_stack,
            int upto = 20);

        // Gets the number of local edits
        int get_local_edit_count();
        
        // Adds edit edit to edit stack, checks assertion
        void add_to_edit_stack(void *raw_edit);
        
        // Gets the status of a variable decision (task)
        char get_decision_status(Assignment decision);

        // Gets the status of a variable decision (task)
        char get_decision_status(Task decision);

        // Returns whether we have a valid lowest bad decision
        bool valid_lbd(
            Deque decided_conflict_literals, 
            Task lowest_bad_decision);
        
        // Picks unassigned variable from the clause, returns the number of unsats
        int pick_from_clause(Clause clause, int *var_id, bool *var_sign);

        // Just returns the number of unsatisfied literals in a clause
        int get_num_unsat(Clause clause);

        // Gets the status of a clause, 's', 'u', or 'n'.
        char check_clause(Clause clause, int *num_unsat, bool from_propagate=false);
        
        // A shortened version of a conflict clause containing only the literals
        // (Assignment structs) that are decided, not unit propagated.
        Deque get_decided_conflict_literals(Clause conflict_clause);
        
        // Returns whether a clause already exists
        bool clause_exists_already(Clause new_clause);

        // Returns whether a clause is fully assigned, populates the result w eval
        bool clause_satisfied(Clause clause, bool *result);

        // Returns whether a clause id is for a conflict clause
        bool is_conflict_clause(int clause_id);
        
        // Resolves two clauses, returns the resulting clause
        Clause resolve_clauses(Clause A, Clause B, int variable);
        
        // Sorts (in place) the variables by decreasing assignment time
        void sort_variables_by_assignment_time(int *variables, int num_vars);
        
        // Performs resoltion, populating the result clause.
        // Returns whether a result could be generated.
        bool conflict_resolution(int culprit_id, Clause &result);
        // Populates result clause with 1UID conflict clause
        // Returns whether a result could be generated.
        bool conflict_resolution_uid(int culprit_id, Clause &result, int decided_var_id);
        bool conflict_resolution_uid_old(int culprit_id, Clause &result, int decided_var_id);

        // Updates formula with given assignment.
        // Returns false on failure and populates Conflict clause.
        bool propagate_assignment(
            int var_id,
            bool value, 
            int implier, 
            int *conflict_id);

        // Uses Sudoku context with variable indexing to immidiately assign
        // other variables
        bool smart_propagate_assignment(
            int var_id, 
            bool value, 
            int implier,
            int *conflict_id);
        
        // Returns the assignment of variables
        bool *get_assignment();

        // Creates and writes to sudoku board from formula
        short **get_sudoku_board();

        // On success, finishes arbitrarily assigning remaining variables
        void assign_remaining();
        
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

        // Converts task + state into work message, returns a COPY of the data
        void *convert_to_work_message(unsigned int *compressed, Task task);
        
        // Converts current formula to integer representation
        unsigned int *to_int_rep();

        // Frees the Cnf formula data structure
        void free_cnf();
};

#endif