#include "prx/libSceOpusCeltDec/include/Decoder.hpp"
#include "prx/libc/include/General.hpp"
#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <opus.h>

namespace {

struct Decoder {
    std::uint32_t context;
    int channels;
    OpusDecoder* codec = nullptr;
};

std::mutex mutex;
std::uint32_t nextContext = 1;
std::unordered_set<std::uint32_t> contexts;
std::unordered_map<void*, std::unique_ptr<Decoder>> decoders;

}

namespace OpusCeltDec {

int APS5_VABI Initialize_nid_no_patch(std::uint32_t* context) {
    if (!context) throw std::invalid_argument("Opus: null context");
    std::lock_guard lock(mutex);
    if (nextContext == 0) throw std::runtime_error("Opus: context identifiers exhausted");
    const auto id = nextContext++;
    contexts.insert(id);
    *context = id;
    return 0;
}

int APS5_VABI Terminate_nid_no_patch(std::uint32_t* context) {
    if (!context) throw std::invalid_argument("Opus: null context");
    std::lock_guard lock(mutex);
    if (!contexts.contains(*context)) throw std::invalid_argument("Opus: unknown context");
    for (const auto& [state, decoder] : decoders) {
        if (decoder->context == *context) throw std::invalid_argument("Opus: context still owns a decoder");
    }
    contexts.erase(*context);
    *context = 0;
    return 0;
}

int APS5_VABI GetSize_nid_no_patch(int channels) {
    if (channels != 1 && channels != 2) NotImplemented_nid_no_patch("Opus: channels other than mono or stereo");
    return opus_decoder_get_size(channels);
}

int APS5_VABI Create_nid_no_patch(std::uint32_t* context, void* state, int sampleRate, int channels) {
    if (!context || !state) throw std::invalid_argument("Opus: null context or decoder state");
    GetSize_nid_no_patch(channels);
    if (sampleRate != 48000) NotImplemented_nid_no_patch("Opus: sample rates other than 48000");
    std::lock_guard lock(mutex);
    if (!contexts.contains(*context)) throw std::invalid_argument("Opus: unknown context");
    if (decoders.contains(state)) throw std::invalid_argument("Opus: decoder state already in use");
    auto decoder = std::make_unique<Decoder>();
    decoder->context = *context;
    decoder->channels = channels;
    decoder->codec = static_cast<OpusDecoder*>(state);
    if (opus_decoder_init(decoder->codec, sampleRate, channels) != OPUS_OK)
        throw std::runtime_error("Opus: decoder initialization failed");
    decoders.emplace(state, std::move(decoder));
    return 0;
}

int APS5_VABI Decode_nid_no_patch(void* state, const std::uint8_t* input, int packetBytes, std::int16_t* pcm, int capacityBytes) {
    if (!state || !input || !pcm || packetBytes <= 0 || capacityBytes <= 0)
        throw std::invalid_argument("Opus: invalid decode arguments");
    if (input[0] < 128) NotImplemented_nid_no_patch("Opus CELT: SILK or hybrid packet");
    const int samples = opus_packet_get_nb_samples(input, packetBytes, 48000);
    if (samples <= 0 || samples > 5760) throw std::invalid_argument("Opus: invalid packet duration");
    std::lock_guard lock(mutex);
    const auto it = decoders.find(state);
    if (it == decoders.end()) throw std::invalid_argument("Opus: unknown decoder state");
    auto& decoder = *it->second;
    const int outputBytes = samples * decoder.channels * static_cast<int>(sizeof(std::int16_t));
    if (capacityBytes < outputBytes) throw std::invalid_argument("Opus: insufficient PCM capacity");
    std::vector<std::int16_t> output(samples * decoder.channels);
    const int decoded = opus_decode(decoder.codec, input, packetBytes, output.data(), samples, 0);
    if (decoded < 0) throw std::invalid_argument("Opus: invalid packet");
    if (decoded != samples) throw std::runtime_error("Opus: unexpected decoded frame dimensions");
    std::memcpy(pcm, output.data(), outputBytes);
    return outputBytes;
}

int APS5_VABI Destroy_nid_no_patch(void* state) {
    std::lock_guard lock(mutex);
    if (decoders.erase(state) == 0) throw std::invalid_argument("Opus: unknown decoder state");
    return 0;
}

}
