# Parallel, Conflict-Driven DPLL SAT solver for Killer Sudoku
Liam Gersten | Alexander Wang | [GitHub](https://github.com/liam-gersten/parallel-sat-solver)

## SUMMARY

We plan to solve Killer Sudoku using the famous Boolean Satisfiability problem in parallel. Our parallel SAT solver implementation will build on DPLL, and will make use of conflict clauses.

## BACKGROUND

<img align="right" width="200" height="200" src="https://i.guim.co.uk/img/media/66ffd3837165af9eb9c6a441566c79021685857d/0_0_860_860/master/860.jpg?width=465&dpr=1&s=none">

To start, a Killer Sudoku instance must be reduced to a conjunctive normal form SAT instance, which can be sequential or parallel if it is computationally intensive. From there, all threads cooperate with their private address spaces and message passing to run the DPLL algorithm with conflict resolution on the CNF formula. This problem stands to benefit from parallelism due to the massive search space, and even larger clause number required. 

In Killer Sudoku, an n * n board (n is a square) will need 9n^2 variables if we represent one variable for each of the nine values a single cell can take. There must be pairwise restrictions for these variables in the same rows, columns, chunks, and chunks with explicit summations. The number of clauses from such restrictions exceeds 7n^3. With integer representations, we start to run into overflow for n > 676. 

## CHALLENGE

Parallel or not, this problem can’t be brute-forced. For just n = 9 as seen in the figure above, we have a search space of 2^729 variable combinations, far exceeding the particles in the known universe. With DPLL, we can prune this search space quite a bit. With DPLL, we recursively divide the search space, and make shortcuts along the way. The algorithm works over an cuts out parts of a formula which will need to be held in memory. The issue arises when one thread wants to try assigning variable x = True, and another tries x = False. These will entail different updates to the same memory in the formula. For this reason, it makes the most sense to give threads a private address space and use the message passing model. 

One challenge with using private address spaces is that different values for the same variable may lead to drastically smaller or larger call stacks, so we have an inherent load balancing issue. We must then implement some form of work stealing, which is tricky with message passing, since a thread must grab the entire problem context (formula + assignments) at some stage in the tree. 

Creating and sending conflict clauses requires both backtracking up the tree, and having a linear history of decisions and unit propagations made down the call stack. It is unclear how a thread is supposed to backtrack out of stolen work to the correct call in the original thread. It’s also possible our program could benefit from telling other threads of a new conflict cause so they may avoid the same mistake made, although it is unclear if this should be anything other than a broadcast of a conflict clause.
We’ll need to have threads periodically check for conflict clause messages, as well as ones that instruct the program to halt if a solution is found.

## RESOURCES

As we will be using a message passing model with private address spaces, we would like to use the same compute resources available to us for assignment 4. While not strictly necessary, it would be helpful to have access to the PSC machines towards the end of our development cycle.
Code-wise, we will be starting only with two header files created during assignment 4, those being the unimplemented state and interconnect classes. These act as abstract ways for threads to communicate and update their memory. 
Our understanding of the DPLL and conflict-driven SAT solvers is based largely on this 15-414 [lecture](https://www.cs.cmu.edu/~15414/lectures/16-satdpll.pdf), although it does not explicitly mention parallelism in the approaches.

## GOALS AND DELIVERABLES

## PLATFORM CHOICE

The DPLL algorithm is based on reducing the search space by taking shortcuts. It does this by erasing (making constant) entire literals or clauses so they don’t need to be looked at again. Threads will need to be able to independently update the formula with their choices without being subject to the choices other threads are making. Considering almost no memory is shared between threads at all, separate address spaces make the most sense here. The GHC machines paired with C++ and MPI will work perfectly fine for our purposes, although our project could benefit from limited access to the PSC machines due to the problem size constraints.

## SCHEDULE