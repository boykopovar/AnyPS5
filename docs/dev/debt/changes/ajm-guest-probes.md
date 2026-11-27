# libSceAjm.native

## add Functional

- [RegisterBatch, sceAjmBatchStart, sceAjmBatchWait, sceAjmBatchStartBuffer](../../../core/libs/prx/libSceAjm.native/src/Ajm.cpp) (libSceAjm.native) - console behavior and error precedence for unreadable batch buffers, unwritable batch IDs or error outputs, and failing batch execution remain unmeasured. AnyPS5 copies batch descriptors, jobs, console chunks, and outputs through `CopyBatchGuestIn` and `CopyBatchGuestOut`, stages jobs and registers outputs before execution with an RAII registration guard that erases the batch ID if execution throws, and validates the error output before erasing batches in `sceAjmBatchWait`.
