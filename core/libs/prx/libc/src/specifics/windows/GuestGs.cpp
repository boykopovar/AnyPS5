#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "prx/libc/include/GuestGs.hpp"
#include "prx/libc/include/HostThreadSlot.hpp"
#include <cstring>

namespace {

struct BaseTag;
using BaseSlot = HostThreadSlot<std::uint64_t, BaseTag>;

struct Replay {
    std::uint64_t start;
    std::uint64_t end;
    std::uint64_t guest;
    std::uint64_t resume;
    std::uint64_t saved;
    std::uint8_t scratch;
};

thread_local Replay pending{};

DWORD64& Register(CONTEXT& context, unsigned reg) {
    switch (reg) {
    case 0: return context.Rax;
    case 1: return context.Rcx;
    case 2: return context.Rdx;
    case 3: return context.Rbx;
    case 4: return context.Rsp;
    case 5: return context.Rbp;
    case 6: return context.Rsi;
    case 7: return context.Rdi;
    case 8: return context.R8;
    case 9: return context.R9;
    case 10: return context.R10;
    case 11: return context.R11;
    case 12: return context.R12;
    case 13: return context.R13;
    case 14: return context.R14;
    default: return context.R15;
    }
}

bool Read(std::uintptr_t address, void* destination, std::size_t size) {
    SIZE_T copied = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), destination, size, &copied) && copied == size;
}

template<typename TValue> TValue Value(const std::uint8_t* bytes) {
    TValue value;
    std::memcpy(&value, bytes, sizeof(value));
    return value;
}

bool Find(std::uintptr_t address, std::uint8_t* entry, std::uintptr_t& image, std::uintptr_t& replay, bool replayAddress = false) {
    MEMORY_BASIC_INFORMATION memory{};
    if (!VirtualQuery(reinterpret_cast<const void*>(address), &memory, sizeof(memory))) return false;
    image = reinterpret_cast<std::uintptr_t>(memory.AllocationBase);
    IMAGE_DOS_HEADER dos{};
    IMAGE_NT_HEADERS64 nt{};
    if (!Read(image, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 0 ||
        !Read(image + dos.e_lfanew, &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
        address < image || address - image >= nt.OptionalHeader.SizeOfImage) return false;
    const auto table = image + dos.e_lfanew + 24 + nt.FileHeader.SizeOfOptionalHeader;
    for (unsigned index = 0; index < nt.FileHeader.NumberOfSections; ++index) {
        IMAGE_SECTION_HEADER section{};
        if (!Read(table + index * sizeof(section), &section, sizeof(section))) return false;
        if (std::memcmp(section.Name, ".ggs\0\0\0\0", 8) != 0) continue;
        const auto size = section.Misc.VirtualSize;
        if (section.VirtualAddress >= nt.OptionalHeader.SizeOfImage || size > nt.OptionalHeader.SizeOfImage - section.VirtualAddress || size < 16) return false;
        const auto start = image + section.VirtualAddress;
        std::uint8_t header[16];
        if (!Read(start, header, sizeof(header)) || std::memcmp(header, "APS5GGS1", 8) != 0 || Value<std::uint32_t>(header + 12) != 64) return false;
        const auto count = Value<std::uint32_t>(header + 8);
        if (count > (size - 16) / 64) return false;
        for (std::uint32_t item = 0; item < count; ++item) {
            if (!Read(start + 16 + static_cast<std::uintptr_t>(item) * 64, entry, 64)) return false;
            replay = start + 16 + static_cast<std::uintptr_t>(item) * 64 + 31;
            if (replayAddress ? address < replay - 7 || address > replay + entry[13] : address != replay - 2) continue;
            return true;
        }
        return false;
    }
    return false;
}

}

extern "C" std::uint64_t GuestGsGet_nid_no_patch() { return BaseSlot::Get(); }
extern "C" void GuestGsSet_nid_no_patch(std::uint64_t base) { BaseSlot::Set(base); }

extern "C" bool GuestGsIsReplay_nid_no_patch(std::uintptr_t address) {
    std::uint8_t entry[64];
    std::uintptr_t image = 0;
    std::uintptr_t replay = 0;
    return Find(address, entry, image, replay, true);
}

bool GuestGsHandleException_nid_no_patch(EXCEPTION_POINTERS* info) {
    auto& context = *info->ContextRecord;
    auto& fault = *info->ExceptionRecord;
    if (auto* active = &pending; active->start != 0) {
        if (context.Rip >= active->start && context.Rip <= active->end) {
            const bool complete = context.Rip == active->end && fault.ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION;
            Register(context, active->scratch) = active->saved;
            context.Rsp += 128;
            context.Rip = context.Rip == active->end ? active->resume : active->guest;
            fault.ExceptionAddress = reinterpret_cast<void*>(context.Rip);
            pending = {};
            return complete;
        }
        return false;
    }
    if (fault.ExceptionCode != EXCEPTION_ILLEGAL_INSTRUCTION || (context.EFlags & 0x100) != 0) return false;
    std::uint8_t entry[64];
    std::uintptr_t image = 0;
    std::uintptr_t replay = 0;
    if (!Find(context.Rip, entry, image, replay)) return false;
    const auto base = entry[8];
    const auto index = entry[9];
    const auto scratch = entry[12];
    const auto length = entry[13];
    const auto resume = image + Value<std::uint32_t>(entry + 4);
    if ((base >= 16 && base != 0xfe && base != 0xff) || (index >= 16 && index != 0xff) || entry[10] > 3 || entry[11] > 1 ||
        (scratch != 3 && scratch != 6) || length == 0 || length > 15 || Value<std::uint32_t>(entry + 4) <= Value<std::uint32_t>(entry) || Value<std::uint32_t>(entry + 4) - Value<std::uint32_t>(entry) > 15 ||
        entry[31 + length] != 0x0f || entry[32 + length] != 0x0b) return false;
    auto address = Value<std::uint64_t>(entry + 16);
    if (base == 0xfe) address += resume;
    else if (base != 0xff) address += Register(context, base) + (base == 4 ? 128 : 0);
    if (index != 0xff) address += Register(context, index) << entry[10];
    if (entry[11]) address = static_cast<std::uint32_t>(address);
    address += GuestGsGet_nid_no_patch();
    pending = {replay, replay + length, image + Value<std::uint32_t>(entry), resume, Register(context, scratch), scratch};
    Register(context, scratch) = address;
    context.Rip = replay;
    return true;
}
#endif
