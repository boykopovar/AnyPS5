#include <windows.h>
#include "prx/libc/include/GuestGs.hpp"
#include <array>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

namespace {

std::atomic<unsigned> reads{};
std::atomic<unsigned> queries{};
unsigned installs = 0;
PVECTORED_EXCEPTION_HANDLER continuation = nullptr;
using Notification = void (CALLBACK *)(ULONG, const void*, void*);
Notification notification = nullptr;

void Require(bool value) { if (!value) std::abort(); }

BOOL WINAPI Read(HANDLE process, LPCVOID source, LPVOID destination, SIZE_T size, SIZE_T* copied) {
    ++reads;
    return ReadProcessMemory(process, source, destination, size, copied);
}

SIZE_T WINAPI Query(LPCVOID address, PMEMORY_BASIC_INFORMATION info, SIZE_T size) {
    ++queries;
    return VirtualQuery(address, info, size);
}

PVOID WINAPI Install(ULONG first, PVECTORED_EXCEPTION_HANDLER handler) {
    ++installs;
    continuation = handler;
    return AddVectoredContinueHandler(first, handler);
}

LONG NTAPI Subscribe(ULONG flags, Notification callback, void*, void** cookie) {
    Require(flags == 0);
    notification = callback;
    *cookie = &notification;
    return 0;
}

LONG NTAPI Unsubscribe(void* cookie) {
    Require(cookie == &notification);
    return 0;
}

FARPROC WINAPI Lookup(HMODULE module, LPCSTR name) {
    if (std::strcmp(name, "LdrRegisterDllNotification") == 0) return reinterpret_cast<FARPROC>(Subscribe);
    if (std::strcmp(name, "LdrUnregisterDllNotification") == 0) return reinterpret_cast<FARPROC>(Unsubscribe);
    return GetProcAddress(module, name);
}

template<typename TValue> void Put(std::uint8_t* target, TValue value) { std::memcpy(target, &value, sizeof(value)); }

std::uint8_t* Image(bool moved, std::uint32_t count) {
    auto* bytes = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x20000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(bytes != nullptr);
    IMAGE_DOS_HEADER dos{};
    dos.e_magic = IMAGE_DOS_SIGNATURE;
    dos.e_lfanew = 0x100;
    std::memcpy(bytes, &dos, sizeof(dos));
    IMAGE_NT_HEADERS64 nt{};
    nt.Signature = IMAGE_NT_SIGNATURE;
    nt.OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
    nt.OptionalHeader.SizeOfImage = 0x20000;
    nt.FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
    nt.FileHeader.NumberOfSections = moved ? 2 : 1;
    std::memcpy(bytes + 0x100, &nt, sizeof(nt));
    IMAGE_SECTION_HEADER section{};
    std::memcpy(section.Name, ".ggs", 4);
    section.VirtualAddress = 0x2000;
    section.Misc.VirtualSize = 16 + 64 * count;
    std::memcpy(bytes + 0x100 + sizeof(nt), &section, sizeof(section));
    std::memcpy(bytes + 0x2000, "APS5GGS1", 8);
    Put(bytes + 0x2008, count);
    Put(bytes + 0x200c, std::uint32_t{64});
    for (std::uint32_t index = 0; index < count; ++index) {
        auto* entry = bytes + 0x2010 + 64 * index;
        Put(entry, std::uint32_t{0x1000});
        Put(entry + 4, std::uint32_t{0x1008});
        entry[8] = 0xff;
        entry[9] = 0xff;
        entry[12] = 3;
        entry[13] = 3;
        std::memcpy(entry + 24, "\x48\x8d\x64\x24\x80\x0f\x0b\x48\x8b\x03\x0f\x0b", 12);
    }
    if (moved) {
        std::memcpy(section.Name, ".ggm", 4);
        section.VirtualAddress = 0x14000;
        section.Misc.VirtualSize = 16 + 128;
        std::memcpy(bytes + 0x100 + sizeof(nt) + sizeof(section), &section, sizeof(section));
        std::memcpy(bytes + 0x14000, "APS5GGM1", 8);
        Put(bytes + 0x14008, std::uint32_t{2});
        Put(bytes + 0x1400c, std::uint32_t{64});
        Put(bytes + 0x14010, std::uint32_t{0x1100});
        Put(bytes + 0x14014, std::uint32_t{8});
        Put(bytes + 0x14050, std::uint32_t{0x1108});
        Put(bytes + 0x14058, std::uint32_t{0x2028});
    }
    return bytes;
}

void Unrelated(std::uintptr_t address) {
    for (const auto code : {EXCEPTION_ACCESS_VIOLATION, EXCEPTION_ILLEGAL_INSTRUCTION, EXCEPTION_SINGLE_STEP, DWORD{0xe06d7363}}) {
        CONTEXT context{};
        context.Rip = address;
        context.Rsp = 0x12345678;
        context.Rbx = 0x11223344;
        context.EFlags = 0x202;
        const auto before = context;
        EXCEPTION_RECORD fault{};
        fault.ExceptionCode = code;
        fault.ExceptionAddress = reinterpret_cast<void*>(address);
        EXCEPTION_POINTERS info{&fault, &context};
        Require(!GuestGsHandleException_nid_no_patch(&info));
        Require(std::memcmp(&before, &context, sizeof(context)) == 0 && fault.ExceptionAddress == reinterpret_cast<void*>(address));
        Require(!GuestGsIsReplay_nid_no_patch(address));
        if (continuation) {
            Require(continuation(&info) == EXCEPTION_CONTINUE_SEARCH);
            Require(std::memcmp(&before, &context, sizeof(context)) == 0);
        }
    }
}

}

extern "C" {
decltype(&ReadProcessMemory) __imp_GuestGsTestReadProcessMemory = Read;
decltype(&VirtualQuery) __imp_GuestGsTestVirtualQuery = Query;
decltype(&AddVectoredContinueHandler) __imp_GuestGsTestAddVectoredContinueHandler = Install;
decltype(&GetProcAddress) __imp_GuestGsTestGetProcAddress = Lookup;
}

int main() {
    Unrelated(1);
    Unrelated(reinterpret_cast<std::uintptr_t>(&main));
    Require(reads == 0 && queries == 0 && installs == 0);
    auto* invalid = Image(true, 1);
    Put(invalid + 0x1400c, std::uint32_t{32});
    Require(!GuestGsRegisterImage_nid_no_patch(invalid));
    Require(installs == 0 && VirtualFree(invalid, 0, MEM_RELEASE));
    auto* replay = Image(false, 1024);
    Require(GuestGsRegisterImage_nid_no_patch(replay));
    Require(installs == 0);
    const auto registeredReads = reads.load();
    for (std::uint32_t index : {0u, 511u, 1023u}) {
        const auto start = reinterpret_cast<std::uintptr_t>(replay + 0x2010 + index * 64);
        Require(GuestGsIsReplay_nid_no_patch(start + 24));
        Require(GuestGsIsReplay_nid_no_patch(start + 34));
        Require(!GuestGsIsReplay_nid_no_patch(start + 23));
        Require(!GuestGsIsReplay_nid_no_patch(start + 35));
    }
    Unrelated(reinterpret_cast<std::uintptr_t>(replay + 0x19000));
    Unrelated(reinterpret_cast<std::uintptr_t>(&main));
    Require(reads == registeredReads && queries == 0);
    auto* moved = Image(true, 1);
    Require(GuestGsRegisterImage_nid_no_patch(moved));
    Require(installs == 1 && continuation && notification);
    Require(GuestGsRegisterImage_nid_no_patch(moved) && installs == 1);
    DWORD previous = 0;
    Require(VirtualProtect(moved, 4096, PAGE_NOACCESS, &previous));
    const auto allReads = reads.load();
    const auto base = reinterpret_cast<std::uintptr_t>(moved);
    CONTEXT context{};
    EXCEPTION_RECORD fault{};
    EXCEPTION_POINTERS info{&fault, &context};
    fault.ExceptionCode = EXCEPTION_ACCESS_VIOLATION;
    context.Rip = base + 0x14022;
    fault.ExceptionAddress = reinterpret_cast<void*>(context.Rip);
    Require(!GuestGsHandleException_nid_no_patch(&info));
    Require(context.Rip == base + 0x1100 && fault.ExceptionAddress == reinterpret_cast<void*>(context.Rip));
    Require(continuation(&info) == EXCEPTION_CONTINUE_SEARCH && context.Rip == base + 0x14020);
    context.Rip = base + 0x1108;
    Require(continuation(&info) == EXCEPTION_CONTINUE_SEARCH && context.Rip == base + 0x2028);
    context.Rip = base + 0x14028;
    Require(!GuestGsHandleException_nid_no_patch(&info) && context.Rip == base + 0x14028);
    Unrelated(1);
    Unrelated(reinterpret_cast<std::uintptr_t>(&main));
    Require(reads == allReads && queries == 0);
    struct Data { ULONG flags; const void* fullName; const void* baseName; void* base; ULONG size; } data{0, nullptr, nullptr, moved, 0x20000};
    std::atomic<bool> stopped{};
    std::vector<std::thread> workers;
    for (unsigned index = 0; index < 4; ++index) workers.emplace_back([&] {
        while (!stopped.load()) GuestGsIsReplay_nid_no_patch(base + 0x14020);
    });
    notification(2, &data, nullptr);
    Require(VirtualFree(moved, 0, MEM_RELEASE));
    for (unsigned index = 0; index < 100; ++index) Unrelated(base + 0x14020);
    stopped = true;
    for (auto& worker : workers) worker.join();
    Require(reads == allReads && queries == 0 && !GuestGsIsReplay_nid_no_patch(base + 0x14020));
    data.base = replay;
    notification(2, &data, nullptr);
    Require(VirtualFree(replay, 0, MEM_RELEASE));
    Require(!GuestGsIsReplay_nid_no_patch(reinterpret_cast<std::uintptr_t>(replay) + 0x2028));
    return 0;
}
