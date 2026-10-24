#include <cstdint>
#include <cstddef>
#include <cstring>
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
constexpr std::uint32_t LpcmStateOffset = 0xb8;
constexpr std::uint32_t PtsStep = 450;

struct AudiodecCpuMemory {
    std::uint32_t ui_size;
    void* p_memory;
    std::uint32_t ui_memory_size;
};

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

std::uint32_t* LpcmState(const AudiodecCpuMemory* memory) {
    return reinterpret_cast<std::uint32_t*>(static_cast<std::uint8_t*>(memory->p_memory) + LpcmStateOffset);
}

void ResetLpcm(const AudiodecCpuMemory* memory) {
    std::uint32_t* state = LpcmState(memory);
    state[0] = 0xffffffffu;
    state[1] = 0xffffffffu;
}

void UnknownCodec(const char* function, std::uint32_t type) {
    (void)type;
    NotImplemented_nid_no_patch(function);
}

float LpcmSample(std::int32_t value) {
    return static_cast<float>(value) * 0x1p-31f;
}

void LpcmTimestamp(const LpcmParam& param, LpcmBsiInfo& info, const AudiodecCpuMemory* memory) {
    std::uint32_t high = param.ptsHigh;
    std::uint32_t low = param.ptsLow;
    info.ptsHigh = high;
    info.ptsLow = low;
    std::uint32_t* state = LpcmState(memory);
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

int DecodeLpcmBd(const AudiodecCtrl* ctrl, const AudiodecCpuMemory* memory) {
    static constexpr std::uint32_t frameUnits[] = {10, 10, 20, 20, 20, 20, 30, 30, 40, 40};
    static constexpr std::uint32_t rates[] = {48, 96, 192};
    static constexpr std::uint32_t bits[] = {16, 24, 24};
    auto* param = static_cast<LpcmParam*>(ctrl->pParam);
    if (param->size != LpcmParamSize) return ErrorLpcmParam;
    const std::uint32_t channelConfig = static_cast<std::uint8_t>(param->header >> 32);
    const std::uint32_t rateIndex = static_cast<std::uint8_t>(param->header >> 40);
    const std::uint32_t bitsIndex = static_cast<std::uint8_t>(param->header >> 48);
    const std::uint32_t frames = static_cast<std::uint8_t>(param->header >> 56);
    if (channelConfig - 1 > 9 || rateIndex - 1 > 2 || bitsIndex - 1 > 2) return ErrorLpcmParam;
    param->channelConfig = channelConfig;
    param->rateIndex = rateIndex;
    param->bitsIndex = bitsIndex;
    const std::uint32_t auBytes = (frameUnits[channelConfig - 1] * rates[rateIndex - 1] * bits[bitsIndex - 1] >> 3) * frames;
    param->auBytes = auBytes;
    if (channelConfig != 2) NotImplemented_nid_no_patch("sceAudiodecCpuDecode: LPCM channel layouts other than stereo");
    if (auBytes > LpcmInputCapacity) NotImplemented_nid_no_patch("sceAudiodecCpuDecode: an LPCM frame larger than the input buffer of the module");
    if (ctrl->pAuInfo->ui_au_size < auBytes) NotImplemented_nid_no_patch("sceAudiodecCpuDecode: an AU shorter than its LPCM frame");
    const auto* input = static_cast<const std::uint8_t*>(ctrl->pAuInfo->p_au_addr);
    const std::uint32_t width = bitsIndex == 1 ? 2 : 3;
    const std::uint32_t samples = auBytes / width;
    if (ctrl->pPcmItem->ui_pcm_size < samples * 4) NotImplemented_nid_no_patch("sceAudiodecCpuDecode: a PCM buffer smaller than the decoded frame");
    std::vector<float> output(samples);
    for (std::uint32_t index = 0; index < samples; ++index) {
        const std::uint8_t* sample = input + index * width;
        std::uint32_t value = static_cast<std::uint32_t>(sample[0]) << 24 | static_cast<std::uint32_t>(sample[1]) << 16;
        if (width == 3) value |= static_cast<std::uint32_t>(sample[2]) << 8;
        output[index] = LpcmSample(static_cast<std::int32_t>(value));
    }
    if (samples != 0) std::memcpy(ctrl->pPcmItem->p_pcm_addr, output.data(), samples * 4);
    ctrl->pPcmItem->ui_pcm_size = samples * 4;
    ctrl->pAuInfo->ui_au_size = auBytes;
    auto* info = static_cast<LpcmBsiInfo*>(ctrl->pBsiInfo);
    LpcmTimestamp(*param, *info, memory);
    info->header = param->header;
    info->channelConfig = channelConfig;
    info->rateIndex = rateIndex;
    info->pcmBytes = ctrl->pPcmItem->ui_pcm_size;
    return 0;
}

int QueryMemSize(const char* function, const AudiodecCtrl* ctrl, AudiodecCpuMemory* memory, std::uint32_t type) {
    if (!ctrl) return ErrorCtrl;
    if (!ctrl->pParam) return ErrorParam;
    if (!memory) return ErrorMemory;
    if (memory->ui_size != StructSize) return ErrorMemorySize;
    if (!KnownType(type)) return ErrorType;
    if (!Lpcm(type)) UnknownCodec(function, type);
    memory->ui_memory_size = LpcmMemorySize;
    return 0;
}

int InitDecoder(const char* function, const AudiodecCtrl* ctrl, const AudiodecCpuMemory* memory, std::uint32_t type) {
    if (!ctrl) return ErrorCtrl;
    if (!ctrl->pParam) return ErrorParam;
    if (!ctrl->pBsiInfo) return ErrorBsiInfo;
    if (!memory) return ErrorMemory;
    if (!memory->p_memory) return ErrorMemoryAddress;
    if (memory->ui_size != StructSize) return ErrorMemorySize;
    if (!KnownType(type)) return ErrorType;
    if (!Lpcm(type)) UnknownCodec(function, type);
    if (!ctrl->pAuInfo || !ctrl->pPcmItem) NotImplemented_nid_no_patch("sceAudiodecCpuInitDecoder: LPCM without an AU or PCM item");
    ResetLpcm(memory);
    return 0;
}

int Decode(const char* function, const AudiodecCtrl* ctrl, const AudiodecCpuMemory* memory, std::uint32_t type) {
    if (!ctrl) return ErrorCtrl;
    if (!ctrl->pParam) return ErrorParam;
    if (!ctrl->pBsiInfo) return ErrorBsiInfo;
    const AudiodecAuInfo* au = ctrl->pAuInfo;
    if (!au) return ErrorAuInfo;
    if (!au->p_au_addr && !(type == TypeTrhd && au->ui_au_size == 0)) return ErrorAuAddress;
    const AudiodecPcmItem* pcm = ctrl->pPcmItem;
    if (!pcm) return ErrorPcmItem;
    if (!pcm->p_pcm_addr) return ErrorPcmAddress;
    if (au->ui_au_size == 0 && type != TypeTrhd) return ErrorAuBytes;
    if (pcm->ui_pcm_size == 0) return ErrorPcmBytes;
    if (au->ui_size != StructSize) return ErrorAuSize;
    if (pcm->ui_size != StructSize) return ErrorPcmSize;
    if (!memory) return ErrorMemory;
    if (!memory->p_memory) return ErrorMemoryAddress;
    if (memory->ui_size != StructSize) return ErrorMemorySize;
    if (!KnownType(type)) return ErrorType;
    if (type != TypeLpcmBd) UnknownCodec(function, type);
    return DecodeLpcmBd(ctrl, memory);
}

int ClearContext(const char* function, const AudiodecCpuMemory* memory, std::uint32_t type) {
    if (!memory) return ErrorClearMemory;
    if (!memory->p_memory) return ErrorMemoryAddress;
    if (memory->ui_size != StructSize) return ErrorMemorySize;
    if (!KnownType(type)) return ErrorType;
    if (!Lpcm(type)) UnknownCodec(function, type);
    ResetLpcm(memory);
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
