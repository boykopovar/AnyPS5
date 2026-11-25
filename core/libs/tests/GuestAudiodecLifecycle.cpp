#include "prx/libSceAudiodec/include/Api.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

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

constexpr std::uint32_t Mp3 = 2;
constexpr std::uint32_t M4aac = 3;
constexpr std::uint32_t ConfigAdts = 1;
constexpr std::uint32_t ConfigRaw = 2;
constexpr std::int32_t ErrorApiFail = static_cast<std::int32_t>(0x807F0000);
constexpr std::int32_t ErrorArg = static_cast<std::int32_t>(0x807F0002);
constexpr std::int32_t ErrorInvalidPcmPointer = static_cast<std::int32_t>(0x807F000E);
constexpr std::int32_t ErrorInvalidHandle = static_cast<std::int32_t>(0x807F000F);
constexpr std::int32_t ErrorInvalidPcmSize = static_cast<std::int32_t>(0x807F0012);
constexpr std::int32_t ErrorM4aacInvalidSamplingFreq = static_cast<std::int32_t>(0x807F0300);
constexpr std::int32_t Slots = 64;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Audiodec lifecycle check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

struct ProtectedRegion {
    std::uint8_t* address;
    std::size_t pageSize;

    ProtectedRegion() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        address = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, pageSize * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        Require(address != nullptr);
        DWORD oldProtect = 0;
        Require(VirtualProtect(address + pageSize, pageSize, PAGE_NOACCESS, &oldProtect) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        pageSize = static_cast<std::size_t>(size);
        void* allocation = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(allocation != MAP_FAILED);
        address = static_cast<std::uint8_t*>(allocation);
        Require(mprotect(address + pageSize, pageSize, PROT_NONE) == 0);
#endif
    }

    void ProtectReadOnly() {
#ifdef _WIN32
        DWORD oldProtect = 0;
        Require(VirtualProtect(address, pageSize, PAGE_READONLY, &oldProtect) != 0);
#else
        Require(mprotect(address, pageSize, PROT_READ) == 0);
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

std::int32_t CreateMp3() {
    AudiodecParamMp3 param{sizeof(param), 1};
    AudiodecMp3Info info{};
    info.ui_size = sizeof(info);
    AudiodecCtrl ctrl{&param, &info, nullptr, nullptr};
    return sceAudiodecCreateDecoder(&ctrl, Mp3);
}

std::int32_t CreateAac(std::uint32_t configNumber, std::uint32_t samplingFreqIndex, AudiodecM4aacInfo& info) {
    AudiodecParamM4aac param{sizeof(param), 1, configNumber, samplingFreqIndex, 2, 0};
    info = {sizeof(info), 0, 0, 0, 0};
    AudiodecCtrl ctrl{&param, &info, nullptr, nullptr};
    return sceAudiodecCreateDecoder(&ctrl, M4aac);
}

void CheckM4aacBsiOnPcmErrors() {
    constexpr std::uint32_t SentinelFreq = 0xA5A5A5A5u;
    constexpr std::uint32_t SentinelChannels = 0x5A5A5A5Au;
    constexpr std::uint32_t SentinelHeaac = 0xCAFEBABEu;
    constexpr std::int32_t SentinelResult = -12345;

    Require(sceAudiodecInitLibrary(M4aac) == 0);
    AudiodecParamM4aac param{sizeof(param), 1, ConfigAdts, 3u, 2u, 0u};
    AudiodecM4aacInfo createInfo{sizeof(createInfo), 0u, 0u, 0u, 0};
    AudiodecCtrl createCtrl{&param, &createInfo, nullptr, nullptr};
    const std::int32_t handle = sceAudiodecCreateDecoder(&createCtrl, M4aac);
    Require(handle > 0);

    std::array<std::uint8_t, 11> adtsFrame{0xFFu, 0xF1u, 0x4Cu, 0x40u, 0x01u, 0x7Fu, 0xFCu, 0x00u, 0x00u, 0x00u, 0x07u};
    std::array<std::uint8_t, 4096> pcm{};

    auto makeSentinelBsi = [] {
        return AudiodecM4aacInfo{sizeof(AudiodecM4aacInfo), SentinelFreq, SentinelChannels, SentinelHeaac, SentinelResult};
    };
    auto checkSentinelUnchanged = [](const AudiodecM4aacInfo& bsi) {
        Require(bsi.ui_sampling_freq == SentinelFreq && bsi.ui_number_of_channels == SentinelChannels && bsi.ui_heaac == SentinelHeaac && bsi.i_result == SentinelResult);
    };

    AudiodecM4aacInfo bsi = makeSentinelBsi();
    AudiodecAuInfo au{sizeof(au), adtsFrame.data(), static_cast<std::uint32_t>(adtsFrame.size())};
    AudiodecPcmItem pcmItem{sizeof(pcmItem), pcm.data(), 4u};
    AudiodecCtrl decodeCtrl{&param, &bsi, &au, &pcmItem};
    Require(sceAudiodecDecode(handle, &decodeCtrl) == ErrorInvalidPcmSize);
    checkSentinelUnchanged(bsi);

    ProtectedRegion readOnlyRegion;
    auto* readOnlyBsi = reinterpret_cast<AudiodecM4aacInfo*>(readOnlyRegion.address);
    *readOnlyBsi = makeSentinelBsi();
    readOnlyRegion.ProtectReadOnly();
    au = {sizeof(au), adtsFrame.data(), static_cast<std::uint32_t>(adtsFrame.size())};
    pcmItem = {sizeof(pcmItem), pcm.data(), 4u};
    AudiodecCtrl roBsiCtrl{&param, readOnlyBsi, &au, &pcmItem};
    Require(sceAudiodecDecode(handle, &roBsiCtrl) == ErrorInvalidPcmSize);
    checkSentinelUnchanged(*readOnlyBsi);

    bsi = makeSentinelBsi();
    au = {sizeof(au), adtsFrame.data(), static_cast<std::uint32_t>(adtsFrame.size())};
    pcmItem = {sizeof(pcmItem), reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x10)), static_cast<std::uint32_t>(pcm.size())};
    Require(sceAudiodecDecode(handle, &decodeCtrl) == ErrorInvalidPcmPointer);
    checkSentinelUnchanged(bsi);

    bsi = makeSentinelBsi();
    au = {sizeof(au), adtsFrame.data(), static_cast<std::uint32_t>(adtsFrame.size())};
    pcmItem = {sizeof(pcmItem), readOnlyRegion.address, static_cast<std::uint32_t>(pcm.size())};
    Require(sceAudiodecDecode(handle, &decodeCtrl) == ErrorInvalidPcmPointer);
    checkSentinelUnchanged(bsi);

    std::array<std::uint8_t, 11> corruptAdts{};
    bsi = makeSentinelBsi();
    au = {sizeof(au), corruptAdts.data(), static_cast<std::uint32_t>(corruptAdts.size())};
    pcmItem = {sizeof(pcmItem), pcm.data(), static_cast<std::uint32_t>(pcm.size())};
    Require(sceAudiodecDecode(handle, &decodeCtrl) == ErrorApiFail);
    Require(bsi.i_result == -4);

    bsi = makeSentinelBsi();
    au = {sizeof(au), adtsFrame.data(), static_cast<std::uint32_t>(adtsFrame.size())};
    pcmItem = {sizeof(pcmItem), pcm.data(), static_cast<std::uint32_t>(pcm.size())};
    Require(sceAudiodecDecode(handle, &decodeCtrl) == 0);
    Require(bsi.i_result == 0 && bsi.ui_sampling_freq == 48000u && bsi.ui_number_of_channels == 1u && bsi.ui_heaac == 0u);
    Require(au.ui_au_size == adtsFrame.size() && pcmItem.ui_pcm_size == 2048u);

    Require(sceAudiodecDeleteDecoder(handle) == 0);
    Require(sceAudiodecTermLibrary(M4aac) == 0);
}

}

int main() {
    Require(sceAudiodecTermLibrary(Mp3) == 0);
    Require(CreateMp3() == ErrorArg);
    Require(sceAudiodecInitLibrary(Mp3) == 0);

    for (const std::int32_t handle : {0, -1, Slots + 1}) Require(sceAudiodecDeleteDecoder(handle) == ErrorInvalidHandle);
    for (std::int32_t slot = 1; slot <= Slots; ++slot) Require(CreateMp3() == slot);
    Require(CreateMp3() == ErrorApiFail);
    Require(sceAudiodecDeleteDecoder(Slots) == 0);
    Require(sceAudiodecDeleteDecoder(Slots) == ErrorInvalidHandle);
    Require(CreateMp3() == Slots);
    for (std::int32_t slot = 1; slot <= Slots; ++slot) Require(sceAudiodecDeleteDecoder(slot) == 0);

    Require(sceAudiodecInitLibrary(M4aac) == 0);
    AudiodecM4aacInfo info{};
    Require(CreateAac(ConfigRaw, 13, info) == ErrorM4aacInvalidSamplingFreq);
    const std::int32_t adts = CreateAac(ConfigAdts, 13, info);
    Require(adts > 0);
    Require(sceAudiodecDeleteDecoder(adts) == 0);
    const std::int32_t raw = CreateAac(ConfigRaw, 12, info);
    Require(raw > 0 && info.ui_sampling_freq == 7350);
    Require(sceAudiodecDeleteDecoder(raw) == 0);
    Require(sceAudiodecTermLibrary(M4aac) == 0);
    Require(sceAudiodecTermLibrary(Mp3) == 0);

    CheckM4aacBsiOnPcmErrors();

    std::puts("Audiodec lifecycle tests passed");
    return 0;
}
