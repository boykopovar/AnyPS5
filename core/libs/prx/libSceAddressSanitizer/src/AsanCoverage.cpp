/* Derived from LLVM compiler-rt lib/sanitizer_common (sanitizer_coverage_libcdep_new.cpp) at llvmorg-16.0.6 (af2ed9f794c422923414d162dd1f48d986e2d6e3) */
/* Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception */
#include "prx/libSceAddressSanitizer/src/AsanRuntime.hpp"
#include "prx/libSceAddressSanitizer/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <filesystem>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

namespace Asan::Runtime {

namespace {

constexpr std::uint64_t Magic64 = 0xC0BFFFFFFFFFFF64ull;

struct GuardController {
    std::shared_mutex mutex;
    bool initialized = false;
    std::deque<std::atomic<uptr>> pcs;
};

GuardController& Controller() {
    static GuardController controller;
    return controller;
}

int Pid() {
#if defined(_WIN32)
    return _getpid();
#else
    return static_cast<int>(getpid());
#endif
}

std::string ModuleBaseName(const std::string& module) {
    const std::size_t slash = module.find_last_of("/\\");
    return slash == std::string::npos ? module : module.substr(slash + 1);
}

void WriteModuleCoverage(const std::string& module, const std::vector<uptr>& offsets) {
    const std::string& directory = GetFlags().coverageDir;
    const std::string guestPath = directory + "/" + ModuleBaseName(module) + "." + std::to_string(Pid()) + ".sancov";
    const std::filesystem::path hostPath = ResolvePath_nid_no_patch(guestPath.c_str());
    std::FILE* const file = std::fopen(hostPath.string().c_str(), "wb");
    if (file == nullptr) {
        Emit(ReportLine(Format("SanitizerCoverage: failed to open %s for writing (reason: %d)\n", guestPath.c_str(), errno)));
        return;
    }
    std::fwrite(&Magic64, sizeof(Magic64), 1, file);
    if (!offsets.empty()) std::fwrite(offsets.data(), sizeof(uptr), offsets.size(), file);
    std::fclose(file);
    Emit(Format("SanitizerCoverage: %s: %zd PCs written\n", guestPath.c_str(), static_cast<std::ptrdiff_t>(offsets.size())));
}

void DumpCoverage(std::vector<uptr> pcs) {
    if (pcs.empty()) return;
    std::sort(pcs.begin(), pcs.end());
    bool moduleFound = false;
    uptr lastBase = 0;
    std::string moduleName;
    std::vector<uptr> offsets;
    for (const uptr pc : pcs) {
        if (pc == 0) continue;
        std::string name;
        uptr offset = 0;
        if (!ModuleAndOffsetForPc(pc, &name, &offset)) {
            Emit(Format("ERROR: unknown pc 0x%zx (may happen if dlclose is used)\n", static_cast<std::size_t>(pc)));
            continue;
        }
        const uptr base = pc - offset;
        if (base != lastBase || !moduleFound) {
            if (moduleFound) WriteModuleCoverage(moduleName, offsets);
            offsets.clear();
            lastBase = base;
            moduleFound = true;
            moduleName = name;
        }
        offsets.push_back(offset);
    }
    if (moduleFound) WriteModuleCoverage(moduleName, offsets);
}

std::vector<uptr> GuardSnapshot() {
    GuardController& controller = Controller();
    std::shared_lock lock(controller.mutex);
    std::vector<uptr> pcs;
    pcs.reserve(controller.pcs.size());
    for (const auto& pc : controller.pcs) pcs.push_back(pc.load(std::memory_order_relaxed));
    return pcs;
}

}

void DumpGuardCoverage() {
    if (!Controller().initialized || !GetFlags().coverage) return;
    DumpCoverage(GuardSnapshot());
}

void InitializeCoverage() {
    static std::once_flag once;
    std::call_once(once, [] {
        if (GetFlags().coverage) std::atexit([] { DumpGuardCoverage(); });
    });
}

}

using namespace Asan::Runtime;

extern "C" {

[[gnu::noinline]] void APS5_VABI __sanitizer_cov_trace_pc_guard(std::uint32_t* guard) {
    const std::uint32_t index = *guard;
    if (index == 0) return;
    GuardController& controller = Controller();
    const uptr pc = PreviousInstructionPc(reinterpret_cast<uptr>(__builtin_return_address(0)));
    std::shared_lock lock(controller.mutex);
    if (index > controller.pcs.size()) return;
    auto& slot = controller.pcs[index - 1];
    if (slot.load(std::memory_order_relaxed) == 0) slot.store(pc, std::memory_order_relaxed);
}

void APS5_VABI __sanitizer_cov_trace_pc_guard_init(std::uint32_t* start, std::uint32_t* stop) {
    if (start == stop) return;
    Require(start != nullptr && stop != nullptr && start < stop, "guest AddressSanitizer: invalid coverage guard range");
    std::uint32_t first = 0;
    if (!GuestCopyIn_nid_no_patch(&first, start, sizeof(first))) throw std::invalid_argument("guest AddressSanitizer: coverage guards are not readable");
    if (first != 0) return;
    EnsureShadow();
    InitializeCoverage();
    GuardController& controller = Controller();
    std::unique_lock lock(controller.mutex);
    controller.initialized = true;
    const std::size_t count = static_cast<std::size_t>(stop - start);
    std::vector<std::uint32_t> indices(count);
    std::uint32_t next = static_cast<std::uint32_t>(controller.pcs.size());
    for (auto& index : indices) index = ++next;
    if (!GuestCopyOut_nid_no_patch(start, indices.data(), count * sizeof(std::uint32_t))) throw std::invalid_argument("guest AddressSanitizer: coverage guards are not writable");
    controller.pcs.resize(next);
}

void APS5_VABI __sanitizer_dump_trace_pc_guard_coverage() {
    DumpGuardCoverage();
}

void APS5_VABI __sanitizer_cov_dump() {
    DumpGuardCoverage();
}

void APS5_VABI __sanitizer_dump_coverage(const std::uintptr_t* pcs, std::uintptr_t count) {
    if (count == 0) return;
    std::vector<uptr> copy(count);
    if (!GuestCopyIn_nid_no_patch(copy.data(), pcs, count * sizeof(uptr))) throw std::invalid_argument("guest AddressSanitizer: coverage PCs are not readable");
    DumpCoverage(std::move(copy));
}

const char* APS5_VABI __sancov_default_options() {
    return "";
}

}
