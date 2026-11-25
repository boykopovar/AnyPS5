#include "prx/libSceAudiodecCpu/include/Api.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "SceTypes.hpp"

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
constexpr std::uint32_t Hevag = 0x1008u;
constexpr std::uint32_t RequiredBytes = 0x190138u;
constexpr std::uint32_t HevagRequiredBytes = 0x1b0u;
constexpr int ErrorCtrl = static_cast<int>(0x80f70009u);
constexpr int ErrorParam = static_cast<int>(0x80f7000au);
constexpr int ErrorParamSize = static_cast<int>(0x80f70004u);
constexpr int ErrorMemory = static_cast<int>(0x80f7000eu);
constexpr int ErrorMemorySize = static_cast<int>(0x80f70008u);
constexpr int ErrorType = static_cast<int>(0x80f70001u);
constexpr int ErrorHevagPcmFormat = static_cast<int>(0x80f70012u);
constexpr int ErrorMemoryBufferSize = static_cast<int>(0x80f70015u);
constexpr int ErrorLpcmParam = static_cast<int>(0x80f70100u);
constexpr int ErrorHevagChannels = static_cast<int>(0x80f70c00u);
constexpr int ErrorHevagSampleRate = static_cast<int>(0x80f70c01u);
constexpr int ErrorHevagInterleave = static_cast<int>(0x80f70c06u);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "AudiodecCpu query guest: %s\n", message);
    std::abort();
}

void ClobberErrno(std::uintptr_t, std::size_t) {
    errno = 99;
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

std::uint64_t MakeLpcmHeader(std::uint32_t channels, std::uint32_t rate, std::uint32_t bits, std::uint32_t frames) {
    return (static_cast<std::uint64_t>(channels) << 32) |
           (static_cast<std::uint64_t>(rate) << 40) |
           (static_cast<std::uint64_t>(bits) << 48) |
           (static_cast<std::uint64_t>(frames) << 56);
}

void TestHevagLifecycle(ProtectedRegion& guarded) {
    std::array<std::uint8_t, 0x40> hevagParam{};
    std::array<std::uint8_t, 0x30> hevagBsi{};
    std::vector<std::uint8_t> hevagMem(0x200u, 0xA5u);

    auto initValidHevagParam = [&] {
        hevagParam.fill(0);
        const std::uint32_t paramSize = 0x40u;
        std::memcpy(hevagParam.data() + 0x00, &paramSize, sizeof(paramSize));
        hevagParam[0x14] = 0x00u;
        hevagParam[0x15] = 0x00u;
        hevagParam[0x16] = 0xbbu;
        hevagParam[0x17] = 0x80u;
        hevagParam[0x22] = 2u;
        const std::uint32_t pcmFormat = 2u;
        std::memcpy(hevagParam.data() + 0x34, &pcmFormat, sizeof(pcmFormat));
        hevagParam[0x38] = 1u;
    };
    initValidHevagParam();

    AudiodecCtrl hevagCtrl{hevagParam.data(), hevagBsi.data(), nullptr, nullptr};
    AudiodecCpuMemory hevagQueryMem{0x18u, nullptr, 0xA5A5A5A5u};
    errno = 71;
    Require(sceAudiodecCpuQueryMemSize(&hevagCtrl, &hevagQueryMem, Hevag) == 0 && errno == 71, "HE-VAG QueryMemSize failed");
    Require(hevagQueryMem.ui_memory_size == HevagRequiredBytes, "HE-VAG QueryMemSize size mismatch");
    Require(hevagQueryMem.ui_size == 0x18u && hevagQueryMem.p_memory == nullptr, "HE-VAG QueryMemSize altered other fields");

    hevagQueryMem.ui_memory_size = 0xA5A5A5A5u;
    errno = 71;
    Require(sceAudiodecCpuInternalQueryMemSize(&hevagCtrl, &hevagQueryMem, Hevag) == 0 && errno == 71, "HE-VAG InternalQueryMemSize failed");
    Require(hevagQueryMem.ui_memory_size == HevagRequiredBytes, "HE-VAG InternalQueryMemSize size mismatch");

    AudiodecCpuMemory smallHevagMem{0x18u, hevagMem.data(), HevagRequiredBytes - 1u};
    Require(sceAudiodecCpuInitDecoder(&hevagCtrl, &smallHevagMem, Hevag) == ErrorMemoryBufferSize, "HE-VAG InitDecoder accepted small memory");
    Require(sceAudiodecCpuClearContext(&smallHevagMem, Hevag) == ErrorMemoryBufferSize, "HE-VAG ClearContext accepted small memory");

    AudiodecCpuMemory validHevagMem{0x18u, hevagMem.data() + 3, HevagRequiredBytes};
    std::uint32_t badSize = 0x38u;
    std::memcpy(hevagParam.data(), &badSize, sizeof(badSize));
    Require(sceAudiodecCpuInitDecoder(&hevagCtrl, &validHevagMem, Hevag) == ErrorParamSize, "HE-VAG InitDecoder accepted bad param size");

    initValidHevagParam();
    hevagParam[0x22] = 0u;
    Require(sceAudiodecCpuInitDecoder(&hevagCtrl, &validHevagMem, Hevag) == ErrorHevagChannels, "HE-VAG InitDecoder accepted 0 channels");

    initValidHevagParam();
    hevagParam[0x22] = 9u;
    Require(sceAudiodecCpuInitDecoder(&hevagCtrl, &validHevagMem, Hevag) == ErrorHevagChannels, "HE-VAG InitDecoder accepted >8 channels");

    initValidHevagParam();
    hevagParam[0x16] = 0x07u;
    hevagParam[0x17] = 0xcfu;
    Require(sceAudiodecCpuInitDecoder(&hevagCtrl, &validHevagMem, Hevag) == ErrorHevagSampleRate, "HE-VAG InitDecoder accepted <2000 Hz");

    initValidHevagParam();
    hevagParam[0x14] = 0x00u;
    hevagParam[0x15] = 0x02u;
    hevagParam[0x16] = 0xeeu;
    hevagParam[0x17] = 0x01u;
    Require(sceAudiodecCpuInitDecoder(&hevagCtrl, &validHevagMem, Hevag) == ErrorHevagSampleRate, "HE-VAG InitDecoder accepted >192000 Hz");

    initValidHevagParam();
    const std::uint32_t badFormat = 3u;
    std::memcpy(hevagParam.data() + 0x34, &badFormat, sizeof(badFormat));
    Require(sceAudiodecCpuInitDecoder(&hevagCtrl, &validHevagMem, Hevag) == ErrorHevagPcmFormat, "HE-VAG InitDecoder accepted bad PCM format");

    initValidHevagParam();
    hevagParam[0x38] = 2u;
    Require(sceAudiodecCpuInitDecoder(&hevagCtrl, &validHevagMem, Hevag) == ErrorHevagInterleave, "HE-VAG InitDecoder accepted bad interleave");

    initValidHevagParam();
    errno = 71;
    Require(sceAudiodecCpuInitDecoder(&hevagCtrl, &validHevagMem, Hevag) == 0 && errno == 71, "valid HE-VAG InitDecoder failed");
    const std::uintptr_t alignedOff = ((reinterpret_cast<std::uintptr_t>(validHevagMem.p_memory) + 7u) & ~static_cast<std::uintptr_t>(7u)) - reinterpret_cast<std::uintptr_t>(hevagMem.data());
    Require(hevagMem[alignedOff + 0x10] == 2u && hevagMem[alignedOff + 0x28] == 1u, "HE-VAG InitDecoder did not store state");

    std::memset(hevagMem.data() + alignedOff + 0x30, 0xA5, 0x80);
    errno = 71;
    Require(sceAudiodecCpuClearContext(&validHevagMem, Hevag) == 0 && errno == 71, "valid HE-VAG ClearContext failed");
    for (std::size_t i = 0; i < 0x80; ++i) {
        Require(hevagMem[alignedOff + 0x30 + i] == 0u, "HE-VAG ClearContext did not clear history");
    }

    AudiodecCpuMemory roHevagMem{0x18u, guarded.address, HevagRequiredBytes};
    errno = 71;
    RequireThrows([&] { sceAudiodecCpuClearContext(&roHevagMem, Hevag); }, "read-only HE-VAG ClearContext accepted");
    Require(errno == 71, "read-only HE-VAG ClearContext changed errno");
}

void TestDvdLpcmDecode(ProtectedRegion& guarded) {
    std::vector<std::uint8_t> dvdMem(0x200u, 0u);
    std::array<std::uint32_t, 10> dvdParam{};
    std::array<std::uint32_t, 8> dvdBsi{};
    std::array<std::uint8_t, 480> auBuf{};
    std::array<float, 240> pcmBuf{};

    auto resetDvd = [&](std::uint32_t rate, std::uint32_t bits, std::uint32_t frames, std::uint32_t ptsHi, std::uint32_t ptsLo) {
        dvdParam.fill(0);
        dvdParam[0] = 0x28u;
        const std::uint64_t header = MakeLpcmHeader(2u, rate, bits, frames);
        std::memcpy(&dvdParam[6], &header, sizeof(header));
        dvdParam[8] = ptsHi;
        dvdParam[9] = ptsLo;
        dvdBsi.fill(0xA5A5A5A5u);
        pcmBuf.fill(0.0f);
    };

    AudiodecAuInfo auInfo{0x18u, auBuf.data(), static_cast<std::uint32_t>(auBuf.size())};
    AudiodecPcmItem pcmItem{0x18u, pcmBuf.data(), static_cast<std::uint32_t>(pcmBuf.size() * sizeof(float))};
    AudiodecCtrl ctrl{dvdParam.data(), dvdBsi.data(), &auInfo, &pcmItem};
    AudiodecCpuMemory mem{0x18u, dvdMem.data(), RequiredBytes};

    resetDvd(1u, 1u, 1u, 0u, 500u);
    Require(sceAudiodecCpuInitDecoder(&ctrl, &mem, LpcmDvd) == 0, "DVD InitDecoder failed");
    auBuf.fill(0);
    auBuf[0] = 0x80u;
    auBuf[1] = 0x00u;
    auBuf[2] = 0x40u;
    auBuf[3] = 0x00u;
    errno = 71;
    Require(sceAudiodecCpuDecode(&ctrl, &mem, LpcmDvd) == 0 && errno == 71, "DVD 16-bit Decode failed");
    Require(dvdParam[1] == 2u && dvdParam[2] == 1u && dvdParam[3] == 1u && dvdParam[4] == 320u, "DVD 16-bit param mismatch");
    Require(auInfo.ui_au_size == 320u && pcmItem.ui_pcm_size == 640u, "DVD 16-bit sizes mismatch");
    Require(pcmBuf[0] == -1.0f && pcmBuf[1] == 0.5f && pcmBuf[2] == 0.0f, "DVD 16-bit sample mismatch");
    Require(dvdBsi[6] == 0u && dvdBsi[7] == 500u && dvdBsi[3] == 640u, "DVD 16-bit BSI mismatch");

    resetDvd(1u, 1u, 1u, 0xffffffffu, 0xffffffffu);
    auInfo.ui_au_size = 320u;
    pcmItem.ui_pcm_size = 640u;
    Require(sceAudiodecCpuInternalDecode(&ctrl, &mem, LpcmDvd) == 0, "DVD PTS continuation failed");
    Require(dvdBsi[6] == 0u && dvdBsi[7] == 650u, "DVD PTS did not advance by 150");

    resetDvd(1u, 2u, 1u, 0xffffffffu, 0xffffffffu);
    auBuf.fill(0);
    auBuf[0] = 0x00u;
    auBuf[1] = 0x00u;
    auBuf[8] = 0x10u;
    auInfo.ui_au_size = 400u;
    pcmItem.ui_pcm_size = 640u;
    Require(sceAudiodecCpuDecode(&ctrl, &mem, LpcmDvd) == 0, "DVD 20-bit Decode failed");
    Require(dvdParam[4] == 400u && pcmItem.ui_pcm_size == 640u, "DVD 20-bit sizes mismatch");
    Require(pcmBuf[0] == 0x1p-19f, "DVD 20-bit sample mismatch");

    resetDvd(1u, 3u, 1u, 0xffffffffu, 0xffffffffu);
    auBuf.fill(0);
    auBuf[0] = 0x00u;
    auBuf[1] = 0x00u;
    auBuf[8] = 0x01u;
    auInfo.ui_au_size = 480u;
    pcmItem.ui_pcm_size = 640u;
    Require(sceAudiodecCpuDecode(&ctrl, &mem, LpcmDvd) == 0, "DVD 24-bit Decode failed");
    Require(dvdParam[4] == 480u && pcmItem.ui_pcm_size == 640u, "DVD 24-bit sizes mismatch");
    Require(pcmBuf[0] == 0x1p-23f, "DVD 24-bit sample mismatch");

    resetDvd(3u, 1u, 1u, 0xffffffffu, 0xffffffffu);
    Require(sceAudiodecCpuDecode(&ctrl, &mem, LpcmDvd) == ErrorLpcmParam, "DVD accepted 192 kHz");

    const std::uint64_t badCh8 = MakeLpcmHeader(8u, 1u, 3u, 1u);
    std::memcpy(&dvdParam[6], &badCh8, sizeof(badCh8));
    Require(sceAudiodecCpuDecode(&ctrl, &mem, LpcmDvd) == ErrorLpcmParam, "DVD accepted 48 kHz 8ch 24-bit");

    resetDvd(1u, 1u, 1u, 0xffffffffu, 0xffffffffu);
    auInfo.ui_au_size = 320u;
    pcmItem.ui_pcm_size = 640u;
    AudiodecCtrl roBsiCtrl{dvdParam.data(), guarded.address, &auInfo, &pcmItem};
    errno = 71;
    RequireThrows([&] { sceAudiodecCpuDecode(&roBsiCtrl, &mem, LpcmDvd); }, "read-only DVD BSI accepted");
    Require(dvdParam[1] == 0u && errno == 71, "failed DVD Decode mutated pParam or errno");
}

}

int main() {
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberErrno);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(ClobberErrno);
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
        const std::uint64_t header = MakeLpcmHeader(2u, 1u, 1u, 1u);
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
    RequireThrows([&] { sceAudiodecCpuInitDecoder(reinterpret_cast<const AudiodecCtrl*>(badPtr), &validMem, LpcmBd); }, "bad InitDecoder ctrl");
    Require(errno == 71, "bad InitDecoder ctrl changed errno");
    errno = 71;
    RequireThrows([&] { sceAudiodecCpuInternalInitDecoder(&validCtrl, reinterpret_cast<const AudiodecCpuMemory*>(badPtr), LpcmBd); }, "bad InitDecoder mem");
    Require(errno == 71, "bad InitDecoder mem changed errno");
    AudiodecCpuMemory badMemPtr{0x18u, badPtr, RequiredBytes};
    errno = 71;
    RequireThrows([&] { sceAudiodecCpuInitDecoder(&validCtrl, &badMemPtr, LpcmBd); }, "bad InitDecoder p_memory");
    Require(errno == 71, "bad InitDecoder p_memory changed errno");
    errno = 71;
    RequireThrows([&] { sceAudiodecCpuClearContext(reinterpret_cast<const AudiodecCpuMemory*>(badPtr), LpcmBd); }, "bad ClearContext mem");
    Require(errno == 71, "bad ClearContext mem changed errno");
    errno = 71;
    RequireThrows([&] { sceAudiodecCpuClearContext(&badMemPtr, LpcmBd); }, "bad ClearContext p_memory");
    Require(errno == 71, "bad ClearContext p_memory changed errno");

    initValid();
    auInfo = {0x18u, auBuf.data(), static_cast<std::uint32_t>(auBuf.size())};
    pcmItem = {0x18u, pcmBuf.data(), static_cast<std::uint32_t>(pcmBuf.size() * sizeof(float))};
    auto checkBadDecode = [&](const AudiodecCtrl& c, const AudiodecCpuMemory& m, const char* msg) {
        errno = 71;
        RequireThrows([&] { sceAudiodecCpuInternalDecode(&c, &m, LpcmBd); }, msg);
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

    TestHevagLifecycle(guarded);
    TestDvdLpcmDecode(guarded);

    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
    std::puts("AudiodecCpu guest memory-size queries passed");
}
