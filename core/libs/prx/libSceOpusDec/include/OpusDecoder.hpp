#ifndef CORE_LIBS_PRX_LIBSCEOPUSDEC_INCLUDE_OPUSDECODER_HPP
#define CORE_LIBS_PRX_LIBSCEOPUSDEC_INCLUDE_OPUSDECODER_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <opus.h>

namespace {

class OpusDecoderLibrary {
public:
    OpusDecoderLibrary(const char* libraryName, bool celtPacketsOnly) : name(libraryName), celtOnly(celtPacketsOnly) {}

    int Initialize(std::uint32_t* context) {
        if (!context) return error(13);
        std::lock_guard lock(mutex);
        for (const auto& [id, owner] : contexts)
            if (owner == context) fail("context already initialized");
        if (nextContext == 0) fail("context identifiers exhausted");
        const auto id = nextContext++;
        contexts.emplace(id, context);
        *context = id;
        return 0;
    }

    int Terminate(const std::uint32_t* context) {
        if (!context) fail("null context");
        std::lock_guard lock(mutex);
        if (!contexts.contains(*context)) fail("unknown context");
        for (const auto& [state, decoder] : decoders)
            if (decoder->context == *context) fail("termination with live decoders is not implemented");
        contexts.erase(*context);
        return 0;
    }

    int GetSize(int channels) {
        if (channels != 1 && channels != 2) return error(3);
        return 640;
    }

    int CreateEx(const std::uint32_t* context, void* state, int sampleRate, int channels) {
        if (!context || !state) return error(0);
        if (celtOnly && channels != 1 && channels != 2) return error(3);
        if (sampleRate != 8000 && sampleRate != 12000 && sampleRate != 16000 && sampleRate != 24000 && sampleRate != 48000) return error(2);
        if (channels != 1 && channels != 2) return error(3);
        std::lock_guard lock(mutex);
        if (!contexts.contains(*context)) fail("unknown context");
        if (decoders.contains(state)) fail("state already owns a decoder");
        auto decoder = std::make_unique<Decoder>();
        decoder->context = *context;
        decoder->channels = channels;
        decoder->sampleRate = sampleRate;
        int error = OPUS_OK;
        decoder->codec.reset(opus_decoder_create(sampleRate, channels, &error));
        if (error == OPUS_ALLOC_FAIL) throw std::bad_alloc();
        if (!decoder->codec || error != OPUS_OK) fail("cannot create libopus decoder");
        decoders.emplace(state, std::move(decoder));
        return 0;
    }

    int Decode(void* state, const std::uint8_t* data, int bytes, std::int16_t* pcm, int capacity) {
        if (!state) return error(0);
        if (bytes != 0 && !data) return error(6);
        if (bytes < 0) return error(7);
        if (!pcm) return error(8);
        if (capacity < 0) return error(9);
        std::lock_guard lock(mutex);
        const auto found = decoders.find(state);
        if (found == decoders.end()) return error(1);
        if (bytes == 0) fail("packet loss concealment is not implemented");
        auto& decoder = *found->second;
        if (celtOnly && (data[0] >> 3) < 16) fail("SILK and hybrid packets are not implemented");
        const int samples = opus_packet_get_nb_samples(data, bytes, decoder.sampleRate);
        if (samples <= 0) fail("invalid packet duration");
        const int outputBytes = samples * decoder.channels * 2;
        if (capacity < outputBytes) fail("PCM buffer is too small");
        std::vector<std::int16_t> decoded(static_cast<std::size_t>(samples) * decoder.channels);
        const int result = opus_decode(decoder.codec.get(), data, bytes, decoded.data(), samples, 0);
        if (result < 0) fail("libopus rejected the packet");
        if (result != samples) fail("unexpected libopus PCM duration");
        std::memcpy(pcm, decoded.data(), static_cast<std::size_t>(outputBytes));
        return outputBytes;
    }

    int Destroy(void* state) {
        if (!state) return error(0);
        std::lock_guard lock(mutex);
        if (decoders.erase(state) == 0) return error(1);
        return 0;
    }

private:
    struct Decoder {
        std::uint32_t context;
        int channels;
        int sampleRate;
        std::unique_ptr<OpusDecoder, void (*)(OpusDecoder*)> codec{nullptr, opus_decoder_destroy};
    };

    const char* name;
    bool celtOnly;
    std::mutex mutex;
    std::unordered_map<std::uint32_t, std::uint32_t*> contexts;
    std::unordered_map<void*, std::unique_ptr<Decoder>> decoders;
    std::uint32_t nextContext = 1;

    int error(std::uint32_t offset) const {
        return static_cast<int>((celtOnly ? 0x81400000u : 0x81740000u) + offset);
    }

    [[noreturn]] void fail(const char* message) const {
        throw std::runtime_error(std::string(name) + ": " + message);
    }
};

} // namespace

#endif
