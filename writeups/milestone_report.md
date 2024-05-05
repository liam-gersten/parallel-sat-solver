# Project Milestone Report
Liam Gersten | Alexander Wang | [GitHub](https://github.com/liam-gersten/parallel-sat-solver)

As of Sunday, April 14th, we have committed no less than 80 hours over the past 3 weeks. We've developed a working parallel algorithm that is modular, easy to debug, free of race conditions, highly memory efficient, and fast for $n \leq 16$.

Bar any serious difficulties, this milestone falls after the estimated halfway point of our project's goals. The only missing tasks not yet completed that we specified in the proposal schedule are conflict learning, and Sudoku-based optimizations, both of which are partially finished already.

Much of the work done surrounded eliminating the many race conditions associated with the work-stealing version of the algorithm. Just figuring out how to terminate the algorithm alone and debugging this solution occupied days of development. Despite this, we have a "fast" solution with poor speedup. Our solution also stalls indefinitely for $n \geq 25$, which will likely be solved via conflict resolution.

As of now, are largest concerns are whether we can get $n \geq 25$ running in reasonable time, and whether good speedup is even attainable. The primary problem with speedup is that $P_0$ tends to make the most optimal choices and is usually good heuristic wise. For this reason, $P_0$ will almost always find a valid solution before the other threads do, effectively ruining program speedup.

While we don't plan on showing a demo of the project during our poster session, we will be walking through the project's ideation, evolving scope, and debugging in detail. Speedup and other various performance graphs will be included.

## Completed Todo List

| Task    | Difficulty | Priority |
| -------- | ------- | ------ |
| Basic Parallel Version | Very Hard | Highest |
| Work Stealing | Very Hard | Very High |
| Work Stealing Optimizations | Very Hard | High |
| Better Unit Propagation | Easy | Moderate | 
| Pure Literal Elimination | Easy | Moderate |

## Remaining Todo List

| Task    | Difficulty | Priority |
| -------- | ------- | ------ |
| Conflict Clauses + Resolution | Very Hard | High |
| Better SAT reduction | Easy | Low |
| Killer Variant of Sudoku | Easy | Moderate |
| Free methods | Medium | High |