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
        short *child_ids;
        char *child_statuses; // 'r', 'u', or 's'
        short num_urgent;

        State(short pid, short nprocs, Cnf &cnf, Deque &task_stack);

        // True if parent hasn't sent us an urgent request
        bool parent_may_have_work();

        // Requests more work to do from parent in non-urgent fashion
        void request_work_from_parent(Interconnect &interconnect);

        // Requests more work to do from one or more children
        void request_work_from_children(Interconnect &interconnect);

        // Sends urgent message to the one remaining child/parent who hasn't
        // sent us one.
        void urgently_request_work(Interconnect &interconnect);

        // Terminates state and current thread
        void abort(Interconnect &interconnect);

        // Updates a child or parent's status
        void set_status(short id, char status);

        // Adds work to stack
        void add_to_stack(Message work, Deque &task_stack);

        // Saves a work request to be filled later
        void stash_request(Message request);

        // Sends work to requester
        void serve_request(Message request);

        // Serves as many stashed requests as possible
        void serve_requests(Interconnect &interconnect);

        // Adds one or two variable assignment tasks to task stack
        int add_tasks_from_formula(Cnf &cnf, Deque &task_stack);
        
        // Runs one iteration of the solver
        bool solve_iteration(Cnf &cnf, Deque &task_stack);

        // Continues solve operation
        bool solve(Cnf &cnf, Deque &task_stack, Interconnect &interconnect);
};

#endif