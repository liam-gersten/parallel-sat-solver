#ifndef HELPERS_H
#define HELPERS_H

#include <string>
#include "mpi.h"

#define PRINT_LEVEL 0

#define PRINT_INTERCONNECT 0

#define PRINT_PROGRESS 1

#define PRINT_INDENT 1

#define CONCISE_FORMULA 1

#define ALWAYS_PREFER_CONFLICT_CLAUSES 1

#define BIAS_CLAUSES_OF_SIZES_CHANGED 0

#define CYCLES_TO_PRINT_PROGRESS 1

#define CYCLES_TO_RECEIVE_MESSAGES 1

#define ENABLE_CONFLICT_RESOLUTION 0

#ifndef DNDEBUG
// Production builds should set NDEBUG=1
#define DNDEBUG false
#endif

// Will have fixed allocation size
struct Clause {
    int *literal_variable_ids; // variable ids for each literal
    bool *literal_signs; // each literal
    int id;
    int num_literals; // size of the two pointers
    // Used to quickly updates the compressed version of the CNF
    unsigned int clause_addition;
    unsigned int clause_addition_index;
};

struct FormulaEdit {
    int edit_id; // var or clause id
    int implier;
    char edit_type; // v c s
    short size_before;
    short size_after;
};

struct Message {
    short sender;
    short type; 
    // 0 = request, 
    // 1 = urgent request, 
    // 2 = urgent upgrade, 
    // 3 = work, 
    // 4 = explicit abort
    void *data;
};

struct Task {
    int implier;
    int var_id;
    bool assignment;
    bool is_backtrack;
};

struct GivenTask {
    int var_id;
    short pid;
    // The pid the task is given to in State::thieves.
    // The pid of our task giver in State::current_task.
    bool assignment;
};

struct Assignment {
    int var_id;
    bool value;
};

struct GridAssignment {
    int row;
    int col;
    int value;
};

// Reads input puzzle file to arrays
int **read_puzzle_file(
    std::string input_filename,
    int *n_ptr,
    int *sqrt_n_ptr,
    int *num_constraints_ptr,
    int *num_assingments_ptr,
    GridAssignment *&assignments);

// Makes a task from inputs
void *make_task(
    int var_id, 
    int implier, 
    bool value, 
    bool backtrack = false);

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

// Returns a copy of a clause
Clause copy_clause(Clause clause);

// Frees the data in a clause
void free_clause(Clause clause);

// Returns whether the clause's variables are sorted
bool clause_is_sorted(Clause clause);

// Returns whether a clause contains a variable, populating the sign if so
bool variable_in_clause(Clause clause, int var_id, bool *sign);

// Makes an assignment ready for a data structure from arguments
void *make_assignment(int var_id, bool value);

// Makes a variable edit
void *variable_edit(int var_id, int implier);

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
    int num_variables,
    bool add_one = false,
    bool only_true = true);

// Displays sudoku board
void print_board(short **board, int n);

// Prints current compressed Cnf
void print_compressed(
    int caller_pid,
    std::string prefix_string, 
    std::string tab_string,
    unsigned int *compressed,
    int n);

// Converts an int list to it's string representation
std::string int_list_to_string(int *group, int count);

// Converts an edit to it's string representation
std::string edit_to_string(FormulaEdit edit);

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
        int *original_element_counts; // Nothing is removed from here.
        // "Bins" defined in order of higherst to lowest priority.
        // Easy to reset these in O(1).
        DoublyLinkedList *one_head; // Size = 1, original size = 1 
        DoublyLinkedList *one_tail; // Size = 1, original size = 1 
        DoublyLinkedList *one_big_head; // Size = 1, original size > 2
        DoublyLinkedList *one_big_tail; // Size = 1, original size > 2
        DoublyLinkedList *one_small_head; // Size = 1, original size = 2
        DoublyLinkedList *one_small_tail; // Size = 1, original size = 2
        DoublyLinkedList *two_big_head; // Size = 2, original size > 2
        DoublyLinkedList *two_big_tail; // Size = 2, original size > 2
        DoublyLinkedList *two_head; // Size = 2, original size = 2
        DoublyLinkedList *two_tail; // Size = 2, original size = 2
        DoublyLinkedList *big_head; // Size > 2
        DoublyLinkedList *big_tail; // Size > 2
        int linked_list_count;
        // Could be within any one of the above
        DoublyLinkedList *iterator; // Used to traverse the LL
        short iterator_size; // Size of element iterator is at
        
    IndexableDLL(int num_to_index);
    // default constructor
    IndexableDLL();

    // Adds value with index to the list, O(1)
    void add_value(void *value, int value_index, int num_elements);

    // Removes value from the list, pointer saved in index still, easy to re-add
    void strip_value(int value_index);

    // Re adds a value to the list, will now be traversable again
    void re_add_value(int value_index);

    // Returns the head an element should be added to given the size change
    DoublyLinkedList *get_head_of_interest(
        int new_size, 
        int original_size);

    // Moves element to a new bin based on a new size
    void change_size_of_value(int value_index, int new_size);

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

    // Joins two lists, first is now at the second's head
    void combine_lists(
        DoublyLinkedList *first_head, 
        DoublyLinkedList *first_tail,
        DoublyLinkedList *second_head,
        DoublyLinkedList *second_tail);

    // Moves all ll items to their original bins.
    void reset_ll_bins();

    // Frees data structures used
    void free_data();
};

// Holds two IndexableDLL structures, one for normal clauses and one for
// conflict clauses.
class Clauses {
    public:
        int max_indexable; // Max amount of normal clauses that can be held
        int num_indexed; // Number of normal clauses we're storing
        int max_conflict_indexable; // Same but for conflict clauses
        int num_conflict_indexed; // Same but for conflict clauses
        IndexableDLL normal_clauses;
        IndexableDLL conflict_clauses;

    Clauses(int num_regular_to_index, int num_conflict_to_index);
    // default constructor
    Clauses();

    // Adds clause to regular clause list, O(1)
    void add_regular_clause(Clause clause);

    // Adds clause to conflict clause list, O(1)
    void add_conflict_clause(Clause clause);

    // Returns whether a clause id is for a conflict clause
    bool is_conflict_clause(int clause_id);

    // Removes from the list, pointer saved in index still, easy to re-add
    void strip_clause(int clause_id);

    // Re adds to the list, will now be traversable again
    void re_add_clause(int clause_id);

    // Moves clause element to a new bin based on a new size
    void change_clause_size(int clause_id, int new_size);

    // Returns saved clause at index
    Clause get_clause(int clause_id);

    // Returns saved clause pointer at index
    Clause *get_clause_ptr(int clause_id);

    // Returns whether the iterator is on the conflict clause linked list
    bool working_on_conflict_clauses();
    
    // Gets clause at iterator
    Clause get_current_clause();

    // Returns the size of the linked list
    int get_linked_list_size();

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
        int count;

        // Default constructor
        Deque();
        
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

        // Returns the ith value from the front of the queue
        void *peak_ith(int i);

        // Converts deque to a list
        void **as_list();

        // Frees all data in the deque
        void free_data();

        // Frees all data structures
        void free_deque();
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

        // Returns self as a list
        void **get_list();

        // Frees all data in the queue
        void free_data();
};

// Gets first task from stack, frees pointer
Task get_task(Deque &task_stack);

// Gets i-th task from stack stack, does not alter anything
Task peak_task(Deque task_stack, int i = 0);

// Returns the opposite task of the given one as a copy 
Task opposite_task(Task task);

// Returns whether the top of the stack says to backtrack
bool backtrack_at_top(Deque task_stack);

// Returns whether the front of the stack says to backtrack
bool backtrack_at_front(Deque task_stack);

// Returns whether the first task is one requiring a recurse() call
bool recurse_required(Deque task_stack);

struct IntDoublyLinkedList {
    IntDoublyLinkedList *prev;
    IntDoublyLinkedList *next;
    int value;
};

class IntDeque {
    public:
        IntDoublyLinkedList *head;
        IntDoublyLinkedList *tail;
        int count;

        // Default constructor
        IntDeque();

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

        // Returns whether an integer value is contained
        bool contains(int value);

        // Removes and returns the min value
        int pop_min();

        // Prints out current int deque
        void print_deque(
            short pid, 
            std::string prefix_string, 
            std::string depth_string);

        // Converts int deque to an int list
        int *as_list();
        
        // Frees all data in the deque
        void free_data();

        // Frees all data structures
        void free_deque();
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