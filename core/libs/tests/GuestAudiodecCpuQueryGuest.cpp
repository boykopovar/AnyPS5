#include "prx/libSceAudiodecCpu/include/Api.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "SceTypes.hpp"
#include "Testing.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

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

void ClobberErrno(std::uintptr_t, std::size_t) {
    errno = 99;
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
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberErrno);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(ClobberErrno);
    std::uint32_t param = 0;
    AudiodecCtrl ctrl{&param, nullptr, nullptr, nullptr};
    AudiodecCpuMemory memory{0x18u, nullptr, 0xA5A5A5A5u};

    Require(sceAudiodecCpuQueryMemSize(nullptr, &memory, LpcmBd) == ErrorCtrl, "null control changed");
    errno = 71;
    ExpectErr<std::runtime_error>([&] { sceAudiodecCpuQueryMemSize(reinterpret_cast<const AudiodecCtrl*>(0x10), &memory, LpcmBd); }, "unreadable control accepted");
    Require(memory.ui_memory_size == 0xA5A5A5A5u, "invalid control modified memory result");
    Require(errno == 71, "invalid control changed errno");
    ExpectErr<std::runtime_error>([&] { sceAudiodecCpuInternalQueryMemSize(&ctrl, reinterpret_cast<AudiodecCpuMemory*>(0x10), LpcmBd); }, "unreadable output struct accepted");
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
    ExpectErr<std::runtime_error>([&] { sceAudiodecCpuQueryMemSize(&ctrl, crossing, LpcmBd); }, "cross-page output struct accepted");

    auto* readOnly = reinterpret_cast<AudiodecCpuMemory*>(guarded.address);
    *readOnly = {0x18u, nullptr, 0xA5A5A5A5u};
    guarded.ProtectReadOnly();
    ExpectErr<std::runtime_error>([&] { sceAudiodecCpuQueryMemSize(&ctrl, readOnly, LpcmBd); }, "read-only output accepted");
    Require(readOnly->ui_memory_size == 0xA5A5A5A5u, "read-only output modified");
    Require(errno == 71, "protected guest copies changed errno");

    std::vector<std::uint8_t> lpcmMem(0x200, 0);
    std::array<std::uint32_t, 10> lpcmParam{};
    std::array<std::uint32_t, 8> lpcmBsi{};
    std::array<std::uint8_t, 960> auBuf{};
    std::array<float, 480> pcmBuf{};
    auto initValid = [&] {
        lpcmParam.fill(0);
        lpcmParam[0] = 0x28u;
        lpcmParam[7] = 0xffffffffu;
        lpcmParam[8] = 0x01010102u;
        lpcmParam[9] = 0xffffffffu;
        const std::uint64_t header = (std::uint64_t{2} << 32) | (std::uint64_t{1} << 40) | (std::uint64_t{1} << 48) | (std::uint64_t{1} << 56);
        std::memcpy(&lpcmParam[6], &header, sizeof(header));
        lpcmParam[8] = 100u;
        lpcmParam[9] = 200u;
        lpcmBsi.fill(0);
        auBuf.fill(0x11);
        pcmBuf.fill(0.0f);
    };
    initValid();
    AudiodecAuInfo auInfo{0x18u, auBuf.data(), static_cast<std::uint32_t>(auBuf.size())};
    AudiodecPcmItem pcmItem{0x18u, pcmBuf.data(), static_cast<std::uint32_t>(pcmBuf.size() * sizeof(float))};
    AudiodecCtrl validCtrl{lpcmParam.data(), lpcmBsi.data(), &auInfo, &pcmItem};
    AudiodecCpuMemory validMem{0x18u, lpcmMem.data(), RequiredBytes};

    errno = 71;
    Require(sceAudiodecCpuInitDecoder(&validCtrl, &validMem, LpcmBd) == 0 && errno == 71, "valid InitDecoder failed");
    errno = 71;
    Require(sceAudiodecCpuDecode(&validCtrl, &validMem, LpcmBd) == 0 && errno == 71, "valid Decode failed");
    Require(auInfo.ui_au_size == 960 && pcmItem.ui_pcm_size == 1920, "valid Decode sizes changed");
    errno = 71;
    Require(sceAudiodecCpuInternalClearContext(&validMem, LpcmDvd) == 0 && errno == 71, "valid ClearContext failed");

    auto* badPtr = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x10));
    errno = 71;
    ExpectErr<std::runtime_error>([&] { sceAudiodecCpuInitDecoder(reinterpret_cast<const AudiodecCtrl*>(badPtr), &validMem, LpcmBd); }, "bad InitDecoder ctrl");
    Require(errno == 71, "bad InitDecoder ctrl changed errno");
    errno = 71;
    ExpectErr<std::runtime_error>([&] { sceAudiodecCpuInternalInitDecoder(&validCtrl, reinterpret_cast<const AudiodecCpuMemory*>(badPtr), LpcmBd); }, "bad InitDecoder mem");
    Require(errno == 71, "bad InitDecoder mem changed errno");
    AudiodecCpuMemory badMemPtr{0x18u, badPtr, RequiredBytes};
    errno = 71;
    ExpectErr<std::runtime_error>([&] { sceAudiodecCpuInitDecoder(&validCtrl, &badMemPtr, LpcmBd); }, "bad InitDecoder p_memory");
    Require(errno == 71, "bad InitDecoder p_memory changed errno");
    errno = 71;
    ExpectErr<std::runtime_error>([&] { sceAudiodecCpuClearContext(reinterpret_cast<const AudiodecCpuMemory*>(badPtr), LpcmBd); }, "bad ClearContext mem");
    Require(errno == 71, "bad ClearContext mem changed errno");
    errno = 71;
    ExpectErr<std::runtime_error>([&] { sceAudiodecCpuClearContext(&badMemPtr, LpcmBd); }, "bad ClearContext p_memory");
    Require(errno == 71, "bad ClearContext p_memory changed errno");

    initValid();
    auInfo = {0x18u, auBuf.data(), static_cast<std::uint32_t>(auBuf.size())};
    pcmItem = {0x18u, pcmBuf.data(), static_cast<std::uint32_t>(pcmBuf.size() * sizeof(float))};
    auto checkBadDecode = [&](const AudiodecCtrl& c, const AudiodecCpuMemory& m, const char* msg) {
        errno = 71;
        ExpectErr<std::runtime_error>([&] { sceAudiodecCpuInternalDecode(&c, &m, LpcmBd); }, msg);
        Require(errno == 71, msg);
    };
    checkBadDecode({badPtr, lpcmBsi.data(), &auInfo, &pcmItem}, validMem, "bad pParam");
    checkBadDecode({lpcmParam.data(), badPtr, &auInfo, &pcmItem}, validMem, "bad pBsiInfo");
    checkBadDecode({lpcmParam.data(), lpcmBsi.data(), reinterpret_cast<AudiodecAuInfo*>(badPtr), &pcmItem}, validMem, "bad pAuInfo");
    checkBadDecode({lpcmParam.data(), lpcmBsi.data(), &auInfo, reinterpret_cast<AudiodecPcmItem*>(badPtr)}, validMem, "bad pPcmItem");
    AudiodecAuInfo badAuAddr{0x18u, badPtr, 960u};
    checkBadDecode({lpcmParam.data(), lpcmBsi.data(), &badAuAddr, &pcmItem}, validMem, "bad p_au_addr");
    AudiodecPcmItem badPcmAddr{0x18u, badPtr, 1920u};
    checkBadDecode({lpcmParam.data(), lpcmBsi.data(), &auInfo, &badPcmAddr}, validMem, "bad p_pcm_addr");
    checkBadDecode(validCtrl, badMemPtr, "bad Decode p_memory");
    auto* readOnlyBsi = guarded.address;
    checkBadDecode({lpcmParam.data(), readOnlyBsi, &auInfo, &pcmItem}, validMem, "read-only decode pBsiInfo");
    Require(lpcmParam[1] == 0 && errno == 71, "failed Decode mutated pParam");

    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
    std::puts("AudiodecCpu guest memory-size queries passed");
}
