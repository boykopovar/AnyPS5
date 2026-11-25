# libSceNKWebKit

## add Functional

- [WTF::WorkerPool](../../../core/libs/prx/libSceNKWebKit/src/WorkerPool.cpp) (libSceNKWebKit) - the constructors, the destructors and `postTask`, not compared with a console module. They follow `WorkerPool.cpp` and `AutomaticThread.cpp` of the 11.x sources. The object has the size the 11.x headers give it, 104 bytes with the reference count at 0 and the name at offset 96, derived, not measured. The members between them are not used: a pointer to a private state sits at offset 8 and the other bytes are left untouched. Worker threads are kernel threads with default attributes and no name, started when a task finds no worker waiting. The timeout is ignored: an idle worker stays until the pool is destroyed, where the source lets it stop and start again. A `pthread_create` that fails throws `std::runtime_error` and leaves the task with the caller, where the source fails a release assertion; no test makes it fail. Closes when: compared with a console module.
- [WorkerPool with an empty task](../../../core/libs/prx/libSceNKWebKit/src/WorkerPool.cpp) (libSceNKWebKit) - `postTask` with an empty `WTF::Function` throws `std::invalid_argument`. In the source an empty task is the pool's own signal to stop a worker for good, and the next wake-up of the pool then fails a release assertion in `AutomaticThread::start`. A call on a destroyed pool also throws `std::invalid_argument`. Closes when: a console module is read for these cases.

## remove `_ZN3WTF10WorkerPool8postTaskEONS_8FunctionIFvvEEE`

## remove `_ZN3WTF10WorkerPoolC1ENS_12ASCIILiteralEjNS_7SecondsE`

## remove `_ZN3WTF10WorkerPoolC2ENS_12ASCIILiteralEjNS_7SecondsE`

## remove `_ZN3WTF10WorkerPoolD1Ev`

## remove `_ZN3WTF10WorkerPoolD2Ev`
