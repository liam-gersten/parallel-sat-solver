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
        int *num_constraints_ptr,
        int *num_assingments_ptr,
        GridAssignment *&assignments) 
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
    int num_assignments;
    fin >> n >> sqrt_n >> num_constraints >> num_assignments;
    *n_ptr = n;
    *sqrt_n_ptr = sqrt_n;
    *num_constraints_ptr = num_constraints;
    *num_assingments_ptr = num_assignments;
    int **constraints = (int **)calloc(sizeof(int *), num_constraints);
    assignments = (GridAssignment *)malloc(
        sizeof(GridAssignment) * num_assignments);

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
    for (int i = 0; i < num_assignments; i++) {
        int row;
        int col;
        int value;
        fin >> row >> col >> value;
        GridAssignment assignment;
        assignment.row = row;
        assignment.col = col;
        assignment.value = value - 1; //needs to be 0-indexed
        assignments[i] = assignment;
    }
    return constraints;
}

// Makes a task from inputs
void *make_task(int var_id, int implier, bool value, bool backtrack) {
    Task task;
    task.implier = implier;
    task.var_id = var_id;
    task.assignment = value;
    task.is_backtrack = backtrack;
    Task *task_ptr = (Task *)malloc(sizeof(Task));
    *task_ptr = task;
    return (void *)task_ptr;
}

// Makes a clause of just two variables
Clause make_small_clause(int var1, int var2, bool sign1, bool sign2) {
    if (var1 > var2) {
        return make_small_clause(var2, var1, sign2, sign1);
    }

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

// Makes a clause of just three variables
Clause make_triple_clause(
        int var1, 
        int var2, 
        int var3, 
        bool sign1, 
        bool sign2, 
        bool sign3) 
    {
    if (var1 > var2) {
        std::swap(var1, var2);
        std::swap(sign1, sign2);
    }
    if (var2 > var3) {
        std::swap(var3, var2);
        std::swap(sign3, sign2);
    }
    if (var1 > var2) {
        std::swap(var1, var2);
        std::swap(sign1, sign2);
    }

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

// Returns a copy of a clause
Clause copy_clause(Clause clause) {
    Clause result;
    result.id = clause.id;
    result.literal_variable_ids = (int *)malloc(
        sizeof(int) * clause.num_literals);
    result.literal_signs = (bool *)malloc(
        sizeof(bool) * clause.num_literals);
    memcpy(result.literal_variable_ids, clause.literal_variable_ids, 
        sizeof(int) * clause.num_literals);
    memcpy(result.literal_signs, clause.literal_signs, 
        sizeof(bool) * clause.num_literals);
    result.num_literals = clause.num_literals;
    result.clause_addition = clause.clause_addition;
    result.clause_addition_index = clause.clause_addition_index;
    return result;
}

// Frees the data in a clause
void free_clause(Clause clause) {
    free(clause.literal_variable_ids);
    free(clause.literal_signs);
    return;
}

// Returns whether the clause's variables are sorted
bool clause_is_sorted(Clause clause) {
    for (int i = 1; i < clause.num_literals; i++) {
        if (clause.literal_variable_ids[i - 1] >= clause.literal_variable_ids[i]) {
            return false;
        }
    }
    return true;
}

// Returns whether a clause contains a variable, populating the sign if so
bool variable_in_clause(Clause clause, int var_id, bool *sign) {
    for (int i = 1; i < clause.num_literals; i++) {
        if (clause.literal_variable_ids[i] == var_id) {
            *sign = clause.literal_signs[i];
            return true;
        }
    }
    return false;
}

// Converts a message received to a clause
Clause message_to_clause(Message message) {
    // TODO: implement this
    Clause result;
    return result;
}

// Makes an assignment ready for a data structure from arguments
void *make_assignment(int var_id, bool value) {
    Assignment assignment;
    assignment.var_id = var_id;
    assignment.value = value;
    Assignment *assignment_ptr = (Assignment *)malloc(sizeof(Assignment));
    *assignment_ptr = assignment;
    return (void *)assignment_ptr;
}

// Makes a variable edit
void *variable_edit(int var_id, int implier) {
    FormulaEdit edit;
    edit.edit_type = 'v';
    edit.edit_id = var_id;
    edit.implier = implier;
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
    edit.edit_type = 's';
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
        int num_variables,
        bool add_one,
        bool only_true) 
    {
    std::string data_string = "[";
    for (int i = 0; i < num_variables; i++) {
        if (only_true) {
            if (assignment[i]) {
                if (add_one) {
                    data_string.append(std::to_string(i + 1));
                } else {
                    data_string.append(std::to_string(i));
                }
                if (i != num_variables - 1) {
                    data_string.append(", ");
                }
            }
        } else {
            if (add_one) {
                data_string.append(std::to_string(i + 1));
            } else {
                data_string.append(std::to_string(i));
            }
            data_string.append("=");
            data_string.append(std::to_string(assignment[i]));
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
    for (int i = 0; i < n; i++) {
        data_string.append(std::to_string(compressed[i]));
        if (i != n - 1) {
            data_string.append(" ");
        }
    }
    data_string.append("]");
    printf("%sPID %d %s %s\n", tab_string.c_str(), caller_pid, prefix_string.c_str(), data_string.c_str());
}

// Converts an int list to it's string representation
std::string int_list_to_string(int *group, int count) {
    std::string result = "[";
    for (int i = 0; i < count; i++) {
        result.append(std::to_string(group[i]));
        if (i != count - 1) {
            result.append(", ");
        }
    }
    result.append("]");
    return result;
}

// Converts an edit to it's string representation
std::string edit_to_string(FormulaEdit edit) {
    std::string result = "{";
    if (edit.edit_type == 'v') {
        result.append("set ");
        result.append(std::to_string(edit.edit_id));
    } else if (edit.edit_type == 'c') {
        result.append("drop ");
        result.append(std::to_string(edit.edit_id));
    } else {
        assert(edit.edit_type == 's');
        result.append("decr ");
        result.append(std::to_string(edit.edit_id));
        result.append(" ");
        result.append(std::to_string(edit.size_before));
        result.append("->");
        result.append(std::to_string(edit.size_after));
    }
    result.append("}");
    return result;
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
    IndexableDLL::original_element_counts = (int *)calloc(
        sizeof(int), num_to_index);
    int ints_to_index = ceil_div(
        num_to_index, (sizeof(int) * 8));
    IndexableDLL::elements_dropped = (bool *)calloc(
        sizeof(int), ints_to_index * 8);
    
    DoublyLinkedList bookend_value;

    IndexableDLL::one_head = (DoublyLinkedList *)malloc(
        sizeof(DoublyLinkedList));
    *IndexableDLL::one_head = bookend_value;
    IndexableDLL::one_tail = (DoublyLinkedList *)malloc(
        sizeof(DoublyLinkedList));
    *IndexableDLL::one_tail = bookend_value;

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
    (*(IndexableDLL::one_head)).next = one_tail;
    (*(IndexableDLL::one_big_head)).next = one_big_tail;
    (*(IndexableDLL::one_small_head)).next = one_small_tail;
    (*(IndexableDLL::two_big_head)).next = two_big_tail;
    (*(IndexableDLL::two_head)).next = two_tail;
    (*(IndexableDLL::big_head)).next = big_tail;

    (*(IndexableDLL::one_tail)).prev = one_head;
    (*(IndexableDLL::one_big_tail)).prev = one_big_head;
    (*(IndexableDLL::one_small_tail)).prev = one_small_head;
    (*(IndexableDLL::two_big_tail)).prev = two_big_head;
    (*(IndexableDLL::two_tail)).prev = two_head;
    (*(IndexableDLL::big_tail)).prev = big_head;

    if (BIAS_CLAUSES_OF_SIZES_CHANGED) {
        (*(IndexableDLL::one_big_tail)).next = one_small_head;
        (*(IndexableDLL::one_small_tail)).next = one_head;
        (*(IndexableDLL::one_tail)).next = two_big_head;
        (*(IndexableDLL::two_big_tail)).next = two_head;
        (*(IndexableDLL::two_tail)).next = big_head;

        (*(IndexableDLL::one_small_head)).prev = one_big_tail;
        (*(IndexableDLL::one_head)).prev = one_small_tail;
        (*(IndexableDLL::two_big_head)).prev = one_tail;
        (*(IndexableDLL::two_head)).prev = two_big_tail;
        (*(IndexableDLL::big_head)).prev = two_tail;
    } else {
        (*(IndexableDLL::one_tail)).next = one_small_head;
        (*(IndexableDLL::one_small_tail)).next = one_big_head;
        (*(IndexableDLL::one_big_tail)).next = two_head;
        (*(IndexableDLL::two_tail)).next = two_big_head;
        (*(IndexableDLL::two_big_tail)).next = big_head;

        (*(IndexableDLL::one_small_head)).prev = one_tail;
        (*(IndexableDLL::one_big_head)).prev = one_small_tail;
        (*(IndexableDLL::two_head)).prev = one_big_tail;
        (*(IndexableDLL::two_big_head)).prev = two_tail;
        (*(IndexableDLL::big_head)).prev = two_big_tail;
    }

    // All are empty
    IndexableDLL::linked_list_count = 0;
    IndexableDLL::iterator_size = -1;
}

// default constructor
IndexableDLL::IndexableDLL() {
    IndexableDLL::max_indexable = 0;
    IndexableDLL::num_indexed = 0;
    IndexableDLL::linked_list_count = 0;
    IndexableDLL::iterator_size = -1;
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
    IndexableDLL::original_element_counts[value_index] = num_elements;
    IndexableDLL::num_indexed += 1;
    DoublyLinkedList *tail_of_interest;
    if (num_elements == 0) {
        IndexableDLL::elements_dropped[value_index] = true;
        return;
    } else if (num_elements == 1) {
        tail_of_interest = IndexableDLL::one_tail;
    } else if (num_elements == 2) {
        tail_of_interest = IndexableDLL::two_tail;
    } else {
        tail_of_interest = IndexableDLL::big_tail;
    }
    DoublyLinkedList *last_element = (*tail_of_interest).prev;
    (*tail_of_interest).prev = current_ptr;
    (*(current_ptr)).prev = last_element;
    (*(current_ptr)).next = tail_of_interest;
    (*last_element).next = current_ptr;
    IndexableDLL::linked_list_count++;
}

// Removes value from the list, pointer saved in index still, easy to re-add
void IndexableDLL::strip_value(int value_index) {
    assert(0 <= value_index && value_index <= IndexableDLL::num_indexed);
    assert(!element_is_dropped(value_index));
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
    if (IndexableDLL::linked_list_count == 0) {
        IndexableDLL::iterator_size = -1;
    }
    IndexableDLL::elements_dropped[value_index] = true;
}

// Re adds a value to the list, will now be traversable again
void IndexableDLL::re_add_value(int value_index) {
    assert(0 <= value_index && value_index <= IndexableDLL::num_indexed);
    assert(element_is_dropped(value_index));
    DoublyLinkedList *current_ptr = IndexableDLL::element_ptrs[value_index];
    int old_size = IndexableDLL::original_element_counts[value_index];
    int new_size = IndexableDLL::element_counts[value_index];
    int original_size = IndexableDLL::original_element_counts[value_index];
    DoublyLinkedList *head_of_interest = get_head_of_interest(
        new_size, original_size);
    DoublyLinkedList *first_element = (*head_of_interest).next;
    (*head_of_interest).next = current_ptr;
    (*(current_ptr)).prev = head_of_interest;
    (*(current_ptr)).next = first_element;
    (*first_element).prev = current_ptr;
    IndexableDLL::linked_list_count++;
    IndexableDLL::elements_dropped[value_index] = false;
}

// Returns the head an element should be added to given the size change
DoublyLinkedList *IndexableDLL::get_head_of_interest(
        int new_size,
        int original_size) 
    {
    assert(0 <= new_size);
    assert(0 <= original_size);
    if (new_size == 1) {
        if (original_size == 1) {
            return IndexableDLL::one_head;
        } else if (original_size == 2) {
            return IndexableDLL::one_small_head;
        }
        return IndexableDLL::one_big_head;
    } else if (new_size == 2) {
        if (original_size == 2) {
            return IndexableDLL::two_head;
        }
        assert(original_size > 2);
        return IndexableDLL::two_big_head;
    }
    return IndexableDLL::big_head;
}

// Moves element to a new bin based on a new size
void IndexableDLL::change_size_of_value(int value_index, int new_size) {
    int old_size = IndexableDLL::element_counts[value_index];
    int original_size = IndexableDLL::original_element_counts[value_index];
    assert(0 <= value_index && value_index <= IndexableDLL::num_indexed);
    assert(0 < old_size);
    assert(0 < new_size);
    assert(new_size != old_size);
    IndexableDLL::element_counts[value_index] = new_size;
    if ((new_size > 2) && (old_size > 2)) {
        // No re-ordering to do
        return;
    }
    strip_value(value_index);
    DoublyLinkedList *current_ptr = IndexableDLL::element_ptrs[value_index];
    DoublyLinkedList *head_of_interest = get_head_of_interest(
        new_size, original_size);
    DoublyLinkedList *first_element = (*head_of_interest).next;
    (*head_of_interest).next = current_ptr;
    (*(current_ptr)).prev = head_of_interest;
    (*(current_ptr)).next = first_element;
    (*first_element).prev = current_ptr;
    IndexableDLL::linked_list_count++;
    IndexableDLL::elements_dropped[value_index] = false;
}

// Returns whether an element is dropped
bool IndexableDLL::element_is_dropped(int value_index) {
    assert(0 <= value_index);
    assert(value_index <= IndexableDLL::num_indexed);
    return IndexableDLL::elements_dropped[value_index];
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
        (IndexableDLL::iterator != IndexableDLL::one_head) &&
        (IndexableDLL::iterator != IndexableDLL::one_big_head) &&
        (IndexableDLL::iterator != IndexableDLL::one_small_head) &&
        (IndexableDLL::iterator != IndexableDLL::two_big_head) &&
        (IndexableDLL::iterator != IndexableDLL::two_head) &&
        (IndexableDLL::iterator != IndexableDLL::big_head) &&
        (IndexableDLL::iterator != IndexableDLL::one_tail) &&
        (IndexableDLL::iterator != IndexableDLL::one_big_tail) &&
        (IndexableDLL::iterator != IndexableDLL::one_small_tail) &&
        (IndexableDLL::iterator != IndexableDLL::two_big_tail) &&
        (IndexableDLL::iterator != IndexableDLL::two_tail)
        );    
}

// Sets iterator to the start
void IndexableDLL::reset_iterator() {
    if (BIAS_CLAUSES_OF_SIZES_CHANGED) {
        IndexableDLL::iterator = IndexableDLL::one_big_head;
    } else {
        IndexableDLL::iterator = IndexableDLL::one_head;
    }
    if (IndexableDLL::linked_list_count == 0) {
        return;
    }
    while (!iterator_position_valid()) {
        if (BIAS_CLAUSES_OF_SIZES_CHANGED) {
            if (IndexableDLL::iterator == one_big_head) {
                IndexableDLL::iterator_size = 1;
            } else if (IndexableDLL::iterator == two_big_head) {
                IndexableDLL::iterator_size = 2;
            } else if (IndexableDLL::iterator == big_head) {
                IndexableDLL::iterator_size = 3;
            }
        } else {
            if (IndexableDLL::iterator == one_head) {
                IndexableDLL::iterator_size = 1;
            } else if (IndexableDLL::iterator == two_head) {
                IndexableDLL::iterator_size = 2;
            } else if (IndexableDLL::iterator == big_head) {
                IndexableDLL::iterator_size = 3;
            }
        }
        IndexableDLL::iterator = (*(IndexableDLL::iterator)).next;
    }
}

// Moves iterator forward
void IndexableDLL::advance_iterator() {
    IndexableDLL::iterator = (*(IndexableDLL::iterator)).next;
    if (IndexableDLL::iterator == IndexableDLL::big_tail) {
        IndexableDLL::iterator_size = -1;
        return;
    }
    // Keep moving until we find a valid element or the end
    while (!iterator_position_valid()) {
        if (BIAS_CLAUSES_OF_SIZES_CHANGED) {
            if (IndexableDLL::iterator == one_big_head) {
                IndexableDLL::iterator_size = 1;
            } else if (IndexableDLL::iterator == two_big_head) {
                IndexableDLL::iterator_size = 2;
            } else if (IndexableDLL::iterator == big_head) {
                IndexableDLL::iterator_size = 3;
            }
        } else {
            if (IndexableDLL::iterator == one_head) {
                IndexableDLL::iterator_size = 1;
            } else if (IndexableDLL::iterator == two_head) {
                IndexableDLL::iterator_size = 2;
            } else if (IndexableDLL::iterator == big_head) {
                IndexableDLL::iterator_size = 3;
            }
        }
        
        IndexableDLL::iterator = (*(IndexableDLL::iterator)).next;
        if (IndexableDLL::iterator == IndexableDLL::big_tail) {
            IndexableDLL::iterator_size = -1;
            break;
        }
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

// Resets the ordering and drops
void IndexableDLL::reset() {
    memcpy(IndexableDLL::element_counts, IndexableDLL::original_element_counts, 
        sizeof(int) * num_indexed);
    for (int id = 0; id < IndexableDLL::num_indexed; id++) {
        if (element_is_dropped(id)) {
            re_add_value(id);
        }
    }
    reset_ll_bins();
}

// Frees data structures used
void IndexableDLL::free_data() {
    // TODO: implement this
    for (int i = 0; i < IndexableDLL::num_indexed; i++) {
        DoublyLinkedList *element_ptr = IndexableDLL::element_ptrs[i];
        DoublyLinkedList element = *element_ptr;
        free(element_ptr);
        Clause *clause_ptr = (Clause *)element.value;
        Clause clause = *clause_ptr;
        free(clause_ptr);
        free(clause.literal_variable_ids);
        free(clause.literal_signs);
    }
    free(IndexableDLL::element_ptrs);
    free(IndexableDLL::element_counts);
    free(IndexableDLL::elements_dropped);
    free(IndexableDLL::original_element_counts);
    free(IndexableDLL::one_head);
    free(IndexableDLL::one_tail);
    free(IndexableDLL::one_big_head);
    free(IndexableDLL::one_big_tail);
    free(IndexableDLL::one_small_head);
    free(IndexableDLL::one_small_tail);
    free(IndexableDLL::two_big_head);
    free(IndexableDLL::two_big_tail);
    free(IndexableDLL::two_head);
    free(IndexableDLL::two_tail);
    free(IndexableDLL::big_head);
    free(IndexableDLL::big_tail);
    return;
}

// Holds two IndexableDLL structures, one for normal clauses and one for
// conflict clauses.
Clauses::Clauses(int num_regular_to_index, int num_conflict_to_index) {
    Clauses::max_indexable = num_regular_to_index;
    Clauses::num_indexed = 0;
    Clauses::max_conflict_indexable = num_conflict_to_index;
    Clauses::num_conflict_indexed = 0;
    Clauses::num_clauses_dropped = 0;
    IndexableDLL normal_clauses(num_regular_to_index);
    IndexableDLL conflict_clauses(num_conflict_to_index);
    Clauses::normal_clauses = normal_clauses;
    Clauses::conflict_clauses = conflict_clauses;

    reset_iterator();
}

// default constructor
Clauses::Clauses() {
    Clauses::max_indexable = 0;
    Clauses::num_indexed = 0;
    Clauses::max_conflict_indexable = 0;
    Clauses::num_conflict_indexed = 0;
}

// Adds clause to regular clause list, O(1)
void Clauses::add_regular_clause(Clause clause) {
    assert(clause.num_literals > 0);
    Clause *clause_ptr = (Clause *)malloc(sizeof(Clause));
    *clause_ptr = clause;
    Clauses::normal_clauses.add_value(
        (void *)clause_ptr, Clauses::num_indexed, clause.num_literals);
    Clauses::num_indexed++;
}

// Adds clause to conflict clause list, O(1)
void Clauses::add_conflict_clause(Clause clause) {
    assert(clause.num_literals > 0);
    Clause *clause_ptr = (Clause *)malloc(sizeof(Clause));
    *clause_ptr = clause;
    Clauses::conflict_clauses.add_value(
        (void *)clause_ptr, Clauses::num_conflict_indexed, clause.num_literals);
    Clauses::conflict_clauses.strip_value(Clauses::num_conflict_indexed);
    Clauses::conflict_clauses.re_add_value(Clauses::num_conflict_indexed);
    Clauses::num_conflict_indexed++;
}

// Returns whether a clause id is for a conflict clause
bool Clauses::is_conflict_clause(int clause_id) {
    return (Clauses::max_indexable <= clause_id);
}

// Removes from the list, pointer saved in index still, easy to re-add
void Clauses::drop_clause(int clause_id) {
    Clauses::num_clauses_dropped++;
    if (clause_id >= Clauses::max_indexable) {
        Clauses::conflict_clauses.strip_value(
            clause_id - Clauses::max_indexable);
    } else {
        Clauses::normal_clauses.strip_value(clause_id);
    }
}

// Re adds to the list, will now be traversable again
void Clauses::re_add_clause(int clause_id) {
    Clauses::num_clauses_dropped--;
    if (clause_id >= Clauses::max_indexable) {
        Clauses::conflict_clauses.re_add_value(
            clause_id - Clauses::max_indexable);
    } else {
        Clauses::normal_clauses.re_add_value(clause_id);
    }
}

// Moves clause element to a new bin based on a new size
void Clauses::change_clause_size(int clause_id, int new_size) {
    if (clause_id >= Clauses::max_indexable) {
        Clauses::conflict_clauses.change_size_of_value(
            clause_id - Clauses::max_indexable, new_size);
    } else {
        Clauses::normal_clauses.change_size_of_value(clause_id, new_size);
    }
}

// Returns whether a clause is dropped
bool Clauses::clause_is_dropped(int clause_id) {
    if (clause_id >= Clauses::max_indexable) {
        return Clauses::conflict_clauses.element_is_dropped(
            clause_id - Clauses::max_indexable);
    }
    return Clauses::normal_clauses.element_is_dropped(clause_id);
}

// Returns saved clause at index
Clause Clauses::get_clause(int clause_id) {
    return *(get_clause_ptr(clause_id));
}

// Returns saved clause pointer at index
Clause *Clauses::get_clause_ptr(int clause_id) {
    void *result_ptr;
    if (clause_id >= Clauses::max_indexable) {
        result_ptr = Clauses::conflict_clauses.get_value(
            clause_id - Clauses::max_indexable);
    } else {
        result_ptr = Clauses::normal_clauses.get_value(clause_id);
    }
    return (Clause *)result_ptr;
}

// Returns whether the iterator is on the conflict clause linked list
bool Clauses::working_on_conflict_clauses() {
    short normal_size = Clauses::normal_clauses.iterator_size;
    short conflict_size = Clauses::conflict_clauses.iterator_size;
    if (conflict_size == -1) {
        return false;
    } else if (normal_size == -1) {
        return true;
    } else if (conflict_size == -1 && normal_size == -1) {
        assert(false);
    } else if (ALWAYS_PREFER_CONFLICT_CLAUSES) {
        return true;
    } else if (normal_size < conflict_size) {
        return false;
    }
    return true;
}

// Gets clause at iterator
Clause Clauses::get_current_clause() {
    void *result_ptr;
    if (working_on_conflict_clauses()) {
        result_ptr = Clauses::conflict_clauses.get_current_value();
    } else {
        result_ptr = Clauses::normal_clauses.get_current_value();
    }
    return *((Clause *)result_ptr);
}

// Returns the size of the linked list
int Clauses::get_linked_list_size() {
    return Clauses::normal_clauses.get_linked_list_size() 
        + Clauses::conflict_clauses.get_linked_list_size();
}

// Sets iterator to the start
void Clauses::reset_iterator() {
    Clauses::normal_clauses.reset_iterator();
    Clauses::conflict_clauses.reset_iterator();
}

// Moves iterator forward
void Clauses::advance_iterator() {
    if (working_on_conflict_clauses()) {
        Clauses::conflict_clauses.advance_iterator();
    } else {
        Clauses::normal_clauses.advance_iterator();
    }
}

// Returns whether the iterator is at the end
bool Clauses::iterator_is_finished() {
    return Clauses::normal_clauses.iterator_is_finished() 
        && Clauses::conflict_clauses.iterator_is_finished();
}

// Moves all ll items to their original bins.
void Clauses::reset_ll_bins() {
    Clauses::normal_clauses.reset_ll_bins();
    Clauses::conflict_clauses.reset_ll_bins();
}

// Resets the ordering and drops
void Clauses::reset() {
    Clauses::num_clauses_dropped = 0;
    Clauses::normal_clauses.reset();
    Clauses::conflict_clauses.reset();
}

// Frees data structures used
void Clauses::free_data() {
    // TODO: implement this
    Clauses::normal_clauses.free_data();
    Clauses::conflict_clauses.free_data();
}

// Default constructor
Deque::Deque() {
    Deque::count = 0;
    Deque::head = (DoublyLinkedList *)malloc(sizeof(DoublyLinkedList));
    Deque::tail = (DoublyLinkedList *)malloc(sizeof(DoublyLinkedList));
    DoublyLinkedList head_value;
    DoublyLinkedList tail_value;
    head_value.next = Deque::tail;
    tail_value.prev = Deque::head;
    *Deque::head = head_value;
    *Deque::tail = tail_value;
}

// Adds element to front of queue
void Deque::add_to_front(void *value) {
    DoublyLinkedList current;
    current.value = value;
    DoublyLinkedList *current_ptr = (DoublyLinkedList *)malloc(
        sizeof(DoublyLinkedList)); 
    DoublyLinkedList *current_first = (*Deque::head).next;
    (*Deque::head).next = current_ptr;
    current.prev = Deque::head;
    current.next = current_first;
    (*current_first).prev = current_ptr;
    *current_ptr = current;
    Deque::count++;
}

// Adds element to back of queue
void Deque::add_to_back(void *value) {
    DoublyLinkedList current;
    current.value = value;
    DoublyLinkedList *current_ptr = (DoublyLinkedList *)malloc(
        sizeof(DoublyLinkedList)); 
    DoublyLinkedList *current_last = (*Deque::tail).prev;
    (*Deque::tail).prev = current_ptr;
    current.next = Deque::tail;
    current.prev = current_last;
    (*current_last).next = current_ptr;
    *current_ptr = current;
    Deque::count++;
}

// Removes and retuns element at front of queue
void *Deque::pop_from_front() {
    assert(Deque::count > 0);
    DoublyLinkedList *current_ptr = (*Deque::head).next;
    DoublyLinkedList current = *current_ptr;
    free(current_ptr);
    void *value = current.value;
    (*(current.next)).prev = Deque::head;
    (*Deque::head).next = current.next;
    Deque::count--;
    return value;
}

// Removes and retuns element at back of queue
void *Deque::pop_from_back() {
    assert(Deque::count > 0);
    DoublyLinkedList *current_ptr = (*Deque::tail).prev;
    DoublyLinkedList current = *current_ptr;
    free(current_ptr);
    void *value = current.value;
    (*(current.prev)).next = Deque::tail;
    (*Deque::tail).prev = current.prev;
    Deque::count--;
    return value;
}

// Returns the front value without removing it
void *Deque::peak_front() {
    assert(Deque::count > 0);
    return (*((*(Deque::head)).next)).value;
}

// Returns the back value without removing it
void *Deque::peak_back() {
    assert(Deque::count > 0);
    return (*((*(Deque::tail)).prev)).value;
}

// Returns the ith value from the front of the queue
void *Deque::peak_ith(int i) {
    assert(0 <= i && i < Deque::count);
    DoublyLinkedList *current = (*Deque::head).next;
    for (int j = 0; j < i; j++) {
        current = (*current).next;
    }
    return (*current).value;
}

// Converts deque to a list
void **Deque::as_list() {
    void **result;
    if (Deque::count == 0) {
        return result;
    }
    result = (void **)malloc(sizeof(void *) * Deque::count);
    DoublyLinkedList *current = (*Deque::head).next;
    for (int i = 0; i < Deque::count; i++) {
        void *value = (*current).value;
        result[i] = value;
        current = (*current).next;
    }
    return result;
}

// Frees all data in the deque
void Deque::free_data() {
    while (Deque::count > 0) {
        void *value = pop_from_front();
        free(value);
    }
}

// Frees all data structures
void Deque::free_deque() {
    free_data();
    free(Deque::head);
    free(Deque::tail);
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

// Returns self as a list
void **Queue::get_list() {
    assert(Queue::count > 0);
    void **result = (void **)malloc(sizeof(void *) * Queue::count);
    unsigned int i = 0;
    LinkedList *current = Queue::head;
    while (true) {
        result[i] = (*current).value;
        i++;  
        if (current == Queue::tail) {
            break;
        }
        current = (*current).next;
    }
    assert(i == Queue::count);
    return result;
}

// Frees all data in the queue
void Queue::free_data(bool keep_values) {
    while (Queue::count > 0) {
        void *current = pop_from_front();
        if (!keep_values) {
            free(current);
        }
    }
}

// Gets first task from stack, frees pointer
Task get_task(Deque &task_stack) {
    void *task_ptr = task_stack.pop_from_front();
    Task task = (*((Task *)task_ptr));
    free(task_ptr);
    return task;
}

// Gets i-th task from stack stack, does not alter anything
Task peak_task(Deque task_stack, int i) {
    assert(0 <= i && i < task_stack.count);
    void *task_ptr = task_stack.peak_ith(i);
    Task task = (*((Task *)task_ptr));
    return task;
}

// Returns the opposite task of the given one as a copy 
Task opposite_task(Task task) {
    Task new_task;
    new_task.implier = task.implier;
    new_task.var_id = task.var_id;
    new_task.assignment = !task.assignment;
    new_task.is_backtrack = task.is_backtrack;
    return new_task;
}

// Returns whether the top of the stack says to backtrack
bool backtrack_at_top(Deque task_stack) {
    if (task_stack.count == 0) {
        return false;
    }
    void *task_ptr = task_stack.peak_back();
    Task task = (*((Task *)task_ptr));
    return (task.is_backtrack);
}

// Returns whether the front of the stack says to backtrack
bool backtrack_at_front(Deque task_stack) {
    if (task_stack.count == 0) {
        return false;
    }
    void *task_ptr = task_stack.peak_front();
    Task task = (*((Task *)task_ptr));
    return (task.is_backtrack);
}

// Returns whether the first task is one requiring a recurse() call
bool recurse_required(Deque task_stack) {
    if (task_stack.count == 0) {
        return false;
    }
    Task first_task = peak_task(task_stack);
    if (first_task.is_backtrack) {
        return false;
    }
    if (task_stack.count == 1) {
        return true;
    }
    Task second_task = peak_task(task_stack, 1);
    return (second_task.var_id == first_task.var_id);
}

// Default constructor
IntDeque::IntDeque() {
    IntDeque::count = 0;
    IntDeque::head = (IntDoublyLinkedList *)malloc(sizeof(IntDoublyLinkedList));
    IntDeque::tail = (IntDoublyLinkedList *)malloc(sizeof(IntDoublyLinkedList));
    IntDoublyLinkedList head_value;
    IntDoublyLinkedList tail_value;
    head_value.next = IntDeque::tail;
    tail_value.prev = IntDeque::head;
    *IntDeque::head = head_value;
    *IntDeque::tail = tail_value;
}

// Adds element to front of queue
void IntDeque::add_to_front(int value) {
    IntDoublyLinkedList current;
    current.value = value;
    IntDoublyLinkedList *current_ptr = (IntDoublyLinkedList *)malloc(
        sizeof(IntDoublyLinkedList)); 
    IntDoublyLinkedList *current_first = (*IntDeque::head).next;
    (*IntDeque::head).next = current_ptr;
    current.prev = IntDeque::head;
    current.next = current_first;
    (*current_first).prev = current_ptr;
    *current_ptr = current;
    IntDeque::count++;
}

// Adds element to back of queue
void IntDeque::add_to_back(int value) {
    IntDoublyLinkedList current;
    current.value = value;
    IntDoublyLinkedList *current_ptr = (IntDoublyLinkedList *)malloc(
        sizeof(IntDoublyLinkedList)); 
    IntDoublyLinkedList *current_last = (*IntDeque::tail).prev;
    (*IntDeque::tail).prev = current_ptr;
    current.next = IntDeque::tail;
    current.prev = current_last;
    (*current_last).next = current_ptr;
    *current_ptr = current;
    IntDeque::count++;
}

// Removes and retuns element at front of queue
int IntDeque::pop_from_front() {
    assert(IntDeque::count > 0);
    IntDoublyLinkedList *current_ptr = (*IntDeque::head).next;
    IntDoublyLinkedList current = *current_ptr;
    free(current_ptr);
    int value = current.value;
    (*(current.next)).prev = IntDeque::head;
    (*IntDeque::head).next = current.next;
    IntDeque::count--;
    return value;
}

// Removes and retuns element at back of queue
int IntDeque::pop_from_back() {
    assert(IntDeque::count > 0);
    IntDoublyLinkedList *current_ptr = (*IntDeque::tail).prev;
    IntDoublyLinkedList current = *current_ptr;
    free(current_ptr);
    int value = current.value;
    (*(current.prev)).next = IntDeque::tail;
    (*IntDeque::tail).prev = current.prev;
    IntDeque::count--;
    return value;
}

// Returns the front value without removing it
int IntDeque::peak_front() {
    assert(IntDeque::count > 0);
    return (*((*(IntDeque::head)).next)).value;
}

// Returns the back value without removing it
int IntDeque::peak_back() {
    assert(IntDeque::count > 0);
    return (*((*(IntDeque::tail)).prev)).value;
}

// Returns whether an integer value is contained
bool IntDeque::contains(int value) {
    if (IntDeque::count == 0) {
        return false;
    }
    IntDoublyLinkedList *current = (*IntDeque::head).next;
    while (current != IntDeque::tail) {
        if ((*current).value == value) {
            return true;
        }
        current = (*current).next;
    }
    return false;
}

// Removes and returns the min value
int IntDeque::pop_min() {
    assert(IntDeque::count > 0);
    IntDoublyLinkedList *current = (*IntDeque::head).next;
    IntDoublyLinkedList *min_ptr = current;
    int min_value = (*current).value;
    current = (*current).next;
    for (int i = 1; i < IntDeque::count; i++) {
        int value = (*current).value;
        if (value < min_value) {
            min_value = value;
            min_ptr = current;
        }
        current = (*current).next;
    }
    IntDoublyLinkedList *prev = (*current).prev;
    IntDoublyLinkedList *next = (*current).next;
    (*prev).next = next;
    (*next).prev = prev;
    IntDeque::count--;
    return min_value;
}

// Prints out current int deque
void IntDeque::print_deque(
        short pid,
        std::string prefix_string, 
        std::string depth_string) 
    {
    std::string data_string = std::to_string(IntDeque::count);
    data_string.append(" edit counts: (");
    int num_to_check = IntDeque::count;
    while (num_to_check > 0) {
        int value = pop_from_front();
        data_string.append(std::to_string(value));
        add_to_back(value);
        num_to_check--;
        if (num_to_check > 0) {
            data_string.append(" ");
        }
    }
    data_string.append(")");
    printf("%sPID %d %s %s\n", depth_string.c_str(), pid, 
        prefix_string.c_str(), data_string.c_str());
}

// Converts int deque to an int list
int *IntDeque::as_list() {
    int *result;
    if (IntDeque::count == 0) {
        return result;
    }
    result = (int *)malloc(sizeof(int) * IntDeque::count);
    IntDoublyLinkedList *current = (*IntDeque::head).next;
    for (int i = 0; i < IntDeque::count; i++) {
        int value = (*current).value;
        result[i] = value;
        current = (*current).next;
    }
    return result;
}

// Frees all data in the deque
void IntDeque::free_data() {
    while (IntDeque::count > 0) {
        pop_from_front();
    }
}

// Frees all data structures
void IntDeque::free_deque() {
    free_data();
    free(IntDeque::head);
    free(IntDeque::tail);
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