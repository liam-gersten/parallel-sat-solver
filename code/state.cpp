#include "state.h"
#include <cassert>

State::State(
        short pid, 
        short nprocs, 
        short branching_factor, 
        bool pick_greedy,
        bool use_smart_prop,
        bool explicit_true) 
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
    State::cchild_statuses = (char *)malloc(
        sizeof(char) * (State::num_children + 1));
    for (short child = 0; child <= State::num_children; child++) {
        State::cchild_statuses[child] = 'w';
    }
    State::rrequests_sent = (char *)calloc(
        sizeof(char), State::num_children + 1);
    for (short child = 0; child < State::num_children; child++) {
        State::rrequests_sent[child] = 'n';
    }
    if (pid == 0) {
        // Don't want to request the parent that doesn't exist
        State::rrequests_sent[State::num_children] = 'u';
    } else {
        State::rrequests_sent[State::num_children] = 'n';
    }
    State::nnum_urgent = 0;
    State::nnum_requesting = 0;
    State::num_non_trivial_tasks = 0;
    State::process_finished = false;
    State::was_explicit_abort = false;
    State::calls_to_solve = 0;
    State::pick_greedy = pick_greedy;
    State::use_smart_prop = use_smart_prop;
    State::explicit_true = explicit_true;
    State::print_index = 0;
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

// Prints out the current progress of the solver
void State::print_progress(Cnf &cnf, Deque &task_stack) {
    unsigned int assigned_variables = cnf.num_vars_assigned;
    unsigned int unassigned_variables = cnf.num_variables - assigned_variables;
    unsigned int remaining_clauses = cnf.clauses.num_indexed - cnf.num_clauses_dropped;
    printf("PID %d: [ depth %d || %d unassigned variables || %d remaining clauses || %d size work stack ]\n", State::pid, cnf.depth, unassigned_variables, remaining_clauses, task_stack.count);
}

// Returns whether there are any other processes requesting our work
bool State::workers_requesting() {
    return State::nnum_requesting > 0;
}

// Returns whether we should forward an urgent request
bool State::should_forward_urgent_request() {
    if (State::pid == 0) {
        return (State::nnum_urgent == State::num_children - 1);
    }
    return (State::nnum_urgent == State::num_children);
}

// Returns whether we should implicitly abort due to urgent requests
bool State::should_implicit_abort() {
    if (State::pid == 0) {
        return (State::nnum_urgent == State::num_children);
    }
    return (State::nnum_urgent == State::num_children + 1);
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
void *State::grab_work_from_stack(Cnf &cnf, Deque &task_stack) {
    if (PRINT_LEVEL > 0) printf("PID %d grabbing work from stack\n", State::pid);
    assert(State::num_non_trivial_tasks > 1);
    assert(task_stack.count >= State::num_non_trivial_tasks);
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

// Picks recipient index to give work to
short State::pick_work_recipient() {
    // P0 has it's Parent urgently requesi
    short recipient_index;
    for (short child = 0; child <= State::num_children; child++) {
        if (State::nnum_urgent > 0) {
            // Urgent requests take precedence
            // Pick an urgently-requesting child (or parent)
            if (State::cchild_statuses[child] == 'r'
                || State::cchild_statuses[child] == 'w') {
                // Non-urgent
                continue;
            }
        } else {
            // Pick a requesting child (or parent)
            if (State::cchild_statuses[child] == 'w') {
                // Not requesting
                continue;
            }
        }
        if (PRINT_LEVEL > 0) printf("PID %d picked work recipient %d\n", State::pid, pid_from_child_index(child));
        return child;
    }
    assert(false);
    return 0;
}

// Picks recipient index to ask for work
short State::pick_request_recipient() {
    bool make_double_requests = should_forward_urgent_request();
    short recipient_index = -1;
    // Pick recipient we haven't already asked, prefering one who hasn't asked
    // us, with bias for asking our parent (hence reverse direction).
    for (short child = State::num_children; child >= 0; child--) {
        if (make_double_requests) {
            if (State::rrequests_sent[child] == 'u') {
                // We've urgently-asked them already
                continue;
            }
        } else {
            if (State::rrequests_sent[child] != 'n') {
                // We've asked them already
                continue;
            }
        }
        if (State::cchild_statuses[child] == 'u') {
            // Can't ask them, they're out of work entirely
            continue;
        }
        recipient_index = child;
        if (State::cchild_statuses[child] == 'w') {
            // They're not asking us for work, stop here
            break;
        }
    }
    assert(recipient_index != -1);
    if (PRINT_LEVEL > 0) printf("PID %d picked request recipient %d\n", State::pid, pid_from_child_index(recipient_index));
    return recipient_index;
}

// Gives one unit of work to lazy processors
void State::give_work(
        Cnf &cnf, 
        Deque &task_stack, 
        Interconnect &interconnect) 
    {
    if (PRINT_LEVEL > 0) printf("PID %d giving work\n", State::pid);
    short recipient_index = pick_work_recipient();
    assert(State::cchild_statuses[recipient_index] != 'w');
    short recipient_pid = pid_from_child_index(recipient_index);
    void *work;
    // Prefer to give stashed work
    if (interconnect.have_stashed_work()) {
        if (interconnect.have_stashed_work(recipient_pid)) {
            work = (interconnect.get_stashed_work(recipient_pid)).data;
        } else {
            work = (interconnect.get_stashed_work()).data;
        }
    } else {
        work = grab_work_from_stack(cnf, task_stack);
    }
    if (State::cchild_statuses[recipient_index] == 'u') {
        State::nnum_urgent--;
    }
    State::cchild_statuses[recipient_index] = 'w';
    State::nnum_requesting--;
    interconnect.send_work(recipient_pid, work);
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
    if (PRINT_LEVEL > 0) printf("PID %d getting work from interconnect stash\n", State::pid);
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
    assert(State::nnum_urgent <= State::num_children);
    if (should_implicit_abort()) {
        // Self abort
        abort_others(interconnect);
        abort_process();
    } else if (should_forward_urgent_request()) {
        if (PRINT_LEVEL > 0) printf("PID %d urgently asking for work\n", State::pid);
        // Send a single urgent work request
        short dest_index = pick_request_recipient();
        short dest_pid = pid_from_child_index(dest_index);
        if (State::rrequests_sent[dest_index] == 'r') {
            // Send an urgent upgrade
            interconnect.send_work_request(dest_pid, 2);
        } else {
            assert(State::rrequests_sent[dest_index] == 'n');
            // Send an urgent request
            interconnect.send_work_request(dest_pid, 1);
        }
        State::rrequests_sent[dest_index] = 'u';
    } else {
        if (PRINT_LEVEL > 0) printf("PID %d asking for work\n", State::pid);
        // Send a single work request
        short dest_index = pick_request_recipient();
        short dest_pid = pid_from_child_index(dest_index);
        assert(State::rrequests_sent[dest_index] == 'n');
        interconnect.send_work_request(dest_pid, 0);
        State::rrequests_sent[dest_index] = 'r';
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
        assert(should_implicit_abort());
        for (short child = 0; child < State::num_children; child++) {
            if (State::rrequests_sent[child] != 'u') {
                // Child needs one
                short recipient = pid_from_child_index(child);
                if (State::rrequests_sent[child] == 'n') {
                    // Normal urgent request
                    interconnect.send_work_request(recipient, 1);
                } else {
                    // Urgent upgrade
                    interconnect.send_work_request(recipient, 2);
                }
                State::rrequests_sent[child] = 'u';
            }
        }
    }
}

// Handles work received
void State::handle_work_request(
        short sender_pid,
        short version,
        Cnf &cnf,
        Deque &task_stack, 
        Interconnect &interconnect) 
    {
    short child_index = child_index_from_pid(sender_pid);
    if (version == 0) { // Normal request
        if (PRINT_LEVEL > 0) printf("PID %d handling work request from sender %d (child %d)\n", State::pid, sender_pid, child_index);
        assert(State::cchild_statuses[child_index] == 'w');
        // Log the request
        State::cchild_statuses[child_index] = 'r';
        State::nnum_requesting++;
    } else { // Urgent request
        if (version == 1) { // Urgent request
            if (PRINT_LEVEL > 0) printf("PID %d handling urgent work request from sender %d (child %d)\n", State::pid, sender_pid, child_index);
            assert(State::cchild_statuses[child_index] == 'w');
            State::nnum_requesting++;
        } else { // Urgent upgrade
            if (PRINT_LEVEL > 0) printf("PID %d handling urgent upgrade from sender %d (child %d)\n", State::pid, sender_pid, child_index);
            assert(State::cchild_statuses[child_index] != 'u');
            if (State::cchild_statuses[child_index] == 'w') {
                // Effectively a no-op, we already handled the non-urgent 
                // request that came before
                return;
            }
        }
        // Log the request
        State::cchild_statuses[child_index] = 'u';
        State::nnum_urgent++;
        // Check if we need to forward urgent, or abort
        if (out_of_work()) {        
            if (should_implicit_abort()) {
                // Self-abort
                abort_others(interconnect);
                abort_process();
            } else if (should_forward_urgent_request()) {
                // Send a single urgent work request
                short dest_index = pick_request_recipient();
                short dest_pid = pid_from_child_index(dest_index);
                if (State::rrequests_sent[dest_index] == 'r') {
                    // Send an urgent upgrade
                    interconnect.send_work_request(dest_pid, 2);
                } else {
                    assert(State::rrequests_sent[dest_index] == 'n');
                    // Send an urgent request
                    interconnect.send_work_request(dest_pid, 1);
                }
                State::rrequests_sent[dest_index] = 'u';
            }
        }
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
    assert(State::rrequests_sent[child_index] != 'n');
    assert(cchild_statuses[child_index] != 'u');
    assert(!interconnect.have_stashed_work(sender_pid));
    if (out_of_work()) {
        // Reconstruct state from work
        cnf.reconstruct_state(work, task_stack);
        State::num_non_trivial_tasks = 1;
    } else {
        // Add to interconnect work stash
        interconnect.stash_work(message);
    }
    State::rrequests_sent[child_index] = 'n';
}

// Handles an abort message, possibly forwarding it
void State::handle_message(
        Message message, 
        Cnf &cnf, 
        Deque &task_stack, 
        Interconnect &interconnect) 
    {
    if (PRINT_LEVEL > 0) printf("PID %d handling message\n", State::pid);
    assert(0 <= State::nnum_urgent && State::nnum_urgent <= State::num_children);
    assert(State::nnum_urgent <= State::nnum_requesting);
    assert(State::nnum_requesting <= State::num_children + 1);
    assert(0 <= message.type <= 4);
    switch (message.type) {
        case 3: {
            handle_work_message(
                message, cnf, task_stack, interconnect);
            return;
        } case 4: {
            abort_process(true);
            return;
        } default: {
            handle_work_request(
                message.sender, message.type, cnf, task_stack, interconnect);
            return;
        }
    }
    assert(0 <= State::nnum_urgent 
        && State::nnum_urgent <= State::num_children + 1);
    assert(State::nnum_urgent <= State::nnum_requesting);
    assert(State::nnum_requesting <= State::num_children + 1);
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
        bool first_choice;
        if (State::explicit_true) {
            first_choice = true;
        } else {
            first_choice = State::pick_greedy ? new_var_sign : !new_var_sign;
        }
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
    unsigned int vars_assigned_at_start = cnf.num_vars_assigned;
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
    if (print_index == 100 && PRINT_PROGRESS) {
        print_progress(cnf, task_stack);
        print_index = 0;
    }
    print_index++;
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
        if (State::use_smart_prop) {
            if (!cnf.smart_propagate_assignment(var_id, assignment)) {
                // Conflict clause found
                print_data(cnf, task_stack, "Smart prop fail");
                cnf.backtrack();
                return false;
            }
        }
        if (cnf.clauses.get_linked_list_size() == 0) {
            if (PRINT_PROGRESS) print_progress(cnf, task_stack);
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
        interconnect.clean_dead_messages();
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
        if (print_index % 10 == 0) {
            while (interconnect.async_receive_message(message) && !State::process_finished) {
                handle_message(message, cnf, task_stack, interconnect);
                // TODO: serve work here?
            }
        }
        while (workers_requesting() && can_give_work(task_stack, interconnect)) {
            give_work(cnf, task_stack, interconnect);
        }
    }
    return false;
}
