#ifndef HELPERS_H
#define HELPERS_H

#include <string>

// Will have fixed allocation size
struct Clause {
    int *literal_variable_ids; // variable ids for each literal
    bool *literal_signs; // each literal
    int num_literals; // size of the two pointers
};

struct FormulaEdit {
    char edit_type; // v c
    int edit_id; // var or clause id
};

// Reads input puzzle file to arrays
int **read_puzzle_file(
    std::string input_filename,
    int *n_ptr,
    int *sqrt_n_ptr,
    int *num_constraints_ptr);

// Makes a variable edit
void *variable_edit(int var_id);

// Makes a clause edit
void *clause_edit(int clause_id);

// Gets string representation of clause
std::string clause_to_string(Clause clause);

// Prints current variable assignment
void print_assignment(
    int caller_pid,
    std::string prefix_string, 
    std::string tab_string,
    bool *assignment,
    int num_variables);

// Raises an error with a print statement
void raise_error(std::string error_message);

// Ceiling division
int ceil_div(int num, int denom);

// Reads 32 bits from bits array as a single signed integer
unsigned int bits_to_int(bool *bits);

// Reads integer into 32 bit array
bool *int_to_bits(unsigned int value);

struct LinkedList {
    void *value;
    LinkedList *next;
};

struct DoublyLinkedList {
    void *value;
    DoublyLinkedList *prev;
    DoublyLinkedList *next;
};

// Doubly linked list that can be indexed.
// Idea is to always save everything, but not always iterate over everything.
class IndexableDLL {
    public:
        int max_indexable;
        int num_indexed;
        int count;
        bool iterator_finished;
        DoublyLinkedList **element_ptrs;
        DoublyLinkedList *head;
        DoublyLinkedList *tail;
        DoublyLinkedList *iterator;
        DoublyLinkedList *dummy_head;
        
    IndexableDLL(int num_to_index);
    // default constructor
    IndexableDLL();

    // Adds value with index to the list, O(1)
    void add_value(void *value, int value_index);

    // Removes value from the list, pointer saved in index still, easy to re-add
    // Should not be used when iterating over elements
    void strip_value(int value_index);

    // Removes value from the list, pointer saved in index still, easy to re-add
    void strip_current();
    
    // Re adds a value to the list, will now be traversable again
    void re_add_value(int value_index);

    // Returns saved value at index
    void *get_value(int value_index);

    // Sets iterator to the start
    void set_to_head();

    // Gets value at iterator
    void *get_current_value();

    // Moves iterator forward
    void advance_iterator();

    // Frees data structures used
    void free_data();
};

class Queue {
    public:
        LinkedList *head;
        LinkedList *tail;
        int count = 0;

        // Adds value to back of queue
        void add_to_back(void *value);

        // Adds value to front of queue
        void add_to_front(void *value);

        // Pops value from front of queue
        void *pop_from_front();

        // Returns the front value without removing it
        void *peak_front();

        // Frees all data in the queue
        void free_data();
};

#endif