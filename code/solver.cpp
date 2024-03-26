#include "solver.h"
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>

//----------------------------------------------------------------
// BEGIN IMPLEMENTATION
//----------------------------------------------------------------

// Makes an input literal from a variable id
InputFormula make_input_literal(int variable_id) {
    InputLiteral current;
    current.variable_id = variable_id;
    InputLiteral *result_ptr = (InputLiteral *)malloc(sizeof(InputLiteral));
    *result_ptr = current;
    InputFormula result;
    result.formula_type = 'l';
    result.formula_struct = (void *)result_ptr;
    return result;
}

// Makes an input not from a sub formula
InputFormula make_input_not(InputFormula sub_formula) {
    InputNot current;
    current.sub_formula = sub_formula;
    InputNot *result_ptr = (InputNot *)malloc(sizeof(InputNot));
    *result_ptr = current;
    InputFormula result;
    result.formula_type = 'n';
    result.formula_struct = (void *)result_ptr;
    return result;
}

// Makes an input and of given more than two sub formulas
InputFormula make_input_and_many(
        int num_sub_formulas, 
        InputFormula *sub_formulas) 
    {
    InputAnd current;
    current.num_sub_formulas = num_sub_formulas;
    current.sub_formulas = sub_formulas;
    InputAnd *result_ptr = (InputAnd *)malloc(sizeof(InputAnd));
    *result_ptr = current;
    InputFormula result;
    result.formula_type = 'a';
    result.formula_struct = (void *)result_ptr;
    return result;
}

// Makes an input or of given more than two sub formulas
InputFormula make_input_or_many(
        int num_sub_formulas, 
        InputFormula *sub_formulas) 
    {
    InputOr current;
    current.num_sub_formulas = num_sub_formulas;
    current.sub_formulas = sub_formulas;
    InputOr *result_ptr = (InputOr *)malloc(sizeof(InputOr));
    *result_ptr = current;
    InputFormula result;
    result.formula_type = 'o';
    result.formula_struct = (void *)result_ptr;
    return result;
}

// Makes an input and of two given sub formulas
InputFormula make_input_and(
        InputFormula left_formula, 
        InputFormula right_formula) 
    {
    InputFormula *sub_formulas = (InputFormula *)calloc(sizeof(InputFormula), 2);
    sub_formulas[0] = left_formula;
    sub_formulas[1] = right_formula;
    return make_input_and_many(2, sub_formulas);
}

// Makes an input and of two given sub formulas
InputFormula make_input_or(
        InputFormula left_formula, 
        InputFormula right_formula)
    {
    InputFormula *sub_formulas = (InputFormula *)calloc(sizeof(InputFormula), 2);
    sub_formulas[0] = left_formula;
    sub_formulas[1] = right_formula;
    return make_input_or_many(2, sub_formulas);
}

//----------------------------------------------------------------
// END IMPLEMENTATION
//----------------------------------------------------------------