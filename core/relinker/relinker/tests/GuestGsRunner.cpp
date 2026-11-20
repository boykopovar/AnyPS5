#include <cstdint>
#include <cstdio>
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
#ifdef GUEST_GS_RUNTIME
#include "prx/libc/include/HostFault.hpp"
#endif
#else
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

template<typename TValue> TValue Read(const std::uint8_t* bytes) {
    TValue value;
    std::memcpy(&value, bytes, sizeof(value));
    return value;
}

#ifdef _WIN32
static std::uintptr_t faultSite;
static std::uintptr_t faultPage;
static std::uintptr_t replaySite;
static std::uint64_t replayStack;
static bool guestFault;
static bool unhandledFault;
static bool faultRepaired;

#ifndef GUEST_GS_RUNTIME
static LONG WINAPI Handle(EXCEPTION_POINTERS* info) {
    return GuestGsHandleException_nid_no_patch(info) ? EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_CONTINUE_SEARCH;
}
#endif

static LONG WINAPI ObserveReplayStart(EXCEPTION_POINTERS* info) {
    const auto& context = *info->ContextRecord;
    if (info->ExceptionRecord->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION && context.Rip == replaySite - 2) {
        if (replayStack != 0 && (!guestFault || !faultRepaired || context.Rsp != replayStack)) ExitProcess(10);
        replayStack = context.Rsp;
        if (unhandledFault) {
            std::fprintf(stderr, "expected guest rip 0x%016llx rsp %016llx\n",
                static_cast<unsigned long long>(faultSite), static_cast<unsigned long long>(replayStack + 128));
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static LONG WINAPI RepairFault(EXCEPTION_POINTERS* info) {
    const auto& context = *info->ContextRecord;
    const auto& fault = *info->ExceptionRecord;
    if (fault.ExceptionCode != EXCEPTION_ACCESS_VIOLATION) return EXCEPTION_CONTINUE_SEARCH;
    if (guestFault || faultRepaired || replayStack == 0 || context.Rip != replaySite || context.Rsp != replayStack || context.Rbx != faultPage ||
        fault.ExceptionAddress != reinterpret_cast<void*>(replaySite) || fault.NumberParameters < 2 || fault.ExceptionInformation[1] != faultPage)
        ExitProcess(11);
    if (unhandledFault) return EXCEPTION_CONTINUE_SEARCH;
    DWORD previous = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(faultPage), 4096, PAGE_READWRITE, &previous)) ExitProcess(12);
    faultRepaired = true;
    return EXCEPTION_CONTINUE_EXECUTION;
}

#ifdef GUEST_GS_RUNTIME
static bool AcceptGuestFault(void* exceptionPointers) noexcept {
    const auto& info = *static_cast<EXCEPTION_POINTERS*>(exceptionPointers);
    const auto& context = *info.ContextRecord;
    const auto& fault = *info.ExceptionRecord;
    if (fault.ExceptionCode != EXCEPTION_ACCESS_VIOLATION) return false;
    if (faultRepaired || replayStack == 0 || context.Rip != faultSite || context.Rsp != replayStack + 128 ||
        context.Rbx != 0x1234567812345678ull || context.Rsi != 0x2234567822345678ull ||
        fault.ExceptionAddress != reinterpret_cast<void*>(faultSite) || fault.NumberParameters < 2 || fault.ExceptionInformation[1] != faultPage)
        ExitProcess(13);
    DWORD previous = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(faultPage), 4096, PAGE_READWRITE, &previous)) ExitProcess(12);
    faultRepaired = true;
    return true;
}
#endif
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
    const auto nt = Read<std::uint32_t>(bytes.data() + 0x3c);
    const auto optional = nt + 24;
    const auto size = Read<std::uint32_t>(bytes.data() + optional + 56);
    image = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    if (!image) return 4;
    std::memcpy(image, bytes.data(), Read<std::uint32_t>(bytes.data() + optional + 60));
    const auto sections = optional + Read<std::uint16_t>(bytes.data() + nt + 20);
    for (unsigned index = 0; index < Read<std::uint16_t>(bytes.data() + nt + 6); ++index) {
        const auto* section = bytes.data() + sections + index * 40;
        std::memcpy(image + Read<std::uint32_t>(section + 12), bytes.data() + Read<std::uint32_t>(section + 20), Read<std::uint32_t>(section + 16));
    }
#else
    image = static_cast<std::uint8_t*>(mmap(nullptr, bytes.size(), PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    if (image == MAP_FAILED) return 4;
    std::memcpy(image, bytes.data(), bytes.size());
#endif
    std::uint64_t* storage;
#ifdef _WIN32
    storage = static_cast<std::uint64_t*>(VirtualAlloc(nullptr, 65536, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
    storage = static_cast<std::uint64_t*>(mmap(nullptr, 65536, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
#endif
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
    unhandledFault = argc > 4 && std::strcmp(argv[3], "unhandled") == 0;
#ifdef GUEST_GS_RUNTIME
    guestFault = argc > 4 && std::strcmp(argv[3], "guest-fault") == 0;
    if (guestFault) LibcSetHostFaultHandler_nid_no_patch(AcceptGuestFault);
#endif
    if (argc > 4 && (guestFault || unhandledFault || std::strcmp(argv[3], "fault") == 0)) {
        const auto* patch = image + std::stoull(argv[4], nullptr, 0);
        if (*patch != 0xe9) return 8;
        faultSite = reinterpret_cast<std::uintptr_t>(patch);
        replaySite = reinterpret_cast<std::uintptr_t>(patch + 5 + Read<std::int32_t>(patch + 1) + 7);
        faultPage = base;
        DWORD previous = 0;
        if (!AddVectoredExceptionHandler(1, ObserveReplayStart) || !AddVectoredExceptionHandler(0, RepairFault) ||
            !VirtualProtect(reinterpret_cast<void*>(faultPage), 4096, PAGE_NOACCESS, &previous)) return 8;
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
