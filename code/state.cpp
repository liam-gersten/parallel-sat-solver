#include "state.h"
#include <list>
#include <cassert> 
#include <unistd.h>

#include <map>

State::State(
        short pid,
        short nprocs,
        short branching_factor,
        short assignment_method)
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
    if (PRINT_LEVEL > 0) printf("PID %d: has %d children\n", pid, (int)State::num_children);
    State::child_statuses = (char *)malloc(
        sizeof(char) * (State::num_children + 1));
    for (short child = 0; child <= State::num_children; child++) {
        State::child_statuses[child] = 'w';
    }
    State::requests_sent = (char *)calloc(
        sizeof(char), State::num_children + 1);
    for (short child = 0; child < State::num_children; child++) {
        State::requests_sent[child] = 'n';
    }
    if (pid == 0) {
        // Don't want to request the parent that doesn't exist
        State::requests_sent[State::num_children] = 'u';
    } else {
        State::requests_sent[State::num_children] = 'n';
    }
    State::num_urgent = 0;
    State::num_requesting = 0;
    State::num_non_trivial_tasks = 0;
    State::process_finished = false;
    State::was_explicit_abort = false;
    State::calls_to_solve = 0;
    State::assignment_method = assignment_method;
    State::current_cycle = 0;
    Deque thieves;
    State::thieves = (Deque *)malloc(sizeof(Deque));
    *State::thieves = thieves;
    GivenTask current_task;
    current_task.pid = -1;
    current_task.var_id = -1;
    State::current_task = current_task;

    State::last_asked_child = State::num_children;
}

// Gets child (or parent) pid from child (or parent) index
short State::pid_from_child_index(short child_index) {
    assert(-1 <= child_index);
    assert(child_index <= State::num_children);
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
    unsigned int remaining_clauses = cnf.clauses.num_indexed - cnf.clauses.num_clauses_dropped;
    printf("PID %d: [ depth %d || %d unassigned variables || %d remaining clauses || %d size work stack ]\n", State::pid, cnf.depth, unassigned_variables, remaining_clauses, task_stack.count);
}

// Returns whether there are any other processes requesting our work
bool State::workers_requesting() {
    return State::num_requesting > 0;
}

// Returns whether we should forward an urgent request
bool State::should_forward_urgent_request() {
    if (State::pid == 0) {
        return (State::num_urgent == State::num_children - 1);
    }
    return (State::num_urgent == State::num_children);
}

// Returns whether we should implicitly abort due to urgent requests
bool State::should_implicit_abort() {
    if (State::pid == 0) {
        return (State::num_urgent == State::num_children);
    }
    return (State::num_urgent == State::num_children + 1);
}

// Returns whether the state is able to supply work to requesters
bool State::can_give_work(Deque task_stack, Interconnect interconnect) {
    return ((State::num_non_trivial_tasks + interconnect.num_stashed_work) > 1);
}

// Ensures the task stack is a valid one
bool State::task_stack_invariant(
        Cnf &cnf, 
        Deque &task_stack, 
        int supposed_num_tasks) 
    {
        // return true;
    assert((supposed_num_tasks == 0) || !backtrack_at_top(task_stack));
    if (State::num_non_trivial_tasks > 0) {
        assert(task_stack.count > 0);
    }
    int num_processed = 0;
    int num_to_process = task_stack.count;
    int true_num_tasks = 0;
    while (num_processed < num_to_process) {
        void *current_ptr = task_stack.pop_from_front();
        Task current = *((Task *)current_ptr);
        if (!current.is_backtrack) {
            // This is a non-trivial task, increment
            true_num_tasks++;
        }
        task_stack.add_to_back(current_ptr);
        num_processed++;
    }
    assert(task_stack.count == num_to_process);
    void **task_list = task_stack.as_list();
    void **thief_list = (*State::thieves).as_list();
    int num_thieves = (*State::thieves).count;
    int total_assigned = 0;
    int num_queued = 0;
    int num_stolen = 0;
    int reported_num_local = 0;
    int reported_num_queued = 0;
    int reported_num_remote = 0;
    int reported_num_stolen = 0;
    for (int i = 0; i < task_stack.count; i++) {
        void *task_ptr = task_list[i];
        Task task = *((Task *)task_ptr);
        if (!task.is_backtrack) {
            Assignment task_assignment;
            task_assignment.var_id = task.var_id;
            task_assignment.value = task.assignment;
            char task_status = cnf.get_decision_status(task_assignment);
            assert(task_status == 'q');
            Assignment opposite_assignment;
            opposite_assignment.var_id = task.var_id;
            opposite_assignment.value = !task.assignment;
            char opposite_status = cnf.get_decision_status(opposite_assignment);
            if (task.implier == -1) {
                // Must be a decided (not unit propagated) variable
                if (!((opposite_status == 'q') 
                    || (opposite_status == 'l') 
                    || (opposite_status == 's')
                    || (opposite_status == 'u'))) {
                        cnf.print_task_stack("Failed task stack", task_stack, 1000);
                        printf("Failed, opposite status = %c, id = %d\n", opposite_status, task.var_id);
                    }
                assert((opposite_status == 'q') 
                    || (opposite_status == 'l') 
                    || (opposite_status == 's')
                    || (opposite_status == 'u'));
            } else {
                // Must be a unit propagated variable
                assert(opposite_status == 'u');
            }
            num_queued++;
        }
    }
    for (int var_id = 0; var_id < cnf.num_variables; var_id++) {
        if (cnf.assigned_true[var_id]) {
            if (cnf.assigned_false[var_id]) {
                printf("Var %d both true and false\n", var_id);
            }
            assert(!cnf.assigned_false[var_id]);
            assert((cnf.true_assignment_statuses[var_id] == 'l')
                || (cnf.true_assignment_statuses[var_id] == 'r'));
            assert((cnf.false_assignment_statuses[var_id] == 'u')
                || (cnf.false_assignment_statuses[var_id] == 'q')
                || (cnf.false_assignment_statuses[var_id] == 's'));
            total_assigned++;
        } else if (cnf.assigned_false[var_id]) {
            assert(!cnf.assigned_true[var_id]);
            assert((cnf.false_assignment_statuses[var_id] == 'l')
                || (cnf.false_assignment_statuses[var_id] == 'r'));
            assert((cnf.true_assignment_statuses[var_id] == 'u')
                || (cnf.true_assignment_statuses[var_id] == 'q')
                || (cnf.true_assignment_statuses[var_id] == 's'));
            total_assigned++;
        } else {
            assert((cnf.false_assignment_statuses[var_id] == 'u')
                || (cnf.false_assignment_statuses[var_id] == 'q')
                || (cnf.false_assignment_statuses[var_id] == 's'));
            assert((cnf.true_assignment_statuses[var_id] == 'u')
                || (cnf.true_assignment_statuses[var_id] == 'q')
                || (cnf.true_assignment_statuses[var_id] == 's'));
        }
        Assignment left_assignment;
        left_assignment.var_id = var_id;
        left_assignment.value = true;
        char left_status = cnf.get_decision_status(left_assignment);
        Assignment right_assignment;
        right_assignment.var_id = var_id;
        right_assignment.value = false;
        char right_status = cnf.get_decision_status(right_assignment);
        std::list<char> statuses = {left_status, right_status};
        for (char status : statuses) {
            switch (status) {
                case 'l': {
                    reported_num_local++;
                    break;
                } case 'q': {
                    reported_num_queued++;
                    break;
                } case 'r': {
                    reported_num_remote++;
                    break;
                } case 's': {
                    reported_num_stolen++;
                    break;
                }
            }
        }
    }
    assert(num_queued <= reported_num_queued);
    // assert(num_stolen == reported_num_stolen);
    assert(total_assigned == reported_num_local + reported_num_remote);
    if (task_stack.count > 0) {
        free(task_list);
    }
    if ((*State::thieves).count > 0) {
        free(thief_list);
    }
    return true;
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

            cnf.assignment_times[variable_id] = -1; // IMPORTANT
            cnf.assignment_depths[variable_id] = -1;
            return;
        } case 'c': {
            // Add clause drop to it's int offset
            int clause_id = edit.edit_id;
            if (clause_id >= cnf.clauses.max_indexable) return; //don't do for conflict clauses
            Clause clause = cnf.clauses.get_clause(clause_id);
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
void *State::grab_work_from_stack(
        Cnf &cnf,
        Deque &task_stack,
        short recipient_pid)
    {
    if (PRINT_LEVEL > 0) printf("PID %d: grabbing work from stack\n", State::pid);
    print_data(cnf, task_stack, "grabbing work from stack");
    assert(State::num_non_trivial_tasks > 1);
    assert(task_stack.count >= State::num_non_trivial_tasks);
    // Get the actual work as a copy
    void *top_task_ptr = task_stack.pop_from_back();
    Task top_task = *((Task *)(top_task_ptr));
    free(top_task_ptr);
    void *work = cnf.convert_to_work_message(cnf.oldest_compressed, top_task);
    if (top_task.assignment) {
        cnf.true_assignment_statuses[top_task.var_id] = 's';
    } else {
        cnf.false_assignment_statuses[top_task.var_id] = 's';
    }
    GivenTask task_to_give;
    task_to_give.var_id = top_task.var_id;
    task_to_give.pid = recipient_pid;
    task_to_give.assignment = top_task.assignment;
    GivenTask *task_to_give_ptr = (GivenTask *)malloc(sizeof(GivenTask));
    *task_to_give_ptr = task_to_give;
    (*State::thieves).add_to_front((void *)task_to_give_ptr);
    // Prune the top of the tree
    while (backtrack_at_top(task_stack)) {
        assert(cnf.eedit_stack.count > 0);
        // Two deques loose their top element, one looses at least one element
        Deque edits_to_apply = *((Deque *)((cnf.eedit_stack).pop_from_back()));
        while (edits_to_apply.count > 0) {
            void *formula_edit_ptr = edits_to_apply.pop_from_back();
            FormulaEdit edit = *((FormulaEdit *)formula_edit_ptr);
            free(formula_edit_ptr);
            apply_edit_to_compressed(cnf, cnf.oldest_compressed, edit);
        }
        edits_to_apply.free_deque();
        // Ditch the backtrack task at the top
        task_stack.pop_from_back();
    }
    State::num_non_trivial_tasks--;
    assert(State::num_non_trivial_tasks >= 1);
    if (PRINT_LEVEL > 1) printf("PID %d: grabbing work from stack done\n", State::pid);
    print_data(cnf, task_stack, "grabbed work from stack");
    if (PRINT_LEVEL > 5) print_compressed(
        cnf.pid, "giving work", cnf.depth_str, (unsigned int *)work, cnf.work_ints);
    return work;
}

// Picks recipient index to give work to
short State::pick_work_recipient() {
    // P0 has it's Parent urgently requesi
    short recipient_index;
    for (short child = 0; child <= State::num_children; child++) {
        if (State::num_urgent > 0) {
            // Urgent requests take precedence
            // Pick an urgently-requesting child (or parent)
            if (State::child_statuses[child] == 'r'
                || State::child_statuses[child] == 'w') {
                // Non-urgent
                continue;
            }
        } else {
            // Pick a requesting child (or parent)
            if (State::child_statuses[child] == 'w') {
                // Not requesting
                continue;
            }
        }
        if (PRINT_LEVEL > 0) printf("PID %d: picked work recipient %d\n", State::pid, pid_from_child_index(child));
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
    // us, with bias for asking our parent (hence reverse direction). <- nah
    for (short ctr = State::num_children; ctr >= 0; ctr--) {
        short child = State::last_asked_child - ctr + State::num_children;
        child = child % (State::num_children + 1);

        if (make_double_requests) {
            if (State::requests_sent[child] == 'u') {
                // We've urgently-asked them already
                continue;
            }
        } else {
            if (State::requests_sent[child] != 'n') {
                // We've asked them already
                continue;
            }
        }
        if (State::child_statuses[child] == 'u') {
            // Can't ask them, they're out of work entirely
            continue;
        }
        recipient_index = child;
        if (State::child_statuses[child] == 'w') {
            // They're not asking us for work, stop here
            break;
        }
    }
    // cycle choice by 1 each time [or random??]
    State::last_asked_child = (State::last_asked_child+1) % (State::num_children+1);
    return recipient_index;
}

// Gives one unit of work to lazy processors
void State::give_work(
        Cnf &cnf, 
        Deque &task_stack, 
        Interconnect &interconnect) 
    {
    if (PRINT_LEVEL > 0) printf("PID %d: giving work\n", State::pid);
    short recipient_index = pick_work_recipient();
    assert(State::child_statuses[recipient_index] != 'w');
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
        assert(task_stack.count > 0);
        work = grab_work_from_stack(cnf, task_stack, recipient_pid);
    }
    if (State::child_statuses[recipient_index] == 'u') {
        State::num_urgent--;
    }
    State::child_statuses[recipient_index] = 'w';
    State::num_requesting--;
    interconnect.send_work(recipient_pid, work);
}

// Gets stashed work, returns true if any was grabbed
bool State::get_work_from_interconnect_stash(
        Cnf &cnf, 
        Deque &task_stack, 
        Interconnect &interconnect) 
    {
    // NICE: Perhaps we get stashed work from a sender who is no longer 
    // asking us for work now (if possible)?
    // Preference for children or parents?
    if (!interconnect.have_stashed_work()) {
        return false;
    }
    if (PRINT_LEVEL > 0) printf("PID %d: getting work from interconnect stash\n", State::pid);
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
void State::ask_for_work(
        Cnf &cnf, 
        Deque &task_stack, 
        Interconnect &interconnect) 
    {
    assert(!interconnect.have_stashed_work());
    assert(State::num_urgent <= State::num_children);
    if (should_implicit_abort()) {
        // Self abort
        abort_others(interconnect);
        abort_process(task_stack, interconnect);
    } else if (should_forward_urgent_request()) {
        if (PRINT_LEVEL > 0) printf("PID %d: urgently asking for work\n", State::pid);
        // Send a single urgent work request
        short dest_index = pick_request_recipient();
        if (dest_index == -1) {
            return;
        }
        short dest_pid = pid_from_child_index(dest_index);
        if (State::requests_sent[dest_index] == 'r') {
            // Send an urgent upgrade
            interconnect.send_work_request(dest_pid, 2);
        } else {
            assert(State::requests_sent[dest_index] == 'n');
            // Send an urgent request
            interconnect.send_work_request(dest_pid, 1);
        }
        State::requests_sent[dest_index] = 'u';
    } else {
        if (PRINT_LEVEL > 0) printf("PID %d: asking for work\n", State::pid);
        // Send a single work request
        short dest_index = pick_request_recipient();
        if (dest_index == -1) {
            return;
        }
        short dest_pid = pid_from_child_index(dest_index);
        assert(State::requests_sent[dest_index] == 'n');
        interconnect.send_work_request(dest_pid, 0);
        State::requests_sent[dest_index] = 'r';
    }
}

// Invalidates (erases) ones work
void State::invalidate_work(Deque &task_stack) {
    assert(false); // unfinished
    // TODO: undo all local edits as well
    State::num_non_trivial_tasks = 0;
    task_stack.free_data();
}

// Empties/frees data structures and immidiately returns
void State::abort_process(
        Deque &task_stack, 
        Interconnect &interconnect,
        bool explicit_abort) 
    {
    if (PRINT_LEVEL > 0) printf("PID %d: aborting process\n", State::pid);
    State::process_finished = true;
    State::was_explicit_abort = explicit_abort;
    interconnect.free_interconnect();
    free(State::child_statuses);
    free(State::requests_sent);
    (*State::thieves).free_deque();
    free(State::thieves);
    task_stack.free_deque();
}

// Sends messages to children to force them to abort
void State::abort_others(Interconnect &interconnect, bool explicit_abort) {
    if (explicit_abort) {
        // Success, broadcase explicit abort to every process
        for (short i = 0; i < State::nprocs; i++) {
            if (i == State::pid) {
                continue;
            }
            interconnect.send_abort_message(i);
        }
    } else {
        printf("PID %d: aborting others\n", State::pid);
        // Any children who did not receive an urgent request from us should
        assert(should_implicit_abort());
        for (short child = 0; child <= State::num_children; child++) {
            short recipient = pid_from_child_index(child);
            if (State::requests_sent[child] != 'u') {
                // Child needs ones
                if (State::requests_sent[child] == 'n') {
                    // Normal urgent request
                    printf("PID %d sending urgent req to %d\n", State::pid, (int)recipient);
                    interconnect.send_work_request(recipient, 1);
                } else {
                    // Urgent upgrade
                    printf("PID %d sending urgent upgrade to %d\n", State::pid, (int)recipient);
                    interconnect.send_work_request(recipient, 2);
                }
                State::requests_sent[child] = 'u';
            } else {
                printf("PID %d already sent urgent to %d\n", State::pid, (int)recipient);
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
        if (PRINT_LEVEL > 0) printf("PID %d: handling work request from sender %d (child %d)\n", State::pid, sender_pid, child_index);
        assert(State::child_statuses[child_index] == 'w');
        // Log the request
        State::child_statuses[child_index] = 'r';
        State::num_requesting++;
    } else { // Urgent request
        if (version == 1) { // Urgent request
            if (PRINT_LEVEL > 0) printf("PID %d: handling urgent work request from sender %d (child %d)\n", State::pid, sender_pid, child_index);
            assert(State::child_statuses[child_index] == 'w');
            State::num_requesting++;
        } else { // Urgent upgrade
            if (PRINT_LEVEL > 0) printf("PID %d: handling urgent upgrade from sender %d (child %d)\n", State::pid, sender_pid, child_index);
            assert(State::child_statuses[child_index] != 'u');
            if (State::child_statuses[child_index] == 'w') {
                // Effectively a no-op, we already handled the non-urgent 
                // request that came before
                return;
            }
        }
        // Log the request
        State::child_statuses[child_index] = 'u';
        State::num_urgent++;
        // Check if we need to forward urgent, or abort
        if (out_of_work()) {        
            if (should_implicit_abort()) {
                // Self-abort
                printf("implicit abort from pid %d (in handle_work_request)\n", pid);
                abort_others(interconnect);
                abort_process(task_stack, interconnect);
            } else if (should_forward_urgent_request()) {
                // Send a single urgent work request
                short dest_index = pick_request_recipient();
                short dest_pid = pid_from_child_index(dest_index);
                if (State::requests_sent[dest_index] == 'r') {
                    // Send an urgent upgrade
                    interconnect.send_work_request(dest_pid, 2);
                } else {
                    assert(State::requests_sent[dest_index] == 'n');
                    // Send an urgent request
                    interconnect.send_work_request(dest_pid, 1);
                }
                State::requests_sent[dest_index] = 'u';
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
    if (PRINT_LEVEL > 0) printf("PID %d: handling work message\n", State::pid);
    short sender_pid = message.sender;
    short child_index = child_index_from_pid(sender_pid);
    void *work = message.data;
    // assert(State::requests_sent[child_index] != 'n'); // TODO: dunno why this is wrong?
    assert(child_statuses[child_index] != 'u');
    assert(!interconnect.have_stashed_work(sender_pid));
    if (out_of_work()) {
        // Reconstruct state from work
        Task task = cnf.extract_task_from_work(work);
        State::current_task.assignment = task.assignment;
        State::current_task.var_id = task.var_id;
        // NICE: implement forwarding
        State::current_task.pid = sender_pid;
        cnf.reconstruct_state(work, task_stack);
        (*State::thieves).free_data();

        // apply task immediately so we cant backtrack out
        int cid;
        bool valid = cnf.propagate_assignment(task.var_id, task.assignment, -1, &cid, false);
        if (valid) {
            cnf.assignment_times[task.var_id] = -1;
            cnf.assignment_depths[task.var_id] = -1;
            if (cnf.assigned_true[task.var_id]) {
                cnf.true_assignment_statuses[task.var_id] = 'r';
                cnf.false_assignment_statuses[task.var_id] = 'u';
            } else if (cnf.assigned_false[task.var_id]) {
                cnf.true_assignment_statuses[task.var_id] = 'u';
                cnf.false_assignment_statuses[task.var_id] = 'r';
            }
            add_tasks_from_formula(cnf, task_stack);
        } else {
            invalidate_work(task_stack);
        }
        
        // State::num_non_trivial_tasks = 1;
        print_data(cnf, task_stack, "Post reconstruct");
    } else {
        // Add to interconnect work stash
        interconnect.stash_work(message);
    }
    State::requests_sent[child_index] = 'n';
}

// Handles an abort message, possibly forwarding it
void State::handle_message(
        Message message, 
        Cnf &cnf, 
        Deque &task_stack, 
        Interconnect &interconnect) 
    {
    if (PRINT_LEVEL > 0) printf("PID %d: handling message (type %d)\n", State::pid, message.type);
    assert(0 <= State::num_urgent && State::num_urgent <= State::num_children);
    assert(State::num_urgent <= State::num_requesting);
    assert(State::num_requesting <= State::num_children + 1);
    // assert(0 <= message.type && message.type <= 4);
    switch (message.type) {
        case 3: {
            handle_work_message(
                message, cnf, task_stack, interconnect);
            return;
        } case 4: {
            abort_process(task_stack, interconnect, true);
            free(message.data);
            return;
        } case 5: {
            invalidate_work(task_stack);
            free(message.data);
            return;
        } case 6: {
            assert(SEND_CONFLICT_CLAUSES);
            Clause conflict_clause = message_to_clause(message);
            handle_remote_conflict_clause(
                cnf, 
                task_stack, 
                conflict_clause, 
                interconnect);
            free(message.data);
            return;
        } default: {
            // 0, 1, or 2
            handle_work_request(
                message.sender, message.type, cnf, task_stack, interconnect);
            free(message.data);
            return;
        }
    }
    assert(0 <= State::num_urgent 
        && State::num_urgent <= State::num_children + 1);
    assert(State::num_urgent <= State::num_requesting);
    assert(State::num_requesting <= State::num_children + 1);
}

// Edits history to make it appear as though the conflict clause is
// normal.
void State::insert_conflict_clause_history(Cnf &cnf, Clause conflict_clause) {
    if (PRINT_LEVEL > 1) printf("%s\tPID %d: inserting conflict clause %d into history\n", cnf.depth_str.c_str(), State::pid, conflict_clause.id);
    // TODO: drop_var_id logic should be checked
    bool look_for_drop = false;
    cnf.clause_satisfied(conflict_clause, &look_for_drop);

    int drop_var_id = -1;
    int drop_time = State::current_cycle+1;
    int num_unsat = 0;
    std::map<int, int> depth_to_al;
    for (int i = 0; i < conflict_clause.num_literals; i++) {
        int var_id = conflict_clause.literal_variable_ids[i];
        bool literal_sign = conflict_clause.literal_signs[i];
        int time = cnf.assignment_times[var_id];

        if (cnf.assigned_true[var_id] && (time != -1)) {
            if (literal_sign) {
                if (time <= drop_time) {
                    drop_var_id = var_id;
                    drop_time = time;
                }
            } else {
                if (time >= 0) depth_to_al.insert({time, var_id});
            }
        } else if (cnf.assigned_false[var_id] && (time != -1)) {
            if (!literal_sign) {
                if (time <= drop_time) {
                    drop_var_id = var_id;
                    drop_time = time;
                }
            } else {
                if (time >= 0) depth_to_al.insert({time, var_id});
            }
        } else if (!cnf.assigned_true[var_id] && !cnf.assigned_false[var_id]) {
            num_unsat++;
        }
    }

    assert(look_for_drop == (drop_var_id != -1));
    int drop_var_iteration = look_for_drop ? cnf.depth - cnf.assignment_depths[drop_var_id] : -1;

    if(look_for_drop) {
        printf("NOT IMPLEMENTED\n");
        std::abort();
    }

    for (auto iter=depth_to_al.rbegin(); iter != depth_to_al.rend(); ++iter) {
        // from latest assign backwards
        int var_id = iter->second;
        Deque *iteration_edits = cnf.variables[var_id].edit_stack_ptr;
        (*iteration_edits).add_to_front(size_change_edit(conflict_clause.id, num_unsat + 1, num_unsat));
        num_unsat++;
    }
    return;

    // Restore the change we made
    if (PRINT_LEVEL > 1) printf("%s\tPID %d: made %ld insertions into conflict clause history\n", cnf.depth_str.c_str(), State::pid, depth_to_al.size());
    if (PRINT_LEVEL > 2) cnf.print_edit_stack("new edit stack", (cnf.eedit_stack));
}

// Sends messages to specified theives in light of conflict
void State::inform_thieves_of_conflict(
        Deque selected_thieves,
        Clause conflict_clause,
        Interconnect &interconnect,
        bool invalidate)
    {
    for (int i = 0; i < selected_thieves.count; i++) {
        void *thief_ptr = selected_thieves.pop_from_front();
        GivenTask thief_task = *((GivenTask *)thief_ptr);
        selected_thieves.add_to_back(thief_ptr);
        short recipient_pid = thief_task.pid;
        if (invalidate) {
            interconnect.send_invalidation(recipient_pid);
        } else {
            interconnect.send_conflict_clause(recipient_pid, conflict_clause);
        }
    }
}

// Slits thieves based on midpoint (not included), populates results
void State::split_thieves(
        Task midpoint, 
        Deque &thieves_before, 
        Deque &thieves_after) 
    {
    bool seen_midpoint = false;
    short num_thieves = (*State::thieves).count;
    for (int i = 0; i < num_thieves; i++) {
        void *thief_ptr = (*State::thieves).pop_from_front();
        GivenTask thief_task = *((GivenTask *)thief_ptr);
        (*State::thieves).add_to_back(thief_ptr);
        if ((thief_task.var_id == midpoint.var_id) 
            && (thief_task.assignment == midpoint.assignment)) {
            seen_midpoint = true;
            // Don't include midpoint
            continue;
        }
        if (seen_midpoint) {
            thieves_after.add_to_back(thief_ptr);
        } else {
            thieves_before.add_to_back(thief_ptr);
        }
    }
}

// Adds tasks based on what a conflict clause says (always greedy)
int State::add_tasks_from_conflict_clause(
        Cnf &cnf, 
        Deque &task_stack, 
        Clause conflict_clause) 
    {
    if (PRINT_LEVEL > 3) printf("%sPID %d: adding tasks from conflict clause\n", cnf.depth_str.c_str(), State::pid);
    int new_var_id;
    bool new_var_sign;
    int num_unsat = cnf.pick_from_clause(
        conflict_clause, &new_var_id, &new_var_sign);
    assert(0 < num_unsat);
    if (PRINT_LEVEL > 1) printf("%sPID %d: picked new var %d from clause %d %s\n", cnf.depth_str.c_str(), State::pid, new_var_id, conflict_clause.id, cnf.clause_to_string_current(conflict_clause, false).c_str());
    if (num_unsat == 1) {
        void *only_task = make_task(new_var_id, conflict_clause.id, new_var_sign);
        task_stack.add_to_front(only_task);
        State::num_non_trivial_tasks++;
        if (new_var_sign) {
            cnf.true_assignment_statuses[new_var_id] = 'q';
        } else {
            cnf.false_assignment_statuses[new_var_id] = 'q';
        }
        if (PRINT_LEVEL >= 3) printf("%sPID %d: variable %d unit propped from conflict clause %d\n", cnf.depth_str.c_str(), State::pid, new_var_id, conflict_clause.id);
        return 1;
    } else {
        // Add a backtrack task to do last
        if (task_stack.count > 0) {
            void *backtrack_task = make_task(new_var_id, -1, true, true);
            task_stack.add_to_front(backtrack_task);
        }
        bool first_choice = new_var_sign;
        void *important_task = make_task(new_var_id, -1, first_choice);
        task_stack.add_to_front(important_task);
        State::num_non_trivial_tasks += 2;
        if (first_choice) {
            cnf.true_assignment_statuses[new_var_id] = 'q';
            cnf.false_assignment_statuses[new_var_id] = 'u';
        } else {
            cnf.true_assignment_statuses[new_var_id] = 'u';
            cnf.false_assignment_statuses[new_var_id] = 'q';
        }
        if (PRINT_LEVEL >= 3) printf("%sPID %d: variable %d explicitly decided from conflict clause %d\n", cnf.depth_str.c_str(), State::pid, new_var_id, conflict_clause.id);
        return 2;
    }
}

// Adds a conflict clause to the clause list
// EDITED: NO LONGER PICKS FROM CLAUSE
void State::add_conflict_clause(
        Cnf &cnf, 
        Clause &conflict_clause,
        Deque &task_stack,
        bool pick_from_clause,
        bool toFront) 
    {
    if (PRINT_LEVEL > 1) printf("%sPID %d: adding conflict clause\n", cnf.depth_str.c_str(), State::pid);
    if (PRINT_LEVEL > 2) cnf.print_cnf("Before conflict clause", cnf.depth_str, (PRINT_LEVEL <= 3));
    if (PRINT_LEVEL > 2) cnf.print_task_stack("Before conflict clause", task_stack);
    if (PRINT_LEVEL > 2) printf("Conflict clause num unsat = %d\n", cnf.get_num_unsat(conflict_clause));
    assert(clause_is_sorted(conflict_clause));
    int new_clause_id = cnf.clauses.max_indexable + cnf.clauses.num_conflict_indexed;
    conflict_clause.id = new_clause_id;
    insert_conflict_clause_history(cnf, conflict_clause);
    for (int lit = 0; lit < conflict_clause.num_literals; lit++) {
        int var_id = conflict_clause.literal_variable_ids[lit];
        bool sgn = conflict_clause.literal_signs[lit];
        int pm_id = sgn ? new_clause_id : -(new_clause_id+1); // negative means neg occurence of literal
        (*((cnf.variables[var_id]).clauses_containing)).push_back(pm_id);
    }
    cnf.clauses.add_conflict_clause(conflict_clause, toFront);
    // hash
    cnf.clause_hash.insert(conflict_clause);

    int actual_size = cnf.get_num_unsat(conflict_clause);
    cnf.clauses.num_unsats[new_clause_id] = actual_size;
    if (actual_size == 0) {
        cnf.clauses.drop_clause(new_clause_id);
    } else if (actual_size != conflict_clause.num_literals) {
        if (PRINT_LEVEL > 2) printf("Changing conflict clause size to %d\n", actual_size);
        cnf.clauses.change_clause_size(conflict_clause.id, actual_size); //TODO: add to end if remote?
    }
    if (PRINT_LEVEL > 2) cnf.print_cnf("With conflict clause", cnf.depth_str, true);
    if (PRINT_LEVEL > 2) cnf.print_task_stack("With conflict clause", task_stack);
}

// Handles the current REMOTE conflict clause
void State::handle_remote_conflict_clause(
        Cnf &cnf, 
        Deque &task_stack, 
        Clause conflict_clause,
        Interconnect &interconnect) 
    {

    if (cnf.clauses.max_conflict_indexable == cnf.clauses.num_conflict_indexed - 1 || out_of_work()) {
        free_clause(conflict_clause);
        return;
    }

    // test using original valuation
    bool false_so_far = true;
    bool preassigned_false_so_far = true;
    int num_unsat = 0;
    for (int i = 0; i < conflict_clause.num_literals; i++) {
        int lit = conflict_clause.literal_variable_ids[i];
        if ((cnf.assigned_true[lit] && conflict_clause.literal_signs[i])
            || (cnf.assigned_false[lit] && !conflict_clause.literal_signs[i])) {
            // is true, can add (or ignore?) - DROP CLAUSE IN CONFLICT CLAUSE HISTORY NOT IMPLEMENTED
            // no need to backtrack
            free_clause(conflict_clause);
            return;
        } else if (!cnf.assigned_true[lit] && !cnf.assigned_false[lit]) { //unassigned
            false_so_far = false;
            preassigned_false_so_far = false;
            num_unsat++;
        } else {
            // lit evals to false
            if (cnf.assignment_times[lit] != -1) {
                preassigned_false_so_far = false;
            }
        }
    }

    if (preassigned_false_so_far) {
        invalidate_work(task_stack);
        free_clause(conflict_clause);
        return;
    }
    
    if (num_unsat > CONFLICT_CLAUSE_UNSAT_LIMIT * cnf.n) {
        free_clause(conflict_clause);
        return;
    }

    if (false_so_far) {
        // unsat -> backtrack
        std::map<int, int> lit_to_depth;
        for (int i = 0; i < conflict_clause.num_literals; i++) {
            int lit = conflict_clause.literal_variable_ids[i];
            int depth = cnf.assignment_depths[lit];
            lit_to_depth.insert({depth, lit});
        }
        auto iter = lit_to_depth.rbegin();
        int last_d = iter->first;

        // backtrack until I see the first depth
        while (true) {
            if (task_stack.count == 0) {
                // still need to backtrack, but on "bad" choice of first variable
                cnf.backtrack();
                break;
            }

            Task current_task = get_task(task_stack);

            if (current_task.is_backtrack) {
                cnf.backtrack();
                if (cnf.depth == last_d-1) {
                    // backtrack until we are exactly past the point of unsat
                    break;
                }
            } else {
                if (current_task.assignment) {
                    cnf.true_assignment_statuses[current_task.var_id] = 'u';
                    if (PRINT_LEVEL >= 3) printf("%sPID %d: removed pre-conflict task {%d = T}\n", cnf.depth_str.c_str(), State::pid, current_task.var_id);
                } else {
                    cnf.false_assignment_statuses[current_task.var_id] = 'u';
                    if (PRINT_LEVEL >= 3) printf("%sPID %d: removed pre-conflict task {%d = F}\n", cnf.depth_str.c_str(), State::pid, current_task.var_id);
                }
                State::num_non_trivial_tasks--;   
            }
        }
    }
    add_conflict_clause(cnf, conflict_clause, task_stack, false, false);
    if (PRINT_LEVEL >= 3) printf("%sPID %d: done backjumping from remote cc. Adding cc now\n", cnf.depth_str.c_str(), State::pid);
}

// Handles the current LOCAL conflict clause
void State::handle_local_conflict_clause(
        Cnf &cnf, 
        Deque &task_stack, 
        Clause conflict_clause,
        Interconnect &interconnect,
        bool send)
    {
    int new_clause_id = cnf.clauses.max_indexable + cnf.clauses.num_conflict_indexed;
    if (PRINT_LEVEL > 2) printf("%sPID %d: handle conflict clause %d %s\n", cnf.depth_str.c_str(), State::pid, new_clause_id, cnf.clause_to_string_current(conflict_clause, false).c_str());
    if (PRINT_LEVEL > 2) printf("%sPID %d: handle conflict clause %d %s\n", cnf.depth_str.c_str(), State::pid, new_clause_id, cnf.clause_to_string_current(conflict_clause, true).c_str());

    // TODO: does this ever happen locally?
    assert(!cnf.clause_exists_already(conflict_clause));
    
    // Backtrack to just when the clause would've become unit [or restart if conflict clause is unit]
    if (conflict_clause.num_literals == 1) {
        // backtrack all the way back
        while (task_stack.count > 0) {
            Task current_task = get_task(task_stack);
            if (current_task.is_backtrack) {
                cnf.backtrack();
            } else {
                if (current_task.assignment) {
                    cnf.true_assignment_statuses[current_task.var_id] = 'u';
                    if (PRINT_LEVEL >= 3) printf("%sPID %d: removed pre-conflict task {%d = T}\n", cnf.depth_str.c_str(), State::pid, current_task.var_id);
                } else {
                    cnf.false_assignment_statuses[current_task.var_id] = 'u';
                    if (PRINT_LEVEL >= 3) printf("%sPID %d: removed pre-conflict task {%d = F}\n", cnf.depth_str.c_str(), State::pid, current_task.var_id);
                }
                State::num_non_trivial_tasks--;   
            }
        }
        cnf.backtrack(); //one more since original task has no backtrack task
    } else {
        // what is second-last variable
        std::map<int, int> lit_to_depth;
        for (int i = 0; i < conflict_clause.num_literals; i++) {
            int lit = conflict_clause.literal_variable_ids[i];
            int depth = cnf.assignment_depths[lit];
            lit_to_depth.insert({depth, lit});
        }
        auto iter = lit_to_depth.rbegin();
        int last_d = iter->first;
        ++iter;
        int second_last_d = iter->first;
        assert(last_d > second_last_d);

        // backtrack until I see the second-latest variable
        while (true) {
            if (task_stack.count == 0) {
                // still need to backtrack, but on "bad" choice of first variable
                cnf.backtrack();
                break;
            }
            Task current_task = get_task(task_stack);

            if (current_task.is_backtrack) {
                cnf.backtrack();
                if (cnf.depth == second_last_d) {
                    break;
                }
            } else {
                if (current_task.assignment) {
                    cnf.true_assignment_statuses[current_task.var_id] = 'u';
                    if (PRINT_LEVEL >= 3) printf("%sPID %d: removed pre-conflict task {%d = T}\n", cnf.depth_str.c_str(), State::pid, current_task.var_id);
                } else {
                    cnf.false_assignment_statuses[current_task.var_id] = 'u';
                    if (PRINT_LEVEL >= 3) printf("%sPID %d: removed pre-conflict task {%d = F}\n", cnf.depth_str.c_str(), State::pid, current_task.var_id);
                }
                State::num_non_trivial_tasks--;   
            }
        }
    }
    if (PRINT_LEVEL > 1) printf("%s\tPID %d: done backJUMPING\n", cnf.depth_str.c_str(), State::pid);
    
    // local is responsible
    if (PRINT_LEVEL > 1) printf("%s\tPID %d: local is responsible for conflict\n", cnf.depth_str.c_str(), State::pid);

    add_conflict_clause(cnf, conflict_clause, task_stack, false);
    
    if (PRINT_LEVEL > 1) printf("%sPID %d: finished handling conflict clause\n", cnf.depth_str.c_str(), State::pid);
    if (PRINT_LEVEL > 2) cnf.print_task_stack("Updated", task_stack);
    if (SEND_CONFLICT_CLAUSES && send && conflict_clause.num_literals < SEND_CONFLICT_CLAUSE_LIMIT * cnf.n) {
        if (PRINT_LEVEL > 1) printf("%sPID %d: sent conflict clause: %s\n", cnf.depth_str.c_str(), State::pid, cnf.clause_to_string_current(conflict_clause, false).c_str());
        interconnect.send_conflict_clause(-1, conflict_clause, true);
    }
}

// Adds a clause indicating our entire assignment is invalid
void State::add_failure_clause(Cnf &cnf, Interconnect &interconnect) {
    // generate conflict clause
    Clause cc_done;
    std::vector<int> givens;
    for (int i = 0; i < cnf.num_variables; i++) {
        if (cnf.assignment_times[i] == -1) {
            givens.push_back(i);
        }
    }
    cc_done.num_literals = givens.size();
    cc_done.literal_variable_ids = (int *)malloc(sizeof(int) * givens.size());
    cc_done.literal_signs = (bool *)calloc(sizeof(bool), givens.size()); //all false
    for (int i = 0; i < givens.size(); i++) {
        cc_done.literal_variable_ids[i] = givens[i];
    }

    int new_clause_id = cnf.clauses.max_indexable + cnf.clauses.num_conflict_indexed;
    cc_done.id = new_clause_id;
    for (int lit = 0; lit < cc_done.num_literals; lit++) {
        int var_id = cc_done.literal_variable_ids[lit];
        bool sgn = cc_done.literal_signs[lit];
        int pm_id = sgn ? new_clause_id : -(new_clause_id+1); // negative means neg occurence of literal
        (*((cnf.variables[var_id]).clauses_containing)).push_back(pm_id);
    }
    cnf.clauses.add_conflict_clause(cc_done);
    cnf.clause_hash.insert(cc_done);
    if (SEND_CONFLICT_CLAUSES) {
        interconnect.send_conflict_clause(-1, cc_done, true);
    }
}

// Adds one or two variable assignment tasks to task stack
int State::add_tasks_from_formula(Cnf &cnf, Deque &task_stack) {
    if (PRINT_LEVEL > 3) printf("%sPID %d: adding tasks from formula\n", cnf.depth_str.c_str(), State::pid);
    cnf.clauses.reset_iterator();
    Clause current_clause;
    int current_clause_id;
    int new_var_id;
    bool new_var_sign;
    int num_unsat = 2;

    while (true) {
        current_clause = cnf.clauses.get_current_clause();
        current_clause_id = current_clause.id;
        num_unsat = cnf.pick_from_clause(
            current_clause, &new_var_id, &new_var_sign);
        assert(0 < num_unsat);
        if (new_var_id < cnf.n*cnf.n*cnf.n || num_unsat == 1 || !ALWAYS_PREFER_NORMAL_VARS) break;
        cnf.clauses.advance_iterator();
    }

    if (PRINT_LEVEL > 1) printf("%sPID %d: picked new var %d from clause %d %s\n", cnf.depth_str.c_str(), State::pid, new_var_id, current_clause_id, cnf.clause_to_string_current(current_clause, true).c_str());
    if (num_unsat == 1) {
        void *only_task = make_task(new_var_id, current_clause_id, new_var_sign);
        task_stack.add_to_front(only_task);
        State::num_non_trivial_tasks++;
        if (new_var_sign) {
            cnf.true_assignment_statuses[new_var_id] = 'q';
        } else {
            cnf.false_assignment_statuses[new_var_id] = 'q';
        }
        if (PRINT_LEVEL >= 3) printf("%sPID %d: variable %d unit propped\n", cnf.depth_str.c_str(), State::pid, new_var_id);
        return 1;
    } else {
        // Add a backtrack task to do last
        if (task_stack.count > 0) {
            void *backtrack_task = make_task(new_var_id, -1, true, true);
            task_stack.add_to_front(backtrack_task);
        }
        bool first_choice;
        if (State::assignment_method == 0) {
            // Pick greedy
            first_choice = new_var_sign;
        } else if (State::assignment_method == 1) {
            // Pick opposite of greedy
            first_choice = !new_var_sign;
        } else if (State::assignment_method == 2) {
            // Always set true
            first_choice = true;
        } else {
            // Always set fakse
            first_choice = false;
        }
        void *important_task = make_task(new_var_id, -1, first_choice);
        void *other_task = make_task(new_var_id, -1, !first_choice);
        task_stack.add_to_front(other_task);
        task_stack.add_to_front(important_task);
        State::num_non_trivial_tasks += 2;
        cnf.true_assignment_statuses[new_var_id] = 'q';
        cnf.false_assignment_statuses[new_var_id] = 'q';
        if (PRINT_LEVEL >= 3) printf("%sPID %d: variable %d explicitly decided\n", cnf.depth_str.c_str(), State::pid, new_var_id);
        return 2;
    }
}

// Displays data structure data for debugging purposes
void State::print_data(Cnf &cnf, Deque &task_stack, std::string prefix_str) {
    if (PRINT_LEVEL > 1) cnf.print_task_stack(prefix_str, task_stack);
    if (PRINT_LEVEL > 2) cnf.print_edit_stack(prefix_str, (cnf.eedit_stack));
    if (PRINT_LEVEL > 1) cnf.print_cnf(prefix_str, cnf.depth_str, true);
    if (PRINT_LEVEL > 5) {
        unsigned int *tmp_compressed = cnf.to_int_rep();
        print_compressed(cnf.pid, prefix_str, cnf.depth_str, 
            tmp_compressed, cnf.work_ints);
        free(tmp_compressed);
    }
    if (PRINT_LEVEL > 5) print_compressed(
        cnf.pid, prefix_str, cnf.depth_str, cnf.oldest_compressed, cnf.work_ints);
    if (PRINT_LEVEL > 5) {
        std::string drop_str = "regular dropped [";
        for (int i = 0; i < cnf.clauses.num_indexed; i++) {
            if (cnf.clauses.clause_is_dropped(i)) {
                drop_str.append(std::to_string(i));
                drop_str.append(", ");
            }
        }
        drop_str.append("]");
        printf("%sPID %d: %s %s\n", cnf.depth_str.c_str(), State::pid, prefix_str.c_str(), drop_str.c_str());
        drop_str = "conflict dropped [";
        for (int i = 0; i < cnf.clauses.num_conflict_indexed; i++) {
            int clause_id = cnf.clauses.max_indexable + i;
            if (cnf.clauses.clause_is_dropped(clause_id)) {
                drop_str.append(std::to_string(clause_id));
                drop_str.append(", ");
            }
        }
        drop_str.append("]");
        printf("%sPID %d: %s %s\n", cnf.depth_str.c_str(), State::pid, prefix_str.c_str(), drop_str.c_str());
    }
}

// Runs one iteration of the solver
bool State::solve_iteration(
        Cnf &cnf, 
        Deque &task_stack, 
        Interconnect &interconnect) 
    {
    State::calls_to_solve++;
    assert(State::num_non_trivial_tasks > 0);
    assert(task_stack_invariant(cnf, task_stack, State::num_non_trivial_tasks));
    if (PRINT_LEVEL >= 3) printf("\n");
    bool should_recurse = recurse_required(task_stack);
    Clause conflict_clause;
    int conflict_id;
    Task task = get_task(task_stack);
    int var_id = task.var_id;
    int decided_var_id = var_id;
    int assignment = task.assignment;
    int implier = task.implier;
    if (task.is_backtrack) { // Children backtracked, need to backtrack ourselves
        print_data(cnf, task_stack, "Children backtrack");
        cnf.backtrack();
        return false;
    } else {
        State::num_non_trivial_tasks--;
    }
    if (should_recurse) {
        cnf.recurse();
    }
    if ((State::current_cycle % CYCLES_TO_PRINT_PROGRESS == 0) && PRINT_PROGRESS) {
        print_progress(cnf, task_stack);
        if (State::current_cycle == std::max(
            CYCLES_TO_PRINT_PROGRESS, CYCLES_TO_RECEIVE_MESSAGES)) {
            State::current_cycle = 0;
        }
    }
    
    State::current_cycle++;
    while (true) {
        if (PRINT_LEVEL > 3) print_data(cnf, task_stack, "Loop start");
        bool propagate_result = cnf.propagate_assignment(
            var_id, assignment, implier, &conflict_id);
        if (!propagate_result) {
            print_data(cnf, task_stack, "Prop fail");
            if (ENABLE_CONFLICT_RESOLUTION && task_stack.count > 0) {
                bool resolution_result = cnf.conflict_resolution_uid(
                    conflict_id, conflict_clause, decided_var_id);
                if (resolution_result) {
                    handle_local_conflict_clause(
                        cnf, task_stack, conflict_clause, interconnect);
                    print_data(cnf, task_stack, "Post-handle local conflict clause");
                    // backtracking has perhaps invalidated decided_var_id
                    // TODO: UNSURE HOW TO TEST CORRECTNESS
                    if (task_stack.count == 0) {
                        if (cnf.get_local_edit_count() > 0) {
                            Deque local_edits = *((Deque *)(cnf.eedit_stack.peak_front()));
                            decided_var_id = (*(FormulaEdit *)(local_edits.peak_back())).edit_id;
                        } else {
                            decided_var_id = -1; // calc should begin with unit prop
                        }
                    } else {
                        decided_var_id = peak_task(task_stack).var_id;
                    }
                } else {
                    cnf.backtrack();
                    return false;
                }
            } else {
                cnf.backtrack();
                return false;
            }
        }

        assert(task_stack_invariant(
            cnf, task_stack, State::num_non_trivial_tasks));
        if (cnf.clauses.get_linked_list_size() == 0) {
            if (PRINT_PROGRESS) print_progress(cnf, task_stack);
            cnf.assign_remaining();
            print_data(cnf, task_stack, "Base case success");
            return true;
        }
        if (PRINT_LEVEL > 3) print_data(cnf, task_stack, "Loop end");
        int num_added = add_tasks_from_formula(cnf, task_stack);
        if (num_added == 1) {
            Task task = get_task(task_stack);
            var_id = task.var_id;
            assignment = task.assignment;
            implier = task.implier;
            State::num_non_trivial_tasks--;

            if (decided_var_id == -1) {
                decided_var_id = var_id;
            }
            continue;
        }
        return false;
    }
}

// Continues solve operation, returns true iff a solution was found by
// the current thread.
bool State::solve(Cnf &cnf, Deque &task_stack, Interconnect &interconnect) {
    if (interconnect.pid == 0) {
        add_tasks_from_formula(cnf, task_stack);
        assert(task_stack.count > 0);
    }

    while (!State::process_finished) {
        interconnect.clean_dead_messages();
        if (out_of_work()) {
            if (PRINT_INTERCONNECT) printf(" I(PID %d ran out of work)\n", State::pid);
            bool found_work = get_work_from_interconnect_stash(
                cnf, task_stack, interconnect);
            if (!found_work) {
                ask_for_work(cnf, task_stack, interconnect);
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
        bool result = solve_iteration(cnf, task_stack, interconnect);
        if (result) {
            assert(State::num_non_trivial_tasks >= 0);
            abort_others(interconnect, true);
            abort_process(task_stack, interconnect, true);
            return true;
        } else if (out_of_work()) {
            add_failure_clause(cnf, interconnect);
        }

        if (current_cycle % CYCLES_TO_RECEIVE_MESSAGES == 1) { // dish messages first
            while (interconnect.async_receive_message(message) && !State::process_finished) {
                handle_message(message, cnf, task_stack, interconnect);
                // NICE: serve work here?
            }
            if (State::process_finished) break;
            if (State::current_cycle == std::max(
                CYCLES_TO_PRINT_PROGRESS, CYCLES_TO_RECEIVE_MESSAGES)) {
                State::current_cycle = 0;
            }
        }
        if (!out_of_work()) { // Serve ourselves before others
            while (workers_requesting() && can_give_work(task_stack, interconnect)) {
                give_work(cnf, task_stack, interconnect);
            }
        }
    }
    return false;
}