#include "solver.h"
#include <string>

int main(int argc, char *argv[]) {
    std::string string_formula = "(1 /\ (!(2 \/ (!3))))";
    // Parse

    InputFormula parsed_formula = 
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


}