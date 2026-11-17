# libSceAjm.native

## add Functional

- [sceAjmBatchJobControlBufferRa, sceAjmBatchJobRunBufferRa, sceAjmBatchJobRunSplitBufferRa and sceAjmBatchJobInlineBuffer guest memory](../../../core/libs/prx/libSceAjm.native/src/Ajm.cpp) (libSceAjm.native) - throwing `std::runtime_error` with caller errno preserved when the batch position, a buffer list, the inline input or the inline address output cannot be copied through `GuestCopyIn_nid_no_patch` or `GuestCopyOut_nid_no_patch` is a host-safety fallback, as in libSceAjmi's builders; whether the console faults on unmapped memory is unmeasured. A control or run job is written in one copy after its buffer lists and old job bytes are read, so a failure leaves the batch as it was; an inline chunk keeps the console routine's order, so an unreadable input leaves the new header written.
