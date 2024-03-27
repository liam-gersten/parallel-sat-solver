#include "helpers.h"
#include "solver.h"
#include <string>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <cassert>
#include <algorithm>
#include <unistd.h>


void test_solve_other(Cnf cnf) {
    printf("Depth 1 size = %d\n", cnf.clauses.count);
    return;

}


void test_solve_rec(Cnf &cnf, int depth) {
    if (depth == 2) {
        printf("\tDepth 2 size before = %d\n", cnf.clauses.count);
        while (cnf.clauses.count > 9000) {
            cnf.clauses.pop_from_front();
        }
        printf("\tDepth 2 size after = %d\n", cnf.clauses.count);
        return;
    } else {
        printf("Depth 0 size before = %d\n", cnf.clauses.count);
        test_solve_other(cnf);
        test_solve_rec(cnf, 2);
        test_solve_other(cnf);
        printf("Depth 0 size after = %d\n", cnf.clauses.count);
        return;
    }
}


int main(int argc, char *argv[]) {
    std::string input_filename;

    // Read command line arguments
    int opt;
    while ((opt = getopt(argc, argv, "f:")) != -1) {
        switch (opt) {
        case 'f':
            input_filename = optarg;
            break;
        default:
            std::cerr << "Usage: " << argv[0] << " -f input_filename\n";      
            exit(EXIT_FAILURE);
        }
    }
    printf("filename = %s\n", input_filename.c_str());
    
    int n;
    int sqrt_n;
    int num_constraints;
    int **constraints = read_puzzle_file(
        input_filename, &n, &sqrt_n, &num_constraints);

    printf("n = %d, sqrt_n = %d, num_constraints = %d\n", n, sqrt_n, num_constraints);
    
    Cnf cnf = make_cnf(constraints, n, sqrt_n, num_constraints);

    print_cnf(0, "CNF", "", cnf);

    test_solve_rec(cnf, 0);
    // Solver solver(solver_formula);
    // bool *assignment = (bool *)calloc(sizeof(bool), solver_formula.num_variables);
    // bool is_satisfiable = solver.solve(assignment);
    // if (is_satisfiable) {
    //     print_assignment(0, "satisfying assignment:", "", assignment, 
    //     solver_formula.num_variables);
    // } else {
    //     printf("Not satisfyable\n");
    // }
}