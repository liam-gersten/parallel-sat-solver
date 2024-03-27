#include "helpers.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>

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
    printf("%s PID %d %s %s\n", tab_string.c_str(), caller_pid, prefix_string.c_str(), data_string.c_str());
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

//----------------------------------------------------------------
// END IMPLEMENTATION
//----------------------------------------------------------------