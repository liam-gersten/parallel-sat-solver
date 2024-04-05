#include "state.h"
#include <cassert>

State::State(short pid, short nprocs, Cnf &cnf, Deque &task_stack) {
    State::pid = pid;
    State::nprocs = nprocs;
    State::parent_id = (((pid + 1) / 2) - 1);
    State::num_children = 2;
    State::child_ids = (short *)malloc(sizeof(short) * 2);
    State::child_ids[0] = ((pid + 1) * 2) - 1;
    State::child_ids[1] = ((pid + 1) * 2);
    State::child_statuses = (char *)malloc(sizeof(char) * 3);
    State::child_statuses[0] = 'r';
    State::child_statuses[1] = 'r';
    if (pid == 0) {
        State::child_statuses[2] = 'u';
        State::num_urgent = 1;
    } else {
        State::child_statuses[2] = 's'; // ?
        State::num_urgent = 0;
    }
    State::waiting_on_response = (bool *)calloc(sizeof(bool), 3);
}

// Returns whether there are any other processes requesting our work
bool State::workers_requesting() {
    // TODO: implement this
    return false;
}

// Returns whether the state is able to supply work to requesters
bool State::can_give_work(Deque task_stack) {
    // TODO: implement this
    return false;
}

// Gives one unit of work to lazy processors
void State::give_work(
        Cnf &cnf, 
        Deque &task_stack, 
        Interconnect &interconnect) 
    {
    // TODO: implement this
}

// Gets stashed work, returns true if any was grabbed
bool State::get_work_from_interconnect_stash(
        Cnf &cnf, 
        Deque &task_stack, 
        Interconnect &interconnect) 
    {
    // TODO: implement this
    return false;
}

// Returns whether we are out of work to do
bool State::out_of_work(Deque task_stack) {
    // TODO: implement this
    return false;
}

// Asks parent or children for work
void State::ask_for_work(Cnf &cnf, Interconnect &interconnect) {
    // TODO: implement this
}

// Handles work received
void State::handle_work_request(
        short sender_pid,
        bool is_urgent,
        Cnf &cnf, 
        Deque &task_stack, 
        Interconnect &interconnect) 
    {
    // TODO: implement this
}

// Handles work received
void State::handle_work_message(
        short sender_pid,
        void *work,
        Cnf &cnf, 
        Deque &task_stack, 
        Interconnect &interconnect) 
    {
    // TODO: implement this
}

// Handles an abort message, possibly forwarding it
void State::handle_abort_message(
        short sender_pid,
        Cnf &cnf, 
        Deque &task_stack, 
        Interconnect &interconnect) 
    {
    // TODO: implement this
}

// Handles an abort message, possibly forwarding it
void State::handle_message(
        Message message, 
        Cnf &cnf, 
        Deque &task_stack, 
        Interconnect &interconnect) 
    {
    switch (message.type) {
        case 0: {
            handle_work_request(
                message.sender, false, cnf, task_stack, interconnect);
            return;
        } case 1: {
            handle_work_request(
                message.sender, true, cnf, task_stack, interconnect);
            return;
        } case 2: {
            handle_work_message(
                message.sender, message.data, cnf, task_stack, interconnect);
            return;
        } default: {
            handle_abort_message(message.sender, cnf, task_stack, interconnect);
            return;
        }
    }
}

// Adds one or two variable assignment tasks to task stack
int State::add_tasks_from_formula(Cnf &cnf, Deque &task_stack, bool skip_undo) {
    cnf.clauses.reset_iterator();
    Clause current_clause = *((Clause *)(cnf.clauses.get_current_value()));
    int current_clause_id = current_clause.id;
    int new_var_id;
    bool new_var_sign;
    int num_unsat = cnf.pick_from_clause(
        current_clause, &new_var_id, &new_var_sign);
    if (PRINT_LEVEL > 0) printf("%sPID %d picked new var %d from clause %d %s\n", cnf.depth_str.c_str(), State::pid, new_var_id, current_clause_id, cnf.clause_to_string_current(current_clause, false).c_str());
    if (num_unsat == 1) {
        void *only_task = make_task(new_var_id, new_var_sign);
        task_stack.add_to_front(only_task);
        return 1;
    } else {
        // Add an undo task to do last
        if (!skip_undo) {
            void *undo_task = make_task(-1, true);
            task_stack.add_to_front(undo_task);
        }
        bool first_choice = get_first_pick(new_var_sign);
        void *important_task = make_task(new_var_id, first_choice);
        void *other_task = make_task(new_var_id, !first_choice);
        task_stack.add_to_front(other_task);
        task_stack.add_to_front(important_task);
        return 2;
    }
}

// Displays data structure data for debugging purposes
void print_data(Cnf &cnf, Deque &task_stack, std::string prefix_str) {
    if (PRINT_LEVEL > 1) cnf.print_task_stack(prefix_str, task_stack);
    if (PRINT_LEVEL > 3) cnf.print_edit_stack(prefix_str, *(cnf.edit_stack));
    if (PRINT_LEVEL > 1) cnf.print_cnf(prefix_str, cnf.depth_str, (PRINT_LEVEL >= 2));
    if (PRINT_LEVEL > 4) print_compressed(
        cnf.pid, prefix_str, cnf.depth_str, cnf.to_int_rep(), cnf.work_ints);
}

// Runs one iteration of the solver
bool State::solve_iteration(Cnf &cnf, Deque &task_stack) {
    assert(!backtrack_at_top(task_stack));
    if (PRINT_LEVEL >= 5) printf("\n");
    Task task = get_task(task_stack);
    int var_id = task.var_id;
    int assignment = task.assignment;
    if (var_id == -1) { // Children backtracked, need to backtrack ourselves
        print_data(cnf, task_stack, "Children backtrack");
        cnf.backtrack();
        return false;
    }
    cnf.recurse();
    while (true) {
        assert(!backtrack_at_top(task_stack));
        if (PRINT_LEVEL > 0) printf("%sPID %d Attempting %d = %d\n", cnf.depth_str.c_str(), State::pid, var_id, assignment);
        print_data(cnf, task_stack, "Loop start");
        if (!cnf.propagate_assignment(var_id, assignment)) {
            // Conflict clause found
            print_data(cnf, task_stack, "Prop fail");
            cnf.backtrack();
            return false;
        }
        if (cnf.clauses.get_linked_list_size() == 0) {
            cnf.assign_remaining();
            print_data(cnf, task_stack, "Base case success");
            return true;
        }
        print_data(cnf, task_stack, "Loop end");
        int num_added = add_tasks_from_formula(cnf, task_stack, false);
        if (num_added == 1) {
            Task task = get_task(task_stack);
            var_id = task.var_id;
            assignment = task.assignment;
            continue;
        }
        return false;
    }
}

// Continues solve operation
bool State::solve(Cnf &cnf, Deque &task_stack, Interconnect &interconnect) {
    add_tasks_from_formula(cnf, task_stack, true);
    while (true) {
        bool result = solve_iteration(cnf, task_stack);
        if (result) {
            return true;
        } else if (task_stack.count == 0) {
            return false;
        }
        while (workers_requesting() && can_give_work(task_stack)) {
            give_work(cnf, task_stack, interconnect);
        }
        if (out_of_work(task_stack)) {
            bool still_out_of_work = get_work_from_interconnect_stash(
                cnf, task_stack, interconnect);
            if (still_out_of_work) {
                ask_for_work(cnf, interconnect);
            }
        }
        Message message;
        while (out_of_work(task_stack)) {
            bool message_received = interconnect.async_receive_message(message);
            if (message_received) {
                handle_message(message, cnf, task_stack, interconnect);
            }
        }
        while (interconnect.async_receive_message(message)) {
            handle_message(message, cnf, task_stack, interconnect);
        }
    }
}
