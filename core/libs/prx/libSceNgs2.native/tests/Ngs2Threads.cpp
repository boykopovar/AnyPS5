#include "Ngs2JobTest.hpp"

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <latch>
#include <mutex>
#include <stdexcept>

struct ThreadControl {
    std::mutex mutex;
    std::condition_variable condition;
    bool entered = false;
    bool release = false;
    bool park = true;
    bool failFirst = false;
    bool lifecycle = false;
    bool inlineHandoff = false;
    bool destroyOnSetup = false;
    bool destroyOnCleanup = false;
    bool systemCleanup = false;
    bool infoDuringProcess = false;
    bool updateDuringProcess = false;
    std::atomic<unsigned> infoProcesses = 0;
    std::uintptr_t system = 0;
    std::uintptr_t peer = 0;
    unsigned setups = 0;
    std::uintptr_t voice = 0;
    std::atomic<bool> active = false;
    std::atomic<unsigned> calls = 0;
    std::atomic<unsigned> cleanups = 0;
    std::array<float, 4> before{};
};

static int ThreadValue(std::uintptr_t voice, float value) {
    const Ngs2CustomVoiceUserFx2Param param{{sizeof(param), 0, SCE_NGS2_CUSTOM_VOICE_PARAM_USER_FX2}, &value, sizeof(value)};
    return sceNgs2VoiceControl(voice, &param.header);
}

static void LifecycleHandoff(ThreadControl& state, bool cleanup) {
    const auto helper = [&] {
        Require(ThreadValue(state.peer, cleanup ? 3.0f : 2.0f) == SCE_NGS2_OK);
        std::uint32_t flags = 99;
        Require(sceNgs2VoiceGetStateFlags(state.peer, &flags) == SCE_NGS2_OK && flags == 0);
        if (cleanup) {
            flags = 99;
            Require(sceNgs2VoiceGetStateFlags(state.voice, &flags) == SCE_NGS2_ERROR_INVALID_VOICE_HANDLE && flags == 99);
        }
        std::array<float, 128> audio{};
        const Ngs2RenderBufferInfo output{audio.data(), sizeof(audio), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 2};
        if (state.systemCleanup) {
            Require(cleanup);
            Require(sceNgs2SystemRender(state.system, &output, 1) == SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE);
            Require(sceNgs2SystemDestroy(state.system, nullptr) == SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE);
            return;
        }
        Require(sceNgs2SystemRender(state.system, &output, 1) == SCE_NGS2_OK);
        for (float value : audio) Require(value == 0.0f);
        const auto nested = CreateRack(state.system, SCE_NGS2_RACK_ID_SUBMIXER);
        Require(sceNgs2RackDestroy(nested, nullptr) == SCE_NGS2_OK);
        if ((!cleanup && state.destroyOnSetup) || (cleanup && state.destroyOnCleanup))
            Require(sceNgs2SystemDestroy(state.system, nullptr) == SCE_NGS2_OK);
    };
    if (state.inlineHandoff) helper();
    else {
        std::thread thread(helper);
        thread.join();
    }
}

static int APS5_VABI ThreadSetup(Ngs2UserFx2SetupContext* context) {
    auto& state = *reinterpret_cast<ThreadControl*>(context->user_data);
    if (state.lifecycle) LifecycleHandoff(state, false);
    ++state.setups;
    *static_cast<float*>(context->param) = 1.0f;
    return SCE_NGS2_OK;
}

static int APS5_VABI ThreadCleanup(Ngs2UserFx2CleanupContext* context) {
    auto& state = *reinterpret_cast<ThreadControl*>(context->user_data);
    Require(!state.active.load());
    std::uint32_t flags = 99;
    Require(sceNgs2VoiceGetStateFlags(state.voice, &flags) == SCE_NGS2_ERROR_INVALID_VOICE_HANDLE && flags == 99);
    if (state.lifecycle) LifecycleHandoff(state, true);
    ++state.cleanups;
    return SCE_NGS2_OK;
}

static int APS5_VABI ThreadHandler(Ngs2UserFx2ControlContext* context) {
    auto& state = *reinterpret_cast<ThreadControl*>(context->user_data);
    Require(!state.active.exchange(true));
    const auto index = state.calls.fetch_add(1);
    Require(index < state.before.size());
    state.before[index] = *static_cast<float*>(context->param);
    const float value = *static_cast<const float*>(context->data);
    if (state.park && value == 1.0f) {
        std::unique_lock lock(state.mutex);
        state.entered = true;
        state.condition.notify_all();
        state.condition.wait(lock, [&] { return state.release; });
    }
    *static_cast<float*>(context->param) = value * 2.0f;
    state.active = false;
    return state.failFirst && value == 1.0f ? -55 : SCE_NGS2_OK;
}

static int APS5_VABI ThreadProcess(Ngs2UserFx2ProcessContext* context) {
    const float value = *static_cast<const float*>(context->param) * 0.125f;
    for (unsigned channel = 0; channel < context->num_output_channels; ++channel)
        std::fill_n(context->channel_data[channel], context->num_grain_samples, value);
    return SCE_NGS2_OK;
}

static int APS5_VABI InfoProcess(Ngs2UserFx2ProcessContext* context) {
    auto& state = *reinterpret_cast<ThreadControl*>(context->user_data);
    const auto renders = state.infoProcesses.fetch_add(1);
    std::thread helper([&] {
        std::uintptr_t owner = 0;
        std::uint32_t index = 99;
        Require(sceNgs2VoiceGetOwner(state.voice, &owner, &index) == SCE_NGS2_OK && index == 0);
        Ngs2RackInfo info{};
        Require(sceNgs2RackGetInfo(owner, &info, sizeof(info)) == SCE_NGS2_OK);
        Require(info.render_count == renders && info.active_voice_count == 1 && info.active_channel_work_count == 2);
        if (state.updateDuringProcess && renders == 0) {
            Require(ThreadValue(state.voice, 1.5f) == SCE_NGS2_OK);
            Control(state.voice, SCE_NGS2_VOICE_PARAM_PORT_VOLUME, Ngs2VoicePortVolumeParam{{}, 0, 0.25f});
        }
    });
    helper.join();
    return ThreadProcess(context);
}

static std::uintptr_t ThreadVoice(std::uintptr_t system, ThreadControl& state, std::uintptr_t& rack) {
    const Ngs2CustomUserFx2ModuleOption module{{sizeof(module)}, ThreadSetup, ThreadCleanup, ThreadHandler, state.infoDuringProcess ? InfoProcess : ThreadProcess,
        0, sizeof(float), 0, reinterpret_cast<std::uintptr_t>(&state)};
    Ngs2CustomSubmixerRackOption option{};
    auto& custom = option.custom_rack_option;
    auto& common = custom.rack_option;
    common.size = sizeof(option);
    common.max_grain_samples = 512;
    common.max_voices = 1;
    common.max_matrices = 1;
    common.max_ports = 1;
    custom.state_size = 16;
    custom.num_buffers = 1;
    custom.num_modules = 1;
    custom.module[0].module_id = SCE_NGS2_CUSTOM_MODULE_ID_USER_FX2;
    custom.module[0].option = &module.custom_module_option;
    option.max_channels = 2;
    option.max_inputs = 1;
    Ngs2ContextBufferInfo query{};
    Require(sceNgs2RackQueryBufferSize(SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER, &common, &query) == SCE_NGS2_OK);
    const auto buffer = Buffer(query);
    const int result = TestRackCreate(system, SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER, &common, &buffer, &rack);
    if (state.destroyOnSetup) {
        Require(result == SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE && rack == 0);
        return 0;
    }
    Require(result == SCE_NGS2_OK);
    const auto voice = Voice(rack);
    state.voice = voice;
    Control(voice, SCE_NGS2_CUSTOM_SUBMIXER_VOICE_PARAM_SETUP, Ngs2CustomSubmixerVoiceSetupParam{{}, 2, 2, 0});
    return voice;
}

static void WaitEntered(ThreadControl& state) {
    std::unique_lock lock(state.mutex);
    state.condition.wait(lock, [&] { return state.entered; });
}

static void ReleaseControl(ThreadControl& state) {
    std::lock_guard lock(state.mutex);
    state.release = true;
    state.condition.notify_all();
}

static std::uintptr_t ThreadSystem(const Ngs2JobSchedulerOption* scheduler = nullptr) {
    Ngs2SystemOption option{};
    option.size = sizeof(option);
    option.max_grain_samples = 512;
    option.num_grain_samples = 64;
    option.sample_rate = 48000;
    option.max_voice_channels = 8;
    option.job_scheduler_options[0] = reinterpret_cast<std::uintptr_t>(scheduler);
    Ngs2ContextBufferInfo query{};
    Require(sceNgs2SystemQueryBufferSize(&option, &query) == SCE_NGS2_OK);
    const auto buffer = Buffer(query);
    std::uintptr_t system = 0;
    Require(sceNgs2SystemCreate(&option, &buffer, &system) == SCE_NGS2_OK);
    return system;
}

static void TestSerialization(bool failure) {
    const auto firstBuffer = usedBuffers;
    const auto system = ThreadSystem();
    ThreadControl state, peerState;
    peerState.park = false;
    std::uintptr_t rack = 0, peerRack = 0;
    const auto voice = ThreadVoice(system, state, rack);
    const auto peer = ThreadVoice(system, peerState, peerRack);
    state.failFirst = failure;
    std::thread first([&] { Require(ThreadValue(voice, 1.0f) == (failure ? -55 : SCE_NGS2_OK)); });
    WaitEntered(state);
    std::latch started(2);
    std::atomic<bool> queried = false;
    std::thread second([&] {
        started.count_down();
        Require(ThreadValue(voice, 2.0f) == SCE_NGS2_OK);
    });
    std::thread query([&] {
        started.count_down();
        std::uint32_t flags = 99;
        Require(sceNgs2VoiceGetStateFlags(voice, &flags) == SCE_NGS2_OK && flags == 0);
        queried = true;
    });
    started.wait();
    Require(ThreadValue(peer, 0.75f) == SCE_NGS2_OK);
    Require(state.active.load() && state.calls == 1 && !queried.load());
    ReleaseControl(state);
    first.join();
    second.join();
    query.join();
    Require(queried && state.calls == 2 && state.before[0] == 1.0f && state.before[1] == 2.0f);
    Require(ThreadValue(voice, 3.0f) == SCE_NGS2_OK && state.before[2] == 4.0f);
    Require(sceNgs2RackDestroy(rack, nullptr) == SCE_NGS2_OK);
    Require(sceNgs2RackDestroy(peerRack, nullptr) == SCE_NGS2_OK);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
    Require(state.cleanups == 1 && peerState.cleanups == 1);
    ResetBuffers(firstBuffer);
}

static void TestDestruction(bool destroySystem) {
    const auto firstBuffer = usedBuffers;
    const auto system = ThreadSystem();
    ThreadControl state;
    std::uintptr_t rack = 0;
    const auto voice = ThreadVoice(system, state, rack);
    std::thread control([&] { Require(ThreadValue(voice, 1.0f) == SCE_NGS2_OK); });
    WaitEntered(state);
    std::latch started(1);
    std::thread destroy([&] {
        started.count_down();
        const int result = destroySystem ? sceNgs2SystemDestroy(system, nullptr) : sceNgs2RackDestroy(rack, nullptr);
        Require(result == SCE_NGS2_OK);
    });
    started.wait();
    Require(state.cleanups == 0);
    ReleaseControl(state);
    control.join();
    destroy.join();
    Require(!state.active.load());
    if (destroySystem) {
        Require(state.cleanups == 0);
        Require(sceNgs2RackDestroy(rack, nullptr) == SCE_NGS2_OK);
    }
    Require(state.cleanups == 1);
    if (!destroySystem) Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
    ResetBuffers(firstBuffer);
}

static void TestRenderWait(bool crossSystem) {
    const auto firstBuffer = usedBuffers;
    const auto system = ThreadSystem();
    const auto master = Mastering(system, 2);
    ThreadControl state, peerState;
    peerState.park = false;
    std::uintptr_t rack = 0, peerRack = 0;
    const auto voice = ThreadVoice(system, state, rack);
    const auto peerSystem = crossSystem ? ThreadSystem() : system;
    const auto peer = ThreadVoice(peerSystem, peerState, peerRack);
    Require(ThreadValue(voice, 0.5f) == SCE_NGS2_OK);
    Patch(voice, master);
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    std::thread control([&] { Require(ThreadValue(voice, 1.0f) == SCE_NGS2_OK); });
    WaitEntered(state);
    std::array<float, 128> audio{};
    std::latch started(1);
    std::thread render([&] {
        started.count_down();
        const Ngs2RenderBufferInfo output{audio.data(), sizeof(audio), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 2};
        Require(sceNgs2SystemRender(system, &output, 1) == SCE_NGS2_OK);
    });
    started.wait();
    Require(ThreadValue(peer, 0.75f) == SCE_NGS2_OK);
    ReleaseControl(state);
    control.join();
    render.join();
    for (float value : audio) Require(value == 0.25f);
    Require(sceNgs2RackDestroy(rack, nullptr) == SCE_NGS2_OK);
    Require(sceNgs2RackDestroy(peerRack, nullptr) == SCE_NGS2_OK);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
    if (crossSystem) Require(sceNgs2SystemDestroy(peerSystem, nullptr) == SCE_NGS2_OK);
    Require(state.cleanups == 1 && peerState.cleanups == 1);
    ResetBuffers(firstBuffer);
}

static void TestLifecycle(bool crossSystem, bool inlineHandoff, bool destroyOnSetup, bool destroyOnCleanup) {
    const auto firstBuffer = usedBuffers;
    const auto system = ThreadSystem();
    const auto master = Mastering(system, 2);
    const auto peerSystem = crossSystem ? ThreadSystem() : system;
    ThreadControl peerState;
    peerState.park = false;
    std::uintptr_t peerRack = 0;
    const auto peer = ThreadVoice(peerSystem, peerState, peerRack);
    ThreadControl state;
    state.park = false;
    state.lifecycle = true;
    state.inlineHandoff = inlineHandoff;
    state.destroyOnSetup = destroyOnSetup;
    state.destroyOnCleanup = destroyOnCleanup;
    state.system = system;
    state.peer = peer;
    std::uintptr_t rack = 0;
    const auto voice = ThreadVoice(system, state, rack);
    Require(state.setups == 1 && peerState.calls == 1 && peerState.before[0] == 1.0f);
    if (!destroyOnSetup) {
        Require(ThreadValue(voice, 0.5f) == SCE_NGS2_OK && state.before[0] == 1.0f);
        Patch(voice, master);
        Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
        std::array<float, 128> audio{};
        const Ngs2RenderBufferInfo output{audio.data(), sizeof(audio), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 2};
        Require(sceNgs2SystemRender(system, &output, 1) == SCE_NGS2_OK);
        for (float value : audio) Require(value == 0.125f);
        Require(sceNgs2RackDestroy(rack, nullptr) == SCE_NGS2_OK);
        Require(state.cleanups == 1 && peerState.calls == 2 && peerState.before[1] == 4.0f);
    } else Require(state.cleanups == 0);
    Require(sceNgs2RackDestroy(peerRack, nullptr) == SCE_NGS2_OK);
    Require(sceNgs2SystemDestroy(peerSystem, nullptr) == SCE_NGS2_OK);
    if (crossSystem && !destroyOnSetup && !destroyOnCleanup)
        Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
    Require(peerState.cleanups == 1);
    ResetBuffers(firstBuffer);
}

static void TestSystemCleanupHandoff() {
    const auto firstBuffer = usedBuffers;
    const auto system = ThreadSystem();
    const auto peerSystem = ThreadSystem();
    ThreadControl state, peerState;
    state.park = false;
    peerState.park = false;
    std::uintptr_t rack = 0, peerRack = 0;
    const auto peer = ThreadVoice(peerSystem, peerState, peerRack);
    ThreadVoice(system, state, rack);
    state.lifecycle = true;
    state.systemCleanup = true;
    state.system = system;
    state.peer = peer;
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
    Require(state.cleanups == 0 && peerState.calls == 0);
    Require(sceNgs2RackDestroy(rack, nullptr) == SCE_NGS2_OK);
    Require(state.cleanups == 1 && peerState.calls == 1 && peerState.before[0] == 1.0f);
    Require(ThreadValue(peer, 0.5f) == SCE_NGS2_OK && peerState.before[1] == 6.0f);
    Require(sceNgs2RackDestroy(peerRack, nullptr) == SCE_NGS2_OK && peerState.cleanups == 1);
    Require(sceNgs2SystemDestroy(peerSystem, nullptr) == SCE_NGS2_OK);
    ResetBuffers(firstBuffer);
}

struct LifecycleOrder {
    std::uintptr_t peer = 0;
    unsigned failure = 0;
    std::vector<unsigned> setup;
    std::vector<unsigned> cleanup;
    unsigned allocations = 0;
    unsigned releases = 0;
};

struct LifecycleModule {
    LifecycleOrder* order;
    unsigned index;
    void* param = nullptr;
    std::array<float, 2> before{};
    unsigned controls = 0;
};

static void LifecycleQuery(std::uintptr_t peer) {
    std::thread helper([&] {
        std::uint32_t flags = 99;
        Require(sceNgs2VoiceGetStateFlags(peer, &flags) == SCE_NGS2_OK && flags == 0);
    });
    helper.join();
}

static int APS5_VABI OrderedSetup(Ngs2UserFx2SetupContext* context) {
    auto& module = *reinterpret_cast<LifecycleModule*>(context->user_data);
    const auto index = context->voice_index;
    Require(context->max_voices == 2 && index < 2);
    if (index == 0) module.param = context->param;
    Require(context->param == module.param);
    Require(*static_cast<float*>(context->param) == (index == 0 ? 0.0f : 10.0f * module.index + 1.0f));
    Require(*static_cast<unsigned*>(context->work) == 0);
    *static_cast<unsigned*>(context->work) = 10 * module.index + index + 1;
    *static_cast<float*>(context->param) = 10.0f * module.index + index + 1.0f;
    module.order->setup.push_back(module.index * 2 + index);
    LifecycleQuery(module.order->peer);
    if (module.order->failure == 3 && module.index == 1 && index == 0) throw std::runtime_error("setup failure");
    return (module.order->failure == 1 || module.order->failure == 5) && module.index == 1 && index == 0 ? -55 : 7;
}

static int APS5_VABI OrderedCleanup(Ngs2UserFx2CleanupContext* context) {
    auto& module = *reinterpret_cast<LifecycleModule*>(context->user_data);
    const auto index = context->voice_index;
    Require(context->max_voices == 2 && index < 2 && context->param == module.param);
    Require(*static_cast<float*>(context->param) == 10.0f * module.index + 2.0f);
    Require(*static_cast<unsigned*>(context->work) == 10 * module.index + index + 1);
    module.order->cleanup.push_back(module.index * 2 + index);
    LifecycleQuery(module.order->peer);
    if (module.order->failure == 4 && module.index == 0 && index == 0) throw std::runtime_error("cleanup failure");
    return module.order->failure == 2 && module.index == 0 && index == 0 ? -55 : 7;
}

static int APS5_VABI OrderedControl(Ngs2UserFx2ControlContext* context) {
    auto& module = *reinterpret_cast<LifecycleModule*>(context->user_data);
    Require(module.controls < module.before.size());
    module.before[module.controls++] = *static_cast<float*>(context->param);
    *static_cast<float*>(context->param) = 99.0f;
    return SCE_NGS2_OK;
}

static int APS5_VABI LifecycleAllocate(Ngs2ContextBufferInfo* context) {
    auto& order = *reinterpret_cast<LifecycleOrder*>(context->user_data);
    Require(order.allocations++ == 0 && order.releases == 0);
    context->host_buffer = std::malloc(context->host_buffer_size);
    Require(context->host_buffer != nullptr);
    return SCE_NGS2_OK;
}

static int APS5_VABI LifecycleRelease(Ngs2ContextBufferInfo* context) {
    auto& order = *reinterpret_cast<LifecycleOrder*>(context->user_data);
    Require(order.allocations == 1 && order.releases++ == 0);
    std::free(context->host_buffer);
    return SCE_NGS2_OK;
}

static void TestLifecycleOrder(unsigned failure) {
    const auto firstBuffer = usedBuffers;
    const auto system = ThreadSystem();
    ThreadControl peerState;
    peerState.park = false;
    std::uintptr_t peerRack = 0;
    LifecycleOrder order{ThreadVoice(system, peerState, peerRack), failure, {}, {}};
    std::array<LifecycleModule, 2> modules{{{&order, 0}, {&order, 1}}};
    std::array<Ngs2CustomUserFx2ModuleOption, 2> options{};
    Ngs2CustomSubmixerRackOption option{};
    auto& custom = option.custom_rack_option;
    auto& common = custom.rack_option;
    common.size = sizeof(option);
    common.max_grain_samples = 512;
    common.max_voices = 2;
    common.max_matrices = 1;
    common.max_ports = 1;
    custom.state_size = 16;
    custom.num_buffers = 1;
    custom.num_modules = 2;
    for (unsigned i = 0; i < modules.size(); ++i) {
        options[i] = {{sizeof(options[i])}, OrderedSetup, OrderedCleanup, OrderedControl, ThreadProcess,
            0, sizeof(float), sizeof(unsigned), reinterpret_cast<std::uintptr_t>(&modules[i])};
        custom.module[i].module_id = SCE_NGS2_CUSTOM_MODULE_ID_USER_FX2;
        custom.module[i].option = &options[i].custom_module_option;
    }
    option.max_channels = 2;
    option.max_inputs = 1;
    Ngs2ContextBufferInfo query{};
    Require(sceNgs2RackQueryBufferSize(SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER, &common, &query) == SCE_NGS2_OK);
    const auto buffer = Buffer(query);
    std::uintptr_t rack = 0;
    int result;
    try {
        if (failure >= 3) {
            const Ngs2BufferAllocator allocator{LifecycleAllocate, LifecycleRelease, reinterpret_cast<std::uintptr_t>(&order)};
            result = sceNgs2RackCreateWithAllocator(system, SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER, &common, &allocator, &rack);
        } else result = TestRackCreate(system, SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER, &common, &buffer, &rack);
        Require(failure != 3);
    } catch (const std::runtime_error& error) {
        Require(failure == 3 && std::string(error.what()) == "setup failure");
        result = -55;
    }
    if (failure == 1 || failure == 3 || failure == 5) {
        Require(result == -55 && rack == 0);
        Require(order.setup == std::vector<unsigned>({0, 1, 2}) && order.cleanup.empty());
    } else {
        Require(result == SCE_NGS2_OK && order.setup == std::vector<unsigned>({0, 1, 2, 3}));
        for (unsigned i = 0; i < 2; ++i) {
            std::uintptr_t voice = 0;
            Require(sceNgs2RackGetVoiceHandle(rack, i, &voice) == SCE_NGS2_OK);
            for (unsigned m = 0; m < modules.size(); ++m) {
                const float value = 0.5f;
                const Ngs2CustomVoiceUserFx2Param param{{sizeof(param), 0, SCE_NGS2_CUSTOM_VOICE_PARAM_USER_FX2 | m}, &value, sizeof(value)};
                Require(sceNgs2VoiceControl(voice, &param.header) == SCE_NGS2_OK);
            }
        }
        Require((modules[0].before == std::array<float, 2>({2.0f, 2.0f})));
        Require((modules[1].before == std::array<float, 2>({12.0f, 12.0f})));
        try {
            Require(sceNgs2RackDestroy(rack, nullptr) == SCE_NGS2_OK);
            Require(failure != 4);
        } catch (const std::runtime_error& error) {
            Require(failure == 4 && std::string(error.what()) == "cleanup failure");
        }
        if (failure == 4) {
            Require(order.cleanup == std::vector<unsigned>({0}));
            std::uintptr_t voice = 0;
            Require(sceNgs2RackGetVoiceHandle(rack, 0, &voice) == SCE_NGS2_ERROR_INVALID_RACK_HANDLE && voice == 0);
        } else Require(order.cleanup == (failure == 2 ? std::vector<unsigned>({0, 2, 3}) : std::vector<unsigned>({0, 1, 2, 3})));
    }
    if (failure >= 3) Require(order.allocations == 1 && order.releases == 1);
    Require(sceNgs2RackDestroy(peerRack, nullptr) == SCE_NGS2_OK && peerState.cleanups == 1);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
    ResetBuffers(firstBuffer);
}

static void TestSystemOwnership() {
    const auto firstBuffer = usedBuffers;
    const auto system = ThreadSystem();
    const auto master = Mastering(system, 2);
    ThreadControl state;
    state.park = false;
    std::uintptr_t rack = 0;
    const auto voice = ThreadVoice(system, state, rack);
    Require(ThreadValue(voice, 2.0f) == SCE_NGS2_OK);
    Patch(voice, master);
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    std::array<float, 128> audio{};
    const Ngs2RenderBufferInfo output{audio.data(), sizeof(audio), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 2};
    Require(sceNgs2SystemRender(system, &output, 1) == SCE_NGS2_OK);
    for (float value : audio) Require(value == 0.5f);
    Ngs2ContextBufferInfo released{};
    Require(sceNgs2SystemDestroy(system, &released) == SCE_NGS2_OK && state.cleanups == 0);
    Require(released.host_buffer != nullptr && released.host_buffer_size != 0);
    std::memset(released.host_buffer, 0xda, released.host_buffer_size);
    std::uintptr_t found = 0, owner = 0;
    std::uint32_t index = 99, flags = 99;
    Require(sceNgs2RackGetVoiceHandle(rack, 0, &found) == SCE_NGS2_OK && found == voice);
    Require(sceNgs2VoiceGetOwner(voice, &owner, &index) == SCE_NGS2_OK && owner == rack && index == 0);
    Require(sceNgs2VoiceGetStateFlags(voice, &flags) == SCE_NGS2_OK && flags == 3);
    Ngs2VoicePortInfo port{};
    Require(sceNgs2VoiceGetPortInfo(voice, 0, &port, sizeof(port)) == SCE_NGS2_OK && port.dest_handle == master);
    Require(ThreadValue(voice, 0.75f) == SCE_NGS2_OK && state.before[1] == 4.0f);
    Require(sceNgs2RackDestroy(rack, nullptr) == SCE_NGS2_OK && state.cleanups == 1);
    Require(sceNgs2RackDestroy(rack, nullptr) == SCE_NGS2_ERROR_INVALID_RACK_HANDLE && state.cleanups == 1);
    flags = 99;
    Require(sceNgs2VoiceGetStateFlags(voice, &flags) == SCE_NGS2_ERROR_INVALID_VOICE_HANDLE && flags == 99);
    Require(sceNgs2VoiceGetOwner(master, &owner, &index) == SCE_NGS2_OK);
    Require(sceNgs2RackDestroy(owner, nullptr) == SCE_NGS2_OK);
    ResetBuffers(firstBuffer);
}

static void TestDetachedSampler() {
    const auto firstBuffer = usedBuffers;
    const auto system = ThreadSystem();
    const auto master = Mastering(system, 2);
    const auto rack = CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER);
    const auto voice = Voice(rack);
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 2, 48000, 0, 0, 0}});
    std::array<std::int16_t, 512> pcm{};
    pcm.fill(4096);
    const Ngs2WaveformBlock block{0, sizeof(pcm), 0, 0, pcm.size() / 2, 0, 0};
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, pcm.data(), 0, 1, &block});
    Patch(voice, master);
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    std::array<float, 128> audio{};
    const Ngs2RenderBufferInfo output{audio.data(), sizeof(audio), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 2};
    Require(sceNgs2SystemRender(system, &output, 1) == SCE_NGS2_OK);
    Ngs2SamplerVoiceState before{}, after{};
    Require(sceNgs2VoiceGetState(voice, &before.voice_state, sizeof(before)) == SCE_NGS2_OK);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
    Require(sceNgs2VoiceGetState(voice, &after.voice_state, sizeof(after)) == SCE_NGS2_OK);
    Require(after.voice_state.state_flags == before.voice_state.state_flags && after.num_decoded_samples == before.num_decoded_samples);
    Require(after.decoded_data_size == before.decoded_data_size && after.waveform_data == before.waveform_data);
    Ngs2SamplerVoiceFilterParam filter{{sizeof(filter), 0, SCE_NGS2_SAMPLER_VOICE_PARAM_FILTER}, 0, 1, 1, 0, 1000.0f, 1.0f, 0.5f, {}};
    Require(sceNgs2VoiceControl(voice, &filter.header) == SCE_NGS2_OK);
    filter.frequency = 2000.0f;
    filter.q = 0.75f;
    Require(sceNgs2VoiceControl(voice, &filter.header) == SCE_NGS2_OK);
    filter.type = 0;
    Require(sceNgs2VoiceControl(voice, &filter.header) == SCE_NGS2_OK);
    Event(voice, SCE_NGS2_VOICE_EVENT_STOP_IMM);
    Require(sceNgs2RackDestroy(rack, nullptr) == SCE_NGS2_OK);
    ResetBuffers(firstBuffer);
}

static void TestRackInfo() {
    const auto firstBuffer = usedBuffers;
    const auto system = ThreadSystem();
    struct Case {
        std::uint32_t id;
        unsigned type;
        unsigned voices;
        unsigned works;
        unsigned inputs;
        unsigned matrices;
        unsigned ports;
        unsigned modules;
        unsigned extension;
    };
    unsigned uid = 1;
    for (const auto& item : {Case{SCE_NGS2_RACK_ID_SAMPLER, 0, 256, 256, 0, 1, 8, 13, 32},
                            Case{SCE_NGS2_RACK_ID_SUBMIXER, 1, 1, 8, 1, 1, 8, 13, 16},
                            Case{SCE_NGS2_RACK_ID_MASTERING, 2, 1, 8, 1, 0, 0, 7, 8},
                            Case{SCE_NGS2_RACK_ID_REVERB, 1, 1, 8, 1, 1, 8, 1, 8},
                            Case{SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER, 1, 1, 2, 1, 1, 1, 1, 1112}}) {
        ThreadControl state;
        state.park = false;
        std::uintptr_t rack = 0;
        if (item.id == SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER) ThreadVoice(system, state, rack);
        else rack = CreateRack(system, item.id);
        struct Output {
            Ngs2RackInfo info;
            std::array<unsigned char, 1200> tail;
        } output;
        std::memset(&output, 0xa5, sizeof(output));
        Require(sceNgs2RackGetInfo(0, nullptr, 0) == SCE_NGS2_ERROR_INVALID_RACK_HANDLE);
        Require(sceNgs2RackGetInfo(rack, nullptr, 0) == SCE_NGS2_ERROR_INVALID_OUT_ADDRESS);
        Require(sceNgs2RackGetInfo(rack, &output.info, sizeof(output.info) - 1) == SCE_NGS2_ERROR_INVALID_OUT_SIZE);
        for (auto value : output.tail) Require(value == 0xa5);
        Require(sceNgs2RackGetInfo(rack, &output.info, sizeof(output.info) + item.extension - 1) == SCE_NGS2_OK);
        if (item.id == SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER) {
            for (unsigned i = 56; i < 976; ++i) Require(output.tail[i] == 0xa5);
            for (unsigned i = 984; i < output.tail.size(); ++i) Require(output.tail[i] == 0xa5);
            Ngs2CustomRackModuleInfo module{};
            std::memcpy(&module, output.tail.data() + 16, sizeof(module));
            Require(module.module_id == SCE_NGS2_CUSTOM_MODULE_ID_USER_FX2 && module.option != nullptr && module.option->size == sizeof(Ngs2CustomUserFx2ModuleOption));
            const auto& option = *reinterpret_cast<const Ngs2CustomUserFx2ModuleOption*>(module.option);
            Require(option.setup_handler == ThreadSetup && option.cleanup_handler == ThreadCleanup && option.user_data == reinterpret_cast<std::uintptr_t>(&state));
        } else for (auto value : output.tail) Require(value == 0xa5);
        const auto& info = output.info;
        Require(sizeof(info) == 232 && info.rack_handle == rack && info.owner_system_handle == system);
        Require(info.name[0] == 0 && info.name[1] == static_cast<char>(0xa5) && info.name[63] == static_cast<char>(0xa5));
        Require(info.type == item.type && info.rack_id == item.id && info.uid == uid++);
        Require(info.min_grain_samples == 64 && info.max_grain_samples == 512 && info.max_voices == item.voices);
        Require(info.max_channel_works == item.works && info.max_inputs == item.inputs && info.max_matrices == item.matrices && info.max_ports == item.ports);
        Require(info.max_voice_channels == 8 && info.max_output_channels == 8 && info.num_modules == item.modules && info.num_buffers == 1);
        Require(info.state_flags == 1 && info.render_count == 0 && info.active_voice_count == 0 && info.active_channel_work_count == 0);
        Require(info.last_process_ratio == 0 && info.last_processed_tick == 0);
        Require(sceNgs2RackGetInfo(rack, &output.info, sizeof(output.info) + item.extension) == SCE_NGS2_OK);
        for (unsigned i = item.extension; i < output.tail.size(); ++i) Require(output.tail[i] == 0xa5);
        std::uint32_t first = 0;
        std::memcpy(&first, output.tail.data(), sizeof(first));
        Require(first == (item.id == SCE_NGS2_RACK_ID_SAMPLER ? 256u : item.id == SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER ? 16u : 8u));
        Require(sceNgs2RackDestroy(rack, nullptr) == SCE_NGS2_OK);
    }
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
    const auto next = ThreadSystem();
    const auto rack = CreateRack(next, SCE_NGS2_RACK_ID_SAMPLER);
    Ngs2RackInfo info{};
    Require(sceNgs2RackGetInfo(rack, &info, sizeof(info)) == SCE_NGS2_OK && info.uid == 1);
    Require(sceNgs2RackDestroy(rack, nullptr) == SCE_NGS2_OK);
    Require(sceNgs2SystemDestroy(next, nullptr) == SCE_NGS2_OK);
    ResetBuffers(firstBuffer);
}

static void TestRackInfoCounters(bool external) {
    const auto firstBuffer = usedBuffers;
    Manager manager{{}, 2};
    Ngs2JobSchedulerOption scheduler{};
    scheduler.size = sizeof(scheduler);
    scheduler.user_data = reinterpret_cast<std::uintptr_t>(&manager);
    scheduler.query_handler = Query;
    scheduler.init_handler = Initialize;
    scheduler.destroy_handler = Destroy;
    scheduler.submit_handler = Submit;
    scheduler.flush_handler = Complete;
    scheduler.join_handler = Complete;
    scheduler.num_workers = 2;
    scheduler.max_jobs = 16;
    const auto system = ThreadSystem(external ? &scheduler : nullptr);
    const auto master = Mastering(system, 2);
    ThreadControl state;
    state.park = false;
    state.infoDuringProcess = true;
    std::uintptr_t rack = 0;
    const auto voice = ThreadVoice(system, state, rack);
    Require(ThreadValue(voice, 0.5f) == SCE_NGS2_OK);
    Patch(voice, master);
    const auto check = [&](std::uint64_t renders, unsigned active) {
        Ngs2RackInfo info{};
        Require(sceNgs2RackGetInfo(rack, &info, sizeof(info)) == SCE_NGS2_OK);
        Require(info.render_count == renders && info.active_voice_count == active && info.active_channel_work_count == active * 2);
        Require(info.last_process_ratio == 0 && info.last_processed_tick == 0);
    };
    check(0, 0);
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    check(0, 1);
    std::array<float, 128> audio{};
    const Ngs2RenderBufferInfo output{audio.data(), sizeof(audio), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 2};
    Require(sceNgs2SystemRender(system, &output, 1) == SCE_NGS2_OK);
    check(1, 1);
    Event(voice, SCE_NGS2_VOICE_EVENT_PAUSE);
    check(1, 1);
    Require(sceNgs2SystemRender(system, &output, 1) == SCE_NGS2_OK);
    check(2, 1);
    Event(voice, SCE_NGS2_VOICE_EVENT_STOP);
    check(2, 1);
    Require(sceNgs2SystemRender(system, &output, 1) == SCE_NGS2_OK);
    check(3, 0);
    const auto lateRack = CreateRack(system, SCE_NGS2_RACK_ID_SUBMIXER);
    Ngs2RackInfo late{};
    Require(sceNgs2RackGetInfo(lateRack, &late, sizeof(late)) == SCE_NGS2_OK && late.render_count == 0);
    Require(sceNgs2SystemRender(system, &output, 1) == SCE_NGS2_OK);
    check(4, 0);
    Require(sceNgs2RackGetInfo(lateRack, &late, sizeof(late)) == SCE_NGS2_OK && late.render_count == 1);
    Require(sceNgs2RackDestroy(rack, nullptr) == SCE_NGS2_OK);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
    ResetBuffers(firstBuffer);
}

static void TestDetachedPending(bool external) {
    const auto firstBuffer = usedBuffers;
    Manager manager{{}, 2};
    Ngs2JobSchedulerOption scheduler{};
    scheduler.size = sizeof(scheduler);
    scheduler.user_data = reinterpret_cast<std::uintptr_t>(&manager);
    scheduler.query_handler = Query;
    scheduler.init_handler = Initialize;
    scheduler.destroy_handler = Destroy;
    scheduler.submit_handler = Submit;
    scheduler.flush_handler = Complete;
    scheduler.join_handler = Complete;
    scheduler.num_workers = 2;
    scheduler.max_jobs = 16;
    const auto system = ThreadSystem(external ? &scheduler : nullptr);
    const auto master = Mastering(system, 2);
    ThreadControl state;
    state.park = false;
    state.infoDuringProcess = true;
    state.updateDuringProcess = true;
    std::uintptr_t rack = 0;
    const auto voice = ThreadVoice(system, state, rack);
    Require(ThreadValue(voice, 0.5f) == SCE_NGS2_OK);
    Patch(voice, master);
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    std::array<float, 128> audio{};
    const Ngs2RenderBufferInfo output{audio.data(), sizeof(audio), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 2};
    Require(sceNgs2SystemRender(system, &output, 1) == SCE_NGS2_OK && state.infoProcesses == 1);
    for (float value : audio) Require(value == 0.125f);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK && state.cleanups == 0);
    Ngs2VoicePortInfo port{};
    Require(sceNgs2VoiceGetPortInfo(voice, 0, &port, sizeof(port)) == SCE_NGS2_OK && port.volume == 0.25f);
    Control(voice, SCE_NGS2_CUSTOM_SUBMIXER_VOICE_PARAM_SETUP, Ngs2CustomSubmixerVoiceSetupParam{{}, 2, 2, 0});
    Require(ThreadValue(voice, 0.75f) == SCE_NGS2_OK && state.calls == 3 && state.before[2] == 1.0f);
    Require(sceNgs2RackDestroy(rack, nullptr) == SCE_NGS2_OK && state.cleanups == 1);
    ResetBuffers(firstBuffer);
}

static void TestDetachedControlFamilies() {
    const auto firstBuffer = usedBuffers;
    const auto system = ThreadSystem();
    const auto master = Mastering(system, 2);
    const auto ordinary = Sampler(system, false);
    const auto custom = Sampler(system, true);
    const auto mixer = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SUBMIXER));
    Control(mixer, SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP, Ngs2SubmixerVoiceSetupParam{{}, 2, 0});
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
    for (const auto voice : {ordinary, custom}) {
        const auto setup = voice == ordinary ? SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP : SCE_NGS2_CUSTOM_SAMPLER_VOICE_PARAM_SETUP;
        Control(voice, setup, Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 1, 48000, 0, 0, 0}});
        std::array<std::int16_t, 512> pcm{};
        const Ngs2WaveformBlock block{0, sizeof(pcm), 0, 0, pcm.size(), 0, 0};
        Control(voice, voice == ordinary ? SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS : SCE_NGS2_CUSTOM_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS,
            Ngs2SamplerVoiceWaveformBlocksParam{{}, pcm.data(), 0, 1, &block});
        std::array<float, 4> levels{0.5f, 0.25f, -0.75f, 1.0f};
        Control(voice, SCE_NGS2_VOICE_PARAM_MATRIX_LEVELS, Ngs2VoiceMatrixLevelsParam{{}, 0, levels.size(), levels.data()});
        levels.fill(0);
        Control(voice, SCE_NGS2_VOICE_PARAM_PORT_MATRIX, Ngs2VoicePortMatrixParam{{}, 0, 0});
        Control(voice, SCE_NGS2_VOICE_PARAM_PORT_VOLUME, Ngs2VoicePortVolumeParam{{}, 0, 0.25f});
        Ngs2VoiceMatrixInfo matrix{};
        Require(sceNgs2VoiceGetMatrixInfo(voice, 0, &matrix, sizeof(matrix)) == SCE_NGS2_OK && matrix.num_levels == 4);
        Require(matrix.levels[0] == 0.5f && matrix.levels[1] == 0.25f && matrix.levels[2] == -0.75f && matrix.levels[3] == 1.0f);
        Ngs2VoicePortInfo port{};
        Require(sceNgs2VoiceGetPortInfo(voice, 0, &port, sizeof(port)) == SCE_NGS2_OK && port.matrix_id == 0 && port.volume == 0.25f);
        if (voice == custom) Control(voice, SCE_NGS2_CUSTOM_VOICE_PARAM_PITCH_SHIFT, Ngs2CustomVoicePitchShiftParam{{}, 1200, 0, 0, 0});
        else Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_PITCH, Ngs2SamplerVoicePitchParam{{}, 2.0f});
        Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
        std::uint32_t flags = 99;
        Require(sceNgs2VoiceGetStateFlags(voice, &flags) == SCE_NGS2_OK && flags == 1);
        std::uintptr_t rack = 0;
        std::uint32_t index = 99;
        Require(sceNgs2VoiceGetOwner(voice, &rack, &index) == SCE_NGS2_OK);
        Require(sceNgs2RackDestroy(rack, nullptr) == SCE_NGS2_OK);
    }
    Control(master, SCE_NGS2_MASTERING_VOICE_PARAM_GAIN, Ngs2MasteringVoiceGainParam{{}, 0.5f, 0.25f});
    Control(mixer, SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP, Ngs2SubmixerVoiceSetupParam{{}, 2, 0});
    Event(mixer, SCE_NGS2_VOICE_EVENT_PLAY);
    ResetBuffers(firstBuffer);
}

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "detached-pending") {
        TestDetachedPending(false);
        TestDetachedPending(true);
        TestDetachedControlFamilies();
        std::puts("NGS2 detached pending controls and control families passed");
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "rackinfo") {
        TestRackInfo();
        TestRackInfoCounters(false);
        TestRackInfoCounters(true);
        std::puts("NGS2 rack info layout, metadata and counters passed");
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "ownership") {
        TestSystemOwnership();
        TestSystemCleanupHandoff();
        TestDetachedSampler();
        std::puts("NGS2 detached rack ownership: 3 scenarios passed");
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "lifecycle-guards") {
        TestLifecycleOrder(3);
        TestLifecycleOrder(4);
        TestLifecycleOrder(5);
        std::puts("NGS2 lifecycle failure cleanup: 3 host scenarios passed");
        return 0;
    }
    if (argc > 1) {
        TestLifecycle(false, false, false, false);
        TestLifecycle(true, false, false, false);
        TestLifecycle(false, true, false, false);
        TestLifecycle(true, false, true, false);
        TestLifecycle(true, false, false, true);
        TestLifecycleOrder(0);
        TestLifecycleOrder(1);
        TestLifecycleOrder(2);
        std::puts("NGS2 lifecycle handoffs: 8 scenarios passed");
        return 0;
    }

    TestSerialization(false);
    TestSerialization(true);
    TestDestruction(false);
    TestDestruction(true);
    TestRenderWait(false);
    TestRenderWait(true);
    std::puts("NGS2 cross-thread ownership: 6 scenarios passed");
    return 0;
}
