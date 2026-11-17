# libScePosixForWebKit

## add Functional

- [getenv_np guest output](../../../core/libs/prx/libScePosixForWebKit/Export.cpp) (libScePosixForWebKit) - an unwritable output previously faulted the host; `GuestCopyOut_nid_no_patch` now rejects it with `std::invalid_argument` while preserving host `errno`. This is a host-safety fallback; the console's result for an invalid output pointer is unmeasured.
