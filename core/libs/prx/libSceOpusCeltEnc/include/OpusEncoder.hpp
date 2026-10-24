#ifndef CORE_LIBS_PRX_LIBSCEOPUSCELTENC_INCLUDE_OPUSENCODER_HPP
#define CORE_LIBS_PRX_LIBSCEOPUSCELTENC_INCLUDE_OPUSENCODER_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <opus.h>

namespace {

class OpusEncoderLibrary {
public:
    OpusEncoderLibrary(const char* libraryName, int restrictedApplication, std::uint32_t base) : name(libraryName), application(restrictedApplication), errorBase(base) {}

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
        for (const auto& [state, encoder] : encoders)
            if (encoder->context == *context) fail("termination with live encoders is not implemented");
        contexts.erase(*context);
        return 0;
    }

    int GetSize(int channels) {
        if (channels != 1 && channels != 2) return error(3);
        return opus_encoder_get_size(channels);
    }

    int Create(void* state, int sampleRate, int channels, int createApplication) {
        if (!state) return error(0);
        if (channels != 1 && channels != 2) return error(3);
        if (sampleRate != 8000 && sampleRate != 12000 && sampleRate != 16000 && sampleRate != 24000 && sampleRate != 48000) return error(2);
        if (createApplication != application) return error(2);
        std::lock_guard lock(mutex);
        if (encoders.contains(state)) fail("state already owns an encoder");
        Make(state, 0, sampleRate, channels);
        return 0;
    }

    int CreateEx(const std::uint32_t* context, void* state, int sampleRate, int channels, int createApplication) {
        if (!context || !state) return error(0);
        if (channels != 1 && channels != 2) return error(3);
        if (sampleRate != 8000 && sampleRate != 12000 && sampleRate != 16000 && sampleRate != 24000 && sampleRate != 48000) return error(2);
        if (createApplication != application) return error(2);
        std::lock_guard lock(mutex);
        if (!contexts.contains(*context)) fail("unknown context");
        if (encoders.contains(state)) fail("state already owns an encoder");
        Make(state, *context, sampleRate, channels);
        return 0;
    }

    int Encode(void* state, const std::int16_t* samples, int frameSize, std::uint8_t* data, int maxDataBytes) {
        if (!state) return error(0);
        if (frameSize != 0 && !samples) return error(6);
        if (frameSize < 0) return error(7);
        if (!data) return error(8);
        if (maxDataBytes < 0) return error(9);
        std::lock_guard lock(mutex);
        const auto found = encoders.find(state);
        if (found == encoders.end()) return error(1);
        if (frameSize == 0) fail("a zero-frame encode is not implemented");
        const int bytes = opus_encode(found->second->codec.get(), samples, frameSize, data, maxDataBytes);
        if (bytes < 0) fail("libopus rejected the frame");
        return bytes;
    }

    int EncodeFloat(void* state, const float* samples, int frameSize, std::uint8_t* data, int maxDataBytes) {
        if (!state) return error(0);
        if (frameSize != 0 && !samples) return error(6);
        if (frameSize < 0) return error(7);
        if (!data) return error(8);
        if (maxDataBytes < 0) return error(9);
        std::lock_guard lock(mutex);
        const auto found = encoders.find(state);
        if (found == encoders.end()) return error(1);
        if (frameSize == 0) fail("a zero-frame encode is not implemented");
        const int bytes = opus_encode_float(found->second->codec.get(), samples, frameSize, data, maxDataBytes);
        if (bytes < 0) fail("libopus rejected the frame");
        return bytes;
    }

    int Destroy(void* state) {
        if (!state) return error(0);
        std::lock_guard lock(mutex);
        if (encoders.erase(state) == 0) return error(1);
        return 0;
    }

private:
    struct Encoder {
        std::uint32_t context;
        int channels;
        int sampleRate;
        std::unique_ptr<OpusEncoder, void (*)(OpusEncoder*)> codec{nullptr, opus_encoder_destroy};
    };

    void Make(void* state, std::uint32_t context, int sampleRate, int channels) {
        auto encoder = std::make_unique<Encoder>();
        encoder->context = context;
        encoder->channels = channels;
        encoder->sampleRate = sampleRate;
        int opusError = OPUS_OK;
        encoder->codec.reset(opus_encoder_create(sampleRate, channels, application, &opusError));
        if (opusError == OPUS_ALLOC_FAIL) throw std::bad_alloc();
        if (!encoder->codec || opusError != OPUS_OK) fail("cannot create libopus encoder");
        encoders.emplace(state, std::move(encoder));
    }

    const char* name;
    int application;
    std::uint32_t errorBase;
    std::mutex mutex;
    std::unordered_map<std::uint32_t, std::uint32_t*> contexts;
    std::unordered_map<void*, std::unique_ptr<Encoder>> encoders;
    std::uint32_t nextContext = 1;

    int error(std::uint32_t offset) const {
        return static_cast<int>(errorBase + offset);
    }

    [[noreturn]] void fail(const char* message) const {
        throw std::runtime_error(std::string(name) + ": " + message);
    }
};

} // namespace

#endif
