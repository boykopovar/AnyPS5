# libSceNKWebKit

## add Functional

- [WTF::CrossThreadTaskHandler](../../../core/libs/prx/libSceNKWebKit/src/CrossThreadTaskHandler.cpp) (libSceNKWebKit) - the constructors, the destructors, `postTask`, `postTaskReply` and `kill`, not compared with a console module; `setCompletionCallback` is still a stub. They follow `CrossThreadTaskHandler.cpp` and `CrossThreadQueue.h` of the 11.x sources, which differ from the 2020 ones: a killed queue stops waiting, and an empty task ends the task thread. The object has the size the 11.x headers give it, 120 bytes with the completion callback at offset 112, derived, not measured. The two locks, the flag and the two queues before the callback are not used: a pointer to a private state sits at offset 8 and the other bytes are left untouched. The task thread is a detached kernel thread with default attributes and no name, and the `AutodrainedPoolForRunLoop` argument has no effect. Closes when: compared with a console module.
- [CrossThreadTaskHandler where the source is undefined](../../../core/libs/prx/libSceNKWebKit/src/CrossThreadTaskHandler.cpp) (libSceNKWebKit) - a handler destroyed while its task thread still runs lets the thread finish its current task and stop without calling the completion callback (in the source the thread goes on reading the freed object), `postTaskReply` with an empty `WTF::Function` throws `std::invalid_argument` (the source calls through the null pointer on the main thread), and a call on a destroyed handler throws `std::invalid_argument`. Closes when: a console module is read for these cases.

## remove `_ZN3WTF22CrossThreadTaskHandler13postTaskReplyEONS_15CrossThreadTaskE`

## remove `_ZN3WTF22CrossThreadTaskHandler4killEv`

## remove `_ZN3WTF22CrossThreadTaskHandler8postTaskEONS_15CrossThreadTaskE`

## remove `_ZN3WTF22CrossThreadTaskHandlerC1EPKcNS0_25AutodrainedPoolForRunLoopE`

## remove `_ZN3WTF22CrossThreadTaskHandlerC2EPKcNS0_25AutodrainedPoolForRunLoopE`

## remove `_ZN3WTF22CrossThreadTaskHandlerD0Ev`

## remove `_ZN3WTF22CrossThreadTaskHandlerD1Ev`

## remove `_ZN3WTF22CrossThreadTaskHandlerD2Ev`
