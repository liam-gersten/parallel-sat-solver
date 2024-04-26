#include "state.h"
#include <list>
#include <cassert> 

#include <map>

State::State(
        short pid,
        short nprocs,
        short branching_factor,
        short assignment_method,
        bool use_smart_prop)
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
    State::use_smart_prop = use_smart_prop;
    State::current_cycle = 0;
    Deque thieves;
    State::thieves = (Deque *)malloc(sizeof(Deque));
    *State::thieves = thieves;
    GivenTask current_task;
    current_task.pid = -1;
    current_task.var_id = -1;
    State::current_task = current_task;
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
    assert((supposed_num_tasks == 0) || !backtrack_at_top(task_stack));
    assert(cnf.local_edit_count <= cnf.edit_stack.count);
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
    for (int i = 0; i < num_thieves; i++) {
        void *theif_ptr = thief_list[i];
        GivenTask stolen_task = *((GivenTask *)theif_ptr);
        Assignment task_assignment;
        task_assignment.var_id = stolen_task.var_id;
        task_assignment.value = stolen_task.assignment;
        char task_status = cnf.get_decision_status(task_assignment);
        assert(task_status == 'r');
        Assignment opposite_assignment;
        opposite_assignment.var_id = stolen_task.var_id;
        opposite_assignment.value = !stolen_task.assignment;
        char opposite_status = cnf.get_decision_status(opposite_assignment);
        assert((opposite_status == 'l') || (opposite_status == 's'));
        num_stolen++;
    }
    assert(num_queued <= reported_num_queued);
    assert(num_stolen == reported_num_stolen);
    assert(total_assigned == reported_num_local + reported_num_remote);
    short *clause_sizes = (short *)calloc(sizeof(short), 
        cnf.clauses.max_indexable + cnf.clauses.max_conflict_indexable);
    memset(clause_sizes, -1, 
        (cnf.clauses.max_indexable + cnf.clauses.max_conflict_indexable) 
        * sizeof(short));
    void **edit_list = (cnf.edit_stack).as_list();
    bool found_decided_variable_assignment = false;
    for (int i = 0; i < cnf.local_edit_count; i++) {
        void *edit_ptr = edit_list[i];
        FormulaEdit edit = *((FormulaEdit *)edit_ptr);
        int clause_id = edit.edit_id;
        if (edit.edit_type == 'c') { // Clause drop
            assert(0 <= clause_id && clause_id < cnf.clauses.max_indexable + cnf.clauses.max_conflict_indexable);
            // No size changing clauses after dropping them
            assert(clause_sizes[clause_id] == -1);
            clause_sizes[clause_id] = 0;
        } else if (edit.edit_type == 's') { // Size decrease
            assert(0 <= clause_id && clause_id < cnf.clauses.max_indexable + cnf.clauses.max_conflict_indexable);
            // No duplicate size change edits
            // Size should change always
            assert(edit.size_before != -1);
            assert(clause_sizes[clause_id] < edit.size_before);
            clause_sizes[clause_id] = edit.size_before;
        } else {
            if (cnf.variables[edit.edit_id].implying_clause_id == -1) {
                assert(!found_decided_variable_assignment);
                found_decided_variable_assignment = true;
            }
        }
    }
    if (task_stack.count > 0) {
        free(task_list);
    }
    if ((*State::thieves).count > 0) {
        free(thief_list);
    }
    if (cnf.local_edit_count > 0) {
        free(edit_list);
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
            return;
        } case 'c': {
            // Add clause drop to it's int offset
            int clause_id = edit.edit_id;
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
    assert(State::num_non_trivial_tasks > 1);
    assert(task_stack.count >= State::num_non_trivial_tasks);
    // Get the actual work as a copy
    Task top_task = *((Task *)(task_stack.pop_from_back()));
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
        // Two deques loose their top element, one looses at least one element
        int edits_to_apply = (cnf.edit_counts_stack).pop_from_back();
        assert(edits_to_apply > 0);
        while (edits_to_apply) {
            FormulaEdit edit = *((FormulaEdit *)(((cnf.edit_stack)).pop_from_back()));
            apply_edit_to_compressed(cnf, cnf.oldest_compressed, edit);
            edits_to_apply--;
        }
        // Ditch the backtrack task at the top
        task_stack.pop_from_back();
    }
    State::num_non_trivial_tasks--;
    assert(State::num_non_trivial_tasks >= 1);
    if (PRINT_LEVEL > 1) printf("PID %d: grabbing work from stack done\n", State::pid);
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
    // us, with bias for asking our parent (hence reverse direction).
    for (short child = State::num_children; child >= 0; child--) {
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
    assert(recipient_index != -1);
    if (PRINT_LEVEL > 0) printf("PID %d: picked request recipient %d\n", State::pid, pid_from_child_index(recipient_index));
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
        abort_process(cnf, task_stack, interconnect);
    } else if (should_forward_urgent_request()) {
        if (PRINT_LEVEL > 0) printf("PID %d: urgently asking for work\n", State::pid);
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
    } else {
        if (PRINT_LEVEL > 0) printf("PID %d: asking for work\n", State::pid);
        // Send a single work request
        short dest_index = pick_request_recipient();
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
        Cnf &cnf, 
        Deque &task_stack, 
        Interconnect &interconnect,
        bool explicit_abort) 
    {
    if (PRINT_LEVEL > 0) printf("PID %d: aborting process\n", State::pid);
    State::process_finished = true;
    State::was_explicit_abort = explicit_abort;
    cnf.free_cnf();
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
        if (PRINT_LEVEL > 0) printf("PID %d: explicitly aborting others\n", State::pid);
        // Success, broadcase explicit abort to every process
        for (short i = 0; i < State::nprocs; i++) {
            if (i == State::pid) {
                continue;
            }
            interconnect.send_abort_message(i);
        }
    } else {
        if (PRINT_LEVEL > 0) printf("PID %d: aborting others\n", State::pid);
        // Any children who did not receive an urgent request from us should
        assert(should_implicit_abort());
        for (short child = 0; child < State::num_children; child++) {
            if (State::requests_sent[child] != 'u') {
                // Child needs one
                short recipient = pid_from_child_index(child);
                if (State::requests_sent[child] == 'n') {
                    // Normal urgent request
                    interconnect.send_work_request(recipient, 1);
                } else {
                    // Urgent upgrade
                    interconnect.send_work_request(recipient, 2);
                }
                State::requests_sent[child] = 'u';
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
                abort_others(interconnect);
                abort_process(cnf, task_stack, interconnect);
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
    assert(State::requests_sent[child_index] != 'n');
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
        State::num_non_trivial_tasks = 1;
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
    if (PRINT_LEVEL > 0) printf("PID %d: handling message\n", State::pid);
    assert(0 <= State::num_urgent && State::num_urgent <= State::num_children);
    assert(State::num_urgent <= State::num_requesting);
    assert(State::num_requesting <= State::num_children + 1);
    assert(0 <= message.type <= 4);
    switch (message.type) {
        case 3: {
            handle_work_message(
                message, cnf, task_stack, interconnect);
            return;
        } case 4: {
            abort_process(cnf, task_stack, interconnect, true);
            return;
        } case 5: {
            invalidate_work(task_stack);
            return;
        } case 6: {
            assert(false); // unfinished
            // Clause conflict_clause = message_to_clause(message);
            // TODO: handle remote conflict clause here
            return;
        } default: {
            // 0, 1, or 2
            handle_work_request(
                message.sender, message.type, cnf, task_stack, interconnect);
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
    if (PRINT_LEVEL > 0) printf("%s\tPID %d: inserting conflict clause into history\n", cnf.depth_str.c_str(), State::pid);
    bool look_for_drop = false;
    cnf.clause_satisfied(conflict_clause, &look_for_drop);
    int drop_var_id = -1;
    IntDeque assigned_literals;
    for (int i = 0; i < conflict_clause.num_literals; i++) {
        int var_id = conflict_clause.literal_variable_ids[i];
        bool literal_sign = conflict_clause.literal_signs[i];
        if (cnf.assigned_true[var_id] 
            && (cnf.true_assignment_statuses[var_id] == 'l')) {
            if (literal_sign) {
                drop_var_id = var_id;
            } else {
                assigned_literals.add_to_front(var_id);
            }
        } else if (cnf.assigned_false[var_id]
            && (cnf.false_assignment_statuses[var_id] == 'l')) {
            if (!literal_sign) {
                drop_var_id = var_id;
            } else {
                assigned_literals.add_to_front(var_id);
            }
        }
    }
    int insertions_made = 0;
    int iterations_checked = 0;
    if (PRINT_LEVEL > 2) printf("%s\tPID %d: need %d insertions for decrement of clause size\n", cnf.depth_str.c_str(), State::pid, assigned_literals.count);
    if (look_for_drop) {
        printf("%s\tPID %d: need drop edit for clause\n", cnf.depth_str.c_str(), State::pid);
    }
    // Need to include the current iteration's edits on the edit stack
    (cnf.edit_counts_stack).add_to_front(cnf.local_edit_count);
    int iterations_to_check = (cnf.edit_counts_stack).count;
    DoublyLinkedList *current_edit_ptr = (*(((cnf.edit_stack).head))).next;
    IntDoublyLinkedList *current_iteration = (*(((cnf.edit_counts_stack).head))).next;
    if (PRINT_LEVEL > 2) printf("%s\tPID %d: iteration edit counts = %s\n", cnf.depth_str.c_str(), State::pid, int_list_to_string((cnf.edit_counts_stack).as_list(), cnf.edit_counts_stack.count).c_str());
    int num_unsat = cnf.get_num_unsat(conflict_clause);
    while ((iterations_checked < iterations_to_check) 
        && (assigned_literals.count > 0)) {
        if (PRINT_LEVEL > 3) printf("%s\t\tPID %d: checking iteration (%d/%d), %d edits left\n", cnf.depth_str.c_str(), State::pid, iterations_checked, iterations_to_check, assigned_literals.count);
        int current_count = (*current_iteration).value;
        int new_count = current_count;
        int edits_seen = 0;
        while ((edits_seen < current_count) && (assigned_literals.count > 0)) {
            if (PRINT_LEVEL > 4) printf("%s\t\t\tPID %d: inspecting edit (%d/%d), %d edits left\n", cnf.depth_str.c_str(), State::pid, edits_seen, current_count, assigned_literals.count);
            FormulaEdit current_edit = *((FormulaEdit *)(
                (*current_edit_ptr).value));
            if (current_edit.edit_type == 'v') {
                int var_id = current_edit.edit_id;
                if (look_for_drop && var_id == drop_var_id) {
                    // Add the drop edit
                    void *drop_edit = clause_edit(conflict_clause.id);
                    DoublyLinkedList drop_element;
                    drop_element.value = drop_edit;
                    drop_element.prev = (*current_edit_ptr).prev;
                    drop_element.next = current_edit_ptr;
                    DoublyLinkedList *drop_element_ptr = (
                        DoublyLinkedList *)malloc(sizeof(DoublyLinkedList));
                    *drop_element_ptr = drop_element;
                    DoublyLinkedList *prev = (*current_edit_ptr).prev;
                    (*prev).next = drop_element_ptr;
                    (*current_edit_ptr).prev = drop_element_ptr;
                    new_count++;
                    look_for_drop = false;
                    insertions_made++;
                    if (PRINT_LEVEL > 5) printf("%s\t\t\tPID %d: inserted drop edit\n", cnf.depth_str.c_str(), State::pid);
                } else {
                    int num_to_check = assigned_literals.count;
                    while (num_to_check > 0) {
                        int literal_var_id = assigned_literals.pop_from_front();
                        if (literal_var_id == var_id) {
                            // Drecrease clause size edit
                            void *size_change = size_change_edit(
                                conflict_clause.id, num_unsat + 1, num_unsat);
                            FormulaEdit edit = *((FormulaEdit *)size_change);
                            DoublyLinkedList size_change_element;
                            size_change_element.value = size_change;
                            size_change_element.prev = (*current_edit_ptr).prev;
                            size_change_element.next = current_edit_ptr;
                            DoublyLinkedList *size_change_element_ptr = (
                                DoublyLinkedList *)malloc(
                                    sizeof(DoublyLinkedList));
                            *size_change_element_ptr = size_change_element;
                            DoublyLinkedList *prev = (*current_edit_ptr).prev;
                            (*prev).next = size_change_element_ptr;
                            (*current_edit_ptr).prev = size_change_element_ptr;
                            new_count++;
                            num_unsat++;
                            insertions_made++;
                            if (PRINT_LEVEL > 5) printf("%s\t\t\tPID %d: inserted decrement edit\n", cnf.depth_str.c_str(), State::pid);
                            break;
                        } else {
                            assigned_literals.add_to_back(literal_var_id);
                            num_to_check--;
                        }
                    }
                }
            } else {
                if (PRINT_LEVEL > 5) printf("%s\t\t\tPID %d: non-variable edit ignored\n", cnf.depth_str.c_str(), State::pid);
                assert(current_edit.edit_id != conflict_clause.id);
            }
            current_edit_ptr = (*current_edit_ptr).next;
            edits_seen++;
        }
        if (new_count != current_count) {
            (cnf.edit_stack).count += (new_count - current_count);
            (*current_iteration).value = new_count;
            if (PRINT_LEVEL > 4) printf("%s\t\tPID %d: checked iteration (%d/%d), %d edits added\n", cnf.depth_str.c_str(), State::pid, iterations_checked, iterations_to_check, new_count - current_count);
        } else {
            if (PRINT_LEVEL > 4) printf("%s\t\tPID %d: checked iteration (%d/%d), no edits added\n", cnf.depth_str.c_str(), State::pid, iterations_checked, iterations_to_check);
        }
        iterations_checked++;
        current_iteration = (*current_iteration).next;
    }
    assert(assigned_literals.count == 0);
    // Restore the change we made
    cnf.local_edit_count = (cnf.edit_counts_stack).pop_from_front();
    if (PRINT_LEVEL > 1) printf("%s\tPID %d: made %d insertions into conflict clause history\n", cnf.depth_str.c_str(), State::pid, insertions_made);
    if (PRINT_LEVEL > 2) cnf.print_edit_stack("new edit stack", (cnf.edit_stack));
    if ((cnf.edit_counts_stack).count > 0) { 
        if (PRINT_LEVEL > 3) printf("%s\tPID %d: new iteration edit counts = %s + %d\n", cnf.depth_str.c_str(), State::pid, int_list_to_string((cnf.edit_counts_stack).as_list(), cnf.edit_counts_stack.count).c_str(), cnf.local_edit_count);
    }
    assigned_literals.free_deque();
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

// Moves to lowest point in call stack when conflict clause is useful
void State::backtrack_to_conflict_head(
        Cnf &cnf, 
        Deque &task_stack,
        Clause conflict_clause,
        Task lowest_bad_decision) 
    {
    if (PRINT_LEVEL > 0) printf("%sPID %d: backtrack to conflict head with lowest decision at var %d\n", cnf.depth_str.c_str(), State::pid, lowest_bad_decision.var_id);
    if (PRINT_LEVEL > 1) cnf.print_task_stack("Pre conflict backtrack", task_stack);
    if (PRINT_LEVEL > 2) cnf.print_edit_stack("Pre conflict backtrack", (cnf.edit_stack));
    if (PRINT_LEVEL > 3) printf("%sPID %d: Pre conflict edit counts = %s + %d\n", cnf.depth_str.c_str(), State::pid, int_list_to_string((cnf.edit_counts_stack).as_list(), (cnf.edit_counts_stack).count).c_str(), cnf.local_edit_count);
    assert(cnf.get_num_unsat(conflict_clause) == 0);
    int num_backtracks = 0;
    assert(task_stack.count > 0);
    Task current_task;
    bool make_initial_backtrack = false;
    if (task_stack.count >= 2) {
        Task first_task = peak_task(task_stack);
        Task second_task = peak_task(task_stack, 1);
        Task opposite_first = opposite_task(first_task);
        if (cnf.get_decision_status(opposite_first) == 'l') {
            make_initial_backtrack = true;
        }
    } else {
        make_initial_backtrack = true;
    }
    if (make_initial_backtrack) {
        printf("Backtrack initial\n");
        cnf.backtrack();
        num_backtracks++;
    }
    bool tasks_equal = false;
    while ((!tasks_equal) && (task_stack.count > 0)) {
        // Clause is not useful to us, must backtrack more
        current_task = get_task(task_stack);
        tasks_equal = (lowest_bad_decision.var_id == current_task.var_id)
            && (lowest_bad_decision.is_backtrack == current_task.is_backtrack);
        if (current_task.is_backtrack) {
            if (!tasks_equal) {
                cnf.backtrack();
                num_backtracks++;
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
        if (PRINT_LEVEL > 5) cnf.print_task_stack("During backracking", task_stack);
        if (PRINT_LEVEL > 5) cnf.print_edit_stack("During backracking", (cnf.edit_stack));
    }
    if (PRINT_LEVEL > 1) printf("%sPID %d: backtracked %d times\n", cnf.depth_str.c_str(), State::pid, num_backtracks);
    cnf.print_task_stack("Post conflict backtrack", task_stack);
    if (PRINT_LEVEL > 3) cnf.print_edit_stack("Post conflict backtrack", (cnf.edit_stack));
    if (PRINT_LEVEL > 4) printf("%sPID %d: Post conflict edit counts = %s + %d\n", cnf.depth_str.c_str(), State::pid, int_list_to_string((cnf.edit_counts_stack).as_list(), (cnf.edit_counts_stack).count).c_str(), cnf.local_edit_count);
    assert(cnf.get_num_unsat(conflict_clause) >= 1);
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
        Clause conflict_clause,
        Deque &task_stack,
        bool pick_from_clause) 
    {
    if (PRINT_LEVEL > 0) printf("%sPID %d: adding conflict clause\n", cnf.depth_str.c_str(), State::pid);
    if (PRINT_LEVEL > 2) cnf.print_cnf("Before conflict clause", cnf.depth_str, (PRINT_LEVEL <= 3));
    if (PRINT_LEVEL > 2) cnf.print_task_stack("Before conflict clause", task_stack);
    if (PRINT_LEVEL > 2) printf("Conflict clause num unsat = %d\n", cnf.get_num_unsat(conflict_clause));
    assert(clause_is_sorted(conflict_clause));
    int new_clause_id = cnf.clauses.max_indexable + cnf.clauses.num_conflict_indexed;
    conflict_clause.id = new_clause_id;
    insert_conflict_clause_history(cnf, conflict_clause);
    int *clause_id_ptr = (int *)malloc(sizeof(int));
    *clause_id_ptr = new_clause_id;
    for (int lit = 0; lit < conflict_clause.num_literals; lit++) {
        int var_id = conflict_clause.literal_variable_ids[lit];
        (*(((cnf.variables[var_id])).clauses_containing)).add_to_back(
            (void *)clause_id_ptr);
    }
    cnf.clauses.add_conflict_clause(conflict_clause);
    int actual_size = cnf.get_num_unsat(conflict_clause);
    if (actual_size == 0) {
        cnf.clauses_dropped[new_clause_id] = true;
        cnf.num_clauses_dropped++;
        cnf.clauses.strip_clause(new_clause_id);
    } else if (actual_size != conflict_clause.num_literals) {
        if (PRINT_LEVEL > 2) printf("Changing conflict clause size to %d\n", actual_size);
        cnf.clauses.change_clause_size(conflict_clause.id, actual_size);
    }
    // cnf.clauses.reset_iterator();
    // bool conflict_first = (
    //         cnf.clauses.get_current_clause().id == new_clause_id);
    // if ((task_stack.count == 0) || pick_from_clause || conflict_first) {
    //     // Pick new tasks for next iteration
    //     int num_added;
    //     if (pick_from_clause || conflict_first) {
    //         num_added = add_tasks_from_conflict_clause(
    //             cnf, task_stack, conflict_clause);
    //     } else {
    //         num_added = add_tasks_from_formula(cnf, task_stack);
    //     }
    //     assert(num_added > 0);
    // }
    // if (PRINT_LEVEL > 1) printf("%sPID %d: added conflict clause %d\n", cnf.depth_str.c_str(), State::pid, new_clause_id);
    if (PRINT_LEVEL > 2) cnf.print_cnf("With conflict clause", cnf.depth_str, true);
    if (PRINT_LEVEL > 2) cnf.print_task_stack("With conflict clause", task_stack);
    if (PRINT_LEVEL > 3) printf("%sPID %d: With conflict edit counts = %s + %d\n", cnf.depth_str.c_str(), State::pid, int_list_to_string((cnf.edit_counts_stack).as_list(), (cnf.edit_counts_stack).count).c_str(), cnf.local_edit_count);
}

// Returns who is responsible for making the decision that runs contrary
// to decided_conflict_literals, one of
// 'l' (in local), 'r' (in remote), or 's' (in stealers).
// Also populates the lowest (most-recent) bad decision made.
char State::blame_decision(
        Cnf &cnf,
        Deque &task_stack,
        Deque decided_conflict_literals, 
        Task *lowest_bad_decision) 
    {
    if (PRINT_LEVEL > 1) printf("%sPID %d: blaming decision\n", cnf.depth_str.c_str(), State::pid);
    char result = 'r';
    if (decided_conflict_literals.count == 0) {
        if (task_stack.count == 0) {
            return result;
        } else if (task_stack.count == 1) {
            Task top_task = peak_task(task_stack);
            top_task.assignment = !top_task.assignment;
            *lowest_bad_decision = top_task;
            return 'l';
        }
        Task top_task = peak_task(task_stack);
        if (!top_task.is_backtrack) {
            top_task = peak_task(task_stack, 1);
        }
        top_task.assignment = !top_task.assignment;
        *lowest_bad_decision = top_task;
        return 'l';
    }
    void **decided_literals_list = decided_conflict_literals.as_list();
    for (int i = 0; i < decided_conflict_literals.count; i++) {
        void *decision_ptr = decided_literals_list[i];
        Assignment good_decision = *((Assignment *)decision_ptr);
        Assignment bad_decision;
        bad_decision.var_id = good_decision.var_id;
        bad_decision.value = !good_decision.value;
        char good_status = cnf.get_decision_status(good_decision);
        char bad_status = cnf.get_decision_status(bad_decision);
        // Then conflict clause is satisfied
        assert(good_status != 'l');
        // Ensure the conflict clause is unsatisfiable
        assert((bad_status == 'l') || (bad_status == 'r'));
        if (bad_status == 'l') {
            if (good_status == 'u') {
                // Should be queued or stolen
                assert(false);
            } else if (good_status == 'q') {
                result = 'l';
                break;
            } else if (good_status == 'r') {
                // Can't locally assign something contrary to remote assignment
                assert(false);
            } else { // 's'
                if (result == 'r') {
                    result = 's';
                }
            }
        } else { // 'r'
            assert(good_status == 'u');
        }
    }
    if (result == 'l') {
        if (PRINT_LEVEL > 2) printf("%sPID %d: searching for local culprit\n", cnf.depth_str.c_str(), State::pid);
        cnf.print_task_stack("should be in", task_stack);
        // Find lowest bad decision locally (good one is queued up)
        void **tasks = task_stack.as_list();
        int num_tasks = task_stack.count;
        // Check each task in order
        for (int task_num = 0; task_num < num_tasks; task_num++) {
            void *task_ptr = tasks[task_num];
            Task task = *((Task *)task_ptr);
            if (task_num == num_tasks - 1) {
                // Don't expect it to be a backtrack task
                if (task.is_backtrack) {
                    continue;
                }
            } else {
                // Should be a backtrack task
                if (!task.is_backtrack) {
                    continue;
                }
            }
            // Check to see if the task is a bad decision
            for (int i = 0; i < decided_conflict_literals.count; i++) {
                void *decision_ptr = decided_literals_list[i];
                Assignment good_decision = *((Assignment *)decision_ptr);
                if (good_decision.var_id == task.var_id) {
                    // Found!
                    Task bad_decision;
                    bad_decision.var_id = task.var_id;
                    bad_decision.assignment = !task.assignment;
                    bad_decision.implier = task.implier;
                    bad_decision.is_backtrack = task.is_backtrack;
                    *lowest_bad_decision = bad_decision;
                    free(decided_literals_list);
                    free(tasks);
                    return result;
                }
            }
        }
        free(tasks);
        assert(false);
    } else if (result == 's') {
        if (PRINT_LEVEL > 2) printf("%sPID %d: searching for stolen culprit\n", cnf.depth_str.c_str(), State::pid);
        // Find lowest bad decision as opposite of lowest stolen good decision 
        void **stolen_tasks = (*State::thieves).as_list();
        int num_tasks = (*State::thieves).count;
        // Check each task in order
        for (int task_num = 0; task_num < num_tasks; task_num++) {
            void *task_ptr = stolen_tasks[task_num];
            GivenTask task = *((GivenTask *)task_ptr);
            // Check to see if the task is a bad decision
            for (int i = 0; i < decided_conflict_literals.count; i++) {
                void *decision_ptr = decided_literals_list[i];
                Assignment good_decision = *((Assignment *)decision_ptr);
                if (good_decision.var_id == task.var_id) {
                    // Found!
                    Task bad_decision;
                    bad_decision.var_id = good_decision.var_id;
                    bad_decision.assignment = !good_decision.value;
                    bad_decision.is_backtrack = false;
                    *lowest_bad_decision = bad_decision;
                    free(decided_literals_list);
                    return result;
                }
            }
        }
    }
    free(decided_literals_list);
    return result;
}

// Handles a recieved conflict clause
void State::handle_conflict_clause(
        Cnf &cnf, 
        Deque &task_stack, 
        Clause conflict_clause,
        Interconnect &interconnect,
        bool blamed_for_error)
    {
    int new_clause_id = cnf.clauses.max_indexable + cnf.clauses.num_conflict_indexed;
    printf("%sPID %d: handle conflict clause %d %s\n", cnf.depth_str.c_str(), State::pid, new_clause_id, cnf.clause_to_string_current(conflict_clause, false).c_str());
    printf("%sPID %d: handle conflict clause %d %s\n", cnf.depth_str.c_str(), State::pid, new_clause_id, cnf.clause_to_string_current(conflict_clause, true).c_str());
    bool clause_value;
    bool clause_satisfied = cnf.clause_satisfied(
        conflict_clause, &clause_value);
    // TODO: handle conflict clause reconstruction & message passing
    if (cnf.clause_exists_already(conflict_clause)) {
        if (PRINT_LEVEL > 1) printf("%s\tPID %d: conflict clause already exists\n", cnf.depth_str.c_str(), State::pid);
        assert(State::nprocs > 1);
        if (blamed_for_error || (clause_satisfied && !clause_value)) {
            // TODO: swap the clause forward
        }
        return;
    } 
    if (!clause_satisfied) {
        if (PRINT_LEVEL > 1) printf("%s\tPID %d: conflict clause unsatisfied\n", cnf.depth_str.c_str(), State::pid);
        add_conflict_clause(cnf, conflict_clause, task_stack);
        return;
    } else if (clause_value) {
        if (PRINT_LEVEL > 1) printf("%s\tPID %d: conflict clause true already\n", cnf.depth_str.c_str(), State::pid);
        print_assignment(State::pid, "current assignment: ", cnf.depth_str.c_str(), cnf.get_assignment(), cnf.num_variables, false, false);
        // TODO: add hist edit here
        add_conflict_clause(cnf, conflict_clause, task_stack);
        return;
    } else {
        if (PRINT_LEVEL > 1) printf("%s\tPID %d: conflict clause must be dealt with\n", cnf.depth_str.c_str(), State::pid);
    }
    // The places to backtrack to are those decided, not unit propagated.
    // We save these beforehand, as backtracking will change whether
    // we believe they are decided or unit propagated.
    Deque decided_conflict_literals = cnf.get_decided_conflict_literals(
        conflict_clause);
    Task lowest_bad_decision;
    char culprit = blame_decision(
        cnf, task_stack, decided_conflict_literals, &lowest_bad_decision);
    if (lowest_bad_decision.is_backtrack) {
        printf("%sPID %d: Lowest bad decision B%d\n", cnf.depth_str.c_str(), State::pid, lowest_bad_decision.var_id);
    } else {
        printf("%sPID %d: Lowest bad decision %d = %d\n", cnf.depth_str.c_str(), State::pid, lowest_bad_decision.var_id, lowest_bad_decision.assignment);
    }
    assert(cnf.valid_lbd(decided_conflict_literals, lowest_bad_decision));
    bool blame_remote = false;
    switch (culprit) {
        case 'l': {
            // local is responsible
            if (PRINT_LEVEL > 1) printf("%s\tPID %d: local is responsible for conflict\n", cnf.depth_str.c_str(), State::pid);
            backtrack_to_conflict_head(
                cnf, task_stack, conflict_clause, lowest_bad_decision);
            add_conflict_clause(cnf, conflict_clause, task_stack, true);
            inform_thieves_of_conflict(
                (*State::thieves), conflict_clause, interconnect);
            break;
        } case 'r': {
            assert(State::nprocs > 1);
            // remote is responsible
            if (PRINT_LEVEL > 1) printf("%s\tPID %d: remote is responsible for conflict\n", cnf.depth_str.c_str(), State::pid);
            invalidate_work(task_stack);
            inform_thieves_of_conflict(
                (*State::thieves), conflict_clause, interconnect, true);
            blame_remote = true;
            break;
        } case 's': {
            assert(State::nprocs > 1);
            // thieves are responsible
            if (PRINT_LEVEL > 1) printf("%s\tPID %d: thieves are responsible for conflict\n", cnf.depth_str.c_str(), State::pid);
            invalidate_work(task_stack);
            Deque thieves_to_invalidate;
            Deque thieves_to_give;
            split_thieves(
                lowest_bad_decision, thieves_to_invalidate, thieves_to_give);
            inform_thieves_of_conflict(
                thieves_to_invalidate, conflict_clause, interconnect, true);
            inform_thieves_of_conflict(
                thieves_to_give, conflict_clause, interconnect);
            break;
        }
    }
    if (State::current_task.pid != -1) {
        // Send conflict clause to remote
        interconnect.send_conflict_clause(
            State::current_task.pid, conflict_clause);
    }
    decided_conflict_literals.free_data();
    if (PRINT_LEVEL > 1) printf("%sPID %d: finished handling conflict clause\n", cnf.depth_str.c_str(), State::pid);
    if (PRINT_LEVEL > 2) cnf.print_task_stack("Updated", task_stack);
}


// Handles the current LOCAL conflict clause
void State::handle_local_conflict_clause(
        Cnf &cnf, 
        Deque &task_stack, 
        Clause conflict_clause,
        Interconnect &interconnect)
    {
    int new_clause_id = cnf.clauses.max_indexable + cnf.clauses.num_conflict_indexed;
    if (PRINT_LEVEL > 2) printf("%sPID %d: handle conflict clause %d %s\n", cnf.depth_str.c_str(), State::pid, new_clause_id, cnf.clause_to_string_current(conflict_clause, false).c_str());
    if (PRINT_LEVEL > 2) printf("%sPID %d: handle conflict clause %d %s\n", cnf.depth_str.c_str(), State::pid, new_clause_id, cnf.clause_to_string_current(conflict_clause, true).c_str());

    // TODO: does this ever happen locally?
    if (cnf.clause_exists_already(conflict_clause)) {
        if (PRINT_LEVEL > 1) printf("%s\tPID %d: conflict clause already exists\n", cnf.depth_str.c_str(), State::pid);
        assert(false);
        // TODO: swap the clause forward
        return;
    } 
    
    // Backtrack to just when the clause would've become unit [or restart if conflict clause is unit]
    if (conflict_clause.num_literals == 1) {
        // backtrack all the way back
        while (task_stack.count > 0) {
            Task current_task = get_task(task_stack);
            if (current_task.is_backtrack) {
                printf("c\n");
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
        printf("d\n");
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
                // printf("b\n");
                cnf.backtrack();
                // break from while loop
            //     printf("%d\n", cnf.depth);
            //     cnf.print_task_stack("after bt:", task_stack);
            //     printf("%sPID %d: %s edit counts = %s + %d\n", cnf.depth_str.c_str(), cnf.pid, "debug", int_list_to_string((cnf.edit_counts_stack).as_list(), (cnf.edit_counts_stack).count).c_str(), cnf.local_edit_count);
            //     printf("\n");
            // printf("tsc2: %d\n", task_stack.count);
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

    // TODO: where to add conflict clause?
    add_conflict_clause(cnf, conflict_clause, task_stack, false);
    inform_thieves_of_conflict(
        (*State::thieves), conflict_clause, interconnect);
    
    // Send conflict clause to remote
    if (State::current_task.pid != -1) {
        interconnect.send_conflict_clause(
            State::current_task.pid, conflict_clause);
    }
    if (PRINT_LEVEL > 1) printf("%sPID %d: finished handling conflict clause\n", cnf.depth_str.c_str(), State::pid);
    if (PRINT_LEVEL > 2) cnf.print_task_stack("Updated", task_stack);
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

    // OPT 1: only pick directly related variables - 2x faster
    while (true) {
        current_clause = cnf.clauses.get_current_clause();
        current_clause_id = current_clause.id;
        num_unsat = cnf.pick_from_clause(
            current_clause, &new_var_id, &new_var_sign);
        assert(0 < num_unsat);
        if (new_var_id < cnf.n*cnf.n*cnf.n || num_unsat == 1) break;
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
void print_data(Cnf &cnf, Deque &task_stack, std::string prefix_str) {
    if (PRINT_LEVEL > 1) cnf.print_task_stack(prefix_str, task_stack);
    if (PRINT_LEVEL > 2) cnf.print_edit_stack(prefix_str, (cnf.edit_stack));
    if (PRINT_LEVEL > 1) cnf.print_cnf(prefix_str, cnf.depth_str, true);
    if (PRINT_LEVEL > 4) printf("%sPID %d: %s edit counts = %s + %d\n", cnf.depth_str.c_str(), cnf.pid, prefix_str.c_str(), int_list_to_string((cnf.edit_counts_stack).as_list(), (cnf.edit_counts_stack).count).c_str(), cnf.local_edit_count);
    if (PRINT_LEVEL > 5) print_compressed(
        cnf.pid, prefix_str, cnf.depth_str, cnf.to_int_rep(), cnf.work_ints);
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
        if (State::use_smart_prop) {
            propagate_result = propagate_result && cnf.smart_propagate_assignment(
                var_id, assignment, implier, &conflict_id);
        }
        if (!propagate_result) {
            print_data(cnf, task_stack, "Prop fail");
            if (ENABLE_CONFLICT_RESOLUTION) {
                // bool resolution_result = cnf.conflict_resolution(
                //     conflict_id, conflict_clause);
                bool resolution_result = cnf.conflict_resolution_uid(
                    conflict_id, conflict_clause, decided_var_id);
                if (resolution_result) {
                    // Conflict clause generated
                    // handle_conflict_clause(
                    //     cnf, task_stack, conflict_clause, interconnect);
                    handle_local_conflict_clause(
                        cnf, task_stack, conflict_clause, interconnect);
                    print_data(cnf, task_stack, "Post-handle local conflict clause");

                    // a unit prop is forced
                    // TODO: remove or only run in debug mode
                    int ct = 0;
                    for (int i = 0; i < conflict_clause.num_literals; i++) {
                        if (!(cnf.assigned_true[conflict_clause.literal_variable_ids[i]] || cnf.assigned_false[conflict_clause.literal_variable_ids[i]])) {
                            ct++;
                        }
                    }
                    assert(ct == 1);
                    // don't return false - continue to unit prop code later on

                    // backtracking has perhaps invalidated decided_var_id
                    // TODO: UNSURE HOW TO TEST CORRECTNESS
                    if (task_stack.count == 0) {
                        if (cnf.local_edit_count > 0) {
                            decided_var_id = (*(FormulaEdit *)(cnf.edit_stack.peak_back())).edit_id;
                        } else {
                            decided_var_id = -1; // calc should begin with unit prop
                        }
                        // printf("task stack empty case\n");
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
            abort_process(cnf, task_stack, interconnect, true);
            return true;
        }
        if (current_cycle % CYCLES_TO_RECEIVE_MESSAGES == 0) {
            while (interconnect.async_receive_message(message) && !State::process_finished) {
                handle_message(message, cnf, task_stack, interconnect);
                // NICE: serve work here?
            }
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