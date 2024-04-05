#ifndef HELPERS_H
#define HELPERS_H

#include <string>
#include "mpi.h"

#define PRINT_LEVEL 0

#define CONCISE_FORMULA 1

#define RANDOM_FIRST_PICK 1

// Will have fixed allocation size
struct Clause {
    int id;
    int *literal_variable_ids; // variable ids for each literal
    bool *literal_signs; // each literal
    int num_literals; // size of the two pointers
    // Used to quickly updates the compressed version of the CNF
    unsigned int clause_addition;
    unsigned int clause_addition_index;
};

struct FormulaEdit {
    int edit_id; // var or clause id
    char edit_type; // v c d
    short size_before;
    short size_after;
};

struct Message {
    short sender;
    short type; // 0 = req, 1 = urgent req, 2 = work, 3 = abort
    void *data;
};

struct Task {
    int var_id;
    bool assignment;
};

// Reads input puzzle file to arrays
int **read_puzzle_file(
    std::string input_filename,
    int *n_ptr,
    int *sqrt_n_ptr,
    int *num_constraints_ptr);

// Makes a task from inputs
void *make_task(int var_id, bool assignment);

// Makes a clause of just two variables
Clause make_small_clause(int var1, int var2, bool sign1, bool sign2);

// Makes a clause of just two variables
Clause make_triple_clause(
    int var1, 
    int var2, 
    int var3, 
    bool sign1, 
    bool sign2, 
    bool sign3);

// Possibly flips value at random
bool get_first_pick(bool heuristic);

// Makes a variable edit
void *variable_edit(int var_id);

// Makes a clause edit
void *clause_edit(int clause_id);

// Makes a size change edit
void *size_change_edit(int clause_id, int size_before, int size_after);

// Gets string representation of clause
std::string clause_to_string(Clause clause);

// Prints current variable assignment
void print_assignment(
    int caller_pid,
    std::string prefix_string, 
    std::string tab_string,
    bool *assignment,
    int num_variables);

// Displays sudoku board
void print_board(short **board, int n);

// Prints current compressed Cnf
void print_compressed(
    int caller_pid,
    std::string prefix_string, 
    std::string tab_string,
    unsigned int *compressed,
    int n);

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
        int max_indexable; // Max amount of clauses that can be held
        int num_indexed; // Number of clauses we're storing
        DoublyLinkedList **element_ptrs; // Nothing is removed from here.
        int *element_counts; // Nothing is removed from here.
        // "Bins" defined in order of higherst to lowest priority.
        // Easy to reset these in O(1).
        DoublyLinkedList *one_big_head; // Size = 1, originally size > 2
        DoublyLinkedList *one_big_tail; // Size = 1, originally size > 2
        DoublyLinkedList *one_small_head; // Size = 1, originally size = 2
        DoublyLinkedList *one_small_tail; // Size = 1, originally size = 2
        DoublyLinkedList *two_big_head; // Size = 2, originally size > 2
        DoublyLinkedList *two_big_tail; // Size = 2, originally size > 2
        DoublyLinkedList *two_head; // Size = 2, originally size = 2
        DoublyLinkedList *two_tail; // Size = 2, originally size = 2
        DoublyLinkedList *big_head; // Size > 2
        DoublyLinkedList *big_tail; // Size > 2
        int linked_list_count;
        // Could be within any one of the above
        DoublyLinkedList *iterator; // Used to traverse the LL
        
    IndexableDLL(int num_to_index);
    // default constructor
    IndexableDLL();

    // Adds value with index to the list, O(1)
    void add_value(void *value, int value_index, int num_elements);

    // Removes value from the list, pointer saved in index still, easy to re-add
    void strip_value(int value_index);

    // Removes value from the list, pointer saved in index still, easy to re-add
    void strip_current();

    // Re adds a value to the list, will now be traversable again
    void re_add_value(int value_index);

    // Returns the tail an element should be added to given the size change
    DoublyLinkedList *get_tail_of_interest(int old_size, int new_size);

    // Moves element to a new bin based on a new size
    void change_size_of_value(int value_index, int old_size, int new_size);
    
    // Moves element at iterator to a new bin based on a new size
    // This will move the element to an earlier position before the iterator
    void change_size_of_current(int old_size, int new_size);

    // Returns saved value at index
    void *get_value(int value_index);

    // Gets value at iterator
    void *get_current_value();

    // Returns the size of the linked list
    int get_linked_list_size();
    
    // Returns whether the iterator's position is valid
    bool iterator_position_valid();
    
    // Sets iterator to the start
    void reset_iterator();

    // Moves iterator forward
    void advance_iterator();

    // Returns whether the iterator is at the end
    bool iterator_is_finished();

    // Moves all ll items to their original bins.
    void reset_ll_bins();

    // Frees data structures used
    void free_data();
};

class Deque {
    public:
        DoublyLinkedList *head;
        DoublyLinkedList *tail;
        int count = 0;

        // Adds element to front of queue
        void add_to_front(void *value);

        // Adds element to back of queue
        void add_to_back(void *value);

        // Removes and retuns element at front of queue
        void *pop_from_front();

        // Removes and retuns element at back of queue
        void *pop_from_back();

        // Returns the front value without removing it
        void *peak_front();

        // Returns the back value without removing it
        void *peak_back();

        // Frees all data in the deque
        void free_data();
};

class Queue {
    public:
        LinkedList *head;
        LinkedList *tail;
        int count = 0;

        // Queue(int count);

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

// Gets first task from stack, frees pointer
Task get_task(Deque &task_stack);

// Returns whether the top of the stack says to backtrack
bool backtrack_at_top(Deque task_stack);

// Returns whether the front of the stack says to backtrack
bool backtrack_at_front(Deque task_stack);

struct IntDoublyLinkedList {
    IntDoublyLinkedList *prev;
    IntDoublyLinkedList *next;
    int value;
};

class IntDeque {
    public:
        IntDoublyLinkedList *head;
        IntDoublyLinkedList *tail;
        int count = 0;

        // Adds element to front of queue
        void add_to_front(int value);

        // Adds element to back of queue
        void add_to_back(int value);

        // Removes and retuns element at front of queue
        int pop_from_front();

        // Removes and retuns element at back of queue
        int pop_from_back();

        // Returns the front value without removing it
        int peak_front();

        // Returns the back value without removing it
        int peak_back();

        // Frees all data in the deque
        void free_data();
};

struct DeadMessageLinkedList {
  void *message;
  MPI_Request request;
  DeadMessageLinkedList *next;
};

class DeadMessageQueue {
  public:
    DeadMessageLinkedList *head;
    DeadMessageLinkedList *tail;
    int count = 0;

    // Adds value to back of queue
    void add_to_queue(void *message, MPI_Request request);

    // Pops message value from front of queue
    void *pop_from_front();

    // Returns the front request without removing it
    MPI_Request peak_front();
};

#endif