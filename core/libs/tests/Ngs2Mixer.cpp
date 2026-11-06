#include "SceTypes.hpp"
#include "prx/libSceNgs2.native/include/Ngs2Types.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <vector>

extern "C" {
int APS5_VABI sceNgs2SystemQueryBufferSize(const Ngs2SystemOption* option, Ngs2ContextBufferInfo* buffer_info);
int APS5_VABI sceNgs2SystemCreate(const Ngs2SystemOption* option, const Ngs2ContextBufferInfo* buffer_info, Ngs2Handle* handle);
int APS5_VABI sceNgs2SystemDestroy(Ngs2Handle system_handle, Ngs2ContextBufferInfo* buffer_info);
int APS5_VABI sceNgs2SystemRender(Ngs2Handle system_handle, const Ngs2RenderBufferInfo* buffer_info, std::uint32_t num_buffer_info);
int APS5_VABI sceNgs2RackQueryBufferSize(std::uint32_t rack_id, const Ngs2RackOption* option, Ngs2ContextBufferInfo* buffer_info);
int APS5_VABI sceNgs2RackCreate(Ngs2Handle system_handle, std::uint32_t rack_id, const Ngs2RackOption* option, const Ngs2ContextBufferInfo* buffer_info, Ngs2Handle* handle);
int APS5_VABI sceNgs2RackGetVoiceHandle(Ngs2Handle rack_handle, std::uint32_t voice_id, Ngs2Handle* handle);
int APS5_VABI sceNgs2VoiceControl(Ngs2Handle voice_handle, const Ngs2VoiceParamHeader* param_list);
int APS5_VABI sceNgs2VoiceRunCommands(Ngs2Handle voice_handle, const void* commands, std::uint32_t num_commands);
int APS5_VABI sceNgs2VoiceGetStateFlags(Ngs2Handle voice_handle, std::uint32_t* state_flags);
}

namespace {

int failures = 0;

void Require(const bool condition, const char* what, const std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "Ngs2Mixer: %s at %s:%u\n", what, location.file_name(), static_cast<unsigned>(location.line()));
    ++failures;
}

// One parameter block: {u16 size, i16 next = 0, u32 id} followed by the payload.
template <class Payload>
std::vector<std::uint8_t> MakeParam(const std::uint32_t id, const Payload& payload) {
    std::vector<std::uint8_t> block(8 + sizeof(Payload));
    const auto size = static_cast<std::uint16_t>(block.size());
    std::memcpy(block.data(), &size, sizeof(size));
    std::memcpy(block.data() + 4, &id, sizeof(id));
    std::memcpy(block.data() + 8, &payload, sizeof(Payload));
    return block;
}

int Control(const Ngs2Handle voice, const std::vector<std::uint8_t>& block) {
    return sceNgs2VoiceControl(voice, reinterpret_cast<const Ngs2VoiceParamHeader*>(block.data()));
}

int Command(const Ngs2Handle voice, const std::uint32_t type, const std::uint32_t value) {
    const std::array<std::uint32_t, 8> record{type, 0x400, value, 0, 0xCAFEBABE, 0xDEADBEEF, 0, 0};
    return sceNgs2VoiceRunCommands(voice, record.data(), 1);
}

int Volume(const Ngs2Handle voice, const float volume) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &volume, sizeof(bits));
    return Command(voice, 6, bits);
}

Ngs2Handle MakeVoice(const Ngs2Handle system, const std::uint32_t rackId, std::vector<std::vector<std::uint8_t>>& storage) {
    Ngs2ContextBufferInfo info{};
    Require(sceNgs2RackQueryBufferSize(rackId, nullptr, &info) == 0, "rack buffer size query failed");
    storage.emplace_back(info.host_buffer_size);
    info.host_buffer = storage.back().data();
    Ngs2Handle rack = 0;
    Ngs2Handle voice = 0;
    Require(sceNgs2RackCreate(system, rackId, nullptr, &info, &rack) == 0, "rack creation failed");
    Require(sceNgs2RackGetVoiceHandle(rack, 0, &voice) == 0, "voice handle lookup failed");
    return voice;
}

struct PatchPayload {
    std::uint32_t port;
    std::uint32_t destInput;
    std::uint64_t destHandle;
};

struct BlocksPayload {
    std::uint64_t data;
    std::uint32_t flags;
    std::uint32_t numBlocks;
    std::uint64_t blocks;
};

struct Block {
    std::uint64_t dataOffset;
    std::uint64_t dataSize;
    std::uint32_t numRepeats;
    std::uint32_t numSkipSamples;
    std::uint32_t numSamples;
    std::uint32_t reserved;
    std::uint64_t userData;
};

std::int16_t Expected(const std::int16_t sample, const float gain) {
    return static_cast<std::int16_t>(static_cast<float>(sample) / 32768.0f * gain * 32767.0f);
}

} // namespace

int main() {
    Ngs2ContextBufferInfo systemInfo{};
    Require(sceNgs2SystemQueryBufferSize(nullptr, &systemInfo) == 0, "system buffer size query failed");
    std::vector<std::uint8_t> systemBuffer(systemInfo.host_buffer_size);
    systemInfo.host_buffer = systemBuffer.data();
    Ngs2Handle system = 0;
    Require(sceNgs2SystemCreate(nullptr, &systemInfo, &system) == 0, "system creation failed");

    // ANIMAL WELL's chain: sampler -> submixer -> mastering -> render buffer.
    std::vector<std::vector<std::uint8_t>> rackStorage;
    const Ngs2Handle sampler = MakeVoice(system, 0x1000, rackStorage);
    const Ngs2Handle submixer = MakeVoice(system, 0x2000, rackStorage);
    const Ngs2Handle mastering = MakeVoice(system, 0x3000, rackStorage);

    constexpr std::uint32_t Frames = 300;
    std::vector<std::int16_t> pcm(Frames);
    for (std::uint32_t frame = 0; frame < Frames; ++frame) pcm[frame] = static_cast<std::int16_t>(static_cast<int>(frame) * 100 - 10000);
    const Block block{0, Frames * sizeof(std::int16_t), 0, 0, Frames, 0, 0};

    Require(Control(sampler, MakeParam(0x10000000, std::array<std::uint32_t, 8>{0x12, 1, 48000})) == 0, "sampler setup failed");
    Require(Control(sampler, MakeParam(0x10000001, BlocksPayload{reinterpret_cast<std::uint64_t>(pcm.data()), 4, 1, reinterpret_cast<std::uint64_t>(&block)})) == 0, "waveform blocks failed");
    Require(Control(sampler, MakeParam(5, PatchPayload{0, 0, submixer})) == 0, "sampler patch failed");
    Require(Control(submixer, MakeParam(0x20000000, std::array<std::uint32_t, 2>{2, 0})) == 0, "submixer setup failed");
    Require(Control(submixer, MakeParam(5, PatchPayload{0, 0, mastering})) == 0, "submixer patch failed");
    Require(Control(mastering, MakeParam(0x30000000, std::array<std::uint32_t, 2>{2, 0})) == 0, "mastering setup failed");
    Require(Control(mastering, MakeParam(0x30000005, std::array<std::uint32_t, 2>{0, 0})) == 0, "mastering output failed");
    Require(Control(sampler, MakeParam(0x20010001, std::array<std::uint32_t, 2>{0, 0})) < 0, "a reverb parameter applied to a sampler");
    Require(Control(sampler, MakeParam(0x10000005, std::array<std::uint32_t, 2>{0, 0})) < 0, "a zero pitch was accepted");
    Require(Control(sampler, MakeParam(0x10000005, std::array<std::uint32_t, 2>{0x3f800000, 0})) == 0, "the intro's unit pitch was rejected");
    Require(Control(sampler, MakeParam(0x10000009, std::array<std::uint32_t, 2>{0, 0})) < 0, "an unmodelled sampler parameter reported success");
    for (const Ngs2Handle voice : {mastering, submixer, sampler}) Require(Command(voice, 2, 1) == 0, "play event failed");
    Require(Volume(submixer, 0.5f) == 0, "volume command failed");
    Require(Volume(submixer, -1.0f) < 0, "a negative volume was accepted");

    constexpr std::uint32_t Grain = 256;
    std::vector<std::int16_t> stereo(Grain * 2, 0x55);
    std::vector<std::int16_t> mono(Grain, 0x55);
    const std::array<Ngs2RenderBufferInfo, 2> buffers{{
        {stereo.data(), stereo.size() * sizeof(std::int16_t), 0x12, 2},
        {mono.data(), mono.size() * sizeof(std::int16_t), 0x12, 1},
    }};
    Require(sceNgs2SystemRender(system, buffers.data(), 2) == 0, "first render failed");
    bool firstGrain = true;
    for (std::uint32_t frame = 0; frame < Grain; ++frame) {
        const std::int16_t expected = Expected(pcm[frame], 0.5f);
        firstGrain &= std::abs(stereo[frame * 2] - expected) <= 1 && std::abs(stereo[frame * 2 + 1] - expected) <= 1;
    }
    Require(firstGrain, "the stereo buffer does not carry the sampler at half volume");
    bool monoSilent = true;
    for (const std::int16_t sample : mono) monoSilent &= sample == 0;
    Require(monoSilent, "an unrouted render buffer is not silent");

    Require(sceNgs2SystemRender(system, buffers.data(), 2) == 0, "second render failed");
    bool tail = true;
    for (std::uint32_t frame = 0; frame < Grain; ++frame) {
        const std::int16_t expected = frame + Grain < Frames ? Expected(pcm[frame + Grain], 0.5f) : 0;
        tail &= std::abs(stereo[frame * 2] - expected) <= 1;
    }
    Require(tail, "the block tail is wrong or the voice did not end with its block");
    std::uint32_t flags = 0;
    Require(sceNgs2VoiceGetStateFlags(sampler, &flags) == 0 && (flags & 0x2) == 0, "a finished sampler still reports playing");

    // A looping block keeps the voice alive; stop silences it.
    const Block loop{0, Frames * sizeof(std::int16_t), 0xFFFFFFFF, 0, Frames, 0, 0};
    Require(Control(sampler, MakeParam(0x10000001, BlocksPayload{reinterpret_cast<std::uint64_t>(pcm.data()), 4, 1, reinterpret_cast<std::uint64_t>(&loop)})) == 0, "looping blocks failed");
    Require(Command(sampler, 2, 1) == 0, "replay failed");
    for (int grain = 0; grain < 4; ++grain) Require(sceNgs2SystemRender(system, buffers.data(), 2) == 0, "looping render failed");
    Require(sceNgs2VoiceGetStateFlags(sampler, &flags) == 0 && (flags & 0x2) != 0, "a looping sampler stopped");
    // The fourth grain starts at frame 768 of the endless loop: 768 mod 300 = 168.
    Require(std::abs(stereo[0] - Expected(pcm[(3 * Grain) % Frames], 0.5f)) <= 1, "the loop does not wrap to the block start");
    Require(Command(sampler, 2, 2) == 0, "stop failed");
    Require(sceNgs2SystemRender(system, buffers.data(), 2) == 0, "render after stop failed");
    bool stopped = true;
    for (const std::int16_t sample : stereo) stopped &= sample == 0;
    Require(stopped, "a stopped sampler still produces sound");

    // A 24 kHz source at 48 kHz lands halfway between frames: the ramp interpolates linearly.
    Require(Control(sampler, MakeParam(0x10000000, std::array<std::uint32_t, 8>{0x12, 1, 24000})) == 0, "half-rate setup failed");
    Require(Control(sampler, MakeParam(0x10000001, BlocksPayload{reinterpret_cast<std::uint64_t>(pcm.data()), 4, 1, reinterpret_cast<std::uint64_t>(&block)})) == 0, "half-rate blocks failed");
    Require(Command(sampler, 2, 1) == 0, "half-rate play failed");
    Require(sceNgs2SystemRender(system, buffers.data(), 2) == 0, "half-rate render failed");
    bool interpolated = true;
    for (std::uint32_t frame = 0; frame + 1 < Grain; frame += 2) {
        const auto midpoint = static_cast<std::int16_t>((pcm[frame / 2] + pcm[frame / 2 + 1]) / 2);
        interpolated &= std::abs(stereo[frame * 2] - Expected(pcm[frame / 2], 0.5f)) <= 1;
        interpolated &= std::abs(stereo[(frame + 1) * 2] - Expected(midpoint, 0.5f)) <= 1;
    }
    Require(interpolated, "a half-rate source is not linearly interpolated");
    Require(Command(sampler, 2, 2) == 0, "half-rate stop failed");

    // A pitched step crosses a short block with overshoot, then skips one frame in the next.
    Require(Control(sampler, MakeParam(0x10000000, std::array<std::uint32_t, 8>{0x12, 1, 48000})) == 0, "pitched setup failed");
    Require(Control(sampler, MakeParam(0x10000005, std::array<float, 2>{1.5f, 0.0f})) == 0, "pitched rate failed");
    const std::array<Block, 2> joined{{
        {0, 4 * sizeof(std::int16_t), 0, 0, 4, 0, 0},
        {4 * sizeof(std::int16_t), (Frames - 4) * sizeof(std::int16_t), 0, 1, Frames - 4, 0, 0},
    }};
    Require(Control(sampler, MakeParam(0x10000001, BlocksPayload{reinterpret_cast<std::uint64_t>(pcm.data()), 4, 2, reinterpret_cast<std::uint64_t>(joined.data())})) == 0, "joined blocks failed");
    Require(Command(sampler, 2, 1) == 0, "joined play failed");
    Require(sceNgs2SystemRender(system, buffers.data(), 2) == 0, "joined render failed");
    bool phasePreserved = true;
    for (std::uint32_t frame = 3; frame < 150; ++frame) {
        const double position = 5.5 + (frame - 3) * 1.5;
        const auto sourceFrame = static_cast<std::uint32_t>(position);
        const auto sample = static_cast<std::int16_t>(pcm[sourceFrame] + (pcm[sourceFrame + 1] - pcm[sourceFrame]) * (position - sourceFrame));
        phasePreserved &= std::abs(stereo[frame * 2] - Expected(sample, 0.5f)) <= 1;
    }
    Require(phasePreserved, "a pitched block join lost its overshoot or fractional phase");
    Require(Command(sampler, 2, 2) == 0, "joined stop failed");
    Require(Control(sampler, MakeParam(0x10000005, std::array<float, 2>{1.0f, 0.0f})) == 0, "unit pitch restore failed");

    // Reverb send: an impulse through ANIMAL WELL's own I3DL2 block (traced at boot) yields a
    // tail that starts after the shortest comb delay and decays.
    const Ngs2Handle reverb = MakeVoice(system, 0x2001, rackStorage);
    Require(Control(sampler, MakeParam(5, PatchPayload{0, 0, reverb})) == 0, "sampler to reverb patch failed");
    Require(Control(reverb, MakeParam(5, PatchPayload{0, 0, mastering})) == 0, "reverb patch failed");
    Require(Control(reverb, MakeParam(0x20010000, std::array<std::uint32_t, 4>{2, 2, 0, 0})) == 0, "reverb setup failed");
    const std::array<std::uint32_t, 22> i3dl2{0x3f19999a, 0x3ecccccd, 0xfffffc18, 0, 0, 0x403a3d71, 0x3fa66666, 0xfffffda6, 0x3c75c28f,
                                              0xfffffed2, 0x3cb43958, 0x42c80000, 0x42c80000, 0x459c4000, 8};
    Require(Control(reverb, MakeParam(0x20010001, i3dl2)) == 0, "the title's I3DL2 block was rejected");
    Require(Command(reverb, 2, 1) == 0, "reverb play failed");
    std::vector<std::int16_t> impulse(Frames, 0);
    impulse[0] = 32767;
    const Block once{0, Frames * sizeof(std::int16_t), 0, 0, Frames, 0, 0};
    Require(Control(sampler, MakeParam(0x10000001, BlocksPayload{reinterpret_cast<std::uint64_t>(impulse.data()), 4, 1, reinterpret_cast<std::uint64_t>(&once)})) == 0, "impulse blocks failed");
    Require(Command(sampler, 2, 1) == 0, "impulse play failed");
    std::vector<double> energy;
    for (int grain = 0; grain < 200; ++grain) {
        Require(sceNgs2SystemRender(system, buffers.data(), 2) == 0, "reverb render failed");
        double sum = 0.0;
        for (const std::int16_t sample : stereo) sum += static_cast<double>(sample) * sample;
        energy.push_back(sum);
    }
    Require(energy[2] == 0.0, "the reverb answered before its shortest comb delay");
    double early = 0.0;
    double late = 0.0;
    for (int grain = 5; grain < 40; ++grain) early += energy[grain];
    for (int grain = 160; grain < 195; ++grain) late += energy[grain];
    Require(early > 0.0, "the reverb produced no tail");
    Require(late < early * 0.05, "the reverb tail does not decay");

    // A sampler rack created without an option holds 256 voices (one per sound, as the title expects).
    {
        Ngs2ContextBufferInfo info{};
        Require(sceNgs2RackQueryBufferSize(0x1000, nullptr, &info) == 0, "sampler buffer size query failed");
        rackStorage.emplace_back(info.host_buffer_size);
        info.host_buffer = rackStorage.back().data();
        Ngs2Handle rack = 0;
        Ngs2Handle voice = 0;
        Require(sceNgs2RackCreate(system, 0x1000, nullptr, &info, &rack) == 0, "default sampler rack failed");
        Require(sceNgs2RackGetVoiceHandle(rack, 255, &voice) == 0 && voice != 0, "the default sampler rack lacks voice 255");
        Require(sceNgs2RackGetVoiceHandle(rack, 256, &voice) != 0, "the default sampler rack has more than 256 voices");
    }

    Require(sceNgs2SystemDestroy(system, nullptr) == 0, "system destroy failed");
    if (failures != 0) return EXIT_FAILURE;
    std::puts("Ngs2Mixer: ok");
    return EXIT_SUCCESS;
}
