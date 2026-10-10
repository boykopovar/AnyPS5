#ifndef CORE_LIBS_PRX_LIBSCENGS2_TESTS_NGS2TEST_HPP
#define CORE_LIBS_PRX_LIBSCENGS2_TESTS_NGS2TEST_HPP

#include "prx/libSceNgs2.native/include/Ngs2Types.hpp"

#include <Testing/Test.hpp>

#include <cstdint>
#include <string>
#include <vector>

extern "C" {
int APS5_VABI sceNgs2ParseWaveformData(const void*, size_t, Ngs2WaveformInfo*);
int APS5_VABI sceNgs2ParseWaveformFile(const char*, uint32_t, Ngs2WaveformInfo*);
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
int APS5_VABI sceNgs2RackGetInfo(uintptr_t, Ngs2RackInfo*, size_t);
int APS5_VABI sceNgs2VoiceControl(uintptr_t, const Ngs2VoiceParamHeader*);
int APS5_VABI sceNgs2VoiceRunCommands(uintptr_t, const Ngs2VoiceCommand*, size_t);
int APS5_VABI sceNgs2VoiceGetState(uintptr_t, Ngs2VoiceState*, size_t);
int APS5_VABI sceNgs2VoiceGetStateFlags(uintptr_t, uint32_t*);
int APS5_VABI sceNgs2VoiceGetPortInfo(uintptr_t, uint32_t, Ngs2VoicePortInfo*, size_t);
int APS5_VABI sceNgs2VoiceQueryInfo(uintptr_t, uint32_t, void*, size_t);
}

namespace Ngs2Testing {

inline constexpr std::uint32_t Grain = 8;

class Ngs2Fixture {
public:
    Ngs2Fixture() = default;

    ~Ngs2Fixture() {
        for (auto system = systems.rbegin(); system != systems.rend(); ++system) {
            try {
                sceNgs2SystemDestroy(*system, nullptr);
            } catch (...) {
            }
        }
    }

    Ngs2Fixture(const Ngs2Fixture&) = delete;
    Ngs2Fixture& operator=(const Ngs2Fixture&) = delete;

    Ngs2ContextBufferInfo Buffer(const Ngs2ContextBufferInfo& query) {
        auto& storage = buffers.emplace_back(query.host_buffer_size / sizeof(std::uint64_t) + 1);
        Ngs2ContextBufferInfo info{};
        info.host_buffer = storage.data();
        info.host_buffer_size = query.host_buffer_size;
        return info;
    }

    uintptr_t CreateSystem() {
        Ngs2ContextBufferInfo query{};
        Testing::RequireEqual(sceNgs2SystemQueryBufferSize(nullptr, &query), SCE_NGS2_OK, "query the system buffer size");
        Testing::Require(query.host_buffer_size != 0, "system buffer size is not zero");
        const auto info = Buffer(query);
        uintptr_t system = 0;
        Testing::RequireEqual(sceNgs2SystemCreate(nullptr, &info, &system), SCE_NGS2_OK, "create the system");
        Testing::Require(system != 0, "system handle is not zero");
        systems.push_back(system);
        Testing::RequireEqual(sceNgs2SystemSetGrainSamples(system, Grain), SCE_NGS2_OK, "set the grain size");
        return system;
    }

    uintptr_t CreateRack(uintptr_t system, std::uint32_t rackId, const Ngs2RackOption* option = nullptr) {
        Ngs2ContextBufferInfo query{};
        Testing::RequireEqual(sceNgs2RackQueryBufferSize(rackId, option, &query), SCE_NGS2_OK,
                              "query the buffer size of rack " + std::to_string(rackId));
        const auto info = Buffer(query);
        uintptr_t rack = 0;
        Testing::RequireEqual(sceNgs2RackCreate(system, rackId, option, &info, &rack), SCE_NGS2_OK,
                              "create rack " + std::to_string(rackId));
        Testing::Require(rack != 0, "rack handle is not zero");
        return rack;
    }

    uintptr_t Mastering(uintptr_t system, std::uint32_t channels);

    void Track(uintptr_t system) {
        systems.push_back(system);
    }

private:
    std::vector<std::vector<std::uint64_t>> buffers;
    std::vector<uintptr_t> systems;
};

inline uintptr_t RackVoice(uintptr_t rack, std::uint32_t index) {
    uintptr_t voice = 0;
    Testing::RequireEqual(sceNgs2RackGetVoiceHandle(rack, index, &voice), SCE_NGS2_OK,
                          "get voice " + std::to_string(index));
    Testing::Require(voice != 0, "voice handle is not zero");
    return voice;
}

inline uintptr_t Voice(uintptr_t rack) {
    return RackVoice(rack, 0);
}

template<typename TParam>
void Control(uintptr_t voice, std::uint32_t id, TParam param) {
    param.header = {static_cast<std::uint16_t>(sizeof(TParam)), 0, id};
    Testing::RequireEqual(sceNgs2VoiceControl(voice, &param.header), SCE_NGS2_OK, "voice control " + std::to_string(id));
}

inline void Event(uintptr_t voice, std::uint32_t eventId) {
    Control(voice, SCE_NGS2_VOICE_PARAM_EVENT, Ngs2VoiceEventParam{{}, eventId});
}

inline void Patch(uintptr_t source, uintptr_t dest) {
    Control(source, SCE_NGS2_VOICE_PARAM_PATCH, Ngs2VoicePatchParam{{}, 0, 0, dest});
}

inline uintptr_t Ngs2Fixture::Mastering(uintptr_t system, std::uint32_t channels) {
    const auto voice = Voice(CreateRack(system, SCE_NGS2_RACK_ID_MASTERING));
    Control(voice, SCE_NGS2_MASTERING_VOICE_PARAM_SETUP, Ngs2MasteringVoiceSetupParam{{}, channels, 0});
    Control(voice, SCE_NGS2_MASTERING_VOICE_PARAM_OUTPUT, Ngs2MasteringVoiceOutputParam{{}, 0, 0});
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    return voice;
}

inline std::uint32_t Flags(uintptr_t voice) {
    std::uint32_t flags = 0xffffffff;
    Testing::RequireEqual(sceNgs2VoiceGetStateFlags(voice, &flags), SCE_NGS2_OK, "get voice state flags");
    return flags;
}

inline void RequireDestroyed(uintptr_t system) {
    Testing::RequireEqual(sceNgs2SystemDestroy(system, nullptr), SCE_NGS2_OK, "destroy the system");
}

} // namespace Ngs2Testing

#endif
