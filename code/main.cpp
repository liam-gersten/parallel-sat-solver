#include "helpers.h"
#include "solver.h"
#include <string>
#include <cassert>

int main(int argc, char *argv[]) {
    std::string string_formula = "(1 /\ (!(2 \/ (!3))))";
    // Parse
    InputFormula formula = 
        make_input_and(
            make_input_literal(1),
            make_input_not(
                make_input_or(
                    make_input_literal(2),
                    make_input_not(
                        make_input_literal(3)
                    )
                )
            )
        );
    to_cnf(formula);
    assert(is_in_cnf(formula));
    Cnf solver_formula = convert_to_cnf_type(formula);
    free_input_formula(formula);
    Solver solver(solver_formula);
    bool *assignment = (bool *)calloc(sizeof(bool), solver_formula.num_variables);
    bool is_satisfiable = solver.solve(assignment);
    if (is_satisfiable) {
        print_assignment(0, "satisfying assignment:", "", assignment, 
        solver_formula.num_variables);
    } else {
        printf("Not satisfyable\n");
    }
}