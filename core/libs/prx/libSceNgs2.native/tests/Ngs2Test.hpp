#ifndef CORE_LIBS_PRX_LIBSCENGS2_TESTS_NGS2TEST_HPP
#define CORE_LIBS_PRX_LIBSCENGS2_TESTS_NGS2TEST_HPP

#include "prx/libSceNgs2.native/include/Ngs2Types.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

extern "C" {
int APS5_VABI sceNgs2ParseWaveformData(const void*, size_t, Ngs2WaveformInfo*);
int APS5_VABI sceNgs2CalcWaveformBlock(const Ngs2WaveformFormat*, uint32_t, uint32_t, Ngs2WaveformBlock*);
int APS5_VABI sceNgs2SystemResetOption(Ngs2SystemOption*);
int APS5_VABI sceNgs2SystemQueryBufferSize(const Ngs2SystemOption*, Ngs2ContextBufferInfo*);
int APS5_VABI sceNgs2SystemCreate(const Ngs2SystemOption*, const Ngs2ContextBufferInfo*, uintptr_t*);
int APS5_VABI sceNgs2SystemCreateWithAllocator(const Ngs2SystemOption*, const Ngs2BufferAllocator*, uintptr_t*);
int APS5_VABI sceNgs2SystemDestroy(uintptr_t, Ngs2ContextBufferInfo*);
int APS5_VABI sceNgs2SystemGetInfo(uintptr_t, Ngs2SystemInfo*, size_t);
int APS5_VABI sceNgs2SystemSetGrainSamples(uintptr_t, uint32_t);
int APS5_VABI sceNgs2SystemSetSampleRate(uintptr_t, uint32_t);
int APS5_VABI sceNgs2SystemSetUserData(uintptr_t, uintptr_t);
int APS5_VABI sceNgs2SystemGetUserData(uintptr_t, uintptr_t*);
int APS5_VABI sceNgs2SystemLock(uintptr_t);
int APS5_VABI sceNgs2SystemUnlock(uintptr_t);
int APS5_VABI sceNgs2SystemRender(uintptr_t, const Ngs2RenderBufferInfo*, uint32_t);
int APS5_VABI sceNgs2RackQueryBufferSize(uint32_t, const Ngs2RackOption*, Ngs2ContextBufferInfo*);
int APS5_VABI sceNgs2RackCreate(uintptr_t, uint32_t, const Ngs2RackOption*, const Ngs2ContextBufferInfo*, uintptr_t*);
int APS5_VABI sceNgs2RackCreateWithAllocator(uintptr_t, uint32_t, const Ngs2RackOption*, const Ngs2BufferAllocator*, uintptr_t*);
int APS5_VABI sceNgs2RackDestroy(uintptr_t, Ngs2ContextBufferInfo*);
int APS5_VABI sceNgs2RackGetVoiceHandle(uintptr_t, uint32_t, uintptr_t*);
int APS5_VABI sceNgs2VoiceControl(uintptr_t, const Ngs2VoiceParamHeader*);
int APS5_VABI sceNgs2VoiceRunCommands(uintptr_t, const Ngs2VoiceCommand*, size_t);
int APS5_VABI sceNgs2VoiceGetState(uintptr_t, Ngs2VoiceState*, size_t);
int APS5_VABI sceNgs2VoiceGetStateFlags(uintptr_t, uint32_t*);
}

inline void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "NGS2 check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

inline constexpr std::uint32_t Grain = 8;

inline std::vector<std::uint64_t> buffers[32];
inline std::size_t usedBuffers = 0;

inline Ngs2ContextBufferInfo Buffer(const Ngs2ContextBufferInfo& query) {
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
    Require(sceNgs2RackCreate(system, rackId, nullptr, &info, &rack) == SCE_NGS2_OK && rack != 0);
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
