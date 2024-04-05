#include "state.h"
#include <cassert>

State::State(short pid, short nprocs, Cnf &cnf, Deque &task_stack) {
    State::pid = pid;
    State::nprocs = nprocs;
    State::parent_id = (((pid + 1) / 2) - 1);
    State::num_children = 2;
    State::child_statuses = (char *)malloc(sizeof(char) * 3);
    State::child_statuses[0] = 'u'; // No work by default
    State::child_statuses[1] = 'u'; // No work by default
    if (pid == 0) {
        State::child_statuses[2] = 'u';
        State::num_urgent = 1;
    } else {
        State::child_statuses[2] = 's'; // ?
        State::num_urgent = 0;
    }
    State::waiting_on_response = (bool *)calloc(sizeof(bool), 3);
    State::num_requesting = 0;
    State::num_urgent = 0;
    State::num_non_trivial_tasks = 0;
}

// Gets child (or parent) pid from child (or parent) index
short State::pid_from_child_index(short child_index) {
    assert(0 <= child_index && child_index <= State::num_children);
    if (child_index == State::num_children) {
        // Is parent
        return State::parent_id;
    }
    assert(child_index == 0 || child_index == 1);
    return ((State::pid + 1) * 2) - 1 + child_index;
}

// Gets child (or parent) index from child (or parent) pid
short State::child_index_from_pid(short child_pid) {
    assert(0 <= child_pid && child_pid <= State::nprocs);
    if (child_pid == (((State::pid + 1) * 2) - 1)) {
        return 0;
    } else if (child_pid == ((State::pid + 1) * 2)) {
        return 1;
    }
    // Is parent
    assert(child_pid == (((State::pid + 1) / 2) - 1));
    return State::num_children;
}

// Returns whether there are any other processes requesting our work
bool State::workers_requesting() {
    return State::num_requesting > 0;
}

// Returns whether the state is able to supply work to requesters
bool State::can_give_work(Deque task_stack, Interconnect interconnect) {
    return ((State::num_non_trivial_tasks + interconnect.num_stashed_work) > 1);
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
    // TODO: Perhaps we get stashed work from a sender who is no longer 
    // asking us for work now (if possible)?
    // Preference for children or parents?
    if (!interconnect.have_stashed_work()) {
        return false;
    }
    Message work_message = interconnect.get_stashed_work();
    handle_work_message(
        work_message.sender, work_message.data, cnf, task_stack, interconnect);
    return true;
}

// Returns whether we are out of work to do
bool State::out_of_work(Deque task_stack) {
    return State::num_non_trivial_tasks == 0;
}

// Asks parent or children for work
void State::ask_for_work(Cnf &cnf, Interconnect &interconnect) {
    assert(!interconnect.have_stashed_work());
    assert(State::num_urgent <= State::num_children);
    if (State::num_urgent == State::num_children) {
        // Send a single urgent work request
        short dest_index;
        // Find index of the only adjecent node that hasn't urgently requested
        for (short i = 0; i <= State::num_children; i++) {
            if (State::child_statuses[i] != 'u') {
                dest_index = i;
                break;
            }
        }
        short dest_pid = pid_from_child_index(dest_index);
        interconnect.send_work_request(dest_pid, true);
        State::waiting_on_response[dest_index] = true;
    } else {
        // Send a single work request to my parent.
        // If the parent has already been requested and hasn't responded,
        // ask a child who has not been requested yet if applicable.
        bool parent_requested = State::waiting_on_response[State::num_children];
        bool parent_empty = State::child_statuses[State::num_children] == 'u';
        if (!parent_requested && !parent_empty) {
            interconnect.send_work_request(State::parent_id, false);
            State::waiting_on_response[State::num_children] = true;
        } else {
            short dest_index;
            // Find index of valid child
            for (short i = 0; i < State::num_children; i++) {
                bool child_requested = State::waiting_on_response[i];
                bool child_empty = State::child_statuses[i] == 'u';
                if (!child_requested && !child_empty) {
                    dest_index = i;
                    break;
                }
            }
            short dest_pid = pid_from_child_index(dest_index);
            interconnect.send_work_request(dest_pid, false);
            State::waiting_on_response[dest_index] = true;
        }
    }
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
        State::num_non_trivial_tasks++;
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
        State::num_non_trivial_tasks += 2;
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
    assert(State::num_non_trivial_tasks > 0);
    if (PRINT_LEVEL >= 5) printf("\n");
    Task task = get_task(task_stack);
    int var_id = task.var_id;
    int assignment = task.assignment;
    if (var_id == -1) { // Children backtracked, need to backtrack ourselves
        print_data(cnf, task_stack, "Children backtrack");
        cnf.backtrack();
        return false;
    } else {
        State::num_non_trivial_tasks--;
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
            State::num_non_trivial_tasks--;
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
        assert(State::num_non_trivial_tasks >= 0);
        if (result) {
            return true;
        } else if (task_stack.count == 0) {
            return false;
        }
        while (workers_requesting() && can_give_work(task_stack, interconnect)) {
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
