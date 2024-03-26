#ifndef SOLVER_H
#define SOLVER_H

// Used to convert input to CNF

struct InputFormula {
    char formula_type; // 'l', 'n', 'a', or 'o'
    void *formula_struct;
};

struct InputLiteral {
    int variable_id;
};

struct InputNot {
    InputFormula sub_formula;
};

struct InputAnd {
    int num_sub_formulas;
    InputFormula *sub_formulas;
};

struct InputOr {
    int num_sub_formulas;
    InputFormula *sub_formulas;
};

// Makes an input literal from a variable id
InputFormula make_input_literal(int variable_id);

// Makes an input not from a sub formula
InputFormula make_input_not(InputFormula sub_formula);

// Makes an input and of given more than two sub formulas
InputFormula make_input_and_many(int num_sub_formulas, InputFormula *sub_formulas);

// Makes an input or of given more than two sub formulas
InputFormula make_input_or_many(int num_sub_formulas, InputFormula *sub_formulas);

// Makes an input and of two given sub formulas
InputFormula make_input_and(
    InputFormula left_formula, 
    InputFormula right_formula);

// Makes an input and of two given sub formulas
InputFormula make_input_or(
    InputFormula left_formula, 
    InputFormula right_formula);

// Used to solve CNF

struct Literal {
    bool sign; // if is true or false
    char status;
    // 's' if satisfied (constant)
    // 'u' if unsatisfied (constant)
    // 'o' otherwise
};

// Will have fixed allocation size
struct Clause {
    int *literal_variable_ids; // variable ids for each literal
    Literal *literals; // each literal
    int num_literals; // size of the two pointers
    char clause_value; // 's' for satisfied, 'u' for unsatisfied, 'o' for other
};

struct LinkedList {
    void *value;
    LinkedList *next;
};

struct VariableLocations {
    int variable_id; // id of variable
    int num_clauses_containing; // number of clauses that contain the variable
    int num_literals_containing; // number of literals that contain the variable
    int padding;
    // Will have dynamic allocation size
    LinkedList *clauses_containing; // LL saving pointers to clause structures 
    LinkedList *literals_containing; // LL saving pointers to literal structures 
};

struct Cnf {
    int num_clauses;
    int num_variables;
    LinkedList *clauses; // dynamic number
    VariableLocations *variables; // static number
};

// Returns whether an input formula is in CNF
bool is_in_cnf(InputFormula formula);

// Converts input formula to CNF
void to_cnf(InputFormula formula);

// Converts an input formula in CNF to the Cnf type used by the solver.
Cnf convert_to_cnf_type(InputFormula formula);

// Recursively frees the input formula data structure
void free_input_formula(InputFormula formula);

// Recursively frees the Cnf formula data structure
void free_cnf_formula(Cnf formula);

class Solver {
    public:
        Cnf cnf_formula;

        // Initialized with input formula
        Solver(Cnf formula);

        // Solver, returns true and populates assignment if satisfiable
        bool solve(bool *assignment);

        // Sets variable's value in cnf formula to given value 
        void assign_variable_value(int variable_id, bool value);

        // Effectively undoes assign_variable_value, useful for recursive
        // backtracking
        void reset_variable_value(int variable_id);

        // Gets the truth value of a clause given variable assignments
        bool evaluate_clause(bool *assignment);

        // Gets the truth value of a cnf given variable assignments
        bool evaluate_cnf(bool *assignment);
};

#endif