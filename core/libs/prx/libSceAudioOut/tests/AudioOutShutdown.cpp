#include "prx/libSceAudioOut/include/AudioOut2.hpp"
#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/Shutdown.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

namespace {

constexpr std::uint32_t grain = 256;
constexpr std::uint32_t frequency = 48000;
constexpr std::uint16_t portTypeMain = 0;
constexpr std::uint32_t attributeData = 0;
constexpr std::uint32_t formatStereoFloat = 2u << 8;
constexpr int user = 0x10000000;
constexpr std::uint32_t formatS16Stereo = 1;
constexpr std::uint64_t startedOutputs = 3;
constexpr auto settle = std::chrono::milliseconds(100);
constexpr auto exitLimit = std::chrono::seconds(3);

std::atomic<std::uint64_t> outputs{0};
std::vector<float> pcm(static_cast<std::size_t>(grain) * 2, 0.25f);

void Fail(const char* message) {
    std::fprintf(stderr, "%s\n", message);
    std::fflush(stderr);
    std::_Exit(1);
}

void RequireParked() {
    const auto before = outputs.load();
    std::this_thread::sleep_for(settle);
    if (outputs.load() != before) Fail("the mixer thread kept outputting audio after the audio shutdown");
    std::puts("the mixer thread was parked before the audio tables were destroyed");
    std::fflush(stdout);
}

void DelayExit() {
    std::this_thread::sleep_for(settle);
}

void ReportExit() {
    std::puts("the exit finished while a push waited for an advance");
    std::fflush(stdout);
}

void SetData(AudioOut2PortHandle port) {
    const void* pointer = pcm.data();
    const AudioOut2Attribute attribute{attributeData, 0, &pointer, sizeof(pointer)};
    if (sceAudioOut2PortSetAttributes(port, &attribute, 1) != 0) Fail("a port attribute set returned an error");
}

void CreatePort(AudioOut2ContextHandle& context, AudioOut2PortHandle& port) {
    AudioOut2ContextParam params{};
    if (sceAudioOut2ContextResetParam(&params) != 0) Fail("context parameters were not reset");
    params.num_grains = grain;
    if (sceAudioOut2ContextCreate(&params, nullptr, 0, &context) != 0) Fail("context was not created");
    AudioOut2PortParam portParams{};
    portParams.port_type = portTypeMain;
    portParams.data_format = formatStereoFloat;
    portParams.sampling_freq = frequency;
    if (sceAudioOut2PortCreate(context, &portParams, &port) != 0) Fail("port was not created");
}

void PushAudioOut2() {
    AudioOut2ContextHandle context = 0;
    AudioOut2PortHandle port = 0;
    CreatePort(context, port);
    std::thread([context, port] {
        for (;;) {
            SetData(port);
            if (sceAudioOut2ContextPush(context, 1) != 0) Fail("a push during exit returned an error");
            outputs++;
        }
    }).detach();
}

void WaitInPushAudioOut2() {
    AudioOut2ContextHandle context = 0;
    AudioOut2PortHandle port = 0;
    CreatePort(context, port);
    SetData(port);
    if (sceAudioOut2ContextAdvance(context) != 0) Fail("an advance returned an error");
    if (sceAudioOut2ContextPush(context, 1) != 0) Fail("a push returned an error");
    std::thread([context] {
        outputs++;
        sceAudioOut2ContextPush(context, 1);
        Fail("a push waiting for an advance returned during exit");
    }).detach();
}

void AdvanceAtExit() {
    AudioOut2ContextHandle context = 0;
    AudioOut2PortHandle port = 0;
    CreatePort(context, port);
    SetData(port);
    LibcRegisterShutdown_nid_postfix(DelayExit);
    LibcRequestExit_nid_postfix(0);
    sceAudioOut2ContextAdvance(context);
    Fail("an advance after the exit request returned");
}

void OutputAudioOut() {
    const int port = sceAudioOutOpen(user, 0, 0, grain, frequency, formatS16Stereo);
    if (port <= 0) Fail("port was not opened");
    static std::vector<std::int16_t> data(static_cast<std::size_t>(grain) * 2, 0x1000);
    std::thread([port] {
        for (;;) {
            if (sceAudioOutOutput(port, data.data()) < 0) Fail("an output during exit returned an error");
            outputs++;
        }
    }).detach();
}

}

int main(int argc, char** argv) {
#ifdef _WIN32
    _putenv_s("SDL_AUDIODRIVER", "dummy");
#else
    setenv("SDL_AUDIODRIVER", "dummy", 1);
#endif
    const bool waiting = argc == 2 && std::strcmp(argv[1], "--audio-out2-waiting-push") == 0;
    if (argc == 2 && std::strcmp(argv[1], "--audio-out2") == 0) PushAudioOut2();
    else if (argc == 2 && std::strcmp(argv[1], "--audio-out") == 0) OutputAudioOut();
    else if (waiting) WaitInPushAudioOut2();
    else if (argc == 2 && std::strcmp(argv[1], "--audio-out2-advance") == 0) AdvanceAtExit();
    else Fail("usage: audio_out_shutdown_tests --audio-out2 | --audio-out2-waiting-push | --audio-out2-advance | --audio-out");
    while (outputs.load() < (waiting ? 1 : startedOutputs)) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (waiting) std::this_thread::sleep_for(settle);
    std::thread([] {
        std::this_thread::sleep_for(exitLimit);
        Fail("the exit did not finish");
    }).detach();
    if (std::atexit(waiting ? ReportExit : RequireParked) != 0) Fail("atexit registration failed");
    LibcRequestExit_nid_postfix(0);
    LibcAwaitExit_nid_postfix();
}
