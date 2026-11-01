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

struct EncoderCodes {
    std::uint32_t baseError;
    std::uint32_t nullErrorOffset;
    std::uint32_t nullSamplesErrorOffset;
    std::uint32_t frameErrorOffset;
    std::uint32_t nullDataErrorOffset;
    std::uint32_t sizeErrorOffset;
    std::uint32_t rateErrorOffset;
    std::uint32_t channelsErrorOffset;
    std::uint32_t applicationErrorOffset;
    std::uint32_t destroyStateErrorOffset;
    std::uint32_t initializeNullErrorOffset;
    std::size_t stateSize;
    std::size_t stateMagicOffset;
    std::size_t stateInstanceOffset;
    std::size_t stateRateOffset;
    std::size_t stateChannelsOffset;
    std::uint16_t stateMagic;
    std::uint8_t frameSteps[6];
    std::size_t frameStepCount;
    void (*setupState)(void* state, std::uint32_t instanceId, int rate, int channels);
};

class OpusEncoderLibrary {
public:
    OpusEncoderLibrary(const char* libraryName, int callerApplication, int codecApplication, const EncoderCodes& libraryCodes) : name(libraryName), requiredApplication(callerApplication), application(codecApplication), codes(libraryCodes), errorBase(libraryCodes.baseError) {}

    int Initialize(std::uint32_t* context) {
        if (!context) return error(codes.initializeNullErrorOffset);
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
        for (const auto& [id, encoder] : encoders)
            if (encoder->context == *context) fail("termination with live encoders is not implemented");
        contexts.erase(*context);
        return 0;
    }

    int GetSize(int channels) {
        if (channels != 1 && channels != 2) return error(codes.channelsErrorOffset);
        return static_cast<int>(codes.stateSize);
    }

    int Create(void* state, int sampleRate, int channels, int createApplication) {
        if (!state) return error(codes.nullErrorOffset);
        if (!acceptedRate(sampleRate)) return error(codes.rateErrorOffset);
        if (channels != 1 && channels != 2) return error(codes.channelsErrorOffset);
        if (createApplication != requiredApplication) return error(codes.applicationErrorOffset);
        std::lock_guard lock(mutex);
        Make(state, 0, sampleRate, channels);
        return 0;
    }

    int CreateEx(const std::uint32_t* context, void* state, int sampleRate, int channels, int createApplication) {
        if (!context || !state) return error(codes.nullErrorOffset);
        if (!acceptedRate(sampleRate)) return error(codes.rateErrorOffset);
        if (channels != 1 && channels != 2) return error(codes.channelsErrorOffset);
        if (createApplication != requiredApplication) return error(codes.applicationErrorOffset);
        std::lock_guard lock(mutex);
        if (!contexts.contains(*context)) fail("unknown context");
        Make(state, *context, sampleRate, channels);
        return 0;
    }

    int Encode(void* state, const std::int16_t* samples, int frameSize, std::uint8_t* data, int maxDataBytes) {
        if (!state) return error(codes.nullErrorOffset);
        if (!samples) return error(codes.nullSamplesErrorOffset);
        if (frameSize == 0) return error(codes.frameErrorOffset);
        std::lock_guard lock(mutex);
        if (!hasMarker(state)) return error(codes.destroyStateErrorOffset);
        if (!data) return error(codes.nullDataErrorOffset);
        if (maxDataBytes == 0) return error(codes.sizeErrorOffset);
        if (!acceptedFrame(stateRate(state), frameSize)) return error(codes.frameErrorOffset);
        const auto found = encoders.find(stateWord(state, codes.stateInstanceOffset));
        if (found == encoders.end()) fail("state does not reference a live encoder");
        const int bytes = opus_encode(found->second->codec.get(), samples, frameSize, data, maxDataBytes);
        if (bytes < 0) fail("libopus rejected the frame");
        return bytes;
    }

    int EncodeFloat(void* state, const float* samples, int frameSize, std::uint8_t* data, int maxDataBytes) {
        if (!state) return error(codes.nullErrorOffset);
        if (!samples) return error(codes.nullSamplesErrorOffset);
        if (frameSize == 0) return error(codes.frameErrorOffset);
        std::lock_guard lock(mutex);
        if (!hasMarker(state)) return error(codes.destroyStateErrorOffset);
        if (frameSize < 0) fail("the converted frame reservation does not fit the stack");
        if (static_cast<std::uint64_t>(frameSize) * stateWord(state, codes.stateChannelsOffset) * 4 > 0x800000) fail("the converted frame reservation does not fit the stack");
        if (!data) return error(codes.nullDataErrorOffset);
        if (maxDataBytes == 0) return error(codes.sizeErrorOffset);
        if (!acceptedFrame(stateRate(state), frameSize)) return error(codes.frameErrorOffset);
        const auto found = encoders.find(stateWord(state, codes.stateInstanceOffset));
        if (found == encoders.end()) fail("state does not reference a live encoder");
        const int bytes = opus_encode_float(found->second->codec.get(), samples, frameSize, data, maxDataBytes);
        if (bytes < 0) fail("libopus rejected the frame");
        return bytes;
    }

    int Destroy(void* state) {
        if (!state) return error(codes.nullErrorOffset);
        std::uint16_t magic = 0;
        std::memcpy(&magic, static_cast<std::uint8_t*>(state) + codes.stateMagicOffset, sizeof magic);
        if (magic != codes.stateMagic) return error(codes.destroyStateErrorOffset);
        std::lock_guard lock(mutex);
        encoders.erase(stateWord(state, codes.stateInstanceOffset));
        setWord(state, codes.stateMagicOffset, 0);
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
        if (hasMarker(state)) encoders.erase(stateWord(state, codes.stateInstanceOffset));
        const auto instanceId = nextInstanceId++;
        std::memset(state, 0, codes.stateSize);
        codes.setupState(state, instanceId, sampleRate, channels);
        auto encoder = std::make_unique<Encoder>();
        encoder->context = context;
        encoder->channels = channels;
        encoder->sampleRate = sampleRate;
        int opusError = OPUS_OK;
        encoder->codec.reset(opus_encoder_create(sampleRate, channels, application, &opusError));
        if (opusError == OPUS_ALLOC_FAIL) {
            std::memset(state, 0, codes.stateSize);
            throw std::bad_alloc();
        }
        if (!encoder->codec || opusError != OPUS_OK) {
            std::memset(state, 0, codes.stateSize);
            fail("cannot create libopus encoder");
        }
        encoders.emplace(instanceId, std::move(encoder));
    }

    static bool acceptedRate(int rate) {
        return rate == 8000 || rate == 12000 || rate == 16000 || rate == 24000 || rate == 48000;
    }

    bool acceptedFrame(int rate, int frameSize) const {
        if (frameSize < 0 || rate <= 0) return false;
        for (std::size_t index = 0; index < codes.frameStepCount; ++index)
            if (frameSize == rate * codes.frameSteps[index] / 400) return true;
        return false;
    }

    std::uint32_t stateWord(const void* state, std::size_t offset) const {
        std::uint32_t word = 0;
        std::memcpy(&word, static_cast<const std::uint8_t*>(state) + offset, sizeof word);
        return word;
    }

    void setWord(void* state, std::size_t offset, std::uint32_t value) const {
        std::memcpy(static_cast<std::uint8_t*>(state) + offset, &value, sizeof value);
    }

    int stateRate(void* state) const {
        return static_cast<int>(stateWord(state, codes.stateRateOffset));
    }

    bool hasMarker(void* state) const {
        std::uint16_t magic = 0;
        std::memcpy(&magic, static_cast<std::uint8_t*>(state) + codes.stateMagicOffset, sizeof magic);
        return magic == codes.stateMagic;
    }

    const char* name;
    int requiredApplication;
    int application;
    EncoderCodes codes;
    std::uint32_t errorBase;
    std::mutex mutex;
    std::unordered_map<std::uint32_t, std::uint32_t*> contexts;
    std::unordered_map<std::uint32_t, std::unique_ptr<Encoder>> encoders;
    std::uint32_t nextContext = 1;
    std::uint32_t nextInstanceId = 1;

    int error(std::uint32_t offset) const {
        return static_cast<int>(errorBase + offset);
    }

    [[noreturn]] void fail(const char* message) const {
        throw std::runtime_error(std::string(name) + ": " + message);
    }
};

} // namespace

#endif
