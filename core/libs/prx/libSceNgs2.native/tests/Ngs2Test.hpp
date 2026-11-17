#ifndef CORE_LIBS_PRX_LIBSCENGS2_TESTS_NGS2TEST_HPP
#define CORE_LIBS_PRX_LIBSCENGS2_TESTS_NGS2TEST_HPP

#include "prx/libSceNgs2.native/include/Exports.hpp"
#include "Testing.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <vector>

inline bool FrameInfoThrows(const Ngs2WaveformFormat& format) {
    std::uint32_t values[4] = {0xa1, 0xa2, 0xa3, 0xa4};
    bool threw = false;
    try {
        sceNgs2GetWaveformFrameInfo(&format, &values[0], &values[1], &values[2], &values[3]);
    } catch (const std::exception&) {
        threw = true;
    }
    Require(values[0] == 0xa1 && values[1] == 0xa2 && values[2] == 0xa3 && values[3] == 0xa4);
    return threw;
}

#if defined(NGS2_TEST_GRAIN)
inline constexpr std::uint32_t Grain = NGS2_TEST_GRAIN;
#else
inline constexpr std::uint32_t Grain = 64;
#endif

template <typename TSignal>
inline float SamplerReference(std::uint32_t frame, float pitch, TSignal signal) {
    const float position = frame * pitch;
    const auto index = static_cast<std::uint32_t>(position);
    const auto source = [&](std::uint32_t offset) { return offset < 4 ? 0.0f : signal(offset - 4); };
    if (position == index) return source(index + 1);
    Require(position - index == 0.5f);
    return (source(index) * -0.0303300861f + source(index + 1) * 0.530330062f) +
           (source(index + 2) * 0.530330062f + source(index + 3) * -0.0303300861f);
}

inline std::vector<std::vector<std::uint64_t>> buffers;
inline std::size_t usedBuffers = 0;

struct TestRackBuffer {
    std::uintptr_t rack;
    void* buffer;
};
inline std::vector<TestRackBuffer> testRackBuffers;

inline int TestRackCreate(std::uintptr_t system, std::uint32_t id, const Ngs2RackOption* option,
                          const Ngs2ContextBufferInfo* buffer, std::uintptr_t* rack) {
    const int result = sceNgs2RackCreate(system, id, option, buffer, rack);
    if (result == SCE_NGS2_OK) testRackBuffers.push_back({*rack, buffer->host_buffer});
    return result;
}

inline void ResetBuffers(std::size_t first) {
    for (auto it = testRackBuffers.begin(); it != testRackBuffers.end();) {
        bool owned = false;
        for (std::size_t i = first; i < usedBuffers; ++i) if (buffers[i].data() == it->buffer) owned = true;
        if (!owned) { ++it; continue; }
        const auto rack = it->rack;
        testRackBuffers.erase(it);
        const int result = sceNgs2RackDestroy(rack, nullptr);
        Require(result == SCE_NGS2_OK || result == SCE_NGS2_ERROR_INVALID_RACK_HANDLE);
        it = testRackBuffers.begin();
    }
    usedBuffers = first;
}

inline Ngs2ContextBufferInfo Buffer(const Ngs2ContextBufferInfo& query) {
    if (usedBuffers == buffers.size()) buffers.emplace_back();
    auto& storage = buffers[usedBuffers++];
    storage.resize(query.host_buffer_size / sizeof(std::uint64_t) + 1);
    Ngs2ContextBufferInfo info{};
    info.host_buffer = storage.data();
    info.host_buffer_size = query.host_buffer_size;
    return info;
}

inline uintptr_t CreateSystem() {
    Ngs2ContextBufferInfo query{};
    Require(sceNgs2SystemQueryBufferSize(nullptr, &query) == SCE_NGS2_OK && query.host_buffer_size != 0);
    const auto info = Buffer(query);
    uintptr_t system = 0;
    Require(sceNgs2SystemCreate(nullptr, &info, &system) == SCE_NGS2_OK && system != 0);
    Require(sceNgs2SystemSetGrainSamples(system, Grain) == SCE_NGS2_OK);
    return system;
}

inline uintptr_t CreateRack(uintptr_t system, std::uint32_t rackId) {
    Ngs2ContextBufferInfo query{};
    Require(sceNgs2RackQueryBufferSize(rackId, nullptr, &query) == SCE_NGS2_OK);
    const auto info = Buffer(query);
    uintptr_t rack = 0;
    Require(TestRackCreate(system, rackId, nullptr, &info, &rack) == SCE_NGS2_OK && rack != 0);
    return rack;
}

inline uintptr_t Voice(uintptr_t rack) {
    uintptr_t voice = 0;
    Require(sceNgs2RackGetVoiceHandle(rack, 0, &voice) == SCE_NGS2_OK && voice != 0);
    return voice;
}

template <typename TParam>
inline void Control(uintptr_t voice, std::uint32_t id, TParam param) {
    param.header = {static_cast<std::uint16_t>(sizeof(TParam)), 0, id};
    Require(sceNgs2VoiceControl(voice, &param.header) == SCE_NGS2_OK);
}

inline void Event(uintptr_t voice, std::uint32_t eventId) {
    Control(voice, SCE_NGS2_VOICE_PARAM_EVENT, Ngs2VoiceEventParam{{}, eventId});
}

inline void Patch(uintptr_t source, uintptr_t dest) {
    Control(source, SCE_NGS2_VOICE_PARAM_PATCH, Ngs2VoicePatchParam{{}, 0, 0, dest});
}

inline uintptr_t Mastering(uintptr_t system, std::uint32_t channels) {
    const auto voice = Voice(CreateRack(system, SCE_NGS2_RACK_ID_MASTERING));
    Control(voice, SCE_NGS2_MASTERING_VOICE_PARAM_SETUP, Ngs2MasteringVoiceSetupParam{{}, channels, 0});
    Control(voice, SCE_NGS2_MASTERING_VOICE_PARAM_OUTPUT, Ngs2MasteringVoiceOutputParam{{}, 0, 0});
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    return voice;
}

inline std::uint32_t Flags(uintptr_t voice) {
    std::uint32_t flags = 0xffffffff;
    Require(sceNgs2VoiceGetStateFlags(voice, &flags) == SCE_NGS2_OK);
    return flags;
}

#endif
