#pragma once
#include <cstddef>
#include <cstdint>

namespace AsanRuntime {

struct ErrorReport {
    bool generic;
    std::uintptr_t pc;
    std::uintptr_t bp;
    std::uintptr_t sp;
    std::uintptr_t address;
    bool isWrite;
    std::size_t accessSize;
    const char* description;
};

extern "C" ErrorReport* CurrentErrorReport_nid_no_patch() noexcept;

using ErrorReportCallback = void (APS5_VABI*)(const char*);
extern "C" ErrorReportCallback LoadErrorReportCallback_nid_no_patch() noexcept;

}  // namespace AsanRuntime
