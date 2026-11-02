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

template<typename TValue> TValue Read(const std::uint8_t* bytes) {
    TValue value;
    std::memcpy(&value, bytes, sizeof(value));
    return value;
}

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
    if (!GuestGsRegisterImage_nid_no_patch(image)) return 4;
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
