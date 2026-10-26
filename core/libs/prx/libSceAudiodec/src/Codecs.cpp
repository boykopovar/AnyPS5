#include "prx/libSceAudiodec/src/Codecs.hpp"

#include "libatrac9.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/log.h>
}

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace Audiodec {

namespace {

constexpr std::uint32_t AAC_SAMPLE_RATES[13] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350};

void writeSample(float value, std::int32_t wordSize, std::uint8_t* out) {
    if (wordSize == WORD_SIZE_FLOAT) {
        std::memcpy(out, &value, sizeof(value));
        return;
    }
    const auto sample = static_cast<std::int16_t>(std::clamp(std::lrint(value * 32768.0), -32768L, 32767L));
    std::memcpy(out, &sample, sizeof(sample));
}

class FfmpegDecoder : public Decoder {
public:
    FfmpegDecoder(AVCodecID id, std::int32_t wordSize, std::vector<std::uint8_t> extradata = {}) : wordSize(wordSize), extradata(std::move(extradata)), id(id) {
        static const bool quiet = (av_log_set_level(AV_LOG_QUIET), true);
        (void)quiet;
        open();
    }

    ~FfmpegDecoder() override { avcodec_free_context(&context); }

    void Reset() override { avcodec_flush_buffers(context); }

protected:
    DecodeResult decodePacket(const std::uint8_t* data, std::size_t size, std::uint8_t* pcm, std::size_t pcmSize) {
        AVPacket* packet = av_packet_alloc();
        AVFrame* frame = av_frame_alloc();
        if (!packet || !frame) {
            av_packet_free(&packet);
            av_frame_free(&frame);
            throw std::bad_alloc();
        }
        packet->data = const_cast<std::uint8_t*>(data);
        packet->size = static_cast<int>(size);
        DecodeResult result{};
        result.consumed = size;
        if (avcodec_send_packet(context, packet) < 0) result.status = DecodeStatus::InvalidData;
        while (result.status == DecodeStatus::Ok && avcodec_receive_frame(context, frame) == 0) {
            const auto format = static_cast<AVSampleFormat>(frame->format);
            if (format != AV_SAMPLE_FMT_FLTP && format != AV_SAMPLE_FMT_S16P) throw std::runtime_error("libSceAudiodec: FFmpeg sample format " + std::to_string(frame->format) + " is not converted");
            const auto channels = static_cast<std::size_t>(frame->ch_layout.nb_channels);
            const auto samples = static_cast<std::size_t>(frame->nb_samples);
            const std::size_t sampleBytes = wordSize == WORD_SIZE_FLOAT ? 4 : 2;
            if (result.produced + samples * channels * sampleBytes > pcmSize) {
                result.status = DecodeStatus::NotEnoughRoom;
            } else {
                for (std::size_t sample = 0; sample < samples; ++sample) {
                    for (std::size_t channel = 0; channel < channels; ++channel) {
                        const float value = format == AV_SAMPLE_FMT_FLTP ? reinterpret_cast<const float*>(frame->extended_data[channel])[sample]
                                                                         : reinterpret_cast<const std::int16_t*>(frame->extended_data[channel])[sample] / 32768.0f;
                        writeSample(value, wordSize, pcm + result.produced);
                        result.produced += sampleBytes;
                    }
                }
            }
            result.channels = static_cast<std::uint32_t>(channels);
            result.sampleRate = static_cast<std::uint32_t>(frame->sample_rate);
            result.highEfficiency = context->profile == AV_PROFILE_AAC_HE || context->profile == AV_PROFILE_AAC_HE_V2;
            av_frame_unref(frame);
        }
        av_packet_free(&packet);
        av_frame_free(&frame);
        return result;
    }

    void reopen(std::vector<std::uint8_t> replacement) {
        avcodec_free_context(&context);
        extradata = std::move(replacement);
        open();
    }

private:
    void open() {
        const AVCodec* codec = avcodec_find_decoder(id);
        context = codec ? avcodec_alloc_context3(codec) : nullptr;
        if (!context) throw std::runtime_error("libSceAudiodec: cannot allocate an FFmpeg decoder");
        if (!extradata.empty()) {
            context->extradata = static_cast<std::uint8_t*>(av_mallocz(extradata.size() + AV_INPUT_BUFFER_PADDING_SIZE));
            if (!context->extradata) throw std::bad_alloc();
            std::memcpy(context->extradata, extradata.data(), extradata.size());
            context->extradata_size = static_cast<int>(extradata.size());
        }
        if (avcodec_open2(context, codec, nullptr) < 0) throw std::runtime_error("libSceAudiodec: cannot open an FFmpeg decoder");
    }

    std::int32_t wordSize;
    std::vector<std::uint8_t> extradata;
    AVCodecID id;
    AVCodecContext* context = nullptr;
};

class Mp3Decoder final : public FfmpegDecoder {
public:
    explicit Mp3Decoder(std::int32_t wordSize) : FfmpegDecoder(AV_CODEC_ID_MP3, wordSize) {}

    DecodeResult Decode(const std::uint8_t* data, std::size_t size, std::uint8_t* pcm, std::size_t pcmSize) override {
        Mp3Header header{};
        if (!ParseMp3Header(data, size, header)) return {DecodeStatus::InvalidData};
        if (header.frameBytes > size) return {DecodeStatus::PartialInput};
        DecodeResult result = decodePacket(data, header.frameBytes, pcm, pcmSize);
        result.mp3 = header;
        return result;
    }
};

struct AacLayout {
    std::uint32_t objectType;
    std::uint32_t samplingIndex;
    std::uint32_t channelConfiguration;
    bool operator==(const AacLayout&) const = default;
};

enum class Sbr {
    Implicit,
    Absent,
    WithoutParametricStereo,
};

std::vector<std::uint8_t> audioSpecificConfig(const AacLayout& layout, Sbr sbr, std::uint32_t sbrSampleRate) {
    std::vector<std::uint8_t> bytes;
    std::size_t bits = 0;
    const auto put = [&](std::uint32_t value, int width) {
        for (int bit = width - 1; bit >= 0; --bit, ++bits) {
            if (bits % 8 == 0) bytes.push_back(0);
            bytes.back() |= static_cast<std::uint8_t>((value >> bit & 1) << (7 - bits % 8));
        }
    };
    put(layout.objectType, 5);
    put(layout.samplingIndex, 4);
    put(layout.channelConfiguration, 4);
    put(0, 3);
    if (sbr == Sbr::Implicit) return bytes;
    put(0x2B7, 11);
    put(5, 5);
    put(sbr == Sbr::WithoutParametricStereo, 1);
    if (sbr == Sbr::WithoutParametricStereo) {
        const auto* rate = std::find(std::begin(AAC_SAMPLE_RATES), std::end(AAC_SAMPLE_RATES), sbrSampleRate);
        if (rate == std::end(AAC_SAMPLE_RATES)) {
            put(15, 4);
            put(sbrSampleRate, 24);
        } else {
            put(static_cast<std::uint32_t>(rate - std::begin(AAC_SAMPLE_RATES)), 4);
        }
        put(0x548, 11);
        put(0, 1);
    }
    return bytes;
}

class AacDecoder final : public FfmpegDecoder {
public:
    AacDecoder(std::int32_t wordSize, bool adts, bool highEfficiency, std::vector<AacLayout> layouts)
        : FfmpegDecoder(AV_CODEC_ID_AAC, wordSize, layouts.empty() ? std::vector<std::uint8_t>{} : audioSpecificConfig(layouts.front(), highEfficiency ? Sbr::Implicit : Sbr::Absent, 0)),
          adts(adts), highEfficiency(highEfficiency), sbr(highEfficiency ? Sbr::Implicit : Sbr::Absent), layouts(std::move(layouts)) {}

    DecodeResult Decode(const std::uint8_t* data, std::size_t size, std::uint8_t* pcm, std::size_t pcmSize) override {
        if (!adts) return decodeAccessUnit(data, size, pcm, pcmSize);
        if (size < 7 || data[0] != 0xFF || (data[1] & 0xF6) != 0xF0) return {DecodeStatus::InvalidData};
        const auto frameBytes = static_cast<std::size_t>((data[3] & 3) << 11 | data[4] << 3 | data[5] >> 5);
        const std::size_t headerBytes = (data[1] & 1) != 0 ? 7 : 9;
        if (frameBytes <= headerBytes) return {DecodeStatus::InvalidData};
        if (frameBytes > size) return {DecodeStatus::PartialInput};
        const AacLayout layout{(data[2] >> 6) + 1u, data[2] >> 2 & 15u, (data[2] & 1u) << 2 | data[3] >> 6};
        if (layout.channelConfiguration == 0) {
            if (!layouts.empty()) {
                layouts.clear();
                reopen({});
            }
            return decodePacket(data, frameBytes, pcm, pcmSize);
        }
        if (layouts.empty() || layouts.front() != layout) {
            layouts = {layout};
            sbr = highEfficiency ? Sbr::Implicit : Sbr::Absent;
            reopen(audioSpecificConfig(layout, sbr, 0));
        }
        DecodeResult result = decodeAccessUnit(data + headerBytes, frameBytes - headerBytes, pcm, pcmSize);
        result.consumed = frameBytes;
        return result;
    }

private:
    DecodeResult decodeAccessUnit(const std::uint8_t* data, std::size_t size, std::uint8_t* pcm, std::size_t pcmSize) {
        for (std::size_t index = 0; index < layouts.size(); ++index) {
            if (index != 0) reopen(audioSpecificConfig(layouts[index], sbr, 0));
            DecodeResult result = decodePacket(data, size, pcm, pcmSize);
            if (result.status == DecodeStatus::InvalidData) continue;
            layouts = {layouts[index]};
            if (sbr == Sbr::Implicit && layouts.front().channelConfiguration == 1 && result.channels > 1) {
                sbr = Sbr::WithoutParametricStereo;
                reopen(audioSpecificConfig(layouts.front(), sbr, result.sampleRate));
                result = decodePacket(data, size, pcm, pcmSize);
            }
            return result;
        }
        if (layouts.size() > 1) reopen(audioSpecificConfig(layouts.front(), sbr, 0));
        return {DecodeStatus::InvalidData};
    }

    bool adts;
    bool highEfficiency;
    Sbr sbr;
    std::vector<AacLayout> layouts;
};

class At9Decoder final : public Decoder {
public:
    At9Decoder(std::int32_t wordSize, const std::uint8_t (&config)[4]) : wordSize(wordSize) {
        std::memcpy(this->config, config, sizeof(this->config));
        Reset();
    }

    ~At9Decoder() override { Atrac9ReleaseHandle(handle); }

    void Reset() override {
        if (handle) Atrac9ReleaseHandle(handle);
        handle = Atrac9GetHandle();
        if (!handle || Atrac9InitDecoder(handle, config) != 0) throw std::runtime_error("libSceAudiodec: cannot initialize the ATRAC9 decoder");
        Atrac9GetCodecInfo(handle, &info);
    }

    DecodeResult Decode(const std::uint8_t* data, std::size_t size, std::uint8_t* pcm, std::size_t pcmSize) override {
        const auto superframeSize = static_cast<std::size_t>(info.superframeSize);
        if (size < superframeSize) return {DecodeStatus::PartialInput};
        const std::size_t sampleBytes = wordSize == WORD_SIZE_FLOAT ? 4 : 2;
        const std::size_t frameBytes = static_cast<std::size_t>(info.frameSamples) * info.channels * sampleBytes;
        if (frameBytes * info.framesInSuperframe > pcmSize) return {DecodeStatus::NotEnoughRoom};
        DecodeResult result{};
        std::size_t offset = 0;
        for (int frame = 0; frame < info.framesInSuperframe; ++frame) {
            int used = 0;
            const int status = wordSize == WORD_SIZE_FLOAT
                ? Atrac9DecodeF32(handle, data + offset, static_cast<int>(superframeSize - offset), reinterpret_cast<float*>(pcm + result.produced), &used, 0)
                : Atrac9Decode(handle, data + offset, static_cast<int>(superframeSize - offset), reinterpret_cast<short*>(pcm + result.produced), &used, 0);
            if (status != 0 || used <= 0 || offset + static_cast<std::size_t>(used) > superframeSize) {
                Reset();
                return {DecodeStatus::InvalidData};
            }
            offset += static_cast<std::size_t>(used);
            result.produced += frameBytes;
        }
        result.consumed = superframeSize;
        result.channels = static_cast<std::uint32_t>(info.channels);
        result.sampleRate = static_cast<std::uint32_t>(info.samplingRate);
        return result;
    }

    const Atrac9CodecInfo& Info() const { return info; }

private:
    std::int32_t wordSize;
    std::uint8_t config[4];
    void* handle = nullptr;
    Atrac9CodecInfo info{};
};

}

bool ParseMp3Header(const std::uint8_t* data, std::size_t size, Mp3Header& header) {
    if (size < 4) return false;
    const std::uint32_t word = std::uint32_t{data[0]} << 24 | std::uint32_t{data[1]} << 16 | std::uint32_t{data[2]} << 8 | data[3];
    const auto version = word >> 19 & 3;
    const auto layer = word >> 17 & 3;
    const auto bitrateIndex = word >> 12 & 15;
    const auto rateIndex = word >> 10 & 3;
    if ((word & 0xFFE00000u) != 0xFFE00000u || version == 1 || layer != 1 || bitrateIndex == 0 || bitrateIndex == 15 || rateIndex == 3) return false;
    static constexpr std::uint32_t rates[4][3] = {{11025, 12000, 8000}, {0, 0, 0}, {22050, 24000, 16000}, {44100, 48000, 32000}};
    static constexpr std::uint32_t mpeg1Kbps[16] = {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0};
    static constexpr std::uint32_t mpeg2Kbps[16] = {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0};
    const bool mpeg1 = version == 3;
    const std::uint32_t sampleRate = rates[version][rateIndex];
    const std::uint32_t bitrate = (mpeg1 ? mpeg1Kbps : mpeg2Kbps)[bitrateIndex] * 1000;
    header.word = word;
    header.frameBytes = (mpeg1 ? 144 : 72) * bitrate / sampleRate + (word >> 9 & 1);
    header.crc = (word >> 16 & 1) == 0;
    header.mode = static_cast<std::uint8_t>(word >> 6 & 3);
    header.modeExtension = static_cast<std::uint8_t>(word >> 4 & 3);
    header.copyright = static_cast<std::uint8_t>(word >> 3 & 1);
    header.original = static_cast<std::uint8_t>(word >> 2 & 1);
    header.emphasis = static_cast<std::uint8_t>(word & 3);
    return true;
}

std::uint32_t AacSampleRate(std::uint32_t index) {
    return index < 13 ? AAC_SAMPLE_RATES[index] : 0;
}

std::unique_ptr<Decoder> CreateMp3(std::int32_t wordSize) {
    return std::make_unique<Mp3Decoder>(wordSize);
}

std::unique_ptr<Decoder> CreateAac(std::int32_t wordSize, bool adts, std::uint32_t samplingFreqIndex, std::uint32_t channels, bool highEfficiency) {
    std::vector<AacLayout> layouts;
    if (!adts) {
        const std::uint32_t lowComplexity = 2;
        layouts.push_back({lowComplexity, samplingFreqIndex, channels < 2 ? 1u : 2u});
        for (std::uint32_t channelConfiguration = 3; channelConfiguration <= 7; ++channelConfiguration) {
            if ((channelConfiguration == 7 ? 8 : channelConfiguration) <= channels) layouts.push_back({lowComplexity, samplingFreqIndex, channelConfiguration});
        }
    }
    return std::make_unique<AacDecoder>(wordSize, adts, highEfficiency, std::move(layouts));
}

std::unique_ptr<Decoder> CreateAt9(std::int32_t wordSize, const std::uint8_t (&config)[4], At9Format& format) {
    if (!ValidAt9Config(config)) return nullptr;
    auto decoder = std::make_unique<At9Decoder>(wordSize, config);
    const auto& info = decoder->Info();
    format = {static_cast<std::uint32_t>(info.channels), static_cast<std::uint32_t>(info.samplingRate), static_cast<std::uint32_t>(info.superframeSize),
              static_cast<std::uint32_t>(info.framesInSuperframe), static_cast<std::uint32_t>(info.frameSamples)};
    return decoder;
}

bool ValidAt9Config(const std::uint8_t (&config)[4]) {
    void* handle = Atrac9GetHandle();
    std::uint8_t copy[4];
    std::memcpy(copy, config, sizeof(copy));
    const bool valid = handle && Atrac9InitDecoder(handle, copy) == 0;
    if (handle) Atrac9ReleaseHandle(handle);
    return valid;
}

}
