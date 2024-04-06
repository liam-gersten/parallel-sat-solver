#include "state.h"
#include <cassert>

State::State(
        short pid, 
        short nprocs, 
        short branching_factor, 
        bool pick_greedy) 
    {
    State::pid = pid;
    State::nprocs = nprocs;
    State::parent_id = (pid - 1) / branching_factor;
    State::num_children = branching_factor;
    State::branching_factor = branching_factor;
    short actual_num_children = 0;
    for (short child = 0; child < branching_factor; child++) {
        short child_pid = pid_from_child_index(child);
        if (child_pid >= State::nprocs) {
            break;
        }
        actual_num_children++;
    }
    State::num_children = actual_num_children;
    if (PRINT_LEVEL > 0) printf("PID %d has %d children\n", pid, (int)State::num_children);
    State::child_statuses = (char *)malloc(
        sizeof(char) * (State::num_children + 1));
    for (short child = 0; child < State::num_children; child++) {
        State::child_statuses[child] = 's';
    }
    if (pid == 0) {
        State::child_statuses[State::num_children] = 'u';
        State::num_urgent = 1;
    } else {
        State::child_statuses[State::num_children] = 's'; // ?
        State::num_urgent = 0;
    }
    State::requests_sent = (char *)calloc(
        sizeof(char), State::num_children + 1);
    for (short child = 0; child <= State::num_children; child++) {
        State::requests_sent[child] = 'n';
    }
    State::num_requesting = 0;
    State::num_urgent = 0;
    State::num_non_trivial_tasks = 0;
    State::process_finished = false;
    State::was_explicit_abort = false;
    State::calls_to_solve = 0;
    State::pick_greedy = pick_greedy;
}

// Gets child (or parent) pid from child (or parent) index
short State::pid_from_child_index(short child_index) {
    assert(0 <= child_index && child_index <= State::num_children);
    if (child_index == State::num_children) {
        // Is parent
        return State::parent_id;
    }
    return (State::pid * State::branching_factor) + child_index + 1;
}

// Gets child (or parent) index from child (or parent) pid
short State::child_index_from_pid(short child_pid) {
    assert(0 <= child_pid && child_pid <= State::nprocs);
    if (child_pid == State::parent_id) {
        return State::num_children;
    }
    return child_pid - 1 - (State::pid * State::branching_factor);
}

// Returns whether there are any other processes requesting our work
bool State::workers_requesting() {
    return State::num_requesting > 0;
}

// Returns whether the state is able to supply work to requesters
bool State::can_give_work(Deque task_stack, Interconnect interconnect) {
    return ((State::num_non_trivial_tasks + interconnect.num_stashed_work) > 1);
}

// Applies an edit to the given compressed CNF
void State::apply_edit_to_compressed(
        Cnf &cnf,
        unsigned int *compressed, 
        FormulaEdit edit) 
    {
    switch (edit.edit_type) {
        case 'v': {
            // Add variable set to it's int offset
            int variable_id = edit.edit_id;
            VariableLocations location = cnf.variables[variable_id];
            unsigned int mask_to_add = location.variable_addition;
            unsigned int offset;
            if (cnf.assigned_true[variable_id]) {
                offset = location.variable_true_addition_index;
            } else {
                assert(cnf.assigned_false[variable_id]);
                offset = location.variable_false_addition_index;
            }
            compressed[offset] += mask_to_add;
            return;
        } case 'c': {
            // Add clause drop to it's int offset
            int clause_id = edit.edit_id;
            Clause clause = *((Clause *)(cnf.clauses.get_value(clause_id)));
            unsigned int mask_to_add = clause.clause_addition;
            unsigned int offset = clause.clause_addition_index;
            compressed[offset] += mask_to_add;
            return;
        } default: {
            return;
        }
    }
}

// Grabs work from the top of the task stack, updates Cnf structures
void *State::steal_work(Cnf &cnf, Deque &task_stack) {
    if (PRINT_LEVEL > 0) printf("PID %d stealing work\n", State::pid);
    assert(State::num_non_trivial_tasks > 1);
    // Get the actual work as a copy
    Task top_task = *((Task *)(task_stack.pop_from_back()));
    void *work = cnf.convert_to_work_message(cnf.oldest_compressed, top_task);
    // Prune the top of the tree
    while (backtrack_at_top(task_stack)) {
        // Two deques loose their top element, one looses at least one element
        int edits_to_apply = (*cnf.edit_counts_stack).pop_from_back();
        assert(edits_to_apply > 0);
        while (edits_to_apply) {
            FormulaEdit edit = *((FormulaEdit *)((*(cnf.edit_stack)).pop_from_back()));
            apply_edit_to_compressed(cnf, cnf.oldest_compressed, edit);
            edits_to_apply--;
        }
        // Ditch the backtrack task at the top
        task_stack.pop_from_back();
    }
    State::num_non_trivial_tasks--;
    assert(State::num_non_trivial_tasks >= 1);
    return work;
}

// Gives one unit of work to lazy processors
void State::give_work(
        Cnf &cnf, 
        Deque &task_stack, 
        Interconnect &interconnect) 
    {
    if (PRINT_LEVEL > 0) printf("PID %d giving work\n", State::pid);
    short recipient_index;
    short recipient_pid;
    void *work;
    // Prefer to give stashed work
    if (interconnect.have_stashed_work()) {
        for (short child = 0; child <= State::num_children; child++) {
            if (State::num_urgent > 0) {
                // Urgent requests take precedence
                // Pick an urgently-requesting child that has stashed work,
                // or an arbitrary one.
                if (State::child_statuses[child] != 'u' 
                    && State::child_statuses[child] != 'd') {
                    continue;
                }
            } else {
                // Check every requesting child (or parent) and see if they 
                // request their own work back, or pick arbitrary recipient
                if (State::child_statuses[child] == 's') {
                    continue;
                }
            }
            recipient_index = child;
            recipient_pid = pid_from_child_index(child);
            if (interconnect.have_stashed_work(recipient_pid)) {
                break;
            }
        }
        // Now pick the work to send
        if (interconnect.have_stashed_work(recipient_pid)) {
            work = (interconnect.get_stashed_work(recipient_pid)).data;
        } else {
            work = (interconnect.get_stashed_work(recipient_pid)).data;
        }
    } else {
        // Pick some recipient
        for (short child = 0; child <= State::num_children; child++) {
            if (State::num_urgent > 0) {
                // must be an urgently-requesting child
                if (State::child_statuses[child] != 'u'
                    && State::child_statuses[child] != 'd') {
                    continue;
                }
            } else {
                // must be a requesting child
                if (State::child_statuses[child] == 's') {
                    continue;
                }
            }
            recipient_index = child;
            recipient_pid = pid_from_child_index(child);
            break;
        }
        work = steal_work(cnf, task_stack);
    }
    if (State::child_statuses[recipient_index] == 'd') {
        set_work_responds_to_dual_requests(work);
        State::child_statuses[recipient_index] = 'r';
        State::num_urgent--;
    } else if (State::child_statuses[recipient_index] == 'u') {
        State::child_statuses[recipient_index] = 's';
        State::num_urgent--;
        State::num_requesting--;
    } else {
        State::child_statuses[recipient_index] = 's';
        State::num_requesting--;
    }
    interconnect.send_work(recipient_pid, work);
}

// Gets stashed work, returns true if any was grabbed
bool State::get_work_from_interconnect_stash(
        Cnf &cnf, 
        Deque &task_stack, 
        Interconnect &interconnect) 
    {
    if (PRINT_LEVEL > 0) printf("PID %d getting work from interconnect stash\n", State::pid);
    // TODO: Perhaps we get stashed work from a sender who is no longer 
    // asking us for work now (if possible)?
    // Preference for children or parents?
    if (!interconnect.have_stashed_work()) {
        return false;
    }
    Message work_message = interconnect.get_stashed_work();
    handle_work_message(
        work_message, cnf, task_stack, interconnect);
    return true;
}

// Returns whether we are out of work to do
bool State::out_of_work() {
    return State::num_non_trivial_tasks == 0;
}

// Asks parent or children for work, called once when we finish our work
void State::ask_for_work(Cnf &cnf, Interconnect &interconnect) {
    assert(!interconnect.have_stashed_work());
    assert(State::num_urgent <= State::num_children);
    if (State::num_urgent == State::num_children + 1) {
        // Self abort
        abort_others(interconnect);
        abort_process();
    } else if (State::num_urgent == State::num_children) {
        if (PRINT_LEVEL > 0) printf("PID %d urgently asking for work\n", State::pid);
        // Send a single urgent work request
        short dest_index;
        // Find index of the only adjecent node that hasn't urgently requested
        for (short i = 0; i <= State::num_children; i++) {
            if (State::child_statuses[i] != 'u' && State::child_statuses[i] != 'd') {
                dest_index = i;
                break;
            }
        }
        short dest_pid = pid_from_child_index(dest_index);
        assert(State::requests_sent[dest_index] != 'u');
        interconnect.send_work_request(dest_pid, true);
        if (State::requests_sent[dest_pid] == 'r') {
            State::requests_sent[dest_index] = 'd';
        } else {
            assert(State::requests_sent[dest_pid] == 'n');
            State::requests_sent[dest_index] = 'u';
        }
    } else {
        if (PRINT_LEVEL > 0) printf("PID %d asking for work\n", State::pid);
        // Send a single work request to my parent.
        // If the parent has already been requested and hasn't responded,
        // ask a child who has not been requested yet if applicable.
        bool parent_requested = (State::requests_sent[State::num_children] != 'n');
        bool parent_empty = State::child_statuses[State::num_children] == 'u';
        if (!parent_requested && !parent_empty) {
            interconnect.send_work_request(State::parent_id, false);
            State::requests_sent[State::num_children] = 'r';
        } else {
            short dest_index;
            // Find index of valid child
            for (short i = 0; i < State::num_children; i++) {
                bool child_requested = (State::requests_sent[i] != 'n');
                bool child_empty = State::child_statuses[i] == 'u';
                if (!child_requested && !child_empty) {
                    dest_index = i;
                    break;
                }
            }
            short dest_pid = pid_from_child_index(dest_index);
            interconnect.send_work_request(dest_pid, false);
            State::requests_sent[dest_index] = 'r';
        }
    }
}

// Empties/frees data structures and immidiately returns
void State::abort_process(bool explicit_abort) {
    if (PRINT_LEVEL > 0) printf("PID %d aborting process\n", State::pid);
    State::process_finished = true;
    State::was_explicit_abort = explicit_abort;
}

// Sends messages to children to force them to abort
void State::abort_others(Interconnect &interconnect, bool explicit_abort) {
    if (explicit_abort) {
        if (PRINT_LEVEL > 0) printf("PID %d explicitly aborting others\n", State::pid);
        // Success, broadcase explicit abort to every process
        for (short i = 0; i < State::nprocs; i++) {
            interconnect.send_abort_message(i);
        }
    } else {
        if (PRINT_LEVEL > 0) printf("PID %d aborting others\n", State::pid);
        // Any children who did not receive an urgent request from us should
        assert(State::num_urgent == State::num_children + 1);
        for (short child = 0; child < State::num_children; child++) {
            if (State::requests_sent[child] != 'u' 
                && State::requests_sent[child] != 'd') {
                // Child needs one
                short recipient = pid_from_child_index(child);
                interconnect.send_work_request(recipient, true);
                State::requests_sent[child] = 'd';
            }
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
    short child_index = child_index_from_pid(sender_pid);
    if (is_urgent) {
        if (PRINT_LEVEL > 0) printf("PID %d handling urgent work request from sender %d (child %d)\n", State::pid, sender_pid, child_index);
        assert(State::child_statuses[child_index] != 'u');
        if (State::child_statuses[child_index] == 's') {
            State::num_requesting++;
            State::child_statuses[child_index] = 'u';
        } else {
            assert(State::child_statuses[child_index] == 'r');
            State::child_statuses[child_index] = 'd';
        }
        State::num_urgent++;
        if (out_of_work()) {        
            if (State::num_urgent == State::num_children + 1) {
                // Self-abort
                printf(" Urgent = %d\n", State::num_urgent);
                abort_others(interconnect);
                abort_process();
            } else if (State::num_urgent == State::num_children) {
                // Forward urgent request
                for (short child = 0; child <= State::num_children; child++) {
                    if (State::child_statuses[child] != 'u') {
                        short child_pid = pid_from_child_index(child);
                        interconnect.send_work_request(child_pid, true);
                        if (State::requests_sent[child_pid] == 'r') {
                            State::requests_sent[child_pid] = 'd';
                        } else {
                            assert(State::requests_sent[child_pid] == 's');
                            State::requests_sent[child_pid] = 'u';
                        }
                        return;
                    }
                }
            }
        }
    } else {
        if (PRINT_LEVEL > 0) printf("PID %d handling work request from sender %d (child %d)\n", State::pid, sender_pid, child_index);
        assert(State::child_statuses[child_index] == 's');
        // Log the request
        State::child_statuses[child_index] = 'r';
        State::num_requesting++;
    }
}

// Handles work received
void State::handle_work_message(
        Message message,
        Cnf &cnf, 
        Deque &task_stack, 
        Interconnect &interconnect) 
    {
    if (PRINT_LEVEL > 0) printf("PID %d handling work message\n", State::pid);
    short sender_pid = message.sender;
    short child_index = child_index_from_pid(sender_pid);
    void *work = message.data;
    assert(State::requests_sent[child_index] != 'n');
    assert(child_statuses[child_index] != 'u');
    assert(!interconnect.have_stashed_work(sender_pid));
    if (out_of_work()) {
        // Reconstruct state from work
        cnf.reconstruct_state(work, task_stack);
        State::num_non_trivial_tasks = 1;
    } else {
        // Add to interconnect work stash
        interconnect.stash_work(message);
    }
    if (State::requests_sent[child_index] == 'd') {
        if (work_responds_to_dual_requests(work)) {
            State::requests_sent[child_index] = 'n';
        } else {
            State::requests_sent[child_index] = 'u';
        }
    } else {
        State::requests_sent[child_index] = 'n';
    }
}

// Handles an abort message, possibly forwarding it
void State::handle_message(
        Message message, 
        Cnf &cnf, 
        Deque &task_stack, 
        Interconnect &interconnect) 
    {
    if (PRINT_LEVEL > 0) printf("PID %d handling message\n", State::pid);
    assert(0 <= State::num_urgent && State::num_urgent <= State::num_children);
    assert(State::num_urgent <= State::num_requesting);
    assert(State::num_requesting <= State::num_children + 1);
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
                message, cnf, task_stack, interconnect);
            return;
        } default: {
            assert(message.type == 3);
            abort_process(true);
            return;
        }
    }
    assert(0 <= State::num_urgent 
        && State::num_urgent <= State::num_children + 1);
    assert(State::num_urgent <= State::num_requesting);
    assert(State::num_requesting <= State::num_children + 1);
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
    if (PRINT_LEVEL > 1) printf("%sPID %d picked new var %d from clause %d %s\n", cnf.depth_str.c_str(), State::pid, new_var_id, current_clause_id, cnf.clause_to_string_current(current_clause, false).c_str());
    if (num_unsat == 1) {
        void *only_task = make_task(new_var_id, new_var_sign);
        task_stack.add_to_front(only_task);
        State::num_non_trivial_tasks++;
        return 1;
    } else {
        // Add an undo task to do last
        if (!skip_undo && (task_stack.count > 0)) {
            void *undo_task = make_task(-1, true);
            task_stack.add_to_front(undo_task);
        }
        bool first_choice = State::pick_greedy ? new_var_sign : !new_var_sign;
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
    State::calls_to_solve++;
    assert(State::num_non_trivial_tasks > 0);
    task_stack_invariant(task_stack, State::num_non_trivial_tasks);
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
    while (true) {
        task_stack_invariant(task_stack, State::num_non_trivial_tasks);
        if (PRINT_LEVEL > 1) printf("%sPID %d Attempting %d = %d\n", cnf.depth_str.c_str(), State::pid, var_id, assignment);
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
        cnf.recurse();
        return false;
    }
}

// Continues solve operation, returns true iff a solution was found by
// the current thread.
bool State::solve(Cnf &cnf, Deque &task_stack, Interconnect &interconnect) {
    if (interconnect.pid == 0) {
        add_tasks_from_formula(cnf, task_stack, true);
        assert(task_stack.count > 0);
    }
    while (!State::process_finished) {
        if (out_of_work()) {
            bool found_work = get_work_from_interconnect_stash(
                cnf, task_stack, interconnect);
            if (!found_work) {
                ask_for_work(cnf, interconnect);
            }
        }
        Message message;
        while (out_of_work() && !State::process_finished) {
            bool message_received = interconnect.async_receive_message(message);
            if (message_received) {
                handle_message(message, cnf, task_stack, interconnect);
            }
        }
        if (State::process_finished) break;
        bool result = solve_iteration(cnf, task_stack);
        assert(State::num_non_trivial_tasks >= 0);
        if (result) {
            abort_others(interconnect, true);
            abort_process(true);
            return true;
        }
        while (interconnect.async_receive_message(message) && !State::process_finished) {
            handle_message(message, cnf, task_stack, interconnect);
            // TODO: serve work here?
        }
        while (workers_requesting() && can_give_work(task_stack, interconnect)) {
            give_work(cnf, task_stack, interconnect);
        }
    }
    return false;
}
