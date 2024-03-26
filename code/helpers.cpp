#include "helpers.h"
#include <string>

//----------------------------------------------------------------
// BEGIN IMPLEMENTATION
//----------------------------------------------------------------

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

//----------------------------------------------------------------
// END IMPLEMENTATION
//----------------------------------------------------------------