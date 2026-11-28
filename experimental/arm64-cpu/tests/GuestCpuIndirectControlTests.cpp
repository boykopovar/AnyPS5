#include "GuestCpu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <vector>

using anyps5::experimental::GuestCpu;

static bool check(const char* label, std::uint64_t actual, std::uint64_t expected) {
    if (actual == expected) return true;
    std::fprintf(stderr, "%s: expected %llu got %llu\n", label,
        static_cast<unsigned long long>(expected),
        static_cast<unsigned long long>(actual));
    return false;
}
static bool rejects(const std::vector<std::uint8_t>& code, std::size_t stack = 64,
                    std::size_t limit = 100) {
    try { (void)GuestCpu(code, stack).run(limit); }
    catch (const std::runtime_error&) { return true; }
    return false;
}
int main() {
    // mov eax,8; jmp rax; unreachable; mov eax,42; ret.
    const std::array<std::uint8_t, 14> jumpRegister{
        0xB8, 8, 0, 0, 0, 0xFF, 0xE0, 0x90,
        0xB8, 42, 0, 0, 0, 0xC3
    };
    if (!check("indirect JMP register", GuestCpu(jumpRegister).run(), 42))
        return EXIT_FAILURE;

    // mov ecx,9; call rcx; ret; padding; mov eax,42; ret.
    const std::array<std::uint8_t, 15> callRegister{
        0xB9, 9, 0, 0, 0, 0xFF, 0xD1, 0xC3, 0x90,
        0xB8, 42, 0, 0, 0, 0xC3
    };
    GuestCpu callCpu(callRegister, 64);
    if (!check("indirect CALL register", callCpu.run(), 42) ||
        !check("CALL stack restored", callCpu.registerValue(4), 64))
        return EXIT_FAILURE;

    // Store the code offset in guest stack memory, then indirect call
    // through [rsp-8]. The call pushes a return address below that slot.
    const std::array<std::uint8_t, 20> callMemory{
        0xB8, 14, 0, 0, 0,
        0x89, 0x44, 0x24, 0xF8,
        0xFF, 0x54, 0x24, 0xF8,
        0xC3,
        0xB8, 42, 0, 0, 0, 0xC3
    };
    GuestCpu memoryCpu(callMemory, 64);
    if (!check("indirect CALL memory", memoryCpu.run(), 42) ||
        !check("memory CALL stack restored", memoryCpu.registerValue(4), 64))
        return EXIT_FAILURE;

    const std::array<std::uint8_t, 14> jumpMemory{
        0xB8, 13, 0, 0, 0,
        0x89, 0x44, 0x24, 0xF8,
        0xFF, 0x64, 0x24, 0xF8,
        0xB8
    };
    std::vector<std::uint8_t> jumpMemoryCode(jumpMemory.begin(), jumpMemory.end());
    jumpMemoryCode.insert(jumpMemoryCode.end(), {42, 0, 0, 0, 0xC3});
    if (!check("indirect JMP memory", GuestCpu(jumpMemoryCode).run(), 42))
        return EXIT_FAILURE;

    // Invalid targets must not silently wrap or escape the guest code.
    if (!rejects({0xB8, 255, 0, 0, 0, 0xFF, 0xE0}) ||
        !rejects({0xB8, 255, 0, 0, 0, 0xFF, 0xD0}) ||
        !rejects({0xFF, 0x20}) || // JMP [rax], RAX=0, zero target loops
        !rejects({0xFF, 0xE0}, 64, 5) || // self-loop via RAX=0
        !rejects({0xFF, 0xF0})) { // unsupported FF /6
        std::fprintf(stderr, "invalid indirect control flow accepted\n");
        return EXIT_FAILURE;
    }
    std::puts("Indirect CALL/JMP tests passed");
    return EXIT_SUCCESS;
}
