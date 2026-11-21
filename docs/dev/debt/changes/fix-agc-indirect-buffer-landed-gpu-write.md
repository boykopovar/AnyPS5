# libSceAgcDriver

## add Functional

- [Indirect buffers the write tracker cannot see](../../../core/libs/prx/libSceAgcDriver/Execution/src/Driver/Queues/Submission.cpp) (libSceAgcDriver) - an INDIRECT_BUFFER or COND_INDIRECT_BUFFER outside the write tracker (write watching off, a range outside the watch, the `agc_driver_gpu_written_indirect_buffer` block on Windows) is read again and compared with its submit-time copy at each execution, so a CPU rewrite of it runs as memory holds it; inside the watch it runs as copied. A buffer copied while a recorded GPU write over it was pending is also read again, one compare of its 6740 dwords per submission in MXGP 26 (PPSA30839). The generation and pending-write check are taken before the copy. A store that changes the buffer between the copy and the check that the copy matches memory leaves it untracked, and it runs as copied.
