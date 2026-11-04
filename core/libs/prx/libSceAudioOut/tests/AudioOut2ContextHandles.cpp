#include "prx/libSceAudioOut/libSceAudioOut.hpp"
#include "SceTypes.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <random>
#include <string>

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

static void SetEnvironment(const char* name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

namespace {

constexpr int errorInvalidHandle = static_cast<int>(0x80260503);
constexpr std::uint32_t formatStereoFloat = 2u << 8;
constexpr AudioOut2PortHandle portSentinel = 0x6B6B;

AudioOut2ContextHandle CreateContext() {
    AudioOut2ContextParam params{};
    Require(sceAudioOut2ContextResetParam(&params) == 0, "the context parameters did not reset");
    AudioOut2ContextHandle context = 0;
    Require(sceAudioOut2ContextCreate(&params, nullptr, 0, &context) == 0 && context != 0, "a context was not created");
    return context;
}

int CreatePort(AudioOut2ContextHandle context, AudioOut2PortHandle* port) {
    AudioOut2PortParam params{};
    params.data_format = formatStereoFloat;
    params.sampling_freq = 48000;
    return sceAudioOut2PortCreate(context, &params, port);
}

void RequireInvalidContext(AudioOut2ContextHandle context) {
    AudioOut2PortHandle port = portSentinel;
    Require(CreatePort(context, &port) == errorInvalidHandle, "a port was created on a context that does not exist");
    Require(port == portSentinel, "a refused port creation wrote a handle");
    Require(sceAudioOut2LoPortCreate(context, nullptr, &port) == errorInvalidHandle, "a low-latency port was created on a context that does not exist");
    std::uint32_t level = 77;
    std::uint32_t available = 78;
    Require(sceAudioOut2ContextGetQueueLevel(context, &level, &available) == errorInvalidHandle, "a context that does not exist reported a queue level");
    Require(level == 77 && available == 78, "a refused queue level query wrote its outputs");
    Require(sceAudioOut2ContextPush(context, 0) == errorInvalidHandle, "a context that does not exist took a push");
    Require(sceAudioOut2ContextPush(context, 1) == errorInvalidHandle, "a context that does not exist took a blocking push");
    Require(sceAudioOut2ContextAdvance(context) == errorInvalidHandle, "a context that does not exist advanced");
    Require(sceAudioOut2ContextSetAttributes(context, nullptr, 0) == errorInvalidHandle, "a context that does not exist took attributes");
    Require(sceAudioOut2ContextDestroy(context) == errorInvalidHandle, "a context that does not exist was destroyed");
    Require(sceAudioOut2LoContextDestroy(context) == errorInvalidHandle, "a context that does not exist was destroyed through the low-latency call");
}

void DestroyedContextIsInvalid() {
    const auto context = CreateContext();
    AudioOut2PortHandle port = 0;
    Require(CreatePort(context, &port) == 0, "a port was not created on a live context");
    Require(sceAudioOut2ContextDestroy(context) == 0, "a live context was not destroyed");
    Require(sceAudioOut2PortDestroy(port) == errorInvalidHandle, "a port outlived its destroyed context");
    RequireInvalidContext(context);
}

void UncreatedContextIsInvalid() {
    const auto live = CreateContext();
    RequireInvalidContext(live + 8);
    std::uint64_t notAContext = 0;
    RequireInvalidContext(reinterpret_cast<AudioOut2ContextHandle>(&notAContext));
    std::uint32_t level = 99;
    std::uint32_t available = 99;
    Require(sceAudioOut2ContextGetQueueLevel(live, &level, &available) == 0 && level + available == 1, "a live context lost its queue level");
    Require(sceAudioOut2ContextDestroy(live) == 0, "a live context was not destroyed");
}

}

int main() {
    const auto path = std::filesystem::temp_directory_path() / ("anyps5_audio_out2_context_handles-" + std::to_string(std::random_device{}()) + ".raw");
    std::filesystem::remove(path);
    SetEnvironment("SDL_AUDIODRIVER", "disk");
    SetEnvironment("SDL_DISKAUDIOFILE", path.string());
    UncreatedContextIsInvalid();
    DestroyedContextIsInvalid();
    std::filesystem::remove(path);
    std::puts("audio out2 context handles: PASS");
    return 0;
}
