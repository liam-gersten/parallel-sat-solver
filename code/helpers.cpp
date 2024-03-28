#include "helpers.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <limits.h>

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
            clause_string.append(" U ");
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
    std::string data_string = "vars = [";
    for (int i = 0; i < num_variables; i++) {
        if (assignment[i]) {
            data_string.append(std::to_string(i));
            if (i != num_variables - 1) {
                data_string.append(", ");
            }
        }
    }
    data_string.append("]");
    printf("%s PID %d %s %s\n", tab_string.c_str(), caller_pid, 
        prefix_string.c_str(), data_string.c_str());
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
    IndexableDLL::count = 0;
    IndexableDLL::iterator_finished = true;
    IndexableDLL::element_ptrs = (DoublyLinkedList **)calloc(
        sizeof(DoublyLinkedList *), num_to_index);
    DoublyLinkedList dummy_head;
    IndexableDLL::dummy_head = (DoublyLinkedList *)malloc(
        sizeof(DoublyLinkedList));
    *IndexableDLL::dummy_head = dummy_head;
}

// default constructor
IndexableDLL::IndexableDLL() {
    IndexableDLL::max_indexable = 0;
    IndexableDLL::num_indexed = 0;
    IndexableDLL::count = 0;
    IndexableDLL::iterator_finished = true;
}

// Adds value with index to the list, O(1)
void IndexableDLL::add_value(void *value, int value_index) {
    if (IndexableDLL::count == IndexableDLL::max_indexable) {
        raise_error("Maximum clause storage size exceeded\n");
    }
    DoublyLinkedList current;
    current.value = value;
    DoublyLinkedList *item_ptr = (DoublyLinkedList *)malloc(
        sizeof(DoublyLinkedList));
    *item_ptr = current;
    // Index stuff
    IndexableDLL::element_ptrs[value_index] = item_ptr;
    IndexableDLL::num_indexed++;
    IndexableDLL::count++;
    // LL stuff
    if (IndexableDLL::count == 1) {
        IndexableDLL::head = item_ptr;
        IndexableDLL::tail = item_ptr;
        IndexableDLL::iterator = item_ptr;
    } else {
        (*(IndexableDLL::tail)).next = item_ptr;
        (*item_ptr).prev = IndexableDLL::tail;
        IndexableDLL::tail = item_ptr;
    }
    // printf("head = %s\n", clause_to_string(*((Clause *)((*(IndexableDLL::head)).value))).c_str());
    // printf("tail = %s\n", clause_to_string(*((Clause *)((*(IndexableDLL::tail)).value))).c_str());
}

// Removes value from the list, pointer saved in index still, easy to re-add
// Should NOT be used when iterating over elements
void IndexableDLL::strip_value(int value_index) {
    DoublyLinkedList *current_element = IndexableDLL::element_ptrs[value_index];
    if (IndexableDLL::count == 1) {
        IndexableDLL::count = 0;
        IndexableDLL::iterator_finished = true;
    } else if (IndexableDLL::head == current_element) {
        IndexableDLL::head = (*current_element).next;
        IndexableDLL::count--;
    } else if (IndexableDLL::tail == current_element) {
        IndexableDLL::head = (*current_element).prev;
        IndexableDLL::count--;
    } else {
        DoublyLinkedList *prev = (*current_element).prev;
        DoublyLinkedList *next = (*current_element).next;
        (*prev).next = next;
        (*next).prev = prev;
        IndexableDLL::count--;
    }
}

// Removes value from the list, pointer saved in index still, easy to re-add
void IndexableDLL::strip_current() {
    if (IndexableDLL::iterator == IndexableDLL::tail) {
        IndexableDLL::iterator_finished = true;
    }
    DoublyLinkedList *current_element = IndexableDLL::iterator;
    (*(IndexableDLL::dummy_head)).next = (*current_element).next;
    IndexableDLL::iterator = IndexableDLL::dummy_head;
    IndexableDLL::count--;
}

// Re adds a value to the list, will now be traversable again
void IndexableDLL::re_add_value(int value_index) {
    DoublyLinkedList *current_element = IndexableDLL::element_ptrs[value_index];
    IndexableDLL::count++;
    if (IndexableDLL::count == 1) {
        IndexableDLL::head = current_element;
        IndexableDLL::tail = current_element;
        IndexableDLL::iterator = current_element;
    } else {
        (*(IndexableDLL::tail)).next = current_element;
        (*current_element).prev = IndexableDLL::tail;
        IndexableDLL::tail = current_element;
    }
}

// Returns saved value at index
void *IndexableDLL::get_value(int value_index) {
    DoublyLinkedList *current_element = IndexableDLL::element_ptrs[value_index];
    return (*current_element).value;
}

// Sets iterator to the start
void IndexableDLL::set_to_head() {
    IndexableDLL::iterator_finished = (IndexableDLL::count == 0);
    IndexableDLL::iterator = IndexableDLL::head;
}

// Gets value at iterator
void *IndexableDLL::get_current_value() {
    return (*(IndexableDLL::iterator)).value;
}

// Moves iterator forward
void IndexableDLL::advance_iterator() {
    if (IndexableDLL::iterator_finished) {
        return;
    }
    if (IndexableDLL::tail == IndexableDLL::iterator) {
        IndexableDLL::iterator_finished = true;
        return;
    }
    IndexableDLL::iterator = (*(IndexableDLL::iterator)).next;
}

// Frees data structures used
void IndexableDLL::free_data() {
    for (int i = 0; i < IndexableDLL::num_indexed; i++) {
        free(IndexableDLL::element_ptrs[i]);
    }
    free(IndexableDLL::element_ptrs);
    free(IndexableDLL::dummy_head);
}

// Adds value to back of queue
void Queue::add_to_front(void *value) {
    LinkedList current;
    current.value = value;
    if (Queue::count == 0) {
        Queue::head = (LinkedList *)malloc(sizeof(LinkedList));
        Queue::tail = Queue::head;
        *Queue::head = current;
    } else {
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
        Queue::head = (LinkedList *)malloc(sizeof(LinkedList));
        Queue::tail = Queue::head;
        *Queue::head = current;
    } else {
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

//----------------------------------------------------------------
// END IMPLEMENTATION
//----------------------------------------------------------------