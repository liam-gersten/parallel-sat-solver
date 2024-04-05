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
        char *child_statuses; // 'r', 'u', or 's'
        bool *request_already_sent;
        short num_requesting;
        short num_urgent;
        int num_non_trivial_tasks;

        State(short pid, short nprocs, Cnf &cnf, Deque &task_stack);

        // Gets pid from child (or parent) index
        short pid_from_child_index(short child_index);

        // Gets child (or parent) index from child (or parent) pid
        short child_index_from_pid(short child_pid);
        
        // Returns whether there are any other processes requesting our work
        bool workers_requesting();

        // Returns whether the state is able to supply work to requesters
        bool can_give_work(Deque task_stack, Interconnect interconnect);
        
        // Grabs work from the top of the task stack, updates Cnf structures
        void *steal_work(Cnf &cnf, Deque &task_stack);
        
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
        bool out_of_work(Deque task_stack);

        // Asks parent or children for work
        void ask_for_work(Cnf &cnf, Interconnect &interconnect);
        
        // Handles work request
        void handle_work_request(
            short sender_pid,
            bool is_urgent,
            Cnf &cnf, 
            Deque &task_stack, 
            Interconnect &interconnect);
        
        // Handles work received
        void handle_work_message(
            short sender_pid,
            void *work,
            Cnf &cnf, 
            Deque &task_stack, 
            Interconnect &interconnect);
        
        // Handles an abort message, possibly forwarding it
        void handle_abort_message(
            short sender_pid,
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

        // Continues solve operation
        bool solve(Cnf &cnf, Deque &task_stack, Interconnect &interconnect);
};

#endif