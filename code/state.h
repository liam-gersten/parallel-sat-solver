#ifndef STATE_H
#define STATE_H

#include <cstdint>
#include <string>
#include "helpers.h"
#include "interconnect.h"
#include "cnf.h"

class State {
    public:
        short pid;
        short nprocs;
        short parent_id;
        short num_children;
        short branching_factor;
        char *child_statuses; 
        // 'r' (requesting), 
        // 'u' (urgently requesting), 
        // 'w' (working)
        char *requests_sent; 
        // 'r' (request),
        // 'u' (urgent request),
        // 'n' (none)
        short num_requesting;
        short num_urgent;
        int num_non_trivial_tasks;
        bool process_finished;
        bool was_explicit_abort;
        unsigned long long int calls_to_solve;
        short assignment_method;
        // 0 greedy
        // 1 opposite of greedy
        // 2 always set True
        // 3 always set False
        bool use_smart_prop;
        int current_cycle;
        Deque *thieves;
        GivenTask current_task;

        State(
            short pid, 
            short nprocs, 
            short branching_factor, 
            short assignment_method,
            bool use_smart_prop);

        // Gets pid from child (or parent) index
        short pid_from_child_index(short child_index);

        // Gets child (or parent) index from child (or parent) pid
        short child_index_from_pid(short child_pid);

        // Prints out the current progress of the solver
        void print_progress(Cnf &cnf, Deque &task_stack);
        
        // Returns whether there are any other processes requesting our work
        bool workers_requesting();

        // Returns whether we should forward an urgent request
        bool should_forward_urgent_request();

        // Returns whether we should implicitly abort due to urgent requests
        bool should_implicit_abort();

        // Returns whether the state is able to supply work to requesters
        bool can_give_work(Deque task_stack, Interconnect interconnect);

        // Ensures the task stack is a valid one, returns result
        bool task_stack_invariant(
            Cnf &cnf, 
            Deque &task_stack, 
            int supposed_num_tasks);
        
        // Applies an edit to the given compressed CNF
        void apply_edit_to_compressed(
            Cnf &cnf, 
            unsigned int *compressed,
            FormulaEdit edit);
        
        // Grabs work from the top of the task stack, updates Cnf structures
        void *grab_work_from_stack(
            Cnf &cnf, 
            Deque &task_stack, 
            short recipient_pid);
        
        // Picks recipient index to give work to
        short pick_work_recipient();

        // Picks recipient index to ask for work
        short pick_request_recipient();

        // Gives one unit of work to lazy processors
        void give_work(
            Cnf &cnf, 
            Deque &task_stack, 
            Interconnect &interconnect);
        
        // Gets stashed work, returns true if any was grabbed
        bool get_work_from_interconnect_stash(
            Cnf &cnf, 
            Deque &task_stack, 
            Interconnect &interconnect);

        // Returns whether we are out of work to do
        bool out_of_work();

        // Asks parent or children for work, called once when we finish our work
        void ask_for_work(Cnf &cnf, Interconnect &interconnect);

        // Invalidates (erases) ones work
        void invalidate_work(Deque &task_stack);
        
        // Empties/frees data structures and immidiately returns
        void abort_process(bool explicit_abort = false);

        // Sends urgent requests to children to force them to abort
        void abort_others(
            Interconnect &interconnect,
            bool explicit_abort = false);
        
        // Handles work request
        void handle_work_request(
            short sender_pid,
            short version,
            Cnf &cnf,
            Deque &task_stack, 
            Interconnect &interconnect);
        
        // Handles work received
        void handle_work_message(
            Message message,
            Cnf &cnf, 
            Deque &task_stack, 
            Interconnect &interconnect);
        
        // Updates self and possibly sends other messages depending on type
        void handle_message(
            Message message, 
            Cnf &cnf, 
            Deque &task_stack, 
            Interconnect &interconnect);

        // Edits history to make it appear as though the conflict clause is
        // normal.
        void insert_conflict_clause_history(Cnf &cnf, Clause conflict_clause);

        // Sends messages to specified theives in light of conflict
        void inform_thieves_of_conflict(
            Deque selected_thieves,
            Clause conflict_clause,
            Interconnect &interconnect,
            bool invalidate = false);
        
        // Slits thieves based on midpoint (not included), populates results
        void split_thieves(
            Task midpoint, 
            Deque &thieves_before, 
            Deque &thieves_after);
        
        // Simply backtracks once and removes the first decided task(s)
        void simple_conflict_backtrack(Cnf &cnf, Deque &task_stack);
        
        // Moves to lowest point in call stack when conflict clause is useful
        void backtrack_to_conflict_head(
            Cnf &cnf, 
            Deque &task_stack, 
            Clause conflict_clause,
            Task lowest_bad_decision);

        // Adds tasks based on what a conflict clause says (always greedy)
        int add_tasks_from_conflict_clause(
            Cnf &cnf, 
            Deque &task_stack, 
            Clause conflict_clause);
        
        // Adds a conflict clause to the clause list
        void add_conflict_clause(
            Cnf &cnf, 
            Clause conflict_clause,
            Deque &task_stack,
            bool pick_from_clause = false);
        
        // Returns who is responsible for making the decision that runs contrary
        // to decided_conflict_literals, one of
        // 'l' (in local), 'r' (in remote), or 's' (in stealers).
        // Also populates the lowest (most-recent) bad decision made.
        char blame_decision(
            Cnf &cnf,
            Deque &task_stack,
            Deque decided_conflict_literals, 
            Task *lowest_bad_decision);
        
        // Handles a recieved or locally-generated conflict clause
        void handle_conflict_clause(
            Cnf &cnf, 
            Deque &task_stack, 
            Clause conflict_clause,
            Interconnect &interconnect,
            bool blamed_for_error = true);

        // Adds one or two variable assignment tasks to task stack
        int add_tasks_from_formula(Cnf &cnf, Deque &task_stack);
        
        // Runs one iteration of the solver
        bool solve_iteration(
            Cnf &cnf, 
            Deque &task_stack, 
            Interconnect &interconnect);

        // Continues solve operation, returns true iff a solution was found by
        // the current thread.
        bool solve(Cnf &cnf, Deque &task_stack, Interconnect &interconnect);
};

#endif