#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

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
    bool other;
};

extern "C" ErrorReport* CurrentErrorReport_nid_no_patch() noexcept;
extern "C" [[noreturn]] void Die_nid_no_patch();
[[noreturn]] void ReportFatal_nid_no_patch(const ErrorReport& state, const std::string& text);
void EmitDiagnostic_nid_no_patch(const std::string& text);

using ErrorReportCallback = void (APS5_VABI*)(const char*);
extern "C" ErrorReportCallback LoadErrorReportCallback_nid_no_patch() noexcept;

}  // namespace AsanRuntime
