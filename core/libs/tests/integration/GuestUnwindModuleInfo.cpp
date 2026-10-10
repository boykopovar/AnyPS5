#include "SceTypes.hpp"
#include "tests/GuestUnwindModuleInfoFixture.hpp"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <windows.h>

extern "C" int APS5_VABI sceKernelGetModuleInfoForUnwind(uint64_t addr, int flags, ModuleInfoForUnwind* info);

static void Require(bool value) { if (!value) std::abort(); }

static bool Throws(std::uint64_t address) {
    ModuleInfoForUnwind info{};
    try {
        sceKernelGetModuleInfoForUnwind(address, 0, &info);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

int main() {
    auto* fixture = GetUnwindFixture();
    Require(fixture != nullptr);
    const auto address = reinterpret_cast<std::uint64_t>(fixture);
    MEMORY_BASIC_INFORMATION memory{};
    Require(VirtualQuery(reinterpret_cast<LPCVOID>(address), &memory, sizeof(memory)) != 0);
    const auto base = reinterpret_cast<std::uint64_t>(memory.AllocationBase);
    Require(base != reinterpret_cast<std::uint64_t>(GetModuleHandleW(nullptr)));

    ModuleInfoForUnwind info{};
    Require(sceKernelGetModuleInfoForUnwind(address, 0, &info) == 0);
    Require(info.st_size == sizeof(ModuleInfoForUnwind));
    Require(info.eh_frame_hdr_addr == reinterpret_cast<std::uint64_t>(&fixture->header));
    Require(info.eh_frame_addr == reinterpret_cast<std::uint64_t>(&fixture->frames));
    Require(info.eh_frame_size == 4 + 12);
    Require(info.seg0_addr == base);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    Require(info.seg0_size == nt->OptionalHeader.SizeOfImage);

    fixture->header.version = 2;
    Require(Throws(address));
    fixture->header.version = 1;
    for (const std::uint8_t encoding : {0x3b, 0x9b, 0x0f, 0xff}) {
        fixture->header.framePointerEncoding = encoding;
        Require(Throws(address));
    }
    fixture->header.framePointerEncoding = 0x1b;
    fixture->frames.cieLength = 0xffffffffu;
    Require(Throws(address));
    fixture->frames.cieLength = 12;
    Require(!Throws(address));

    ModuleInfoForUnwind host{};
    Require(sceKernelGetModuleInfoForUnwind(reinterpret_cast<std::uint64_t>(&GetModuleHandleW), 0, &host) == 0);
    Require(host.eh_frame_hdr_addr == 0 && host.eh_frame_addr == 0 && host.eh_frame_size == 0);
}
