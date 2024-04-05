#include "helpers.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <limits.h>
#include <cassert>
#include <mpi.h>

//----------------------------------------------------------------
// BEGIN IMPLEMENTATION
//----------------------------------------------------------------

// Reads input puzzle file to arrays
int **read_puzzle_file(
        std::string input_filename,
        int *n_ptr,
        int *sqrt_n_ptr,
        int *num_constraints_ptr) 
    {
    std::ifstream fin(input_filename);

    if (!fin) {
    std::cerr << "Unable to open file: " << input_filename << ".\n";
    exit(EXIT_FAILURE);
    }

    /* Read the grid dimension and wire information from file */
    int n;
    int sqrt_n;
    int num_constraints;
    fin >> n >> sqrt_n >> num_constraints;
    *n_ptr = n;
    *sqrt_n_ptr = sqrt_n;
    *num_constraints_ptr = num_constraints;
    int **constraints = (int **)calloc(sizeof(int *), num_constraints);

    for (int i = 0; i < num_constraints; i++) {
    int constraint_sum;
    int constraint_size;
    fin >> constraint_sum >> constraint_size;
    int actual_size = (2 * (constraint_size + 1));
    int *current_constraint = (int *)calloc(sizeof(int), actual_size);
    current_constraint[0] = constraint_sum;
    current_constraint[1] = constraint_size;
    int row;
    int col;
    for (int j = 2; j < actual_size; j += 2) {
        fin >> row >> col;
        current_constraint[j] = row;
        current_constraint[j + 1] = col;
    }
    constraints[i] = current_constraint;
    }
    return constraints;
}

// Makes a task from inputs
void *make_task(int var_id, bool assignment) {
    Task task;
    task.var_id = var_id;
    task.assignment = assignment;
    Task *task_ptr = (Task *)malloc(sizeof(Task));
    *task_ptr = task;
    return (void *)task_ptr;
}

// Makes a clause of just two variables
Clause make_small_clause(int var1, int var2, bool sign1, bool sign2) {
    Clause current;
    current.literal_variable_ids = (int *)malloc(sizeof(int) * 2);
    current.literal_signs = (bool *)malloc(sizeof(bool) * 2);
    (current.literal_variable_ids)[0] = var1;
    (current.literal_variable_ids)[1] = var2;
    (current.literal_signs)[0] = sign1;
    (current.literal_signs)[1] = sign2;
    current.num_literals = 2;
    return current;
}

// Makes a clause of just two variables
Clause make_triple_clause(
        int var1, 
        int var2, 
        int var3, 
        bool sign1, 
        bool sign2, 
        bool sign3) 
    {
    Clause current;
    current.literal_variable_ids = (int *)malloc(sizeof(int) * 3);
    current.literal_signs = (bool *)malloc(sizeof(bool) * 3);
    (current.literal_variable_ids)[0] = var1;
    (current.literal_variable_ids)[1] = var2;
    (current.literal_variable_ids)[2] = var3;
    (current.literal_signs)[0] = sign1;
    (current.literal_signs)[1] = sign2;
    (current.literal_signs)[2] = sign3;
    current.num_literals = 3;
    return current;
}

// Possibly flips value at random
bool get_first_pick(bool heuristic) {
    if (!RANDOM_FIRST_PICK) {
        return heuristic;
    }
    // if ((rand() % 2) == 0) {
    //     return heuristic;
    // }
    return !heuristic;
}

// Makes a variable edit
void *variable_edit(int var_id) {
    FormulaEdit edit;
    edit.edit_type = 'v';
    edit.edit_id = var_id;
    FormulaEdit *edit_ptr = (FormulaEdit *)malloc(sizeof(FormulaEdit));
    *edit_ptr = edit;
    return (void *)edit_ptr;
}

// Makes a clause edit
void *clause_edit(int clause_id) {
    FormulaEdit edit;
    edit.edit_type = 'c';
    edit.edit_id = clause_id;
    FormulaEdit *edit_ptr = (FormulaEdit *)malloc(sizeof(FormulaEdit));
    *edit_ptr = edit;
    return (void *)edit_ptr;
}

// Makes a size change edit
void *size_change_edit(int clause_id, int size_before, int size_after) {
    FormulaEdit edit;
    edit.edit_type = 'i';
    edit.edit_id = clause_id;
    edit.size_before = (short)size_before;
    edit.size_after = (short)size_after;
    FormulaEdit *edit_ptr = (FormulaEdit *)malloc(sizeof(FormulaEdit));
    *edit_ptr = edit;
    return (void *)edit_ptr;
}

// Gets string representation of clause
std::string clause_to_string(Clause clause) {
    std::string clause_string = "(";
    for (int lit = 0; lit < clause.num_literals; lit++) {
        if (!(clause.literal_signs[lit])) {
            clause_string.append("!");
        }
        clause_string.append(
            std::to_string(clause.literal_variable_ids[lit]));
        if (lit != clause.num_literals - 1) {
            clause_string.append(" \\/ ");
        }
    }
    clause_string.append(")");
    return clause_string;
}

// Prints current variable assignment
void print_assignment(
        int caller_pid,
        std::string prefix_string, 
        std::string tab_string,
        bool *assignment,
        int num_variables) 
    {
    std::string data_string = "true vars = [";
    for (int i = 0; i < num_variables; i++) {
        if (assignment[i]) {
            data_string.append(std::to_string(i));
            if (i != num_variables - 1) {
                data_string.append(", ");
            }
        }
    }
    data_string.append("]");
    printf("%sPID %d %s %s\n", tab_string.c_str(), caller_pid, 
        prefix_string.c_str(), data_string.c_str());
}

// Displays sudoku board
void print_board(short **board, int n) {
    std::string padding = "";
    if (n > 9) {
        padding.append(" ");
    }
    printf("\n\n\t -");
    for (int i = 0; i < n; i++) {
        printf("-");
        if (n > 9) {
            printf("-");
        }
        if (i != n - 1) {
            printf("-");
        }
    }
    printf("-\n");
    for (int row = 0; row < n; row++) {
        printf("\t| ");
        for (int col = 0; col < n; col++) {
            int val = board[row][col];
            if (val <= 9) {
                printf("%s", padding.c_str());
            }
            printf("%d", val);
            if (col != n - 1) {
                printf(" ");
            }
        }
        printf(" |\n");
    }
    printf("\t -");
    for (int i = 0; i < n; i++) {
        printf("-");
        if (n > 9) {
            printf("-");
        }
        if (i != n - 1) {
            printf("-");
        }
    }
    printf("-\n\n\n");
}

// Prints current compressed Cnf
void print_compressed(
        int caller_pid,
        std::string prefix_string, 
        std::string tab_string,
        unsigned int *compressed,
        int n) 
    {
    std::string data_string = "compressed: [";
    for (int i = 2; i < n; i++) {
        data_string.append(std::to_string(compressed[i]));
        if (i != n - 1) {
            data_string.append(" ");
        }
    }
    data_string.append("]");
    printf("%sPID %d %s %s\n", tab_string.c_str(), caller_pid, prefix_string.c_str(), data_string.c_str());
}

// Raises an error with a print statement
void raise_error(std::string error_message) {
    printf("\n\nERROR: %s\n\n\n", error_message.c_str());
    exit(1);
}

// Ceiling division
int ceil_div(int num, int denom) {
  int result = num / denom;
  if (result * denom < num) {
    result++;
  }
  return result;
}

// Reads 32 bits from bits array as a single signed integer
unsigned int bits_to_int(bool *bits) {
    unsigned int value = 0;
    unsigned int running = 1;
    for (int i = 0; i < 32; i++) {
        if (bits[i]) {
            value += running;
        }
        running *= 2;
    }
    return value;
}

// Reads integer into 32 bit array
bool *int_to_bits(unsigned int value) {
    bool *bits = (bool *)malloc(sizeof(bool) * 32);
    unsigned int current = value;
    unsigned int running = (1 << 31);
    for (int i = 31; i >= 0; i--) {
        if (current >= running) {
            current -= running;
            bits[i] = true;
        } else {
            bits[i] = false;
        }
        running = (running >> 1);
    }
    return bits;
}

IndexableDLL::IndexableDLL(int num_to_index) {
    IndexableDLL::max_indexable = num_to_index;
    IndexableDLL::num_indexed = 0;
    IndexableDLL::element_ptrs = (DoublyLinkedList **)malloc(
        sizeof(DoublyLinkedList *) * num_to_index);
    IndexableDLL::element_counts = (int *)calloc(sizeof(int), num_to_index);
    
    DoublyLinkedList bookend_value;

    IndexableDLL::one_big_head = (DoublyLinkedList *)malloc(
        sizeof(DoublyLinkedList));
    *IndexableDLL::one_big_head = bookend_value;
    IndexableDLL::one_big_tail = (DoublyLinkedList *)malloc(
        sizeof(DoublyLinkedList));
    *IndexableDLL::one_big_tail = bookend_value;

    IndexableDLL::one_small_head = (DoublyLinkedList *)malloc(
        sizeof(DoublyLinkedList));
    *IndexableDLL::one_small_head = bookend_value;
    IndexableDLL::one_small_tail = (DoublyLinkedList *)malloc(
        sizeof(DoublyLinkedList));
    *IndexableDLL::one_small_tail = bookend_value;

    IndexableDLL::two_big_head = (DoublyLinkedList *)malloc(
        sizeof(DoublyLinkedList));
    *IndexableDLL::two_big_head = bookend_value;
    IndexableDLL::two_big_tail = (DoublyLinkedList *)malloc(
        sizeof(DoublyLinkedList));
    *IndexableDLL::two_big_tail = bookend_value;

    IndexableDLL::two_head = (DoublyLinkedList *)malloc(
        sizeof(DoublyLinkedList));
    *IndexableDLL::two_head = bookend_value;
    IndexableDLL::two_tail = (DoublyLinkedList *)malloc(
        sizeof(DoublyLinkedList));
    *IndexableDLL::two_tail = bookend_value;

    IndexableDLL::big_head = (DoublyLinkedList *)malloc(
        sizeof(DoublyLinkedList));
    *IndexableDLL::big_head = bookend_value;
    IndexableDLL::big_tail = (DoublyLinkedList *)malloc(
        sizeof(DoublyLinkedList));
    *IndexableDLL::big_tail = bookend_value;

    // Tie them together
    (*(IndexableDLL::one_big_head)).next = one_big_tail;
    (*(IndexableDLL::one_big_tail)).next = one_small_head;
    (*(IndexableDLL::one_small_head)).next = one_small_tail;
    (*(IndexableDLL::one_small_tail)).next = two_big_head;
    (*(IndexableDLL::two_big_head)).next = two_big_tail;
    (*(IndexableDLL::two_big_tail)).next = two_head;
    (*(IndexableDLL::two_head)).next = two_tail;
    (*(IndexableDLL::two_tail)).next = big_head;
    (*(IndexableDLL::big_head)).next = big_tail;

    (*(IndexableDLL::one_big_tail)).prev = one_big_head;
    (*(IndexableDLL::one_small_head)).prev = one_big_tail;
    (*(IndexableDLL::one_small_tail)).prev = one_small_head;
    (*(IndexableDLL::two_big_head)).prev = one_small_tail;
    (*(IndexableDLL::two_big_tail)).prev = two_big_head;
    (*(IndexableDLL::two_head)).prev = two_big_tail;
    (*(IndexableDLL::two_tail)).prev = two_head;
    (*(IndexableDLL::big_head)).prev = two_tail;
    (*(IndexableDLL::big_tail)).prev = big_head;

    // All are empty
    IndexableDLL::linked_list_count = 0;
}

// default constructor
IndexableDLL::IndexableDLL() {
    IndexableDLL::max_indexable = 0;
    IndexableDLL::num_indexed = 0;
    IndexableDLL::linked_list_count = 0;
}

// Adds value with index to the list, O(1)
void IndexableDLL::add_value(void *value, int value_index, int num_elements) {
    if (value_index < IndexableDLL::num_indexed) {
        // Already added
        return;
    } else if (IndexableDLL::num_indexed == IndexableDLL::max_indexable) {
        raise_error("\nOut of space for more clauses\n");
    }
    DoublyLinkedList current;
    current.value = value;
    DoublyLinkedList *current_ptr = (DoublyLinkedList *)malloc(
        sizeof(DoublyLinkedList));
    *current_ptr = current;
    IndexableDLL::element_ptrs[value_index] = current_ptr;
    IndexableDLL::element_counts[value_index] = num_elements;
    IndexableDLL::num_indexed += 1;
    DoublyLinkedList *tail_of_interest;
    if (num_elements == 2) {
        tail_of_interest = IndexableDLL::two_tail;
    } else {
        assert(num_elements > 2);
        tail_of_interest = IndexableDLL::big_tail;
    }
    DoublyLinkedList *final_element = (*tail_of_interest).prev;
    (*final_element).next = current_ptr;
    (*(current_ptr)).prev = final_element;
    (*(current_ptr)).next = tail_of_interest;
    (*tail_of_interest).prev = current_ptr;
    IndexableDLL::linked_list_count++;
}

// Removes value from the list, pointer saved in index still, easy to re-add
void IndexableDLL::strip_value(int value_index) {
    assert(0 <= value_index && value_index <= IndexableDLL::num_indexed);
    DoublyLinkedList *current_ptr = IndexableDLL::element_ptrs[value_index];
    DoublyLinkedList *prev = (*current_ptr).prev;
    DoublyLinkedList *next = (*current_ptr).next;
    if (IndexableDLL::iterator == current_ptr) {
        // Move to previous element
        IndexableDLL::iterator = prev;
    }
    (*prev).next = next;
    (*next).prev = prev;
    IndexableDLL::linked_list_count--;
}

// Removes value from the list, pointer saved in index still, easy to re-add
void IndexableDLL::strip_current() {
    DoublyLinkedList *current_ptr = IndexableDLL::iterator;
    DoublyLinkedList *prev = (*current_ptr).prev;
    DoublyLinkedList *next = (*current_ptr).next;
    IndexableDLL::iterator = prev;
    (*prev).next = next;
    (*next).prev = prev;
    IndexableDLL::linked_list_count--;
}

// Re adds a value to the list, will now be traversable again
void IndexableDLL::re_add_value(int value_index) {
    assert(0 <= value_index && value_index <= IndexableDLL::num_indexed);
    DoublyLinkedList *current_ptr = IndexableDLL::element_ptrs[value_index];
    int num_elements = IndexableDLL::element_counts[value_index];
    DoublyLinkedList *tail_of_interest;
    if (num_elements == 2) {
        tail_of_interest = IndexableDLL::two_tail;
    } else {
        assert(num_elements > 2);
        tail_of_interest = IndexableDLL::big_tail;
    }
    DoublyLinkedList *final_element = (*tail_of_interest).prev;
    (*final_element).next = current_ptr;
    (*(current_ptr)).prev = final_element;
    (*(current_ptr)).next = tail_of_interest;
    (*tail_of_interest).prev = current_ptr;
    IndexableDLL::linked_list_count++;
}

// Returns the tail an element should be added to given the size change
DoublyLinkedList *IndexableDLL::get_tail_of_interest(
        int old_size, 
        int new_size) 
    {
    if (new_size == 1) {
        // Decreases to 1
        if (old_size == 2) {
            return IndexableDLL::one_small_tail;
        }
        assert(old_size > 2);
        return IndexableDLL::one_big_tail;
    } else if (new_size == 2) {
        if (old_size == 1) {
            return IndexableDLL::two_tail;
        }
        assert(old_size > 2);
        return IndexableDLL::two_big_tail;
    }
    return IndexableDLL::big_tail;
}

// Moves element to a new bin based on a new size
void IndexableDLL::change_size_of_value(
        int value_index, 
        int old_size, 
        int new_size) {
    assert(0 <= value_index && value_index <= IndexableDLL::num_indexed);
    assert(0 < old_size && 0 < new_size && new_size != old_size);
    if ((new_size > 2) && (old_size > 2)) {
        // No re-ordering to do
        return;
    }
    strip_value(value_index);
    DoublyLinkedList *current_ptr = IndexableDLL::element_ptrs[value_index];
    DoublyLinkedList *tail_of_interest = get_tail_of_interest(
        old_size, new_size);
    DoublyLinkedList *final_element = (*tail_of_interest).prev;
    (*final_element).next = current_ptr;
    (*(current_ptr)).prev = final_element;
    (*(current_ptr)).next = tail_of_interest;
    (*tail_of_interest).prev = current_ptr;
    IndexableDLL::linked_list_count++;
}

// Moves element at iterator to a new bin based on a new size
// This will move the element to an earlier position before the iterator
void IndexableDLL::change_size_of_current(int old_size, int new_size) {
    assert(iterator_position_valid() && (!iterator_is_finished()));
    assert(0 < old_size && 0 < new_size && new_size != old_size);
    if ((new_size > 2) && (old_size > 2)) {
        // No re-ordering to do
        return;
    }
    DoublyLinkedList *current_ptr = IndexableDLL::iterator;
    DoublyLinkedList *prev = (*current_ptr).prev;
    DoublyLinkedList *next = (*current_ptr).next;
    IndexableDLL::iterator = prev;
    (*prev).next = next;
    (*next).prev = prev;
    DoublyLinkedList *tail_of_interest = get_tail_of_interest(
        old_size, new_size);
    DoublyLinkedList *final_element = (*tail_of_interest).prev;
    (*final_element).next = current_ptr;
    (*(current_ptr)).prev = final_element;
    (*(current_ptr)).next = tail_of_interest;
    (*tail_of_interest).prev = current_ptr;
}

// Returns saved value at index
void *IndexableDLL::get_value(int value_index) {
    assert(0 <= value_index && value_index <= IndexableDLL::num_indexed);
    DoublyLinkedList *current_ptr = IndexableDLL::element_ptrs[value_index];
    return (*current_ptr).value;
}

// Gets value at iterator
void *IndexableDLL::get_current_value() {
    assert(iterator_position_valid() && (!iterator_is_finished()));
    DoublyLinkedList *current_ptr = IndexableDLL::iterator;
    return (*current_ptr).value;
}

// Returns the size of the linked list
int IndexableDLL::get_linked_list_size() {
    return IndexableDLL::linked_list_count;
}

// Returns whether the iterator's position is valid (internal)
bool IndexableDLL::iterator_position_valid() {
    // Being at the final tail (big_tail) is valid
    return (
        (IndexableDLL::iterator != IndexableDLL::one_big_head) &&
        (IndexableDLL::iterator != IndexableDLL::one_small_head) &&
        (IndexableDLL::iterator != IndexableDLL::two_big_head) &&
        (IndexableDLL::iterator != IndexableDLL::two_head) &&
        (IndexableDLL::iterator != IndexableDLL::big_head) &&
        (IndexableDLL::iterator != IndexableDLL::one_big_tail) &&
        (IndexableDLL::iterator != IndexableDLL::one_small_tail) &&
        (IndexableDLL::iterator != IndexableDLL::two_big_tail) &&
        (IndexableDLL::iterator != IndexableDLL::two_tail)
        );    
}

// Sets iterator to the start
void IndexableDLL::reset_iterator() {
    IndexableDLL::iterator = IndexableDLL::one_big_head;
    if (IndexableDLL::linked_list_count == 0) {
        return;
    }
    while (!iterator_position_valid()) {
        IndexableDLL::iterator = (*(IndexableDLL::iterator)).next;
    }
}

// Moves iterator forward
void IndexableDLL::advance_iterator() {
    IndexableDLL::iterator = (*(IndexableDLL::iterator)).next;
    // Keep moving until we find a valid element or the end
    while (!iterator_position_valid()) {
        IndexableDLL::iterator = (*(IndexableDLL::iterator)).next;
    }
}

// Returns whether the iterator is at the end
bool IndexableDLL::iterator_is_finished() {
    return ((IndexableDLL::iterator == IndexableDLL::big_tail) 
        || (IndexableDLL::linked_list_count == 0));
}

// Joins two lists, first is now at the second's head
void IndexableDLL::combine_lists(
        DoublyLinkedList *first_head, 
        DoublyLinkedList *first_tail,
        DoublyLinkedList *second_head,
        DoublyLinkedList *second_tail) 
    {
    if ((*first_head).next == first_tail) {
        return;
    }
    DoublyLinkedList *first_first_element = (*first_head).next;
    DoublyLinkedList *first_last_element = (*first_tail).prev;
    DoublyLinkedList *second_first_element = (*second_head).next;
    (*(second_head)).next = first_first_element;
    (*first_first_element).prev = second_head;
    (*first_last_element).next = second_first_element;
    (*second_first_element).prev = first_last_element;
    (*first_head).next = first_tail;
    (*first_tail).prev = first_head;
}

// Moves all ll items to their original bins.
void IndexableDLL::reset_ll_bins() {
    combine_lists(
        IndexableDLL::two_big_head, IndexableDLL::two_big_tail, 
        IndexableDLL::big_head, IndexableDLL::big_tail);
    combine_lists(
        IndexableDLL::one_big_head, IndexableDLL::one_big_tail, 
        IndexableDLL::big_head, IndexableDLL::big_tail);
    combine_lists(
        IndexableDLL::one_small_head, IndexableDLL::one_small_tail, 
        IndexableDLL::two_head, IndexableDLL::two_tail);
}

// Frees data structures used
void IndexableDLL::free_data() {
    // TODO: implement this
    return;
}

// Adds element to front of queue
void Deque::add_to_front(void *value) {
    DoublyLinkedList current;
    current.value = value;
    DoublyLinkedList *current_ptr = (DoublyLinkedList *)malloc(
        sizeof(DoublyLinkedList)); 
    if (Deque::count == 0) {
        *current_ptr = current;
        Deque::head = current_ptr;
        Deque::tail = current_ptr;
    } else {
        current.next = Deque::head;
        *current_ptr = current;
        Deque::head = current_ptr;
    }
    Deque::count++;
}

// Adds element to back of queue
void Deque::add_to_back(void *value) {
    DoublyLinkedList current;
    current.value = value;
    DoublyLinkedList *current_ptr = (DoublyLinkedList *)malloc(
        sizeof(DoublyLinkedList)); 
    if (Deque::count == 0) {
        *current_ptr = current;
        Deque::head = current_ptr;
        Deque::tail = current_ptr;
    } else {
        current.prev = Deque::tail;
        *current_ptr = current;
        Deque::tail = current_ptr;
    }
    Deque::count++;
}

// Removes and retuns element at front of queue
void *Deque::pop_from_front() {
    assert(Deque::count > 0);
    void *value = (*(Deque::head)).value;
    DoublyLinkedList *next = (*(Deque::head)).next;
    free(Deque::head);
    Deque::head = next;
    Deque::count--;
    return value;
}

// Removes and retuns element at back of queue
void *Deque::pop_from_back() {
    assert(Deque::count > 0);
    void *value = (*(Deque::tail)).value;
    DoublyLinkedList *prev = (*(Deque::tail)).prev;
    free(Deque::tail);
    Deque::tail = prev;
    Deque::count--;
    return value;
}

// Returns the front value without removing it
void *Deque::peak_front() {
    assert(Deque::count > 0);
    return (*(Deque::head)).value;
}

// Returns the back value without removing it
void *Deque::peak_back() {
    assert(Deque::count > 0);
    return (*(Deque::tail)).value;
}

// Frees all data in the deque
void Deque::free_data() {
    while (Deque::count > 0) {
        pop_from_front();
    }
}

// Adds value to back of queue
void Queue::add_to_front(void *value) {
    LinkedList current;
    current.value = value;
    if (Queue::count == 0) {
        // printf("Adding to front Empty\n");
        Queue::head = (LinkedList *)malloc(sizeof(LinkedList));
        Queue::tail = Queue::head;
        *Queue::head = current;
    } else {
        // printf("Adding to front non empty\n");
        current.next = Queue::head;
        Queue::head = (LinkedList *)malloc(sizeof(LinkedList));
        *(Queue::head) = current;
    }
    Queue::count++;
}

// Adds value to back of queue
void Queue::add_to_back(void *value) {
    LinkedList current;
    current.value = value;
    if (Queue::count == 0) {
        // printf("Adding to back Empty\n");
        Queue::head = (LinkedList *)malloc(sizeof(LinkedList));
        Queue::tail = Queue::head;
        *Queue::head = current;
    } else {
        // printf("Adding to back non empty\n");
        LinkedList tail = *(Queue::tail);
        tail.next = (LinkedList *)malloc(sizeof(LinkedList));
        *tail.next = current;
        *(Queue::tail) = tail;
        Queue::tail = tail.next;
    }
    Queue::count++;
}

// Pops value from front of queue
void *Queue::pop_from_front() {
    // printf("Poping\n");
    LinkedList current = *(Queue::head);
    free(Queue::head);
    Queue::head = current.next;
    Queue::count--;
    return current.value;
}

// Returns the front value without removing it
void *Queue::peak_front() {
    LinkedList current = *(Queue::head);
    return current.value;
}

// Frees all data in the queue
void Queue::free_data() {
    while (Queue::count > 0) {
        void *current = pop_from_front();
        free(current);
    }
}

// Gets first task from stack, frees pointer
Task get_task(Deque &task_stack) {
    void *task_ptr = task_stack.pop_from_front();
    Task task = (*((Task *)task_ptr));
    free(task_ptr);
    return task;
}

// Returns whether the top of the stack says to backtrack
bool backtrack_at_top(Deque task_stack) {
    void *task_ptr = task_stack.peak_back();
    Task task = (*((Task *)task_ptr));
    return (task.var_id == -1);
}

// Returns whether the front of the stack says to backtrack
bool backtrack_at_front(Deque task_stack) {
    void *task_ptr = task_stack.peak_front();
    Task task = (*((Task *)task_ptr));
    return (task.var_id == -1);
}

// Adds element to front of queue
void IntDeque::add_to_front(int value) {
    IntDoublyLinkedList current;
    current.value = value;
    IntDoublyLinkedList *current_ptr = (IntDoublyLinkedList *)malloc(
        sizeof(IntDoublyLinkedList)); 
    if (IntDeque::count == 0) {
        *current_ptr = current;
        IntDeque::head = current_ptr;
        IntDeque::tail = current_ptr;
    } else {
        current.next = IntDeque::head;
        *current_ptr = current;
        IntDeque::head = current_ptr;
    }
    IntDeque::count++;
}

// Adds element to back of queue
void IntDeque::add_to_back(int value) {
    IntDoublyLinkedList current;
    current.value = value;
    IntDoublyLinkedList *current_ptr = (IntDoublyLinkedList *)malloc(
        sizeof(IntDoublyLinkedList)); 
    if (IntDeque::count == 0) {
        *current_ptr = current;
        IntDeque::head = current_ptr;
        IntDeque::tail = current_ptr;
    } else {
        current.prev = IntDeque::tail;
        *current_ptr = current;
        IntDeque::tail = current_ptr;
    }
    IntDeque::count++;
}

// Removes and retuns element at front of queue
int IntDeque::pop_from_front() {
    assert(IntDeque::count > 0);
    int value = (*(IntDeque::head)).value;
    IntDoublyLinkedList *next = (*(IntDeque::head)).next;
    free(IntDeque::head);
    IntDeque::head = next;
    IntDeque::count--;
    return value;
}

// Removes and retuns element at back of queue
int IntDeque::pop_from_back() {
    assert(IntDeque::count > 0);
    int value = (*(IntDeque::tail)).value;
    IntDoublyLinkedList *prev = (*(IntDeque::tail)).prev;
    free(IntDeque::tail);
    IntDeque::tail = prev;
    IntDeque::count--;
    return value;
}

// Returns the front value without removing it
int IntDeque::peak_front() {
    assert(IntDeque::count > 0);
    return (*(IntDeque::head)).value;
}

// Returns the back value without removing it
int IntDeque::peak_back() {
    assert(IntDeque::count > 0);
    return (*(IntDeque::tail)).value;
}

// Frees all data in the deque
void IntDeque::free_data() {
    while (IntDeque::count > 0) {
        pop_from_front();
    }
}

// Adds value to back of queue
void DeadMessageQueue::add_to_queue(void *message, MPI_Request request) {
  DeadMessageLinkedList current;
  current.message = message;
  current.request = request;
  if (DeadMessageQueue::count == 0) {
    DeadMessageQueue::head = (DeadMessageLinkedList *)malloc(sizeof(DeadMessageLinkedList));
    DeadMessageQueue::tail = DeadMessageQueue::head;
    *DeadMessageQueue::head = current;
  } else {
    DeadMessageLinkedList tail = *(DeadMessageQueue::tail);
    tail.next = (DeadMessageLinkedList *)malloc(sizeof(DeadMessageLinkedList));
    *tail.next = current;
    *(DeadMessageQueue::tail) = tail;
    DeadMessageQueue::tail = tail.next;
  }
  DeadMessageQueue::count++;
}

// Pops value from front of queue
void *DeadMessageQueue::pop_from_front() {
  DeadMessageLinkedList current = *(DeadMessageQueue::head);
  free(DeadMessageQueue::head);
  DeadMessageQueue::head = current.next;
  DeadMessageQueue::count--;
  return current.message;
}

// Returns the front value without removing it
MPI_Request DeadMessageQueue::peak_front() {
  DeadMessageLinkedList current = *(DeadMessageQueue::head);
  return current.request;
}

//----------------------------------------------------------------
// END IMPLEMENTATION
//----------------------------------------------------------------