## [Project Final Report](writeups/final_report.pdf)

## [Project Proposal](writeups/project_proposal.pdf)

## [Project Milestone Report](writeups/project_milestone_report.pdf)

### Requirements:



### Compilation:
Navigate to the code file, and run 

```
make fast
```

The following is a sample command for running a given input file:

```
mpirun -n [number of processors] ./main -f [input file path]
```

Formatting of custom input killer/standard sudokus are as follows:
First line: [size] [sqrt(size)] [# of killer cages] [# of preassigned digits]
For each killer cage, add a new line consisting of: [cage sum] [cage size] [row1] [col1] [row2] ...
Then, for each preassigned digit, add a new line consisting of: [digit] [row] [col].

Extra arguments to main are:
- c: runfile [default] or runtests
- l: when running with runtests, can choose 1 of 44 different hard 16x16 regular sudokus
- f: input file found in the inputs/ subfolder
- b: branching factor for tree restricting process communication, defaults to 2.
- m: alternative assignment biases. 0 chooses greedily, 1 is the opposite of greedy, 2 always set to true, 3 always set to false
- r: reduction method. 0 is naive sudoku SAT reduction; 1 (default) is optimized

The actual test code which was run on PSC is located in actual_script.job.