#include "GuestGsTestImage.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string_view>
#ifdef _WIN32
#include "prx/libc/include/GuestGs.hpp"
#else
#include <csignal>
#include <sys/syscall.h>
#include <ucontext.h>
#include <unistd.h>
#endif

namespace {

std::uint8_t* data;
std::uintptr_t faultSite;
std::uintptr_t resumeSite;
std::string_view mode;
unsigned faults;
bool store;
std::array<std::uint8_t, 16> faultBytes{};

bool Observe(std::uintptr_t rip, std::uint64_t rbx, std::uint64_t rsi, std::uint64_t flags, std::uint32_t mxcsr, const void* vectors) {
    return rip == faultSite && rbx == 0x1234567812345678ull && rsi == 0x2234567822345678ull &&
        (flags & 0x401) == 0x401 && mxcsr == 0xdfb5 && [&] {
            for (unsigned reg = 0; reg < 16; ++reg)
                if (std::memcmp(static_cast<const std::uint8_t*>(vectors) + reg * 16, data + 0x800 + reg * 32, 16) != 0) return false;
            return true;
        }();
}

bool Repair() {
    if (faults == 0) std::memcpy(faultBytes.data(), data + 0xff0, faultBytes.size());
    ++faults;
    if (mode == "twice" && faults == 1) return true;
#ifdef _WIN32
    DWORD previous = 0;
    return VirtualProtect(data + 4096, 4096, PAGE_READWRITE, &previous) != 0;
#else
    return mprotect(data + 4096, 4096, PROT_READ | PROT_WRITE) == 0;
#endif
}

#ifdef _WIN32
LONG WINAPI Handle(EXCEPTION_POINTERS* info) {
    if (GuestGsHandleException_nid_no_patch(info)) return EXCEPTION_CONTINUE_EXECUTION;
    auto& context = *info->ContextRecord;
    if (info->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION ||
        info->ExceptionRecord->ExceptionAddress != reinterpret_cast<void*>(faultSite) ||
        !Observe(context.Rip, context.Rbx, context.Rsi, context.EFlags, context.MxCsr, &context.Xmm0)) return EXCEPTION_CONTINUE_SEARCH;
    if (mode != "align" && mode != "align-base" && (info->ExceptionRecord->NumberParameters < 2 ||
        info->ExceptionRecord->ExceptionInformation[0] != (store ? 1u : 0u) ||
        info->ExceptionRecord->ExceptionInformation[1] != reinterpret_cast<std::uintptr_t>(data + 4096))) return EXCEPTION_CONTINUE_SEARCH;
    if (mode == "align" || mode == "align-base") {
        if (faults++) return EXCEPTION_CONTINUE_SEARCH;
        --context.Rdx;
    } else {
        if (faults >= (mode == "twice" ? 2u : 1u) || !Repair()) return EXCEPTION_CONTINUE_SEARCH;
        if (mode == "skip") context.Rip = resumeSite;
    }
    return EXCEPTION_CONTINUE_EXECUTION;
}
#else
void Handle(int, siginfo_t* info, void* opaque) {
    auto& context = *static_cast<ucontext_t*>(opaque);
    auto* fp = context.uc_mcontext.fpregs;
    if (!fp || !Observe(context.uc_mcontext.gregs[REG_RIP], context.uc_mcontext.gregs[REG_RBX], context.uc_mcontext.gregs[REG_RSI],
        context.uc_mcontext.gregs[REG_EFL], fp->mxcsr, fp->_xmm)) std::_Exit(20);
    if (mode != "align" && mode != "align-base" && info->si_addr != data + 4096) std::_Exit(23);
    if (mode == "align" || mode == "align-base") {
        if (faults++) std::_Exit(21);
        --context.uc_mcontext.gregs[REG_RDX];
    } else {
        if (faults >= (mode == "twice" ? 2u : 1u) || !Repair()) std::_Exit(22);
        if (mode == "skip") context.uc_mcontext.gregs[REG_RIP] = resumeSite;
    }
}
#endif

void Dump(const std::uint8_t* bytes, std::size_t size) {
    constexpr char digits[] = "0123456789abcdef";
    for (std::size_t index = 0; index < size; ++index) std::cout << digits[bytes[index] >> 4] << digits[bytes[index] & 15];
    std::cout << ' ';
}

}

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--features") {
        std::cout << (__builtin_cpu_supports("avx") ? "avx\n" : "sse\n");
        return 0;
    }
    if (argc < 4) return 2;
    mode = argv[3];
    store = argc > 6 && std::string_view(argv[6]) == "store";
    std::ifstream input(argv[1], std::ios::binary);
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input), {}};
    if (bytes.size() < 0x100) return 3;
    auto* image = GuestGsMapTestImage(bytes);
    auto* allocation = GuestGsAllocate(65536, false);
    if (!image || !allocation) return 4;
    for (std::size_t index = 0; index < 65536; ++index) allocation[index] = static_cast<std::uint8_t>(index * 37 + (index >> 4) * 11 + 0x53);
    data = allocation + 32768;
    const std::uint32_t payload[] = {0x7f800001, 0x80000000, 1, 0xff800000};
    std::memcpy(data, payload, sizeof(payload));
    const std::uint32_t mxcsr = 0xdfb5;
    std::memcpy(data + 0x440, &mxcsr, sizeof(mxcsr));
    const auto entry = std::stoull(argv[2], nullptr, 0);
    faultSite = argc > 4 ? reinterpret_cast<std::uintptr_t>(image) + std::stoull(argv[4], nullptr, 0) : 0;
    resumeSite = argc > 5 ? reinterpret_cast<std::uintptr_t>(image) + std::stoull(argv[5], nullptr, 0) : 0;
    auto base = reinterpret_cast<std::uintptr_t>(data);
    if (mode == "zero") base = 0;
    if (mode == "eip") base -= static_cast<std::uint32_t>(faultSite);
    if (mode == "shifted" || mode == "align-base") ++base;
    const bool protectedPage = mode == "read" || mode == "write" || mode == "skip" || mode == "twice" || mode == "edge";
    const bool expectedFault = (protectedPage && mode != "edge") || mode == "align" || mode == "align-base";
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#ifndef GUEST_GS_RUNTIME
    AddVectoredExceptionHandler(1, Handle);
#else
    if (expectedFault) AddVectoredExceptionHandler(1, Handle);
#endif
    GuestGsSet_nid_no_patch(base);
    if (protectedPage) {
        DWORD previous = 0;
        if (!VirtualProtect(data + 4096, 4096, mode == "write" ? PAGE_READONLY : PAGE_NOACCESS, &previous)) return 5;
    }
#else
    struct sigaction action{};
    action.sa_sigaction = Handle;
    action.sa_flags = SA_SIGINFO;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGSEGV, &action, nullptr) || sigaction(SIGBUS, &action, nullptr)) return 5;
    std::uint64_t saved = 0;
    if (syscall(SYS_arch_prctl, 0x1004, &saved) || syscall(SYS_arch_prctl, 0x1001, base)) return 6;
    if (protectedPage && mprotect(data + 4096, 4096, mode == "write" ? PROT_READ : PROT_NONE)) return 5;
#endif
    using Function = std::uint64_t (APS5_VABI *)(std::uint8_t*);
    if (reinterpret_cast<Function>(image + entry)(data) != 42) return 7;
    if (mode == "edge") {
#ifdef _WIN32
        DWORD previous = 0;
        if (!VirtualProtect(data + 4096, 4096, PAGE_READWRITE, &previous)) return 10;
#else
        if (mprotect(data + 4096, 4096, PROT_READ | PROT_WRITE)) return 10;
#endif
    }
#ifdef _WIN32
    if (GuestGsGet_nid_no_patch() != base) return 8;
#else
    if (syscall(SYS_arch_prctl, 0x1001, saved)) return 8;
#endif
    if (faults != (mode == "twice" ? 2u : expectedFault ? 1u : 0u)) return 9;
    Dump(data, 64);
    Dump(data + 0x200, 512);
    Dump(data + 0x400, 48);
    Dump(data + 0xfe0, 64);
    Dump(faultBytes.data(), faultBytes.size());
    std::cout << faults << '\n';
    return 0;
}
