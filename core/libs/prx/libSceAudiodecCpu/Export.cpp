#include "prx/libSceAudiodecCpu/include/Api.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int ErrorAuSize = static_cast<int>(0x80f70006);
constexpr int ErrorPcmSize = static_cast<int>(0x80f70007);
constexpr int ErrorMemorySize = static_cast<int>(0x80f70008);
constexpr int ErrorType = static_cast<int>(0x80f70001);
constexpr int ErrorClearMemory = static_cast<int>(0x80f70002);
constexpr int ErrorParamSize = static_cast<int>(0x80f70004);
constexpr int ErrorBsiInfoSize = static_cast<int>(0x80f70005);
constexpr int ErrorCtrl = static_cast<int>(0x80f70009);
constexpr int ErrorParam = static_cast<int>(0x80f7000a);
constexpr int ErrorBsiInfo = static_cast<int>(0x80f7000b);
constexpr int ErrorAuInfo = static_cast<int>(0x80f7000c);
constexpr int ErrorPcmItem = static_cast<int>(0x80f7000d);
constexpr int ErrorMemory = static_cast<int>(0x80f7000e);
constexpr int ErrorAuAddress = static_cast<int>(0x80f7000f);
constexpr int ErrorPcmAddress = static_cast<int>(0x80f70010);
constexpr int ErrorMemoryAddress = static_cast<int>(0x80f70011);
constexpr int ErrorHevagPcmFormat = static_cast<int>(0x80f70012);
constexpr int ErrorAuBytes = static_cast<int>(0x80f70013);
constexpr int ErrorPcmBytes = static_cast<int>(0x80f70014);
constexpr int ErrorMemoryBufferSize = static_cast<int>(0x80f70015);
constexpr int ErrorLpcmParam = static_cast<int>(0x80f70100);
constexpr int ErrorHevagChannels = static_cast<int>(0x80f70c00);
constexpr int ErrorHevagSampleRate = static_cast<int>(0x80f70c01);
constexpr int ErrorHevagInterleave = static_cast<int>(0x80f70c06);

constexpr std::uint32_t StructSize = 0x18;
constexpr std::uint32_t TypeLpcmBd = 0x1003;
constexpr std::uint32_t TypeLpcmDvd = 0x1004;
constexpr std::uint32_t TypeHevag = 0x1008;
constexpr std::uint32_t TypeTrhd = 0x100b;
constexpr std::uint32_t LpcmMemorySize = 0x190138;
constexpr std::uint32_t LpcmParamSize = 0x28;
constexpr std::uint32_t LpcmInputCapacity = 0x43800;
constexpr std::uint32_t LpcmOutputCapacity = 0x43800;
constexpr std::uint32_t LpcmOutputSamples = LpcmOutputCapacity / 4;
constexpr std::uint32_t LpcmStateOffset = 0xb8;
constexpr std::uint32_t LpcmBdPtsStep = 450;
constexpr std::uint32_t LpcmDvdPtsStep = 150;
constexpr std::uint32_t HevagMemorySize = 0x1b0;
constexpr std::uint32_t HevagParamSize = 0x40;
constexpr std::uint32_t HevagBsiSize = 0x30;
constexpr std::uint32_t HevagStateBytes = 0xb0;
constexpr std::uint32_t HevagHistoryOffset = 0x30;
constexpr std::uint32_t HevagHistoryBytes = 0x80;

struct LpcmParam {
    std::uint32_t size;
    std::uint32_t channelConfig;
    std::uint32_t rateIndex;
    std::uint32_t bitsIndex;
    std::uint32_t auBytes;
    std::uint32_t reserved;
    std::uint64_t header;
    std::uint32_t ptsHigh;
    std::uint32_t ptsLow;
};
static_assert(sizeof(LpcmParam) == LpcmParamSize);

struct LpcmBsiInfo {
    std::uint32_t reserved;
    std::uint32_t channelConfig;
    std::uint32_t rateIndex;
    std::uint32_t pcmBytes;
    std::uint64_t header;
    std::uint32_t ptsHigh;
    std::uint32_t ptsLow;
};

struct HevagParam {
    std::uint32_t size;
    std::uint8_t header[4];
    std::uint32_t field08;
    std::uint32_t field0c;
    std::uint32_t field10;
    std::uint32_t sampleRateBe;
    std::uint32_t field18;
    std::uint32_t field1c;
    std::uint8_t field20;
    std::uint8_t field21;
    std::uint8_t channels;
    std::uint8_t field23;
    std::uint8_t extra[16];
    std::uint32_t pcmFormat;
    std::uint8_t interleave;
    std::uint8_t reserved[7];
};
static_assert(sizeof(HevagParam) == HevagParamSize);

bool KnownType(std::uint32_t type) {
    return (type >= 0x1001 && type <= 0x1008) || type == TypeTrhd || type == 0x100c;
}

bool Lpcm(std::uint32_t type) {
    return type == TypeLpcmBd || type == TypeLpcmDvd;
}

bool ValidGuestRange(const void* address, std::size_t bytes) {
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    return start != 0 && (bytes == 0 || bytes - 1 <= std::numeric_limits<std::uintptr_t>::max() - start);
}

void CopyIn(void* destination, const void* source, std::size_t bytes, const char* message) {
    if (bytes == 0) return;
    const bool copied = ValidGuestRange(source, bytes) && GuestCopyIn_nid_no_patch(destination, source, bytes);
    if (!copied) throw std::runtime_error(message);
}

void CopyOut(void* destination, const void* source, std::size_t bytes, const char* message) {
    if (bytes == 0) return;
    const bool copied = ValidGuestRange(destination, bytes) && GuestCopyOut_nid_no_patch(destination, source, bytes);
    if (!copied) throw std::runtime_error(message);
}

void EnsureWritable(void* destination, std::size_t bytes, const char* message) {
    if (bytes == 0) return;
    const bool writable = ValidGuestRange(destination, bytes) && GuestWritable_nid_no_patch(destination, bytes);
    if (!writable) throw std::runtime_error(message);
}

void* OffsetGuestAddress(void* basePtr, std::size_t offset, const char* message) {
    const auto base = reinterpret_cast<std::uintptr_t>(basePtr);
    if (base == 0 || offset > std::numeric_limits<std::uintptr_t>::max() - base) {
        throw std::runtime_error(message);
    }
    return reinterpret_cast<void*>(base + offset);
}

void* LpcmStateAddress(const AudiodecCpuMemory& memory) {
    return OffsetGuestAddress(memory.p_memory, LpcmStateOffset, "sceAudiodecCpu: invalid LPCM memory address");
}

void* HevagAlignedStateAddress(const AudiodecCpuMemory& memory, std::size_t extraOffset = 0) {
    const auto base = reinterpret_cast<std::uintptr_t>(memory.p_memory);
    if (base == 0 || 7u > std::numeric_limits<std::uintptr_t>::max() - base) {
        throw std::runtime_error("sceAudiodecCpu: invalid HE-VAG memory address");
    }
    const auto aligned = (base + 7u) & ~static_cast<std::uintptr_t>(7u);
    if (extraOffset > std::numeric_limits<std::uintptr_t>::max() - aligned) {
        throw std::runtime_error("sceAudiodecCpu: invalid HE-VAG memory address");
    }
    return reinterpret_cast<void*>(aligned + extraOffset);
}

void ResetLpcm(const AudiodecCpuMemory& memory) {
    const std::uint32_t state[2]{0xffffffffu, 0xffffffffu};
    CopyOut(LpcmStateAddress(memory), state, sizeof(state), "sceAudiodecCpu: unwritable LPCM memory");
}

std::uint32_t ReadBe32(std::uint32_t value) {
    return ((value & 0x000000ffu) << 24) |
           ((value & 0x0000ff00u) << 8) |
           ((value & 0x00ff0000u) >> 8) |
           ((value & 0xff000000u) >> 24);
}

int InitHevag(const AudiodecCtrl& localCtrl, const AudiodecCpuMemory& localMemory) {
    if (localMemory.ui_memory_size < HevagMemorySize) return ErrorMemoryBufferSize;
    HevagParam localParam{};
    CopyIn(&localParam, localCtrl.pParam, sizeof(localParam), "sceAudiodecCpuInitDecoder: unreadable decoder parameter");
    if (localParam.size != HevagParamSize) return ErrorParamSize;
    if (localParam.channels == 0u || localParam.channels > 8u) return ErrorHevagChannels;
    const std::uint32_t sampleRate = ReadBe32(localParam.sampleRateBe);
    if (sampleRate < 2000u || sampleRate > 192000u) return ErrorHevagSampleRate;
    if (localParam.pcmFormat != 1u && localParam.pcmFormat != 2u) return ErrorHevagPcmFormat;
    if (localParam.interleave > 1u) return ErrorHevagInterleave;

    std::array<std::uint8_t, HevagStateBytes> state{};
    std::memcpy(state.data() + 0x00, localParam.header, sizeof(localParam.header));
    std::memcpy(state.data() + 0x04, &localParam.field08, 4);
    std::memcpy(state.data() + 0x08, &localParam.field10, 8);
    state[0x10] = localParam.channels;
    std::memcpy(state.data() + 0x14, localParam.extra, sizeof(localParam.extra));
    std::memcpy(state.data() + 0x24, &localParam.pcmFormat, 4);
    state[0x28] = localParam.interleave;
    void* stateAddress = HevagAlignedStateAddress(localMemory);
    CopyOut(stateAddress, state.data(), state.size(), "sceAudiodecCpuInitDecoder: unwritable HE-VAG memory");
    return 0;
}

int ClearHevag(const AudiodecCpuMemory& localMemory) {
    if (localMemory.ui_memory_size < HevagMemorySize) return ErrorMemoryBufferSize;
    std::array<std::uint8_t, HevagHistoryBytes> zeros{};
    void* historyAddress = HevagAlignedStateAddress(localMemory, HevagHistoryOffset);
    CopyOut(historyAddress, zeros.data(), zeros.size(), "sceAudiodecCpuClearContext: unwritable HE-VAG memory");
    return 0;
}

void UnknownCodec(const char* function, std::uint32_t type) {
    (void)type;
    NotImplemented_nid_no_patch(function);
}

float LpcmSample(std::int32_t value) {
    return static_cast<float>(value) * 0x1p-31f;
}

void LpcmTimestamp(const LpcmParam& param, LpcmBsiInfo& info, std::uint32_t state[2], std::uint32_t ptsStep) {
    std::uint32_t high = param.ptsHigh;
    std::uint32_t low = param.ptsLow;
    info.ptsHigh = high;
    info.ptsLow = low;
    if ((high & low) == 0xffffffffu) {
        if ((state[0] & state[1]) == 0xffffffffu) return;
        high = state[0];
        low = state[1] + ptsStep;
        info.ptsLow = low;
        info.ptsHigh = high;
        if (low < state[1]) {
            high = (high & 1) ? 0 : (high | 1);
            info.ptsHigh = high;
        }
    }
    state[0] = high;
    state[1] = low;
}

int DecodeLpcmBd(const AudiodecCtrl& localCtrl, const AudiodecCpuMemory& localMemory, AudiodecAuInfo& localAu, AudiodecPcmItem& localPcm) {
    static constexpr std::uint32_t frameUnits[] = {10, 10, 20, 20, 20, 20, 30, 30, 40, 40};
    static constexpr std::uint32_t rates[] = {48, 96, 192};
    static constexpr std::uint32_t bits[] = {16, 24, 24};
    LpcmParam localParam{};
    CopyIn(&localParam, localCtrl.pParam, sizeof(localParam), "sceAudiodecCpuDecode: unreadable decoder parameter");
    if (localParam.size != LpcmParamSize) return ErrorLpcmParam;
    const std::uint32_t channelConfig = static_cast<std::uint8_t>(localParam.header >> 32);
    const std::uint32_t rateIndex = static_cast<std::uint8_t>(localParam.header >> 40);
    const std::uint32_t bitsIndex = static_cast<std::uint8_t>(localParam.header >> 48);
    const std::uint32_t frames = static_cast<std::uint8_t>(localParam.header >> 56);
    if (channelConfig - 1 > 9 || rateIndex - 1 > 2 || bitsIndex - 1 > 2) return ErrorLpcmParam;
    const std::uint32_t auBytes = (frameUnits[channelConfig - 1] * rates[rateIndex - 1] * bits[bitsIndex - 1] >> 3) * frames;
    if (channelConfig != 2) NotImplemented_nid_no_patch("sceAudiodecCpuDecode: LPCM channel layouts other than stereo");
    if (auBytes > LpcmInputCapacity) NotImplemented_nid_no_patch("sceAudiodecCpuDecode: an LPCM frame larger than the input buffer of the module");
    if (localAu.ui_au_size < auBytes) NotImplemented_nid_no_patch("sceAudiodecCpuDecode: an AU shorter than its LPCM frame");
    const std::uint32_t width = bitsIndex == 1 ? 2 : 3;
    const std::uint32_t samples = auBytes / width;
    if (localPcm.ui_pcm_size < samples * 4) NotImplemented_nid_no_patch("sceAudiodecCpuDecode: a PCM buffer smaller than the decoded frame");

    LpcmBsiInfo localInfo{};
    CopyIn(&localInfo, localCtrl.pBsiInfo, sizeof(localInfo), "sceAudiodecCpuDecode: unreadable BSI info");
    void* stateAddress = LpcmStateAddress(localMemory);
    std::uint32_t lpcmState[2]{};
    CopyIn(lpcmState, stateAddress, sizeof(lpcmState), "sceAudiodecCpuDecode: unreadable LPCM memory");

    const std::size_t inputBytes = static_cast<std::size_t>(std::min(samples, LpcmOutputSamples)) * width;
    std::vector<std::uint8_t> input(inputBytes);
    CopyIn(input.data(), localAu.p_au_addr, input.size(), "sceAudiodecCpuDecode: unreadable AU buffer");

    std::vector<float> output(samples);
    for (std::uint32_t index = 0; index < samples; ++index) {
        const std::uint8_t* sample = input.data() + (index % LpcmOutputSamples) * width;
        std::uint32_t value = static_cast<std::uint32_t>(sample[0]) << 24 | static_cast<std::uint32_t>(sample[1]) << 16;
        if (width == 3) value |= static_cast<std::uint32_t>(sample[2]) << 8;
        output[index] = LpcmSample(static_cast<std::int32_t>(value));
    }

    EnsureWritable(localCtrl.pParam, sizeof(localParam), "sceAudiodecCpuDecode: unwritable decoder parameter");
    EnsureWritable(localCtrl.pBsiInfo, sizeof(localInfo), "sceAudiodecCpuDecode: unwritable BSI info");
    EnsureWritable(localCtrl.pAuInfo, sizeof(localAu), "sceAudiodecCpuDecode: unwritable AU info");
    EnsureWritable(localCtrl.pPcmItem, sizeof(localPcm), "sceAudiodecCpuDecode: unwritable PCM item");
    EnsureWritable(stateAddress, sizeof(lpcmState), "sceAudiodecCpuDecode: unwritable LPCM memory");
    if (samples != 0) EnsureWritable(localPcm.p_pcm_addr, samples * 4, "sceAudiodecCpuDecode: unwritable PCM buffer");

    localParam.channelConfig = channelConfig;
    localParam.rateIndex = rateIndex;
    localParam.bitsIndex = bitsIndex;
    localParam.auBytes = auBytes;
    localPcm.ui_pcm_size = samples * 4;
    localAu.ui_au_size = auBytes;
    LpcmTimestamp(localParam, localInfo, lpcmState, LpcmBdPtsStep);
    localInfo.header = localParam.header;
    localInfo.channelConfig = channelConfig;
    localInfo.rateIndex = rateIndex;
    localInfo.pcmBytes = localPcm.ui_pcm_size;

    if (samples != 0) {
        CopyOut(localPcm.p_pcm_addr, output.data(), samples * 4, "sceAudiodecCpuDecode: unwritable PCM buffer");
    }
    CopyOut(localCtrl.pParam, &localParam, sizeof(localParam), "sceAudiodecCpuDecode: unwritable decoder parameter");
    CopyOut(localCtrl.pBsiInfo, &localInfo, sizeof(localInfo), "sceAudiodecCpuDecode: unwritable BSI info");
    CopyOut(localCtrl.pAuInfo, &localAu, sizeof(localAu), "sceAudiodecCpuDecode: unwritable AU info");
    CopyOut(localCtrl.pPcmItem, &localPcm, sizeof(localPcm), "sceAudiodecCpuDecode: unwritable PCM item");
    CopyOut(stateAddress, lpcmState, sizeof(lpcmState), "sceAudiodecCpuDecode: unwritable LPCM memory");
    return 0;
}

int DecodeLpcmDvd(const AudiodecCtrl& localCtrl, const AudiodecCpuMemory& localMemory, AudiodecAuInfo& localAu, AudiodecPcmItem& localPcm) {
    static constexpr std::uint32_t dvdRateUnits[] = {80, 160};
    static constexpr std::uint32_t dvdBits[] = {16, 20, 24};
    LpcmParam localParam{};
    CopyIn(&localParam, localCtrl.pParam, sizeof(localParam), "sceAudiodecCpuDecode: unreadable decoder parameter");
    if (localParam.size != LpcmParamSize) return ErrorLpcmParam;
    const std::uint32_t channelConfig = static_cast<std::uint8_t>(localParam.header >> 32);
    const std::uint32_t rateIndex = static_cast<std::uint8_t>(localParam.header >> 40);
    const std::uint32_t bitsIndex = static_cast<std::uint8_t>(localParam.header >> 48);
    const std::uint32_t frames = static_cast<std::uint8_t>(localParam.header >> 56);
    if (rateIndex - 1 > 1 || bitsIndex - 1 > 2 || channelConfig - 1 > 7) return ErrorLpcmParam;

    const bool rate48 = rateIndex == 1;
    const bool rate96 = rateIndex == 2;
    const bool is20 = bitsIndex == 2;
    const bool is24 = bitsIndex == 3;
    if ((channelConfig == 3 || channelConfig == 5) && rate96) return ErrorLpcmParam;
    if (channelConfig == 4 && rate96 && (is20 || is24)) return ErrorLpcmParam;
    if (channelConfig == 6 && (rate96 || (rate48 && is24))) return ErrorLpcmParam;
    if ((channelConfig == 7 || channelConfig == 8) && (rate96 || (rate48 && (is20 || is24)))) return ErrorLpcmParam;

    const std::uint32_t auBytes = (dvdBits[bitsIndex - 1] * dvdRateUnits[rateIndex - 1] * channelConfig >> 3) * frames;
    if (channelConfig != 2) NotImplemented_nid_no_patch("sceAudiodecCpuDecode: LPCM DVD channel layouts other than stereo");
    if (auBytes > LpcmInputCapacity) NotImplemented_nid_no_patch("sceAudiodecCpuDecode: an LPCM frame larger than the input buffer of the module");
    if (localAu.ui_au_size < auBytes) NotImplemented_nid_no_patch("sceAudiodecCpuDecode: an AU shorter than its LPCM frame");

    std::uint32_t samples = 0;
    if (bitsIndex == 1) {
        samples = auBytes / 2;
    } else if (bitsIndex == 2) {
        samples = (auBytes * 2) / 5;
    } else {
        samples = auBytes / 3;
    }
    if ((bitsIndex == 2 || bitsIndex == 3) && (samples % 4) != 0) return ErrorLpcmParam;
    if (samples > LpcmOutputSamples) NotImplemented_nid_no_patch("sceAudiodecCpuDecode: an LPCM frame larger than the output buffer of the module");
    if (localPcm.ui_pcm_size < samples * 4) NotImplemented_nid_no_patch("sceAudiodecCpuDecode: a PCM buffer smaller than the decoded frame");

    LpcmBsiInfo localInfo{};
    CopyIn(&localInfo, localCtrl.pBsiInfo, sizeof(localInfo), "sceAudiodecCpuDecode: unreadable BSI info");
    void* stateAddress = LpcmStateAddress(localMemory);
    std::uint32_t lpcmState[2]{};
    CopyIn(lpcmState, stateAddress, sizeof(lpcmState), "sceAudiodecCpuDecode: unreadable LPCM memory");

    std::size_t inputBytes = 0;
    if (bitsIndex == 1) {
        inputBytes = static_cast<std::size_t>(samples) * 2;
    } else if (bitsIndex == 2) {
        inputBytes = static_cast<std::size_t>(samples / 4) * 10;
    } else {
        inputBytes = static_cast<std::size_t>(samples / 4) * 12;
    }
    std::vector<std::uint8_t> input(inputBytes);
    CopyIn(input.data(), localAu.p_au_addr, input.size(), "sceAudiodecCpuDecode: unreadable AU buffer");

    std::vector<float> output(samples);
    if (bitsIndex == 1) {
        for (std::uint32_t index = 0; index < samples; ++index) {
            const std::uint8_t* sample = input.data() + static_cast<std::size_t>(index) * 2;
            const std::uint32_t value = static_cast<std::uint32_t>(sample[0]) << 24 | static_cast<std::uint32_t>(sample[1]) << 16;
            output[index] = LpcmSample(static_cast<std::int32_t>(value));
        }
    } else if (bitsIndex == 2) {
        for (std::uint32_t index = 0; index < samples; index += 4) {
            const std::uint8_t* group = input.data() + static_cast<std::size_t>(index / 4) * 10;
            const std::uint32_t s0 = static_cast<std::uint32_t>(group[0]) << 24 | static_cast<std::uint32_t>(group[1]) << 16 | static_cast<std::uint32_t>(group[8] & 0xf0u) << 8;
            const std::uint32_t s1 = static_cast<std::uint32_t>(group[2]) << 24 | static_cast<std::uint32_t>(group[3]) << 16 | static_cast<std::uint32_t>(group[8] & 0x0fu) << 12;
            const std::uint32_t s2 = static_cast<std::uint32_t>(group[4]) << 24 | static_cast<std::uint32_t>(group[5]) << 16 | static_cast<std::uint32_t>(group[9] & 0xf0u) << 8;
            const std::uint32_t s3 = static_cast<std::uint32_t>(group[6]) << 24 | static_cast<std::uint32_t>(group[7]) << 16 | static_cast<std::uint32_t>(group[9] & 0x0fu) << 12;
            output[index] = LpcmSample(static_cast<std::int32_t>(s0));
            output[index + 1] = LpcmSample(static_cast<std::int32_t>(s1));
            output[index + 2] = LpcmSample(static_cast<std::int32_t>(s2));
            output[index + 3] = LpcmSample(static_cast<std::int32_t>(s3));
        }
    } else {
        for (std::uint32_t index = 0; index < samples; index += 4) {
            const std::uint8_t* group = input.data() + static_cast<std::size_t>(index / 4) * 12;
            const std::uint32_t s0 = static_cast<std::uint32_t>(group[0]) << 24 | static_cast<std::uint32_t>(group[1]) << 16 | static_cast<std::uint32_t>(group[8]) << 8;
            const std::uint32_t s1 = static_cast<std::uint32_t>(group[2]) << 24 | static_cast<std::uint32_t>(group[3]) << 16 | static_cast<std::uint32_t>(group[9]) << 8;
            const std::uint32_t s2 = static_cast<std::uint32_t>(group[4]) << 24 | static_cast<std::uint32_t>(group[5]) << 16 | static_cast<std::uint32_t>(group[10]) << 8;
            const std::uint32_t s3 = static_cast<std::uint32_t>(group[6]) << 24 | static_cast<std::uint32_t>(group[7]) << 16 | static_cast<std::uint32_t>(group[11]) << 8;
            output[index] = LpcmSample(static_cast<std::int32_t>(s0));
            output[index + 1] = LpcmSample(static_cast<std::int32_t>(s1));
            output[index + 2] = LpcmSample(static_cast<std::int32_t>(s2));
            output[index + 3] = LpcmSample(static_cast<std::int32_t>(s3));
        }
    }

    EnsureWritable(localCtrl.pParam, sizeof(localParam), "sceAudiodecCpuDecode: unwritable decoder parameter");
    EnsureWritable(localCtrl.pBsiInfo, sizeof(localInfo), "sceAudiodecCpuDecode: unwritable BSI info");
    EnsureWritable(localCtrl.pAuInfo, sizeof(localAu), "sceAudiodecCpuDecode: unwritable AU info");
    EnsureWritable(localCtrl.pPcmItem, sizeof(localPcm), "sceAudiodecCpuDecode: unwritable PCM item");
    EnsureWritable(stateAddress, sizeof(lpcmState), "sceAudiodecCpuDecode: unwritable LPCM memory");
    if (samples != 0) EnsureWritable(localPcm.p_pcm_addr, samples * 4, "sceAudiodecCpuDecode: unwritable PCM buffer");

    localParam.channelConfig = channelConfig;
    localParam.rateIndex = rateIndex;
    localParam.bitsIndex = bitsIndex;
    localParam.auBytes = auBytes;
    localPcm.ui_pcm_size = samples * 4;
    localAu.ui_au_size = auBytes;
    LpcmTimestamp(localParam, localInfo, lpcmState, LpcmDvdPtsStep);
    localInfo.header = localParam.header;
    localInfo.channelConfig = channelConfig;
    localInfo.rateIndex = rateIndex;
    localInfo.pcmBytes = localPcm.ui_pcm_size;

    if (samples != 0) {
        CopyOut(localPcm.p_pcm_addr, output.data(), samples * 4, "sceAudiodecCpuDecode: unwritable PCM buffer");
    }
    CopyOut(localCtrl.pParam, &localParam, sizeof(localParam), "sceAudiodecCpuDecode: unwritable decoder parameter");
    CopyOut(localCtrl.pBsiInfo, &localInfo, sizeof(localInfo), "sceAudiodecCpuDecode: unwritable BSI info");
    CopyOut(localCtrl.pAuInfo, &localAu, sizeof(localAu), "sceAudiodecCpuDecode: unwritable AU info");
    CopyOut(localCtrl.pPcmItem, &localPcm, sizeof(localPcm), "sceAudiodecCpuDecode: unwritable PCM item");
    CopyOut(stateAddress, lpcmState, sizeof(lpcmState), "sceAudiodecCpuDecode: unwritable LPCM memory");
    return 0;
}

int QueryMemSize(const char* function, const AudiodecCtrl* ctrl, AudiodecCpuMemory* memory, std::uint32_t type) {
    if (!ctrl) return ErrorCtrl;
    const void* param = nullptr;
    CopyIn(&param, ctrl, sizeof(param), "sceAudiodecCpuQueryMemSize: unreadable control pointer");
    if (!param) return ErrorParam;
    if (!memory) return ErrorMemory;
    std::uint32_t memorySize = 0;
    CopyIn(&memorySize, memory, sizeof(memorySize), "sceAudiodecCpuQueryMemSize: unreadable memory descriptor");
    if (memorySize != StructSize) return ErrorMemorySize;
    if (!KnownType(type)) return ErrorType;
    if (!Lpcm(type) && type != TypeHevag) UnknownCodec(function, type);
    const std::uint32_t outputSize = type == TypeHevag ? HevagMemorySize : LpcmMemorySize;
    auto* destination = reinterpret_cast<std::uint8_t*>(memory) + offsetof(AudiodecCpuMemory, ui_memory_size);
    CopyOut(destination, &outputSize, sizeof(outputSize), "sceAudiodecCpuQueryMemSize: unwritable memory-size output");
    return 0;
}

int InitDecoder(const char* function, const AudiodecCtrl* ctrl, const AudiodecCpuMemory* memory, std::uint32_t type) {
    if (!ctrl) return ErrorCtrl;
    AudiodecCtrl localCtrl{};
    CopyIn(&localCtrl, ctrl, sizeof(localCtrl), "sceAudiodecCpuInitDecoder: unreadable control pointer");
    if (!localCtrl.pParam) return ErrorParam;
    if (!localCtrl.pBsiInfo) return ErrorBsiInfo;
    if (!memory) return ErrorMemory;
    AudiodecCpuMemory localMemory{};
    CopyIn(&localMemory, memory, sizeof(localMemory), "sceAudiodecCpuInitDecoder: unreadable memory descriptor");
    if (!localMemory.p_memory) return ErrorMemoryAddress;
    if (localMemory.ui_size != StructSize) return ErrorMemorySize;
    if (!KnownType(type)) return ErrorType;
    if (type == TypeHevag) return InitHevag(localCtrl, localMemory);
    if (!Lpcm(type)) UnknownCodec(function, type);
    if (!localCtrl.pAuInfo || !localCtrl.pPcmItem) NotImplemented_nid_no_patch("sceAudiodecCpuInitDecoder: LPCM without an AU or PCM item");
    ResetLpcm(localMemory);
    return 0;
}

int Decode(const char* function, const AudiodecCtrl* ctrl, const AudiodecCpuMemory* memory, std::uint32_t type) {
    if (!ctrl) return ErrorCtrl;
    AudiodecCtrl localCtrl{};
    CopyIn(&localCtrl, ctrl, sizeof(localCtrl), "sceAudiodecCpuDecode: unreadable control pointer");
    if (!localCtrl.pParam) return ErrorParam;
    if (!localCtrl.pBsiInfo) return ErrorBsiInfo;
    if (!localCtrl.pAuInfo) return ErrorAuInfo;
    AudiodecAuInfo localAu{};
    CopyIn(&localAu, localCtrl.pAuInfo, sizeof(localAu), "sceAudiodecCpuDecode: unreadable AU info");
    if (!localAu.p_au_addr && !(type == TypeTrhd && localAu.ui_au_size == 0)) return ErrorAuAddress;
    if (!localCtrl.pPcmItem) return ErrorPcmItem;
    AudiodecPcmItem localPcm{};
    CopyIn(&localPcm, localCtrl.pPcmItem, sizeof(localPcm), "sceAudiodecCpuDecode: unreadable PCM item");
    if (!localPcm.p_pcm_addr) return ErrorPcmAddress;
    if (localAu.ui_au_size == 0 && type != TypeTrhd) return ErrorAuBytes;
    if (localPcm.ui_pcm_size == 0) return ErrorPcmBytes;
    if (localAu.ui_size != StructSize) return ErrorAuSize;
    if (localPcm.ui_size != StructSize) return ErrorPcmSize;
    if (!memory) return ErrorMemory;
    AudiodecCpuMemory localMemory{};
    CopyIn(&localMemory, memory, sizeof(localMemory), "sceAudiodecCpuDecode: unreadable memory descriptor");
    if (!localMemory.p_memory) return ErrorMemoryAddress;
    if (localMemory.ui_size != StructSize) return ErrorMemorySize;
    if (!KnownType(type)) return ErrorType;
    if (type == TypeLpcmBd) return DecodeLpcmBd(localCtrl, localMemory, localAu, localPcm);
    if (type == TypeLpcmDvd) return DecodeLpcmDvd(localCtrl, localMemory, localAu, localPcm);
    if (type == TypeHevag) {
        if (localMemory.ui_memory_size < HevagMemorySize) return ErrorMemoryBufferSize;
        std::uint32_t bsiSize = 0;
        CopyIn(&bsiSize, localCtrl.pBsiInfo, sizeof(bsiSize), "sceAudiodecCpuDecode: unreadable BSI info");
        if (bsiSize != HevagBsiSize) return ErrorBsiInfoSize;
    }
    UnknownCodec(function, type);
    return 0;
}

int ClearContext(const char* function, const AudiodecCpuMemory* memory, std::uint32_t type) {
    if (!memory) return ErrorClearMemory;
    AudiodecCpuMemory localMemory{};
    CopyIn(&localMemory, memory, sizeof(localMemory), "sceAudiodecCpuClearContext: unreadable memory descriptor");
    if (!localMemory.p_memory) return ErrorMemoryAddress;
    if (localMemory.ui_size != StructSize) return ErrorMemorySize;
    if (!KnownType(type)) return ErrorType;
    if (type == TypeHevag) return ClearHevag(localMemory);
    if (!Lpcm(type)) UnknownCodec(function, type);
    ResetLpcm(localMemory);
    return 0;
}

}

extern "C" {

int APS5_VABI sceAudiodecCpuClearContext(const AudiodecCpuMemory* memory, std::uint32_t type) {
    return ClearContext(__func__, memory, type);
}

int APS5_VABI sceAudiodecCpuDecode(const AudiodecCtrl* ctrl, const AudiodecCpuMemory* memory, std::uint32_t type) {
    return Decode(__func__, ctrl, memory, type);
}

int APS5_VABI sceAudiodecCpuInitDecoder(const AudiodecCtrl* ctrl, const AudiodecCpuMemory* memory, std::uint32_t type) {
    return InitDecoder(__func__, ctrl, memory, type);
}

int APS5_VABI sceAudiodecCpuInternalClearContext(const AudiodecCpuMemory* memory, std::uint32_t type) {
    return ClearContext(__func__, memory, type);
}

int APS5_VABI sceAudiodecCpuInternalDecode(const AudiodecCtrl* ctrl, const AudiodecCpuMemory* memory, std::uint32_t type) {
    return Decode(__func__, ctrl, memory, type);
}

int APS5_VABI sceAudiodecCpuInternalInitDecoder(const AudiodecCtrl* ctrl, const AudiodecCpuMemory* memory, std::uint32_t type) {
    return InitDecoder(__func__, ctrl, memory, type);
}

int APS5_VABI sceAudiodecCpuInternalQueryMemSize(const AudiodecCtrl* ctrl, AudiodecCpuMemory* memory, std::uint32_t type) {
    return QueryMemSize(__func__, ctrl, memory, type);
}

int APS5_VABI sceAudiodecCpuQueryMemSize(const AudiodecCtrl* ctrl, AudiodecCpuMemory* memory, std::uint32_t type) {
    return QueryMemSize(__func__, ctrl, memory, type);
}

}
