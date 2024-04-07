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
        char *cchild_statuses; 
        // 'r' (requesting), 
        // 'u' (urgently requesting), 
        // 'w' (working)
        char *rrequests_sent; 
        // 'r' (request),
        // 'u' (urgent request),
        // 'n' (none)
        short nnum_requesting;
        short nnum_urgent;
        int num_non_trivial_tasks;
        bool process_finished;
        bool was_explicit_abort;
        unsigned long long int calls_to_solve;
        bool pick_greedy;
        bool use_smart_prop;
        bool explicit_true;
        short print_index;

        int n; // length of sudoku grid
        int sqrt_n;
        int prev_chosen_var;

        State(
            short pid, 
            short nprocs, 
            short branching_factor, 
            bool pick_greedy,
            int n,
            bool use_smart_prop,
            bool explicit_true);

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
        
        // Applies an edit to the given compressed CNF
        void apply_edit_to_compressed(
            Cnf &cnf, 
            unsigned int *compressed,
            FormulaEdit edit);
        
        // Grabs work from the top of the task stack, updates Cnf structures
        void *grab_work_from_stack(Cnf &cnf, Deque &task_stack);
        
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

        // Adds one or two variable assignment tasks to task stack
        int add_tasks_from_formula(Cnf &cnf, Deque &task_stack, bool skip_undo);
        
        // Runs one iteration of the solver
        bool solve_iteration(Cnf &cnf, Deque &task_stack);

        // Continues solve operation, returns true iff a solution was found by
        // the current thread.
        bool solve(Cnf &cnf, Deque &task_stack, Interconnect &interconnect);
};

#endif