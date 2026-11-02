#include "GuestGsTestImage.hpp"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <thread>
#include <vector>
#include <span>
#include "prx/libc/include/general/VabiMacros.hpp"
#ifdef _WIN32
#include <windows.h>
#include "prx/libc/include/GuestGs.hpp"
#else
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

#ifdef _WIN32
static std::uintptr_t faultSite;
static std::uintptr_t faultPage;
static bool faultRepaired;

static LONG WINAPI Handle(EXCEPTION_POINTERS* info) {
    if (GuestGsHandleException_nid_no_patch(info)) return EXCEPTION_CONTINUE_EXECUTION;
    if (faultPage && !faultRepaired && info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        if (info->ContextRecord->Rip != faultSite || info->ContextRecord->Rbx != 0x1234567812345678ull ||
            info->ExceptionRecord->ExceptionAddress != reinterpret_cast<void*>(faultSite)) return EXCEPTION_CONTINUE_SEARCH;
        DWORD previous = 0;
        if (!VirtualProtect(reinterpret_cast<void*>(faultPage), 4096, PAGE_READWRITE, &previous)) return EXCEPTION_CONTINUE_SEARCH;
        faultRepaired = true;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

int main(int argc, char** argv) {
    if (argc < 3) return 2;
    std::ifstream input(argv[1], std::ios::binary);
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input), {}};
    if (bytes.size() < 0x100) return 3;
    const auto entry = std::stoull(argv[2], nullptr, 0);
    std::uint8_t* image;
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#ifndef GUEST_GS_RUNTIME
    AddVectoredExceptionHandler(1, Handle);
#endif
#endif
    image = GuestGsMapTestImage(bytes);
    if (!image) return 4;
    auto* storage = reinterpret_cast<std::uint64_t*>(GuestGsAllocate(65536, false));
    if (!storage) return 4;
    std::span<std::uint64_t> cells(storage, 8192);
    for (std::size_t index = 0; index < cells.size(); ++index) cells[index] = 0x1122334455667788ull;
    auto base = argc > 3 && std::strcmp(argv[3], "zero") == 0 ? 0 : reinterpret_cast<std::uintptr_t>(cells.data() + 4096);
    if (argc > 4 && std::strcmp(argv[3], "eip") == 0)
        base -= static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(image) + std::stoull(argv[4], nullptr, 0));
#ifdef _WIN32
    GuestGsSet_nid_no_patch(base);
#else
    std::uint64_t saved = 0;
    if (syscall(SYS_arch_prctl, 0x1004, &saved) || syscall(SYS_arch_prctl, 0x1001, base)) return 5;
#endif
    using Function = std::uint64_t (APS5_VABI *)(std::uint64_t*);
    if (argc > 3 && std::strcmp(argv[3], "atomic") == 0) {
        cells[4096] = 0;
        std::vector<std::thread> workers;
        for (unsigned index = 0; index < 4; ++index) {
            workers.emplace_back([&] {
#ifdef _WIN32
                GuestGsSet_nid_no_patch(base);
#endif
                for (unsigned repeat = 0; repeat < 128; ++repeat) reinterpret_cast<Function>(image + entry)(cells.data() + 4096);
            });
        }
        for (auto& worker : workers) worker.join();
        if (cells[4096] != 512) return 7;
        std::cout << "atomic 512\n";
        return 0;
    }
#ifdef _WIN32
    if (argc > 4 && (std::strcmp(argv[3], "fault") == 0 || std::strcmp(argv[3], "fault-follow") == 0)) {
        faultSite = reinterpret_cast<std::uintptr_t>(image) + std::stoull(argv[4], nullptr, 0);
        faultPage = base + (std::strcmp(argv[3], "fault-follow") == 0 ? 4096 : 0);
        DWORD previous = 0;
        if (!VirtualProtect(reinterpret_cast<void*>(faultPage), 4096, PAGE_NOACCESS, &previous)) return 8;
    }
#endif
    const auto result = reinterpret_cast<Function>(image + entry)(cells.data() + 4096);
#ifdef _WIN32
    if (faultPage && !faultRepaired) return 9;
    if (GuestGsGet_nid_no_patch() != base) return 6;
#else
    if (syscall(SYS_arch_prctl, 0x1001, saved)) return 5;
#endif
    std::cout << std::hex << result;
    for (unsigned index = 4092; index < 4108; ++index) std::cout << ' ' << cells[index];
    std::cout << '\n';
    return 0;
}
