#include "tests/GuestUnwindModuleInfoFixture.hpp"

#include <cstdint>
#include <windows.h>

asm(".section .ehmeta,\"dw\"\n"
    ".globl UnwindFixtureMeta\n"
    ".p2align 2\n"
    "UnwindFixtureMeta:\n"
    "    .long 0\n"
    ".text\n");

extern "C" IMAGE_DOS_HEADER __ImageBase;
extern "C" std::uint32_t UnwindFixtureMeta;

static UnwindFixture fixture;

extern "C" UnwindFixture* GetUnwindFixture() {
    const auto base = reinterpret_cast<std::uintptr_t>(&__ImageBase);
    fixture.header.frames = static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(&fixture.frames) - reinterpret_cast<std::intptr_t>(&fixture.header.frames));
    UnwindFixtureMeta = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&fixture.header) - base);
    return &fixture;
}
