#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "prx/libc/include/GuestGs.hpp"
#include "prx/libc/include/HostThreadSlot.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <memory>
#include <vector>
#include <stdexcept>

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

struct Table {
    std::uintptr_t start = 0;
    std::vector<std::array<std::uint8_t, 64>> entries;
};

struct Original {
    std::uintptr_t guest;
    std::uintptr_t target;
};

struct Image {
    Image* next = nullptr;
    std::atomic<bool> active{true};
    std::uintptr_t base = 0;
    std::uint32_t size = 0;
    Table replay;
    Table moved;
    std::vector<Original> originals;
};

std::atomic<Image*> images{};
static_assert(std::atomic<Image*>::is_always_lock_free && std::atomic<bool>::is_always_lock_free);

struct NotificationData {
    ULONG flags;
    const void* fullName;
    const void* baseName;
    void* base;
    ULONG size;
};

void CALLBACK Unloaded(ULONG reason, const NotificationData* data, void*) {
    if (reason != 2) return;
    for (auto* image = images.load(std::memory_order_acquire); image; image = image->next)
        if (image->base == reinterpret_cast<std::uintptr_t>(data->base)) image->active.store(false, std::memory_order_release);
}

struct Notifications {
    using Register = LONG (NTAPI *)(ULONG, decltype(&Unloaded), void*, void**);
    using Unregister = LONG (NTAPI *)(void*);
    void* cookie = nullptr;
    Unregister unregister = nullptr;

    Notifications() {
        const auto module = GetModuleHandleW(L"ntdll.dll");
        const auto subscribe = reinterpret_cast<Register>(GetProcAddress(module, "LdrRegisterDllNotification"));
        unregister = reinterpret_cast<Unregister>(GetProcAddress(module, "LdrUnregisterDllNotification"));
        if (!subscribe || !unregister || subscribe(0, Unloaded, nullptr, &cookie) != 0)
            throw std::runtime_error("Cannot register Windows guest GS image lifetime notifications");
    }

    ~Notifications() { unregister(cookie); }
};

Image* Registered(std::uintptr_t address) {
    for (auto* image = images.load(std::memory_order_acquire); image; image = image->next)
        if (address >= image->base && address - image->base < image->size && image->active.load(std::memory_order_acquire)) return image;
    return nullptr;
}

bool LoadTable(std::uintptr_t image, const IMAGE_NT_HEADERS64& nt, const std::vector<IMAGE_SECTION_HEADER>& sections, const char* name, const char* magic, Table& result) {
    for (const auto& section : sections) {
        if (std::memcmp(section.Name, name, 8) != 0) continue;
        if (result.start != 0) return false;
        const auto size = section.Misc.VirtualSize;
        if (section.VirtualAddress >= nt.OptionalHeader.SizeOfImage || size > nt.OptionalHeader.SizeOfImage - section.VirtualAddress || size < 16) return false;
        result.start = image + section.VirtualAddress;
        std::uint8_t header[16];
        if (!Read(result.start, header, sizeof(header)) || std::memcmp(header, magic, 8) != 0 || Value<std::uint32_t>(header + 12) != 64) return false;
        const auto count = Value<std::uint32_t>(header + 8);
        if (count > (size - 16) / 64) return false;
        result.entries.resize(count);
        if (count && !Read(result.start + 16, result.entries.data(), static_cast<std::size_t>(count) * 64)) return false;
    }
    return true;
}

bool ValidReplay(const std::uint8_t* entry, std::uint32_t size) {
    const auto guest = Value<std::uint32_t>(entry);
    const auto resume = Value<std::uint32_t>(entry + 4);
    const auto base = entry[8];
    const auto index = entry[9];
    const auto length = entry[13];
    return guest < size && resume <= size && resume > guest && resume - guest <= 15 &&
        (base < 16 || base == 0xfe || base == 0xff) && (index < 16 || index == 0xff) && entry[10] <= 3 && entry[11] <= 1 &&
        (entry[12] == 3 || entry[12] == 6) && entry[14] <= 1 && length > 0 && length <= 15 &&
        std::memcmp(entry + 24, "\x48\x8d\x64\x24\x80\x0f\x0b", 7) == 0 && entry[31 + length] == 0x0f && entry[32 + length] == 0x0b;
}

const std::uint8_t* Record(const Table& table, std::uintptr_t address, std::uintptr_t& record) {
    if (address < table.start + 16) return nullptr;
    const auto offset = address - table.start - 16;
    const auto index = offset / 64;
    if (index >= table.entries.size()) return nullptr;
    record = table.start + 16 + index * 64;
    return table.entries[index].data();
}

bool Find(std::uintptr_t address, std::uint8_t* entry, std::uintptr_t& image, std::uintptr_t& replay, bool replayAddress = false) {
    const auto* registered = Registered(address);
    if (!registered) return false;
    std::uintptr_t record = 0;
    const auto* found = Record(registered->replay, address, record);
    if (!found) return false;
    replay = record + 31;
    if (replayAddress ? address < record + 24 || address > replay + found[13] : address != record + 29) return false;
    image = registered->base;
    std::memcpy(entry, found, 64);
    return true;
}

std::uintptr_t MovedAddress(std::uintptr_t address, bool original) {
    const auto* image = Registered(address);
    if (!image) return 0;
    if (original) {
        const auto found = std::lower_bound(image->originals.begin(), image->originals.end(), address,
            [](const Original& entry, std::uintptr_t key) { return entry.guest < key; });
        return found != image->originals.end() && found->guest == address ? found->target : 0;
    }
    std::uintptr_t record = 0;
    const auto* entry = Record(image->moved, address, record);
    if (!entry || address < record + 16 || address - record - 16 >= Value<std::uint32_t>(entry + 4)) return 0;
    return image->base + Value<std::uint32_t>(entry);
}

LONG WINAPI Continue(EXCEPTION_POINTERS* info) {
    if (const auto clone = MovedAddress(info->ContextRecord->Rip, true)) info->ContextRecord->Rip = clone;
    return EXCEPTION_CONTINUE_SEARCH;
}

}

extern "C" std::uint64_t GuestGsGet_nid_no_patch() { return BaseSlot::Get(); }
extern "C" void GuestGsSet_nid_no_patch(std::uint64_t base) { BaseSlot::Set(base); }

extern "C" bool GuestGsRegisterImage_nid_no_patch(const void* base) {
    const auto address = reinterpret_cast<std::uintptr_t>(base);
    if (const auto* existing = Registered(address); existing && existing->base == address) return true;
    IMAGE_DOS_HEADER dos{};
    IMAGE_NT_HEADERS64 nt{};
    if (!Read(address, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 0 ||
        !Read(address + dos.e_lfanew, &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
        nt.OptionalHeader.SizeOfImage == 0 || address + nt.OptionalHeader.SizeOfImage < address) return false;
    const auto table = static_cast<std::uint64_t>(dos.e_lfanew) + 24 + nt.FileHeader.SizeOfOptionalHeader;
    if (table > nt.OptionalHeader.SizeOfImage || nt.FileHeader.NumberOfSections > (nt.OptionalHeader.SizeOfImage - table) / sizeof(IMAGE_SECTION_HEADER)) return false;
    std::vector<IMAGE_SECTION_HEADER> sections(nt.FileHeader.NumberOfSections);
    if (!sections.empty() && !Read(address + table, sections.data(), sections.size() * sizeof(IMAGE_SECTION_HEADER))) return false;
    auto image = std::make_unique<Image>();
    image->base = address;
    image->size = nt.OptionalHeader.SizeOfImage;
    if (!LoadTable(address, nt, sections, ".ggs\0\0\0\0", "APS5GGS1", image->replay) ||
        !LoadTable(address, nt, sections, ".ggm\0\0\0\0", "APS5GGM1", image->moved)) return false;
    if (image->replay.entries.empty() && image->moved.entries.empty()) return true;
    for (const auto& entry : image->replay.entries) if (!ValidReplay(entry.data(), image->size)) return false;
    for (std::size_t index = 0; index < image->moved.entries.size(); ++index) {
        const auto* entry = image->moved.entries[index].data();
        const auto guest = Value<std::uint32_t>(entry);
        const auto size = Value<std::uint32_t>(entry + 4);
        const auto target = Value<std::uint32_t>(entry + 8);
        if (guest >= image->size || size > 48 || (size == 0 && target >= image->size)) return false;
        image->originals.push_back({address + guest, size == 0 ? address + target : image->moved.start + 16 + index * 64 + 16});
    }
    std::sort(image->originals.begin(), image->originals.end(), [](const Original& left, const Original& right) { return left.guest < right.guest; });
    static const Notifications notifications;
    if (!image->moved.entries.empty()) {
        struct Handler {
            void* handle = AddVectoredContinueHandler(0, Continue);
            Handler() { if (!handle) throw std::runtime_error("Cannot install the Windows guest GS continue handler"); }
            ~Handler() { RemoveVectoredContinueHandler(handle); }
        };
        static const Handler handler;
    }
    auto* node = image.release();
    node->next = images.load(std::memory_order_acquire);
    while (!images.compare_exchange_weak(node->next, node, std::memory_order_release, std::memory_order_acquire)) {}
    return true;
}

extern "C" bool GuestGsIsReplay_nid_no_patch(std::uintptr_t address) {
    std::uint8_t entry[64];
    std::uintptr_t image = 0;
    std::uintptr_t replay = 0;
    return Find(address, entry, image, replay, true) || MovedAddress(address, false) != 0;
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
    if (const auto guest = MovedAddress(context.Rip, false)) {
        context.Rip = guest;
        fault.ExceptionAddress = reinterpret_cast<void*>(guest);
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
        (scratch != 3 && scratch != 6) || entry[14] > 1 || length == 0 || length > 15 || Value<std::uint32_t>(entry + 4) <= Value<std::uint32_t>(entry) || Value<std::uint32_t>(entry + 4) - Value<std::uint32_t>(entry) > 15 ||
        entry[31 + length] != 0x0f || entry[32 + length] != 0x0b) return false;
    auto address = Value<std::uint64_t>(entry + 16);
    if (base == 0xfe) address += resume;
    else if (base != 0xff) address += Register(context, base) + (base == 4 ? 128 : 0);
    if (index != 0xff) address += Register(context, index) << entry[10];
    if (entry[11]) address = static_cast<std::uint32_t>(address);
    if (entry[14] == 0) address += GuestGsGet_nid_no_patch();
    pending = {replay, replay + length, image + Value<std::uint32_t>(entry), resume, Register(context, scratch), scratch};
    Register(context, scratch) = address;
    context.Rip = replay;
    return true;
}
#endif
