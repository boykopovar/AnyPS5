#include "prx/libSceAudiodecCpu/include/Api.hpp"
#include <algorithm>
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
constexpr int ErrorCtrl = static_cast<int>(0x80f70009);
constexpr int ErrorParam = static_cast<int>(0x80f7000a);
constexpr int ErrorBsiInfo = static_cast<int>(0x80f7000b);
constexpr int ErrorAuInfo = static_cast<int>(0x80f7000c);
constexpr int ErrorPcmItem = static_cast<int>(0x80f7000d);
constexpr int ErrorMemory = static_cast<int>(0x80f7000e);
constexpr int ErrorAuAddress = static_cast<int>(0x80f7000f);
constexpr int ErrorPcmAddress = static_cast<int>(0x80f70010);
constexpr int ErrorMemoryAddress = static_cast<int>(0x80f70011);
constexpr int ErrorAuBytes = static_cast<int>(0x80f70013);
constexpr int ErrorPcmBytes = static_cast<int>(0x80f70014);
constexpr int ErrorLpcmParam = static_cast<int>(0x80f70100);

constexpr std::uint32_t StructSize = 0x18;
constexpr std::uint32_t TypeLpcmBd = 0x1003;
constexpr std::uint32_t TypeLpcmDvd = 0x1004;
constexpr std::uint32_t TypeTrhd = 0x100b;
constexpr std::uint32_t LpcmMemorySize = 0x190138;
constexpr std::uint32_t LpcmParamSize = 0x28;
constexpr std::uint32_t LpcmInputCapacity = 0x43800;
constexpr std::uint32_t LpcmOutputCapacity = 0x43800;
constexpr std::uint32_t LpcmOutputSamples = LpcmOutputCapacity / 4;
constexpr std::uint32_t LpcmStateOffset = 0xb8;
constexpr std::uint32_t PtsStep = 450;

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

void* LpcmStateAddress(const AudiodecCpuMemory& memory) {
    const auto base = reinterpret_cast<std::uintptr_t>(memory.p_memory);
    if (base == 0 || LpcmStateOffset > std::numeric_limits<std::uintptr_t>::max() - base) {
        throw std::runtime_error("sceAudiodecCpu: invalid LPCM memory address");
    }
    return reinterpret_cast<void*>(base + LpcmStateOffset);
}

void ResetLpcm(const AudiodecCpuMemory& memory) {
    const std::uint32_t state[2]{0xffffffffu, 0xffffffffu};
    CopyOut(LpcmStateAddress(memory), state, sizeof(state), "sceAudiodecCpu: unwritable LPCM memory");
}

void UnknownCodec(const char* function, std::uint32_t type) {
    (void)type;
    NotImplemented_nid_no_patch(function);
}

float LpcmSample(std::int32_t value) {
    return static_cast<float>(value) * 0x1p-31f;
}

void LpcmTimestamp(const LpcmParam& param, LpcmBsiInfo& info, std::uint32_t state[2]) {
    std::uint32_t high = param.ptsHigh;
    std::uint32_t low = param.ptsLow;
    info.ptsHigh = high;
    info.ptsLow = low;
    if ((high & low) == 0xffffffffu) {
        if ((state[0] & state[1]) == 0xffffffffu) return;
        high = state[0];
        low = state[1] + PtsStep;
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
    LpcmTimestamp(localParam, localInfo, lpcmState);
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
    if (!Lpcm(type)) UnknownCodec(function, type);
    const std::uint32_t outputSize = LpcmMemorySize;
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
    if (type != TypeLpcmBd) UnknownCodec(function, type);
    return DecodeLpcmBd(localCtrl, localMemory, localAu, localPcm);
}

int ClearContext(const char* function, const AudiodecCpuMemory* memory, std::uint32_t type) {
    if (!memory) return ErrorClearMemory;
    AudiodecCpuMemory localMemory{};
    CopyIn(&localMemory, memory, sizeof(localMemory), "sceAudiodecCpuClearContext: unreadable memory descriptor");
    if (!localMemory.p_memory) return ErrorMemoryAddress;
    if (localMemory.ui_size != StructSize) return ErrorMemorySize;
    if (!KnownType(type)) return ErrorType;
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
