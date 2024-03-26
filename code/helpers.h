#ifndef HELPERS_H
#define HELPERS_H

#include <string>

// Prints current variable assignment
void print_assignment(
  int caller_pid,
  std::string prefix_string, 
  std::string tab_string,
  bool *assignment,
  int num_variables);

#endif