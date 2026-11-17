# libSceAgcDriver

## add Functional

- [Indirect buffers the write tracker cannot see](../../../core/libs/prx/libSceAgcDriver/Execution/src/Driver/Queues/Submission.cpp) (libSceAgcDriver) - an INDIRECT_BUFFER or COND_INDIRECT_BUFFER that the write tracker does not cover is read again and compared with its submit-time copy each time a submission reaches it. That is the case with write watching off (`APS5_NO_WRITE_WATCH=1`), for a range outside the write watch, and for the test block in `agc_driver_gpu_written_indirect_buffer` on Windows. A CPU rewrite of such a buffer after its submission then runs as memory holds it, because the driver cannot tell it from a GPU write; inside the write watch it runs as copied. A buffer copied while a recorded GPU write over it was pending is also read again at execution, which in MXGP 26 (PPSA30839) is one compare of its 6740-dword frame buffer per submission.
