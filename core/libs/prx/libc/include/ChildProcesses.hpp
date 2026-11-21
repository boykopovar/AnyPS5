#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_CHILDPROCESSES_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_CHILDPROCESSES_HPP

#ifdef _WIN32
#include <cstdint>

struct ChildProcessTimes {
    std::uint64_t user;
    std::uint64_t kernel;
};

extern "C" void RegisterChildProcess_nid_no_patch(std::int32_t processId, std::int64_t processHandle);
extern "C" std::int64_t WaitChildProcess_nid_no_patch(std::int32_t processId, unsigned options, int* status, ChildProcessTimes* times = nullptr);
#endif

#endif
