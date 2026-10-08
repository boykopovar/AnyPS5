#include "Ngs2Test.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <numbers>
#include <stdexcept>

static Ngs2ReverbI3dl2Param Parameters() {
    return {1.0f, 0.0f, 0, 0, 0, 0.3f, 1.0f, 0, 0.01f, 0, 0.02f, 100.0f, 100.0f, 5000.0f, {}};
}

template <typename TCall>
static void Reject(TCall call) {
    bool rejected = false;
    try { call(); } catch (const std::exception&) { rejected = true; }
    Require(rejected);
}

struct Graph {
    std::uint32_t grain, inputs, outputs;
    uintptr_t system, rack, reverb, sampler;
    std::vector<std::int16_t> pcm;

    Graph(std::uint32_t frames, std::uint32_t in, std::uint32_t out, const Ngs2ReverbI3dl2Param& param)
        : grain(frames), inputs(in), outputs(out), system(CreateSystem()) {
        Require(sceNgs2SystemSetGrainSamples(system, grain) == SCE_NGS2_OK);
        const auto master = Mastering(system, outputs);
        rack = CreateRack(system, SCE_NGS2_RACK_ID_REVERB);
        reverb = Voice(rack);
        sampler = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER));
        Control(reverb, SCE_NGS2_REVERB_VOICE_PARAM_SETUP, Ngs2ReverbVoiceSetupParam{{}, inputs, outputs, 0, 0});
        Configure(param);
        Patch(reverb, master);
        Event(reverb, SCE_NGS2_VOICE_EVENT_PLAY);
    }

    ~Graph() {
        Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
        usedBuffers = 0;
    }

    void Configure(const Ngs2ReverbI3dl2Param& param) {
        Control(reverb, SCE_NGS2_REVERB_VOICE_PARAM_I3DL2, Ngs2ReverbVoiceI3dl2Param{{}, param});
    }

    void Source(std::vector<std::int16_t> data) {
        pcm = std::move(data);
        Control(sampler, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP,
                Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, inputs, 48000, 0, 0, 0}});
        const Ngs2WaveformBlock block{0, pcm.size() * sizeof(std::int16_t), 0, 0, static_cast<std::uint32_t>(pcm.size() / inputs), 0, 0};
        Control(sampler, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, pcm.data(), 0, 1, &block});
        Patch(sampler, reverb);
        Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);
    }

    std::vector<float> Render(std::uint32_t frames) {
        Require(frames % grain == 0);
        std::vector<float> result(static_cast<std::size_t>(frames) * outputs, -1.0f);
        for (std::uint32_t frame = 0; frame < frames; frame += grain) {
            const Ngs2RenderBufferInfo buffer{result.data() + frame * outputs, grain * outputs * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, outputs};
            Require(sceNgs2SystemRender(system, &buffer, 1) == SCE_NGS2_OK);
        }
        for (float sample : result) Require(std::isfinite(sample));
        return result;
    }
};

static double Energy(const std::vector<float>& samples, std::size_t start, std::size_t end) {
    double energy = 0.0;
    for (auto i = start; i < end; ++i) energy += static_cast<double>(samples[i]) * samples[i];
    return energy;
}

static std::vector<float> Impulse(const Ngs2ReverbI3dl2Param& param, std::uint32_t grain = 128, std::uint32_t frames = 49152, std::uint32_t channels = 1) {
    Graph graph(grain, 1, channels, param);
    graph.Source({16384});
    return graph.Render(frames);
}

static int allocations = 0;

static int APS5_VABI Allocate(Ngs2ContextBufferInfo* info) {
    info->host_buffer = std::malloc(info->host_buffer_size);
    Require(info->host_buffer != nullptr);
    ++allocations;
    return 0;
}

static int APS5_VABI Release(Ngs2ContextBufferInfo* info) {
    std::free(info->host_buffer);
    --allocations;
    return 0;
}

static void TestLifecycle() {
    const Ngs2BufferAllocator allocator{Allocate, Release, 0};
    uintptr_t system = 0, rack = 0;
    Require(sceNgs2SystemCreateWithAllocator(nullptr, &allocator, &system) == 0);
    Ngs2ReverbRackOption option{};
    option.rack_option.size = sizeof(option);
    option.rack_option.max_grain_samples = 512;
    option.rack_option.max_voices = 2;
    option.rack_option.max_matrices = 1;
    option.rack_option.max_ports = 1;
    option.max_channels = 8;
    option.reverb_size = 1;
    Require(sceNgs2RackCreateWithAllocator(system, SCE_NGS2_RACK_ID_REVERB, &option.rack_option, &allocator, &rack) == 0);
    Require(allocations == 2);
    Ngs2RackInfo info{};
    Require(sceNgs2RackGetInfo(rack, &info, sizeof(info)) == 0);
    Require(info.rack_id == SCE_NGS2_RACK_ID_REVERB && info.max_voices == 2 && info.owner_system_handle == system);
    uintptr_t first = Voice(rack), second = 0;
    Require(sceNgs2RackGetVoiceHandle(rack, 1, &second) == 0 && first != second);
    Ngs2VoiceState state{99, 99};
    Require(sceNgs2VoiceGetState(first, &state, sizeof(state)) == 0 && state.state_flags == 0 && state.error_code == 0);
    Require(sceNgs2VoiceGetState(first, &state, sizeof(state) - 1) == SCE_NGS2_ERROR_INVALID_OUT_SIZE);
    Reject([&] { Control(first, SCE_NGS2_REVERB_VOICE_PARAM_I3DL2, Ngs2ReverbVoiceI3dl2Param{{}, Parameters()}); });
    Control(first, SCE_NGS2_REVERB_VOICE_PARAM_SETUP, Ngs2ReverbVoiceSetupParam{{}, 2, 8, 0, 0});
    Event(first, SCE_NGS2_VOICE_EVENT_PLAY);
    Require(sceNgs2VoiceGetState(first, &state, sizeof(state)) == 0 && state.state_flags != 0);
    Require(sceNgs2VoiceGetState(second, &state, sizeof(state)) == 0 && state.state_flags == 0);
    Reject([&] { Control(first, SCE_NGS2_REVERB_VOICE_PARAM_SETUP, Ngs2ReverbVoiceSetupParam{{}, 0, 8, 0, 0}); });
    Reject([&] { Control(first, SCE_NGS2_REVERB_VOICE_PARAM_SETUP, Ngs2ReverbVoiceSetupParam{{}, 2, 9, 0, 0}); });
    Reject([&] { Control(first, SCE_NGS2_REVERB_VOICE_PARAM_SETUP, Ngs2ReverbVoiceSetupParam{{}, 2, 8, 1, 0}); });
    Require(Flags(first) != 0);
    for (int field = 0; field < 4; ++field) {
        auto invalid = option;
        if (field == 0) invalid.rack_option.size--;
        if (field == 1) invalid.max_channels = 0;
        if (field == 2) invalid.reverb_size = 2;
        if (field == 3) invalid.rack_option.flags = 1;
        uintptr_t untouched = 123;
        Reject([&] { sceNgs2RackCreateWithAllocator(system, SCE_NGS2_RACK_ID_REVERB, &invalid.rack_option, &allocator, &untouched); });
        Require(untouched == 123 && allocations == 2);
    }
    Require(sceNgs2SystemDestroy(system, nullptr) == 0 && allocations == 0);
    Require(sceNgs2RackDestroy(rack, nullptr) == SCE_NGS2_ERROR_INVALID_RACK_HANDLE);
}

static void TestDryRoutingAndErrors() {
    auto param = Parameters();
    param.wet = 0.0f;
    param.dry = 0.25f;
    Graph graph(64, 2, 8, param);
    graph.Source({16384, -8192, 8192, -16384});
    const auto out = graph.Render(64);
    Require(out[0] == 0.125f && out[1] == -0.0625f && out[8] == 0.0625f && out[9] == -0.125f);
    for (std::uint32_t i = 0; i < 64; ++i)
        for (std::uint32_t channel = 2; channel < 8; ++channel) Require(out[i * 8 + channel] == 0.0f);
    Require(Flags(graph.sampler) == 0);
    for (int field = 0; field < 14; ++field) {
        auto invalid = param;
        switch (field) {
        case 0: invalid.wet = std::numeric_limits<float>::quiet_NaN(); break;
        case 1: invalid.dry = -1.0f; break;
        case 2: invalid.room = 1; break;
        case 3: invalid.room_hf = -10001; break;
        case 4: invalid.reflection_pattern = 1; break;
        case 5: invalid.decay_time = 0.0f; break;
        case 6: invalid.decay_hf_ratio = 2.1f; break;
        case 7: invalid.reflections = 1001; break;
        case 8: invalid.reflections_delay = 0.31f; break;
        case 9: invalid.reverb = 2001; break;
        case 10: invalid.reverb_delay = -0.1f; break;
        case 11: invalid.diffusion = 101.0f; break;
        case 12: invalid.density = std::numeric_limits<float>::infinity(); break;
        case 13: invalid.hf_reference = 0.0f; break;
        }
        Reject([&] { graph.Configure(invalid); });
    }
    Reject([&] { sceNgs2SystemSetSampleRate(graph.system, 44100); });
    Require(sceNgs2SystemSetSampleRate(graph.system, 48000) == 0);
    graph.Source({16384, -8192});
    const auto unchanged = graph.Render(64);
    Require(unchanged[0] == out[0] && unchanged[1] == out[1]);
}

static void TestRoundedBoundaries() {
    struct Boundary {
        float Ngs2ReverbI3dl2Param::*field;
        float minimum;
        float maximum;
    };
    const Boundary boundaries[]{
        {&Ngs2ReverbI3dl2Param::wet, 0.0f, 1.0f},
        {&Ngs2ReverbI3dl2Param::dry, 0.0f, 1.0f},
        {&Ngs2ReverbI3dl2Param::decay_time, 0.1f, 20.0f},
        {&Ngs2ReverbI3dl2Param::decay_hf_ratio, 0.1f, 2.0f},
        {&Ngs2ReverbI3dl2Param::reflections_delay, 0.0f, 0.3f},
        {&Ngs2ReverbI3dl2Param::reverb_delay, 0.0f, 0.1f},
        {&Ngs2ReverbI3dl2Param::diffusion, 0.0f, 100.0f},
        {&Ngs2ReverbI3dl2Param::density, 0.0f, 100.0f},
        {&Ngs2ReverbI3dl2Param::hf_reference, 20.0f, 20000.0f},
    };
    for (const auto& boundary : boundaries) {
        for (bool upper : {false, true}) {
            auto param = Parameters();
            param.*boundary.field = upper ? boundary.maximum : boundary.minimum;
            const auto exact = Impulse(param, 64, 4096);
            const auto direction = upper ? std::numeric_limits<float>::infinity() : -std::numeric_limits<float>::infinity();
            param.*boundary.field = std::nextafter(param.*boundary.field, direction);
            Require(Impulse(param, 64, 4096) == exact);
            param.*boundary.field = std::nextafter(param.*boundary.field, direction);
            Graph graph(64, 1, 1, Parameters());
            Reject([&] { graph.Configure(param); });
        }
    }
}

static void TestDelaysAndMillibels() {
    auto param = Parameters();
    param.reflections_delay = 16.0f / 48000;
    param.reverb_delay = 64.0f / 48000;
    param.reverb = -10000;
    auto out = Impulse(param, 64, 128);
    for (std::size_t i = 0; i < 80; ++i) Require(out[i] == (i == 16 ? 0.5f : 0.0f));
    Require(std::abs(out[80] - 0.000005f) < 1e-10f);
    param.room = -2000;
    const auto quieter = Impulse(param, 64, 128);
    for (std::size_t i = 0; i < out.size(); ++i) Require(std::abs(quieter[i] - out[i] * 0.1f) < 1e-7f);
}

static void TestDecayAndSurround() {
    auto shortParam = Parameters();
    shortParam.decay_time = 0.15f;
    auto longParam = shortParam;
    longParam.decay_time = 0.8f;
    const auto shortTail = Impulse(shortParam);
    const auto longTail = Impulse(longParam);
    Require(Energy(shortTail, 3000, 6000) > Energy(shortTail, 12000, 15000) * 10.0);
    Require(Energy(longTail, 12000, 15000) > Energy(shortTail, 12000, 15000) * 10.0);
    Require(Energy(shortTail, 48000, shortTail.size()) == 0.0);
    const auto surround = Impulse(longParam, 128, 8192, 8);
    std::array<double, 8> energy{};
    for (std::size_t i = 0; i < surround.size(); ++i) energy[i % 8] += surround[i] * surround[i];
    for (std::size_t i = 0; i < energy.size(); ++i) Require(i == 3 ? energy[i] == 0.0 : energy[i] > 0.0);
    auto sparse = longParam;
    sparse.diffusion = 0;
    Require(Impulse(sparse, 128, 8192) != Impulse(longParam, 128, 8192));
    sparse = longParam;
    sparse.density = 0;
    Require(Impulse(sparse, 128, 8192) != Impulse(longParam, 128, 8192));
}

static double Magnitude(const std::vector<float>& samples, double frequency) {
    double real = 0.0, imaginary = 0.0;
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const double phase = 2.0 * std::numbers::pi * frequency * i / 48000;
        real += samples[i] * std::cos(phase);
        imaginary += samples[i] * std::sin(phase);
    }
    return std::hypot(real, imaginary);
}

static void TestHighFrequencyAttenuation() {
    auto param = Parameters();
    param.reflections_delay = 0;
    param.reverb_delay = 0.1f;
    param.room_hf = -6000;
    const auto impulse = Impulse(param, 128, 2048);
    Require(Magnitude(impulse, 16000) < Magnitude(impulse, 100) * 0.25);
    param.room_hf = 0;
    const auto flat = Impulse(param, 128, 2048);
    Require(std::abs(Magnitude(flat, 16000) - Magnitude(flat, 100)) < 1e-6);

    auto brightness = [](const auto& signal) {
        double difference = 0.0;
        for (std::size_t i = 6001; i < 12000; ++i) {
            const double delta = signal[i] - signal[i - 1];
            difference += delta * delta;
        }
        return difference / Energy(signal, 6000, 12000);
    };
    param = Parameters();
    const auto undamped = Impulse(param, 128, 16384);
    param.decay_hf_ratio = 0.25f;
    const auto damped = Impulse(param, 128, 16384);
    Require(brightness(damped) < brightness(undamped) * 0.5);
}

static void TestStereoDownmixAndBounds() {
    auto param = Parameters();
    param.wet = 0;
    param.dry = 1;
    {
        Graph graph(64, 2, 1, param);
        graph.Source({16384, -8192});
        Require(graph.Render(64)[0] == 0.125f);
    }
    param = Parameters();
    param.decay_time = 20;
    param.decay_hf_ratio = 2;
    param.hf_reference = 20000;
    param.reflections = 1000;
    param.reverb = 2000;
    param.density = 0;
    const auto out = Impulse(param);
    for (float sample : out) Require(std::abs(sample) < 32.0f);
}

static void TestGrainsAndLifecycle() {
    auto param = Parameters();
    const auto baseline = Impulse(param, 64, 8192);
    Require(baseline == Impulse(param, 128, 8192));
    Require(baseline == Impulse(param, 256, 8192));
    Graph graph(64, 1, 1, param);
    graph.Source({16384});
    const auto start = graph.Render(4096);
    Require(std::equal(start.begin(), start.end(), baseline.begin()));
    Event(graph.reverb, SCE_NGS2_VOICE_EVENT_PAUSE);
    for (float sample : graph.Render(512)) Require(sample == 0.0f);
    Event(graph.reverb, SCE_NGS2_VOICE_EVENT_RESUME);
    const auto resumed = graph.Render(512);
    Require(std::equal(resumed.begin(), resumed.end(), baseline.begin() + 4096));
    param.wet = 0.5f;
    graph.Configure(param);
    const auto changed = graph.Render(512);
    for (std::size_t i = 0; i < changed.size(); ++i) Require(changed[i] == baseline[4608 + i] * 0.5f);
    Event(graph.reverb, SCE_NGS2_VOICE_EVENT_KILL);
    Event(graph.reverb, SCE_NGS2_VOICE_EVENT_PLAY);
    for (float sample : graph.Render(2048)) Require(sample == 0.0f);
    Control(graph.reverb, SCE_NGS2_REVERB_VOICE_PARAM_SETUP, Ngs2ReverbVoiceSetupParam{{}, 1, 1, 0, 0});
    Patch(graph.reverb, 0);
    Event(graph.reverb, SCE_NGS2_VOICE_EVENT_PLAY);
    Reject([&] { graph.Render(64); });
    graph.Configure(param);
    for (float sample : graph.Render(64)) Require(sample == 0.0f);
}

int main() {
    TestLifecycle();
    TestDryRoutingAndErrors();
    TestRoundedBoundaries();
    TestDelaysAndMillibels();
    TestDecayAndSurround();
    TestHighFrequencyAttenuation();
    TestStereoDownmixAndBounds();
    TestGrainsAndLifecycle();
}
