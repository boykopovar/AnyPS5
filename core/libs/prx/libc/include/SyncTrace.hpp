#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_SYNCTRACE_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_SYNCTRACE_HPP

#include <cstdint>

enum SyncTraceOperation : unsigned {
    SyncTraceCreate = 1,
    SyncTraceSignal = 2,
    SyncTraceWaitBegin = 3,
    SyncTraceWaitEnd = 4,
    SyncTracePoll = 5,
    SyncTraceCancel = 6,
    SyncTraceDelete = 7,
};

extern "C" {

unsigned SyncTraceRingMode_nid_no_patch() noexcept;
void SyncTraceRecord_nid_no_patch(unsigned operation, const void* object, const char* name, std::uint64_t value, std::uint64_t result) noexcept;
void SyncTraceReportRing_nid_no_patch() noexcept;

}

#endif
