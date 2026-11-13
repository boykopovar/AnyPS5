#include "prx/libSceAudiodec/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include <array>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceAudiodec/src/Codecs.hpp"

namespace {

constexpr std::int32_t ERROR_API_FAIL = static_cast<std::int32_t>(0x807F0000);
constexpr std::int32_t ERROR_INVALID_TYPE = static_cast<std::int32_t>(0x807F0001);
constexpr std::int32_t ERROR_ARG = static_cast<std::int32_t>(0x807F0002);
constexpr std::int32_t ERROR_INVALID_PARAM_SIZE = static_cast<std::int32_t>(0x807F0004);
constexpr std::int32_t ERROR_INVALID_BSI_INFO_SIZE = static_cast<std::int32_t>(0x807F0005);
constexpr std::int32_t ERROR_INVALID_AU_INFO_SIZE = static_cast<std::int32_t>(0x807F0006);
constexpr std::int32_t ERROR_INVALID_PCM_ITEM_SIZE = static_cast<std::int32_t>(0x807F0007);
constexpr std::int32_t ERROR_INVALID_CTRL_POINTER = static_cast<std::int32_t>(0x807F0008);
constexpr std::int32_t ERROR_INVALID_PARAM_POINTER = static_cast<std::int32_t>(0x807F0009);
constexpr std::int32_t ERROR_INVALID_BSI_INFO_POINTER = static_cast<std::int32_t>(0x807F000A);
constexpr std::int32_t ERROR_INVALID_AU_INFO_POINTER = static_cast<std::int32_t>(0x807F000B);
constexpr std::int32_t ERROR_INVALID_PCM_ITEM_POINTER = static_cast<std::int32_t>(0x807F000C);
constexpr std::int32_t ERROR_INVALID_AU_POINTER = static_cast<std::int32_t>(0x807F000D);
constexpr std::int32_t ERROR_INVALID_PCM_POINTER = static_cast<std::int32_t>(0x807F000E);
constexpr std::int32_t ERROR_INVALID_HANDLE = static_cast<std::int32_t>(0x807F000F);
constexpr std::int32_t ERROR_INVALID_WORD_LENGTH = static_cast<std::int32_t>(0x807F0010);
constexpr std::int32_t ERROR_INVALID_AU_SIZE = static_cast<std::int32_t>(0x807F0011);
constexpr std::int32_t ERROR_INVALID_PCM_SIZE = static_cast<std::int32_t>(0x807F0012);
constexpr std::int32_t ERROR_M4AAC_INVALID_SAMPLING_FREQ = static_cast<std::int32_t>(0x807F0300);
constexpr std::int32_t ERROR_M4AAC_INVALID_ENABLE_HEAAC = static_cast<std::int32_t>(0x807F0302);
constexpr std::int32_t ERROR_M4AAC_INVALID_CONFIG_NUMBER = static_cast<std::int32_t>(0x807F0303);
constexpr std::int32_t ERROR_M4AAC_INVALID_MAX_CHANNELS = static_cast<std::int32_t>(0x807F0304);
constexpr std::int32_t ERROR_M4AAC_INVALID_ENABLE_NONDELAY_OUTPUT = static_cast<std::int32_t>(0x807F0305);
constexpr std::int32_t ERROR_M4AAC_INVALID_SURROUND_CHANNEL_INTERLEAVE_ORDER = static_cast<std::int32_t>(0x807F0306);
constexpr std::int32_t ERROR_AT9_INVALID_CONFIG_DATA = static_cast<std::int32_t>(0x807F1000);

constexpr std::uint32_t TYPE_AT9 = 1;
constexpr std::uint32_t TYPE_MP3 = 2;
constexpr std::uint32_t TYPE_M4AAC = 3;
constexpr std::uint32_t M4AAC_CONFIG_ADTS = 1;
constexpr std::uint32_t M4AAC_CONFIG_RAW = 2;
constexpr std::int32_t M4AAC_RESULT_DECODE_ERROR = -4;
constexpr std::int32_t M4AAC_RESULT_INSUFFICIENT_DATA = -5;

static_assert(sizeof(AudiodecAuInfo) == 24 && sizeof(AudiodecPcmItem) == 24 && sizeof(AudiodecCtrl) == 32);
static_assert(sizeof(AudiodecParamAt9) == 12 && sizeof(AudiodecAt9Info) == 36);
static_assert(sizeof(AudiodecParamMp3) == 8 && sizeof(AudiodecMp3Info) == 20);
static_assert(sizeof(AudiodecParamM4aac) == 24 && sizeof(AudiodecM4aacInfo) == 20);

struct Instance {
    std::uint32_t codecType = 0;
    std::unique_ptr<Audiodec::Decoder> decoder;
    Audiodec::At9Format at9{};
    std::uint32_t channels = 0;
    std::uint32_t sampleRate = 0;
    std::uint32_t surroundChannelInterleaveOrder = 0;
};

std::mutex g_mutex;
std::array<std::uint32_t, 4> g_initialized{};
std::array<Instance, 64> g_instances{};

bool validType(std::uint32_t codecType) {
    return codecType == TYPE_AT9 || codecType == TYPE_MP3 || codecType == TYPE_M4AAC;
}

std::int32_t wordSizeOf(const void* param) {
    return static_cast<const AudiodecParamMp3*>(param)->i_bw_pcm;
}

bool copyIn(void* dst, const void* src, std::size_t size) {
    if (!src) return false;
    const int saved = errno;
    const bool ok = GuestCopyIn_nid_no_patch(dst, src, size);
    errno = saved;
    return ok;
}

bool copyOut(void* dst, const void* src, std::size_t size) {
    if (!dst) return false;
    const int saved = errno;
    const bool ok = GuestCopyOut_nid_no_patch(dst, src, size);
    errno = saved;
    return ok;
}

union ParamBuffer {
    AudiodecParamAt9 at9;
    AudiodecParamMp3 mp3;
    AudiodecParamM4aac m4aac;
    std::byte bytes[32];
};

union InfoBuffer {
    AudiodecAt9Info at9;
    AudiodecMp3Info mp3;
    AudiodecM4aacInfo m4aac;
};

std::int32_t validateWordSize(std::int32_t wordSize, std::uint32_t codecType) {
    if (wordSize == Audiodec::WORD_SIZE_16BIT) return 0;
    if (wordSize == Audiodec::WORD_SIZE_FLOAT && codecType != TYPE_MP3) return 0;
    return ERROR_INVALID_WORD_LENGTH;
}

std::int32_t loadCreateCtrl(const AudiodecCtrl* guestCtrl, std::uint32_t codecType, AudiodecCtrl& localCtrl, ParamBuffer& param, InfoBuffer& info, std::size_t& infoSizeOut) {
    if (!copyIn(&localCtrl, guestCtrl, sizeof(localCtrl))) return ERROR_INVALID_CTRL_POINTER;
    if (!localCtrl.pParam) return ERROR_INVALID_PARAM_POINTER;
    if (!localCtrl.pBsiInfo) return ERROR_INVALID_BSI_INFO_POINTER;
    std::uint32_t paramSize = 0;
    std::uint32_t infoSize = 0;
    if (!copyIn(&paramSize, localCtrl.pParam, sizeof(paramSize))) return ERROR_INVALID_PARAM_POINTER;
    if (!copyIn(&infoSize, localCtrl.pBsiInfo, sizeof(infoSize))) return ERROR_INVALID_BSI_INFO_POINTER;
    switch (codecType) {
        case TYPE_AT9:
            if (paramSize != sizeof(AudiodecParamAt9)) return ERROR_INVALID_PARAM_SIZE;
            if (infoSize != sizeof(AudiodecAt9Info)) return ERROR_INVALID_BSI_INFO_SIZE;
            break;
        case TYPE_MP3:
            if (paramSize != sizeof(AudiodecParamMp3)) return ERROR_INVALID_PARAM_SIZE;
            if (infoSize != sizeof(AudiodecMp3Info)) return ERROR_INVALID_BSI_INFO_SIZE;
            break;
        default:
            if (paramSize != sizeof(AudiodecParamM4aac) && paramSize != 28 && paramSize != 32) return ERROR_INVALID_PARAM_SIZE;
            if (infoSize != sizeof(AudiodecM4aacInfo)) return ERROR_INVALID_BSI_INFO_SIZE;
            break;
    }
    param = {};
    info = {};
    if (!copyIn(&param, localCtrl.pParam, paramSize)) return ERROR_INVALID_PARAM_POINTER;
    if (!copyIn(&info, localCtrl.pBsiInfo, infoSize)) return ERROR_INVALID_BSI_INFO_POINTER;
    infoSizeOut = infoSize;
    return validateWordSize(wordSizeOf(&param), codecType);
}

bool invalidStreamSize(std::uint32_t size) {
    return size == 0 || size == UINT32_MAX;
}

std::int32_t loadDecodeCtrl(const AudiodecCtrl* guestCtrl, AudiodecCtrl& localCtrl, AudiodecAuInfo& auInfo, AudiodecPcmItem& pcmItem) {
    if (!copyIn(&localCtrl, guestCtrl, sizeof(localCtrl))) return ERROR_INVALID_CTRL_POINTER;
    if (!localCtrl.pParam) return ERROR_INVALID_PARAM_POINTER;
    if (!localCtrl.pBsiInfo) return ERROR_INVALID_BSI_INFO_POINTER;
    if (!localCtrl.pAuInfo) return ERROR_INVALID_AU_INFO_POINTER;
    if (!copyIn(&auInfo, localCtrl.pAuInfo, sizeof(auInfo))) return ERROR_INVALID_AU_INFO_POINTER;
    if (!auInfo.p_au_addr) return ERROR_INVALID_AU_POINTER;
    if (!localCtrl.pPcmItem) return ERROR_INVALID_PCM_ITEM_POINTER;
    if (!copyIn(&pcmItem, localCtrl.pPcmItem, sizeof(pcmItem))) return ERROR_INVALID_PCM_ITEM_POINTER;
    if (!pcmItem.p_pcm_addr) return ERROR_INVALID_PCM_POINTER;
    if (invalidStreamSize(auInfo.ui_au_size)) return ERROR_INVALID_AU_SIZE;
    if (invalidStreamSize(pcmItem.ui_pcm_size)) return ERROR_INVALID_PCM_SIZE;
    if (auInfo.ui_size != sizeof(AudiodecAuInfo)) return ERROR_INVALID_AU_INFO_SIZE;
    if (pcmItem.ui_size != sizeof(AudiodecPcmItem)) return ERROR_INVALID_PCM_ITEM_SIZE;
    return 0;
}

std::int32_t loadDecodeInfo(const AudiodecCtrl& localCtrl, std::uint32_t codecType, InfoBuffer& info) {
    std::uint32_t infoSize = 0;
    if (!copyIn(&infoSize, localCtrl.pBsiInfo, sizeof(infoSize))) return ERROR_INVALID_BSI_INFO_POINTER;
    if (codecType == TYPE_AT9) {
        std::uint32_t paramSize = 0;
        if (!copyIn(&paramSize, localCtrl.pParam, sizeof(paramSize))) return ERROR_INVALID_PARAM_POINTER;
        if (paramSize != sizeof(AudiodecParamAt9)) return ERROR_INVALID_PARAM_SIZE;
        if (infoSize != sizeof(AudiodecAt9Info)) return ERROR_INVALID_BSI_INFO_SIZE;
        return copyIn(&info.at9, localCtrl.pBsiInfo, sizeof(info.at9)) ? 0 : ERROR_INVALID_BSI_INFO_POINTER;
    }
    const std::size_t expected = codecType == TYPE_MP3 ? sizeof(AudiodecMp3Info) : sizeof(AudiodecM4aacInfo);
    if (infoSize != expected) return ERROR_INVALID_BSI_INFO_SIZE;
    return copyIn(&info, localCtrl.pBsiInfo, expected) ? 0 : ERROR_INVALID_BSI_INFO_POINTER;
}

void fillAt9Info(AudiodecAt9Info* info, const Audiodec::At9Format& format) {
    const std::uint32_t superframeSamples = format.frameSamples * format.framesInSuperframe;
    info->ui_channel = format.channels;
    info->ui_bitrate = static_cast<std::uint32_t>(static_cast<std::uint64_t>(format.superframeSize) * 8 * format.sampleRate / superframeSamples);
    info->ui_sampling_rate = format.sampleRate;
    info->ui_super_frame_size = format.superframeSize;
    info->ui_frames_in_super_frame = format.framesInSuperframe;
    info->ui_next_frame_size = format.superframeSize;
    info->ui_frame_samples = format.frameSamples;
    info->i_result = 0;
}

void fillMp3Info(AudiodecMp3Info* info, const Audiodec::Mp3Header& header) {
    info->ui_header = header.word;
    info->uc_crc = header.crc;
    info->uc_mode = header.mode;
    info->uc_mode_extension = header.modeExtension;
    info->uc_copyright = header.copyright;
    info->uc_original = header.original;
    info->uc_emphasis = header.emphasis;
    info->uc_reserved[0] = 0;
    info->uc_reserved[1] = 0;
    info->i_result = 0;
}

std::int32_t createAac(const AudiodecParamM4aac* param, AudiodecM4aacInfo* info, Instance& instance) {
    std::uint32_t enableNondelayOutput = 0;
    std::uint32_t surroundChannelInterleaveOrder = 0;
    const auto* bytes = reinterpret_cast<const std::byte*>(param);
    if (param->ui_size >= 28) std::memcpy(&enableNondelayOutput, bytes + 24, sizeof(enableNondelayOutput));
    if (enableNondelayOutput > 1) return ERROR_M4AAC_INVALID_ENABLE_NONDELAY_OUTPUT;
    if (param->ui_size == 32) std::memcpy(&surroundChannelInterleaveOrder, bytes + 28, sizeof(surroundChannelInterleaveOrder));
    if (surroundChannelInterleaveOrder > 2) return ERROR_M4AAC_INVALID_SURROUND_CHANNEL_INTERLEAVE_ORDER;
    if (param->ui_config_number != M4AAC_CONFIG_ADTS && param->ui_config_number != M4AAC_CONFIG_RAW) return ERROR_M4AAC_INVALID_CONFIG_NUMBER;
    const std::uint32_t sampleRate = Audiodec::AacSampleRate(param->ui_sampling_freq_index);
    if (param->ui_config_number == M4AAC_CONFIG_RAW && sampleRate == 0) return ERROR_M4AAC_INVALID_SAMPLING_FREQ;
    if (param->ui_max_channels != 1 && param->ui_max_channels != 2 && param->ui_max_channels != 6 && param->ui_max_channels != 8) return ERROR_M4AAC_INVALID_MAX_CHANNELS;
    if (param->ui_enable_heaac > 1 || (param->ui_enable_heaac == 1 && param->ui_max_channels > 2)) return ERROR_M4AAC_INVALID_ENABLE_HEAAC;
    const std::uint32_t channels = param->ui_max_channels;
    instance.decoder = Audiodec::CreateAac(param->i_bw_pcm, param->ui_config_number == M4AAC_CONFIG_ADTS, param->ui_sampling_freq_index, channels, param->ui_enable_heaac == 1);
    instance.channels = channels;
    instance.sampleRate = sampleRate;
    instance.surroundChannelInterleaveOrder = surroundChannelInterleaveOrder;
    info->ui_sampling_freq = sampleRate;
    info->ui_number_of_channels = channels;
    info->ui_heaac = 0;
    info->i_result = 0;
    return 0;
}

Instance* find(std::int32_t handle) {
    if (handle <= 0 || static_cast<std::size_t>(handle) > g_instances.size()) return nullptr;
    Instance& instance = g_instances[static_cast<std::size_t>(handle - 1)];
    return instance.decoder ? &instance : nullptr;
}

}

extern "C" {

int32_t APS5_VABI sceAudiodecInitLibrary(uint32_t codec_type) {
    if (!validType(codec_type)) return ERROR_INVALID_TYPE;
    std::lock_guard lock(g_mutex);
    ++g_initialized[codec_type];
    return 0;
}

int32_t APS5_VABI sceAudiodecTermLibrary(uint32_t codec_type) {
    if (!validType(codec_type)) return ERROR_INVALID_TYPE;
    std::lock_guard lock(g_mutex);
    if (g_initialized[codec_type] > 0) --g_initialized[codec_type];
    return 0;
}

int32_t APS5_VABI sceAudiodecCreateDecoder(AudiodecCtrl* ctrl, uint32_t codec_type) {
    if (!validType(codec_type)) return ERROR_INVALID_TYPE;
    AudiodecCtrl localCtrl{};
    ParamBuffer param{};
    InfoBuffer info{};
    std::size_t infoSize = 0;
    if (const std::int32_t result = loadCreateCtrl(ctrl, codec_type, localCtrl, param, info, infoSize); result != 0) return result;
    std::lock_guard lock(g_mutex);
    if (g_initialized[codec_type] == 0) return ERROR_ARG;
    for (std::size_t i = 0; i < g_instances.size(); ++i) {
        Instance& instance = g_instances[i];
        if (instance.decoder) continue;
        instance = {};
        instance.codecType = codec_type;
        if (codec_type == TYPE_AT9) {
            instance.decoder = Audiodec::CreateAt9(param.at9.i_bw_pcm, param.at9.ui_config_data, instance.at9);
            if (!instance.decoder) return ERROR_AT9_INVALID_CONFIG_DATA;
            fillAt9Info(&info.at9, instance.at9);
            if (!copyOut(localCtrl.pBsiInfo, &info.at9, infoSize)) {
                instance = {};
                return ERROR_INVALID_BSI_INFO_POINTER;
            }
        } else if (codec_type == TYPE_MP3) {
            instance.decoder = Audiodec::CreateMp3(param.mp3.i_bw_pcm);
        } else {
            if (const std::int32_t result = createAac(&param.m4aac, &info.m4aac, instance); result != 0) {
                instance = {};
                return result;
            }
            if (!copyOut(localCtrl.pBsiInfo, &info.m4aac, infoSize)) {
                instance = {};
                return ERROR_INVALID_BSI_INFO_POINTER;
            }
        }
        return static_cast<int32_t>(i + 1);
    }
    return ERROR_API_FAIL;
}

int32_t APS5_VABI sceAudiodecDeleteDecoder(int32_t handle) {
    std::lock_guard lock(g_mutex);
    Instance* instance = find(handle);
    if (!instance) return ERROR_INVALID_HANDLE;
    *instance = {};
    return 0;
}

int32_t APS5_VABI sceAudiodecClearContext(int32_t handle) {
    std::lock_guard lock(g_mutex);
    Instance* instance = find(handle);
    if (!instance) return ERROR_INVALID_HANDLE;
    instance->decoder->Reset();
    return 0;
}

int32_t APS5_VABI sceAudiodecDecode(int32_t handle, AudiodecCtrl* ctrl) {
    AudiodecCtrl localCtrl{};
    AudiodecAuInfo auInfo{};
    AudiodecPcmItem pcmItem{};
    if (const std::int32_t result = loadDecodeCtrl(ctrl, localCtrl, auInfo, pcmItem); result != 0) return result;
    std::lock_guard lock(g_mutex);
    Instance* instance = find(handle);
    if (!instance) return ERROR_INVALID_HANDLE;
    InfoBuffer info{};
    if (const std::int32_t result = loadDecodeInfo(localCtrl, instance->codecType, info); result != 0) return result;

    std::vector<std::uint8_t> auData(auInfo.ui_au_size);
    if (!copyIn(auData.data(), auInfo.p_au_addr, auData.size())) return ERROR_INVALID_AU_POINTER;
    std::vector<std::uint8_t> pcmData(pcmItem.ui_pcm_size);

    const Audiodec::DecodeResult decoded = instance->decoder->Decode(auData.data(), static_cast<std::uint32_t>(auData.size()),
                                                                     pcmData.data(), static_cast<std::uint32_t>(pcmData.size()));
    if (instance->codecType == TYPE_M4AAC) {
        if (instance->surroundChannelInterleaveOrder != 0 && decoded.channels > 2) throw std::runtime_error("M4AAC surround channel interleave order " + std::to_string(instance->surroundChannelInterleaveOrder) + " is not implemented for " + std::to_string(decoded.channels) + " decoded channels");
        info.m4aac.i_result = decoded.status == Audiodec::DecodeStatus::InvalidData ? M4AAC_RESULT_DECODE_ERROR
            : decoded.status == Audiodec::DecodeStatus::PartialInput ? M4AAC_RESULT_INSUFFICIENT_DATA : 0;
        if (decoded.channels != 0) {
            info.m4aac.ui_sampling_freq = decoded.sampleRate;
            info.m4aac.ui_number_of_channels = decoded.channels;
            info.m4aac.ui_heaac = decoded.highEfficiency;
        }
        if (!copyOut(localCtrl.pBsiInfo, &info.m4aac, sizeof(info.m4aac))) return ERROR_INVALID_BSI_INFO_POINTER;
    }
    if (decoded.status == Audiodec::DecodeStatus::NotEnoughRoom) return ERROR_INVALID_PCM_SIZE;
    if (decoded.status != Audiodec::DecodeStatus::Ok) {
        auInfo.ui_au_size = 0;
        pcmItem.ui_pcm_size = 0;
        if (!copyOut(localCtrl.pAuInfo, &auInfo, sizeof(auInfo))) return ERROR_INVALID_AU_INFO_POINTER;
        if (!copyOut(localCtrl.pPcmItem, &pcmItem, sizeof(pcmItem))) return ERROR_INVALID_PCM_ITEM_POINTER;
        return ERROR_API_FAIL;
    }
    if (decoded.produced > 0 && !copyOut(pcmItem.p_pcm_addr, pcmData.data(), decoded.produced)) return ERROR_INVALID_PCM_POINTER;
    if (instance->codecType == TYPE_MP3) {
        fillMp3Info(&info.mp3, decoded.mp3);
        if (!copyOut(localCtrl.pBsiInfo, &info.mp3, sizeof(info.mp3))) return ERROR_INVALID_BSI_INFO_POINTER;
    }
    if (instance->codecType == TYPE_AT9) {
        fillAt9Info(&info.at9, instance->at9);
        if (!copyOut(localCtrl.pBsiInfo, &info.at9, sizeof(info.at9))) return ERROR_INVALID_BSI_INFO_POINTER;
    }
    auInfo.ui_au_size = static_cast<std::uint32_t>(decoded.consumed);
    pcmItem.ui_pcm_size = static_cast<std::uint32_t>(decoded.produced);
    if (!copyOut(localCtrl.pAuInfo, &auInfo, sizeof(auInfo))) return ERROR_INVALID_AU_INFO_POINTER;
    if (!copyOut(localCtrl.pPcmItem, &pcmItem, sizeof(pcmItem))) return ERROR_INVALID_PCM_ITEM_POINTER;
    return 0;
}

}
