#include "prx/libSceAddressSanitizer/src/Coverage.hpp"
#include "prx/libSceAddressSanitizer/src/ModuleLookup.hpp"
#include "prx/libSceAddressSanitizer/src/ReportFile.hpp"
#include "prx/libSceAddressSanitizer/src/ShadowCheck.hpp"
#include "prx/libc/include/Api.hpp"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace AsanRuntime::Coverage {

namespace {

constexpr std::uint64_t kMagic64 = 0xC0BFFFFFFFFFFF64ULL;
constexpr std::size_t kPageEntries = 1u << 16;
constexpr std::size_t kMaxPages = 1u << 12;
constexpr bool kCoverageEnabled = false;
constexpr const char* kCoverageDirectory = ".";

struct State {
    std::mutex mutex;
    bool initialized = false;
    std::atomic<std::uint32_t> size{0};
    std::atomic<std::atomic<std::uintptr_t>*> pages[kMaxPages] = {};
};

State& GetState() {
    static State state;
    return state;
}

void WriteModuleCoverage(const std::string& module, const std::uintptr_t* pcs, std::size_t length) {
    const std::string path = std::string(kCoverageDirectory) + "/" + StripModuleName_nid_no_patch(module.c_str()) + "." + std::to_string(HostProcessId_nid_no_patch()) + ".sancov";
    errno = 0;
    FileStream* file = fopen_nid_postfix(path.c_str(), "wb");
    if (!file) {
        ReportFile::Write_nid_no_patch("SanitizerCoverage: failed to open " + path + " for writing (reason: " + std::to_string(errno) + ")\n");
        return;
    }
    fwrite_nid_postfix(&kMagic64, sizeof(kMagic64), 1, file);
    fwrite_nid_postfix(pcs, sizeof(*pcs), length, file);
    fclose_nid_postfix(file);
    ReportFile::Write_nid_no_patch("SanitizerCoverage: " + path + ": " + std::to_string(length) + " PCs written\n");
}

}  // namespace

void InitTracePcGuard_nid_no_patch(std::uint32_t* start, std::uint32_t* end) {
    if (start == end || *start) {
        return;
    }
    State& state = GetState();
    std::lock_guard lock(state.mutex);
    state.initialized = true;
    std::uint32_t index = state.size.load(std::memory_order_relaxed);
    const std::size_t count = static_cast<std::size_t>(end - start);
    if (end < start || count > kMaxPages * kPageEntries - index) {
        InvalidArgument_nid_no_patch(__func__);
    }
    const std::size_t lastPage = (index + count - 1) / kPageEntries;
    for (std::size_t page = index / kPageEntries; page <= lastPage; ++page) {
        if (!state.pages[page].load(std::memory_order_relaxed)) {
            state.pages[page].store(new std::atomic<std::uintptr_t>[kPageEntries](), std::memory_order_release);
        }
    }
    for (std::uint32_t* p = start; p < end; ++p) {
        *p = ++index;
    }
    state.size.store(index, std::memory_order_release);
}

void TracePcGuard_nid_no_patch(std::uint32_t* guard, std::uintptr_t pc) {
    const std::uint32_t index = *guard;
    if (!index) {
        return;
    }
    State& state = GetState();
    if (index > state.size.load(std::memory_order_acquire)) {
        InvalidArgument_nid_no_patch(__func__);
    }
    std::atomic<std::uintptr_t>& entry = state.pages[(index - 1) / kPageEntries].load(std::memory_order_acquire)[(index - 1) % kPageEntries];
    if (entry.load(std::memory_order_relaxed) == 0) {
        entry.store(pc, std::memory_order_relaxed);
    }
}

void DumpTracePcGuard_nid_no_patch() {
    State& state = GetState();
    if (!state.initialized || !kCoverageEnabled) {
        return;
    }
    const std::uint32_t size = state.size.load(std::memory_order_acquire);
    std::vector<std::uintptr_t> pcs(size);
    for (std::uint32_t i = 0; i < size; ++i) {
        pcs[i] = state.pages[i / kPageEntries].load(std::memory_order_acquire)[i % kPageEntries].load(std::memory_order_relaxed);
    }
    DumpCoverage_nid_no_patch(pcs.data(), pcs.size());
}

void DumpCoverage_nid_no_patch(const std::uintptr_t* unsortedPcs, std::uintptr_t length) {
    if (!length) {
        return;
    }
    std::vector<std::uintptr_t> pcs(unsortedPcs, unsortedPcs + length);
    std::sort(pcs.begin(), pcs.end());
    bool moduleFound = false;
    std::uintptr_t lastBase = 0;
    std::size_t moduleStart = 0;
    std::string moduleName;
    for (std::size_t i = 0; i < pcs.size(); ++i) {
        const std::uintptr_t pc = pcs[i];
        if (!pc) {
            continue;
        }
        std::string name;
        std::uintptr_t offset = 0;
        if (!ModuleForAddress_nid_no_patch(pc, name, offset)) {
            char buffer[96];
            std::snprintf(buffer, sizeof(buffer), "ERROR: unknown pc 0x%zx (may happen if dlclose is used)\n", static_cast<std::size_t>(pc));
            ReportFile::Write_nid_no_patch(buffer);
            continue;
        }
        pcs[i] = offset;
        const std::uintptr_t base = pc - offset;
        if (base != lastBase || !moduleFound) {
            if (moduleFound) {
                WriteModuleCoverage(moduleName, &pcs[moduleStart], i - moduleStart);
            }
            lastBase = base;
            moduleStart = i;
            moduleFound = true;
            moduleName = name;
        }
    }
    if (moduleFound) {
        WriteModuleCoverage(moduleName, &pcs[moduleStart], pcs.size() - moduleStart);
    }
}

}  // namespace AsanRuntime::Coverage
