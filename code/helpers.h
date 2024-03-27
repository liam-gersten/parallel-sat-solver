#ifndef HELPERS_H
#define HELPERS_H

#include <string>

// Reads input puzzle file to arrays
int **read_puzzle_file(
    std::string input_filename,
    int *n_ptr,
    int *sqrt_n_ptr,
    int *num_constraints_ptr);

// Prints current variable assignment
void print_assignment(
    int caller_pid,
    std::string prefix_string, 
    std::string tab_string,
    bool *assignment,
    int num_variables);

struct LinkedList {
    void *value;
    LinkedList *next;
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
};

#endif