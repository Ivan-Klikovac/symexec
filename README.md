# SYMEXEC
A symbolic execution engine that operates on GCC's SSA GIMPLE. It includes an in-house constraint solver `cos` instead of offloading the work to an external SMT solver.

The project is active, but still early in development.

## Worth reading:
- https://madhu.cs.illinois.edu/cs598-fall10/king76symbolicexecution.pdf
- https://www.doc.ic.ac.uk/~cristic/papers/klee-osdi-08.pdf

## Installation
Due to the current volatility of the project, I have decided not to use a build system just yet. The code must be compiled with the `compile` script.

## The constraint solver
`cos` uses a range-based representation based on GCC's internal structures to minimize conversion overhead.

Currently it uses an AST-like structure internally. It is unclear if that will remain the case.

## The engine
`symexec` uses a DNF-like internal representation of path conditions (called "states" in the codebase).

DNF was chosen due to its relatively simple provability - proving that a predicate holds for any of the inner conjunctions proves that the predicate holds for the whole expression.

Additionally, adding a new constraint to a state is as simple as adding a new inner conjunction, and merging two states that go on a common code path involves merely adding another disjunction to the expression.

Each basic block is associated with a state which describes the symbolic values of all variables in it. This, aside from being common-sensical, allows for efficient "hot reloads" of the engine, where it only has to reevaluate code that changed between runs, given the output and intermediate data from an existing run.
