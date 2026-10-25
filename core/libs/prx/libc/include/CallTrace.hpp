#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_CALLTRACE_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_CALLTRACE_HPP

#include <cstddef>
#include <cstdint>

struct CallTraceEntry {
    std::uint64_t sequence;
    std::uint64_t thread;
    std::uint64_t returnAddress;
    const char* name;
    const char* library;
};

struct CallTraceCount {
    std::uint64_t calls;
    const char* name;
    const char* library;
};

extern "C" {
void* TraceImport_nid_no_patch(void* target, const char* name, const char* library);
std::size_t CopyCallTrace_nid_no_patch(CallTraceEntry* entries, std::size_t capacity);
std::size_t CopyThreadCallTrace_nid_no_patch(std::uint64_t thread, CallTraceEntry* entries, std::size_t capacity);
std::size_t CopyCallCounts_nid_no_patch(CallTraceCount* entries, std::size_t capacity, std::uint64_t* total);
}

#endif
