#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <unordered_map>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace {

struct Decoder {
    std::uint32_t context;
    int channels;
    AVCodecContext* codec = nullptr;
    ~Decoder() { avcodec_free_context(&codec); }
};

std::mutex mutex;
std::unordered_map<std::uint32_t, std::uint32_t*> contexts;
std::unordered_map<void*, std::unique_ptr<Decoder>> decoders;
std::uint32_t nextContext = 1;

int PacketSamples(const std::uint8_t* packet, int size) {
    const unsigned config = packet[0] >> 3;
    const int frameSamples = config < 12 ? ((config & 3) == 3 ? 2880 : 480 << (config & 3))
                            : config < 16 ? 480 << (config & 1) : 120 << (config & 3);
    int frames = 1;
    switch (packet[0] & 3) {
    case 1:
    case 2: frames = 2; break;
    case 3:
        if (size < 2) throw std::runtime_error("OpusDec: truncated packet frame count");
        frames = packet[1] & 63;
        break;
    }
    if (frames == 0 || frames * frameSamples > 5760)
        throw std::runtime_error("OpusDec: invalid packet duration");
    return frames * frameSamples;
}

} // namespace

extern "C" {

int APS5_VABI sceOpusDecInitialize(std::uint32_t* context) {
    if (!context) throw std::runtime_error("OpusDec: null context output");
    std::lock_guard lock(mutex);
    for (const auto& [id, owner] : contexts)
        if (owner == context) throw std::runtime_error("OpusDec: context already initialized");
    if (nextContext == 0) throw std::runtime_error("OpusDec: context identifiers exhausted");
    const auto id = nextContext++;
    contexts.emplace(id, context);
    *context = id;
    return 0;
}

int APS5_VABI sceOpusDecTerminate(std::uint32_t* context) {
    if (!context) throw std::runtime_error("OpusDec: null context");
    std::lock_guard lock(mutex);
    if (!contexts.contains(*context)) throw std::runtime_error("OpusDec: unknown context");
    for (const auto& [state, decoder] : decoders)
        if (decoder->context == *context) throw std::runtime_error("OpusDec: termination with live decoders is not implemented");
    contexts.erase(*context);
    return 0;
}

int APS5_VABI sceOpusDecGetSize(int channels) {
    if (channels != 1 && channels != 2) throw std::runtime_error("OpusDec: only mono and stereo are supported");
    return 640;
}

int APS5_VABI sceOpusDecCreateEx(std::uint32_t* context, void* state, int sampleRate, int channels) {
    if (!context || !state) throw std::runtime_error("OpusDec: null context or state");
    sceOpusDecGetSize(channels);
    if (sampleRate != 48000) throw std::runtime_error("OpusDec: sample rates other than 48000 Hz are not implemented");
    std::lock_guard lock(mutex);
    if (!contexts.contains(*context)) throw std::runtime_error("OpusDec: unknown context");
    if (decoders.contains(state)) throw std::runtime_error("OpusDec: state already owns a decoder");
    auto decoder = std::make_unique<Decoder>();
    decoder->context = *context;
    decoder->channels = channels;
    const auto* codec = avcodec_find_decoder(AV_CODEC_ID_OPUS);
    if (!codec) throw std::runtime_error("OpusDec: FFmpeg Opus decoder unavailable");
    decoder->codec = avcodec_alloc_context3(codec);
    if (!decoder->codec) throw std::bad_alloc();
    av_channel_layout_default(&decoder->codec->ch_layout, channels);
    decoder->codec->sample_rate = 48000;
    if (avcodec_open2(decoder->codec, codec, nullptr) < 0)
        throw std::runtime_error("OpusDec: cannot open FFmpeg decoder");
    decoders.emplace(state, std::move(decoder));
    return 0;
}

int APS5_VABI sceOpusDecDecode(void* state, const std::uint8_t* data, int bytes, std::int16_t* pcm, int capacity) {
    if (!data || bytes <= 0 || bytes > std::numeric_limits<int>::max() - AV_INPUT_BUFFER_PADDING_SIZE || !pcm || capacity < 0)
        throw std::runtime_error("OpusDec: invalid packet or PCM buffer; packet loss concealment is not implemented");
    std::lock_guard lock(mutex);
    const auto found = decoders.find(state);
    if (found == decoders.end()) throw std::runtime_error("OpusDec: unknown decoder state");
    auto& decoder = *found->second;
    const int samples = PacketSamples(data, bytes);
    const int outputBytes = samples * decoder.channels * 2;
    if (capacity / 2 * 2 < outputBytes) throw std::runtime_error("OpusDec: PCM buffer is too small");
    std::unique_ptr<AVPacket, void (*)(AVPacket*)> packet(av_packet_alloc(), [](AVPacket* p) { av_packet_free(&p); });
    std::unique_ptr<AVFrame, void (*)(AVFrame*)> frame(av_frame_alloc(), [](AVFrame* f) { av_frame_free(&f); });
    if (!packet || !frame || av_new_packet(packet.get(), bytes) < 0) throw std::bad_alloc();
    std::memcpy(packet->data, data, bytes);
    if (avcodec_send_packet(decoder.codec, packet.get()) < 0 || avcodec_receive_frame(decoder.codec, frame.get()) < 0)
        throw std::runtime_error("OpusDec: FFmpeg rejected the packet");
    const auto format = static_cast<AVSampleFormat>(frame->format);
    if (frame->nb_samples != samples || frame->ch_layout.nb_channels != decoder.channels || frame->sample_rate != 48000 ||
        (format != AV_SAMPLE_FMT_FLTP && format != AV_SAMPLE_FMT_FLT))
        throw std::runtime_error("OpusDec: unexpected FFmpeg PCM format");
    for (int sample = 0; sample < samples; ++sample) {
        for (int channel = 0; channel < decoder.channels; ++channel) {
            const auto* source = reinterpret_cast<const float*>(frame->extended_data[format == AV_SAMPLE_FMT_FLTP ? channel : 0]);
            const float value = source[format == AV_SAMPLE_FMT_FLTP ? sample : sample * decoder.channels + channel];
            const auto converted = static_cast<std::int16_t>(std::lrint(std::clamp(value * 32768.0f, -32768.0f, 32767.0f)));
            std::memcpy(reinterpret_cast<std::uint8_t*>(pcm) + (sample * decoder.channels + channel) * 2, &converted, 2);
        }
    }
    return outputBytes;
}

int APS5_VABI sceOpusDecDestroy(void* state) {
    std::lock_guard lock(mutex);
    if (decoders.erase(state) == 0) throw std::runtime_error("OpusDec: unknown decoder state");
    return 0;
}

}
