# libSceNKWebKit

## add Functional

- [WTF::ParallelEnvironment](../../../core/libs/prx/libSceNKWebKit/src/ParallelEnvironment.cpp) (libSceNKWebKit) - the constructors and `execute`, not compared with a console module. They follow `ParallelJobsGeneric.cpp` of the 11.x sources. The object has the layout of the 11.x header (function 0, parameter size 8, number of jobs 16, the `Vector` of workers 24, 40 bytes) because the caller reads the number of jobs and destroys the vector inline: the vector's buffer comes from `fastMalloc` and each worker has its reference count at offset 0. Derived, not measured. Closes when: compared with a console module.
- [ParallelEnvironment quirks kept from the source](../../../core/libs/prx/libSceNKWebKit/src/ParallelEnvironment.cpp) (libSceNKWebKit) - a worker stays reserved by an environment until it has run one job for it, so an environment that never executes keeps its workers for the life of the process; a negative requested number takes one worker per processor core. Two differences: the pool of workers is walked under a lock, where the source walks it unprotected, and `execute` on an environment without a function throws `std::invalid_argument` where the source calls through the null pointer. Closes when: a console module is read for these cases.

## remove `_ZN3WTF19ParallelEnvironment7executeEPv`

## remove `_ZN3WTF19ParallelEnvironmentC1EPFvPvEmi`

## remove `_ZN3WTF19ParallelEnvironmentC2EPFvPvEmi`
