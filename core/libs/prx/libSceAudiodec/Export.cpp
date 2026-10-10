#include <cstdint>
#include <cstddef>
#include <array>
#include <memory>
#include <mutex>
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

std::int32_t validateWordSize(std::int32_t wordSize) {
    if (wordSize == Audiodec::WORD_SIZE_16BIT || wordSize == Audiodec::WORD_SIZE_FLOAT) return 0;
    if (wordSize == 0) return ERROR_INVALID_WORD_LENGTH;
    return ERROR_INVALID_WORD_LENGTH;
}

std::int32_t validateCtrl(const AudiodecCtrl* ctrl, std::uint32_t codecType) {
    if (!ctrl) return ERROR_INVALID_CTRL_POINTER;
    if (!ctrl->pParam) return ERROR_INVALID_PARAM_POINTER;
    if (!ctrl->pBsiInfo) return ERROR_INVALID_BSI_INFO_POINTER;
    const std::uint32_t paramSize = *static_cast<const std::uint32_t*>(ctrl->pParam);
    const std::uint32_t infoSize = *static_cast<const std::uint32_t*>(ctrl->pBsiInfo);
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
            if (paramSize < sizeof(AudiodecParamM4aac)) return ERROR_INVALID_PARAM_SIZE;
            if (infoSize != sizeof(AudiodecM4aacInfo)) return ERROR_INVALID_BSI_INFO_SIZE;
            break;
    }
    return validateWordSize(wordSizeOf(ctrl->pParam));
}

std::int32_t validateBuffers(const AudiodecCtrl* ctrl) {
    if (!ctrl->pAuInfo) return ERROR_INVALID_AU_INFO_POINTER;
    if (!ctrl->pPcmItem) return ERROR_INVALID_PCM_ITEM_POINTER;
    if (ctrl->pAuInfo->ui_size != sizeof(AudiodecAuInfo)) return ERROR_INVALID_AU_INFO_SIZE;
    if (ctrl->pPcmItem->ui_size != sizeof(AudiodecPcmItem)) return ERROR_INVALID_PCM_ITEM_SIZE;
    if (!ctrl->pAuInfo->p_au_addr) return ERROR_INVALID_AU_POINTER;
    if (!ctrl->pPcmItem->p_pcm_addr) return ERROR_INVALID_PCM_POINTER;
    if (ctrl->pAuInfo->ui_au_size == 0) return ERROR_INVALID_AU_SIZE;
    if (ctrl->pPcmItem->ui_pcm_size == 0) return ERROR_INVALID_PCM_SIZE;
    return 0;
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
    if (param->ui_config_number != M4AAC_CONFIG_ADTS && param->ui_config_number != M4AAC_CONFIG_RAW) return ERROR_M4AAC_INVALID_CONFIG_NUMBER;
    const std::uint32_t sampleRate = Audiodec::AacSampleRate(param->ui_sampling_freq_index);
    if (param->ui_config_number == M4AAC_CONFIG_RAW && sampleRate == 0) return ERROR_M4AAC_INVALID_SAMPLING_FREQ;
    if (param->ui_max_channels > 8) return ERROR_M4AAC_INVALID_MAX_CHANNELS;
    if (param->ui_enable_heaac > 1) return ERROR_M4AAC_INVALID_ENABLE_HEAAC;
    const std::uint32_t channels = param->ui_max_channels == 0 ? 2 : param->ui_max_channels;
    instance.decoder = Audiodec::CreateAac(param->i_bw_pcm, param->ui_config_number == M4AAC_CONFIG_ADTS, param->ui_sampling_freq_index, channels);
    instance.channels = channels;
    instance.sampleRate = sampleRate;
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
    if (const std::int32_t result = validateCtrl(ctrl, codec_type); result != 0) return result;
    std::lock_guard lock(g_mutex);
    if (g_initialized[codec_type] == 0) return ERROR_ARG;
    for (std::size_t i = 0; i < g_instances.size(); ++i) {
        Instance& instance = g_instances[i];
        if (instance.decoder) continue;
        instance = {};
        instance.codecType = codec_type;
        if (codec_type == TYPE_AT9) {
            const auto* param = static_cast<const AudiodecParamAt9*>(ctrl->pParam);
            instance.decoder = Audiodec::CreateAt9(param->i_bw_pcm, param->ui_config_data, instance.at9);
            if (!instance.decoder) return ERROR_AT9_INVALID_CONFIG_DATA;
            fillAt9Info(static_cast<AudiodecAt9Info*>(ctrl->pBsiInfo), instance.at9);
        } else if (codec_type == TYPE_MP3) {
            instance.decoder = Audiodec::CreateMp3(static_cast<const AudiodecParamMp3*>(ctrl->pParam)->i_bw_pcm);
        } else if (const std::int32_t result = createAac(static_cast<const AudiodecParamM4aac*>(ctrl->pParam), static_cast<AudiodecM4aacInfo*>(ctrl->pBsiInfo), instance); result != 0) {
            instance = {};
            return result;
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
    std::lock_guard lock(g_mutex);
    Instance* instance = find(handle);
    if (!instance) return ERROR_INVALID_HANDLE;
    if (const std::int32_t result = validateCtrl(ctrl, instance->codecType); result != 0) return result;
    if (const std::int32_t result = validateBuffers(ctrl); result != 0) return result;

    const Audiodec::DecodeResult decoded = instance->decoder->Decode(static_cast<const std::uint8_t*>(ctrl->pAuInfo->p_au_addr), ctrl->pAuInfo->ui_au_size,
                                                                     static_cast<std::uint8_t*>(ctrl->pPcmItem->p_pcm_addr), ctrl->pPcmItem->ui_pcm_size);
    if (instance->codecType == TYPE_M4AAC) {
        auto* info = static_cast<AudiodecM4aacInfo*>(ctrl->pBsiInfo);
        info->i_result = decoded.status == Audiodec::DecodeStatus::InvalidData ? M4AAC_RESULT_DECODE_ERROR
            : decoded.status == Audiodec::DecodeStatus::PartialInput ? M4AAC_RESULT_INSUFFICIENT_DATA : 0;
        if (decoded.channels != 0) {
            info->ui_sampling_freq = decoded.sampleRate;
            info->ui_number_of_channels = decoded.channels;
            info->ui_heaac = decoded.highEfficiency;
        }
    }
    if (decoded.status == Audiodec::DecodeStatus::NotEnoughRoom) return ERROR_INVALID_PCM_SIZE;
    if (decoded.status != Audiodec::DecodeStatus::Ok) {
        ctrl->pAuInfo->ui_au_size = 0;
        ctrl->pPcmItem->ui_pcm_size = 0;
        return ERROR_API_FAIL;
    }
    if (instance->codecType == TYPE_MP3) fillMp3Info(static_cast<AudiodecMp3Info*>(ctrl->pBsiInfo), decoded.mp3);
    if (instance->codecType == TYPE_AT9) fillAt9Info(static_cast<AudiodecAt9Info*>(ctrl->pBsiInfo), instance->at9);
    ctrl->pAuInfo->ui_au_size = static_cast<std::uint32_t>(decoded.consumed);
    ctrl->pPcmItem->ui_pcm_size = static_cast<std::uint32_t>(decoded.produced);
    return 0;
}

}
