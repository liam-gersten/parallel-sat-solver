#include "state.h"

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
}

// True if parent hasn't sent us an urgent request
bool State::parent_may_have_work() {
    return child_statuses[2] != 'u';
}

// Requests more work to do from parent in non-urgent fashion
void State::request_work_from_parent(Interconnect &interconnect) {
    interconnect.send_work_request(parent_id, false);
}

// Requests more work to do from one or more children
void State::request_work_from_children(Interconnect &interconnect) {
    for (int i = 0; i < num_children; i++) {
        if (child_statuses[i] != 'u') {
            int child_id = child_ids[i];
            interconnect.send_work_request(child_id, false);
        }
    }
}

// Sends urgent message to the one remaining child/parent who hasn't
// sent us one.
void State::urgently_request_work(Interconnect &interconnect) {
    // TODO: Implement this
    for (int i = 0; i < num_children; i++) {
        if (child_statuses[i] != 'u') {
            int child_id = child_ids[i];
            interconnect.send_work_request(child_id, true);
            child_statuses[i] = 'u';
            num_urgent++;
            return;
        }
    }
    interconnect.send_work_request(parent_id, true);
    child_statuses[num_children] = 'u';
    num_urgent++;
}

// Terminates state and current thread
void State::abort(Interconnect &interconnect) {
    // TODO: Implement this
    return;
}

// Updates a child or parent's status
void State::set_status(short id, char status) {
    for (int i = 0; i < num_children; i++) {
        if (id == child_ids[i]) {
            child_statuses[i] = status;
            return;
        }
    }
}

// Adds work to stack
void State::add_to_stack(Message work, Deque &task_stack) {
    // TODO: Implement this
    return;
}

// Saves a work request to be filled later
void State::stash_request(Message request) {
    // TODO: Implement this
    return;
}

// Sends work to requester
void State::serve_request(Message request) {
    // TODO: Implement this
    return;
}

// Serves as many stashed requests as possible
void State::serve_requests(Interconnect &interconnect) {
    // TODO: Implement this
    return;
}

// Adds one or two variable assignment tasks to task stack
void State::add_tasks_from_formula(Cnf &cnf, Deque &task_stack) {
    cnf.clauses.reset_iterator();
    Clause current_clause = *((Clause *)(cnf.clauses.get_current_value()));
    int current_clause_id = current_clause.id;
    int new_var_id;
    bool new_var_sign;
    int num_unsat = cnf.pick_from_clause(
        current_clause, &new_var_id, &new_var_sign);
    if (PRINT_LEVEL > 0) printf("%sPID %d picked new var %d from clause %d %s\n", cnf.depth_str.c_str(), 0, new_var_id, current_clause_id, cnf.clause_to_string_current(current_clause, false).c_str());
    // Add an undo task to do last
    void *undo_task = make_task(-1, true);
    task_stack.add_to_front(undo_task);
    if (num_unsat == 1) {
        void *only_task = make_task(new_var_id, new_var_sign);
        task_stack.add_to_front(only_task);
    } else {
        bool first_choice = get_first_pick(new_var_sign);
        void *important_task = make_task(new_var_id, first_choice);
        void *other_task = make_task(new_var_id, !first_choice);
        task_stack.add_to_front(other_task);
        task_stack.add_to_front(important_task);
    }
}

// Runs one iteration of the solver
bool State::solve_iteration(Cnf &cnf, Deque &task_stack) {
    if (PRINT_LEVEL > 1) cnf.print_task_stack(0, "Loop start", task_stack);
    if (PRINT_LEVEL > 1) cnf.print_cnf(0, "Loop start CNF", cnf.depth_str, (PRINT_LEVEL >= 2));
    if (cnf.clauses.get_linked_list_size() == 0) {
        if (PRINT_LEVEL > 0) printf("%sPID %d base case success\n", cnf.depth_str.c_str(), 0);
        if (PRINT_LEVEL > 0) printf("Edit stack size = %d\n", (*cnf.edit_stack).count);
        return true;
    }
    Task task = get_task(task_stack);
    int var_id = task.var_id;
    int assignment = task.assignment;
    if (var_id == -1) {
        // Children backtracked, need to backtrack ourselves
        if (PRINT_LEVEL > 1) cnf.print_task_stack(0, "Children backtrack", task_stack);
        if (task_stack.count == 0) return false;
        cnf.backtrack();
        return false;
    }
    cnf.recurse();
    // Propagate choice
    bool propagate_result = cnf.propagate_assignment(var_id, assignment);
    if (!propagate_result) { // Conflict clause found
        if (PRINT_LEVEL > 1) cnf.print_task_stack(0, "Prop fail", task_stack);
        if (task_stack.count == 0) return false;
        cnf.backtrack();
        return false;
    }
    if (cnf.clauses.get_linked_list_size() == 0) {
        if (PRINT_LEVEL > 0) printf("%sPID %d base case success\n", cnf.depth_str.c_str(), 0);
        if (PRINT_LEVEL > 0) printf("Edit stack size = %d\n", (*cnf.edit_stack).count);
        return true;
    }
    add_tasks_from_formula(cnf, task_stack);
    if (PRINT_LEVEL > 1) cnf.print_task_stack(0, "Loop end", task_stack);
    return false;
}

// Continues solve operation
bool State::solve(Cnf &cnf, Deque &task_stack, Interconnect &interconnect) {
    if (pid == 0) {
        add_tasks_from_formula(cnf, task_stack);
    }
    Message message;
    while (true) {
        bool result = solve_iteration(cnf, task_stack);
        if (result) {
            return true;
        }
        if (task_stack.count == 0) {
            return false;
        }
    }
}
