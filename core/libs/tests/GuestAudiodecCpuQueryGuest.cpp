#include "prx/libSceAudiodecCpu/include/Api.hpp"
#include "SceTypes.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr std::uint32_t LpcmBd = 0x1003u;
constexpr std::uint32_t LpcmDvd = 0x1004u;
constexpr std::uint32_t RequiredBytes = 0x190138u;
constexpr int ErrorCtrl = static_cast<int>(0x80f70009u);
constexpr int ErrorParam = static_cast<int>(0x80f7000au);
constexpr int ErrorMemory = static_cast<int>(0x80f7000eu);
constexpr int ErrorMemorySize = static_cast<int>(0x80f70008u);
constexpr int ErrorType = static_cast<int>(0x80f70001u);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "AudiodecCpu query guest: %s\n", message);
    std::abort();
}

template <typename Operation>
void RequireThrows(Operation operation, const char* message) {
    try {
        operation();
    } catch (const std::runtime_error&) {
        return;
    }
    Require(false, message);
}

struct ProtectedRegion {
    std::uint8_t* address;
    std::size_t pageSize;

    ProtectedRegion() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        address = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, pageSize * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        Require(address != nullptr, "VirtualAlloc failed");
        DWORD oldProtect = 0;
        Require(VirtualProtect(address + pageSize, pageSize, PAGE_NOACCESS, &oldProtect) != 0, "guard page protection failed");
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0, "sysconf page size failed");
        pageSize = static_cast<std::size_t>(size);
        void* allocation = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(allocation != MAP_FAILED, "mmap failed");
        address = static_cast<std::uint8_t*>(allocation);
        Require(mprotect(address + pageSize, pageSize, PROT_NONE) == 0, "guard page protection failed");
#endif
    }

    void ProtectReadOnly() {
#ifdef _WIN32
        DWORD oldProtect = 0;
        Require(VirtualProtect(address, pageSize, PAGE_READONLY, &oldProtect) != 0, "read-only protection failed");
#else
        Require(mprotect(address, pageSize, PROT_READ) == 0, "read-only protection failed");
#endif
    }

    ~ProtectedRegion() {
#ifdef _WIN32
        VirtualFree(address, 0, MEM_RELEASE);
#else
        munmap(address, pageSize * 2);
#endif
    }
};

}

int main() {
    std::uint32_t param = 0;
    AudiodecCtrl ctrl{&param, nullptr, nullptr, nullptr};
    AudiodecCpuMemory memory{0x18u, nullptr, 0xA5A5A5A5u};

    Require(sceAudiodecCpuQueryMemSize(nullptr, &memory, LpcmBd) == ErrorCtrl, "null control changed");
    errno = 71;
    RequireThrows([&] { sceAudiodecCpuQueryMemSize(reinterpret_cast<const AudiodecCtrl*>(0x10), &memory, LpcmBd); }, "unreadable control accepted");
    Require(memory.ui_memory_size == 0xA5A5A5A5u, "invalid control modified memory result");
    Require(errno == 71, "invalid control changed errno");
    RequireThrows([&] { sceAudiodecCpuInternalQueryMemSize(&ctrl, reinterpret_cast<AudiodecCpuMemory*>(0x10), LpcmBd); }, "unreadable output struct accepted");
    Require(errno == 71, "invalid output struct changed errno");

    AudiodecCtrl noParam{};
    Require(sceAudiodecCpuQueryMemSize(&noParam, &memory, LpcmBd) == ErrorParam, "null decoder parameter order changed");
    Require(sceAudiodecCpuQueryMemSize(&ctrl, nullptr, LpcmBd) == ErrorMemory, "null output changed");
    AudiodecCpuMemory wrongSize{0x10u, nullptr, 7u};
    Require(sceAudiodecCpuQueryMemSize(&ctrl, &wrongSize, LpcmBd) == ErrorMemorySize, "invalid size changed");
    Require(wrongSize.ui_memory_size == 7u, "invalid size altered result");
    Require(sceAudiodecCpuQueryMemSize(&ctrl, &memory, 0x1009u) == ErrorType, "invalid codec type changed");
    Require(memory.ui_memory_size == 0xA5A5A5A5u, "invalid codec type altered result");

    Require(sceAudiodecCpuQueryMemSize(&ctrl, &memory, LpcmBd) == 0, "valid Blu-ray size query failed");
    Require(memory.ui_memory_size == RequiredBytes, "valid Blu-ray size changed");
    Require(memory.ui_size == 0x18u && memory.p_memory == nullptr, "valid query altered other output fields");
    memory.ui_memory_size = 0;
    Require(sceAudiodecCpuInternalQueryMemSize(&ctrl, &memory, LpcmDvd) == 0, "valid DVD size query failed");
    Require(memory.ui_memory_size == RequiredBytes, "valid DVD size changed");
    Require(errno == 71, "valid query changed errno");

    ProtectedRegion guarded;
    auto* atEnd = reinterpret_cast<AudiodecCpuMemory*>(guarded.address + guarded.pageSize - sizeof(AudiodecCpuMemory));
    *atEnd = {0x18u, nullptr, 0xA5A5A5A5u};
    Require(sceAudiodecCpuQueryMemSize(&ctrl, atEnd, LpcmBd) == 0, "valid output at guarded page end rejected");
    Require(atEnd->ui_memory_size == RequiredBytes, "valid guarded output changed");
    auto* crossing = reinterpret_cast<AudiodecCpuMemory*>(guarded.address + guarded.pageSize - offsetof(AudiodecCpuMemory, ui_memory_size) - sizeof(std::uint32_t) + 1);
    const std::uint32_t crossingSize = 0x18u;
    std::memcpy(crossing, &crossingSize, sizeof(crossingSize));
    RequireThrows([&] { sceAudiodecCpuQueryMemSize(&ctrl, crossing, LpcmBd); }, "cross-page output struct accepted");

    auto* readOnly = reinterpret_cast<AudiodecCpuMemory*>(guarded.address);
    *readOnly = {0x18u, nullptr, 0xA5A5A5A5u};
    guarded.ProtectReadOnly();
    RequireThrows([&] { sceAudiodecCpuQueryMemSize(&ctrl, readOnly, LpcmBd); }, "read-only output accepted");
    Require(readOnly->ui_memory_size == 0xA5A5A5A5u, "read-only output modified");
    Require(errno == 71, "protected guest copies changed errno");

    std::puts("AudiodecCpu guest memory-size queries passed");
}
