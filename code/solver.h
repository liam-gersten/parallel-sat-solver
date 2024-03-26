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

class Solver {
    public:
        int num_variables;

        // Initialized with input formula
        Solver(InputFormula formula);

        // Converts input formula to CNF
        Cnf to_cnf(InputFormula formula);

        // Solver, returns true and populates valuation if satisfiable
        bool solve_cnf_formula(Cnf cnf_formula, int *valuation);

        // Sets variable's value in cnf formula to given value 
        void assign_variable_value(
            Cnf cnf_formula, 
            int variable_id, 
            bool value);

        // Effectively undoes assign_variable_value, useful for recursive
        // backtracking
        void reset_variable_value(Cnf cnf_formula, int variable_id);

        // Gets the truth value of a clause given variable assignments
        bool evaluate_clause(Clause clause, bool *valuation);

        // Gets the truth value of a cnf given variable assignments
        bool evaluate_cnf(Cnf cnf_formula, bool *valuation);
};

#endif