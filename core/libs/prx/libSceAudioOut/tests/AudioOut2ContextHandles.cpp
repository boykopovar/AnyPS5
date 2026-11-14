#include "prx/libSceAudioOut/libSceAudioOut.hpp"
#include "SceTypes.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

constexpr std::size_t testPageSize = 4096;

void* AllocatePage(bool readable) {
#ifdef _WIN32
    return VirtualAlloc(nullptr, testPageSize, MEM_COMMIT | MEM_RESERVE, readable ? PAGE_READONLY : PAGE_NOACCESS);
#else
    void* page = mmap(nullptr, testPageSize, readable ? PROT_READ : PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return page == MAP_FAILED ? nullptr : page;
#endif
}

void FreePage(void* page) {
#ifdef _WIN32
    VirtualFree(page, 0, MEM_RELEASE);
#else
    munmap(page, testPageSize);
#endif
}


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
    Require(params.max_ports == 8 && params.queue_depth == 1 && params.num_grains == 256 && params.max_object_ports == 0 && params.guarantee_object_ports == 0, "a context parameter default did not match the console");
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

void ContextAttributesNullList() {
    const auto context = CreateContext();
    Require(sceAudioOut2ContextSetAttributes(context, nullptr, 0) == static_cast<int>(0x80268001), "an empty null attribute list was not refused with the console code");
    Require(sceAudioOut2ContextSetAttributes(context, nullptr, 1) == static_cast<int>(0x80268001), "a null attribute list with a count was not refused with the console code");
    Require(sceAudioOut2ContextDestroy(context) == 0, "a live context was not destroyed");
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

void QueueLevelRejectsUnwritableOutputs() {
    const auto context = CreateContext();
    auto* const invalidOutput = reinterpret_cast<std::uint32_t*>(0x10);
    std::uint32_t level = 77;
    std::uint32_t available = 78;
    errno = 73;
    Require(sceAudioOut2ContextGetQueueLevel(context, invalidOutput, &available) == static_cast<int>(0x80260502), "an unwritable queue-level output was accepted");
    Require(available == 78 && errno == 73, "a failed queue-level copy changed the other output or errno");
    Require(sceAudioOut2ContextGetQueueLevel(context, &level, invalidOutput) == static_cast<int>(0x80260502), "an unwritable available-queue output was accepted");
    Require(level <= 1 && errno == 73, "a failed available-queue copy lost the first output or changed errno");
    Require(sceAudioOut2LoContextGetQueueLevel(context, invalidOutput, nullptr) == static_cast<int>(0x80260502), "the low-latency wrapper accepted an unwritable queue-level output");
    Require(errno == 73, "the low-latency wrapper changed errno");
    Require(sceAudioOut2ContextGetQueueLevel(context, &level, &available) == 0 && level + available == 1, "a valid queue query failed after rejected outputs");
    Require(sceAudioOut2ContextDestroy(context) == 0, "the queue-query context was not destroyed");
}

void ContextMemoryNullSize() {
    AudioOut2ContextParam params{};
    Require(sceAudioOut2ContextResetParam(&params) == 0, "the context parameters did not reset");
    Require(sceAudioOut2ContextQueryMemory(&params, nullptr) == static_cast<int>(0x80268001), "a null size pointer was not refused with the console code");
}

void ContextMemoryQuery() {
    constexpr std::size_t consoleSizes[3] = {64108, 84492, 107788};
    constexpr std::uint32_t ports[3] = {1, 8, 16};
    for (int i = 0; i < 3; ++i) {
        AudioOut2ContextParam params{};
        Require(sceAudioOut2ContextResetParam(&params) == 0, "the context parameters did not reset");
        params.max_ports = ports[i];
        std::size_t size = 0;
        Require(sceAudioOut2ContextQueryMemory(&params, &size) == 0 && size == consoleSizes[i], "the context memory is not the size the console asks for");
        params.queue_depth = 8;
        params.num_grains = 1024;
        size = 0;
        Require(sceAudioOut2ContextQueryMemory(&params, &size) == 0 && size == consoleSizes[i], "the queue depth or the grain count changed the context memory");
    }
}

void FirmwareExSignatures() {
    constexpr int errorInvalidParam = static_cast<int>(0x80268001);
    constexpr int errorInvalidArgument = static_cast<int>(0x80260502);
    constexpr int errorInvalidGrains = static_cast<int>(0x8026800d);
    constexpr int errorNotInitialized = static_cast<int>(0x80268006);

    Require(sceAudioOut2GetSystemStateEx(nullptr) == errorNotInitialized, "a system state read ran before initialization");
    Require(sceAudioOut2Terminate() == 0, "terminate with no contexts did not return 0");
    Require(sceAudioOut2GetSystemStateEx(nullptr) == errorNotInitialized, "terminate cleared the initialized flag");
    Require(sceAudioOut2LoInit() == 0, "the low-latency library did not initialize");
    Require(sceAudioOut2ContextBedWrite(0, 0, 0, 0, 0, 0, 0) == errorInvalidParam, "a bed write on a dead context was not refused");

    AudioOut2UserHandle user = 0;
    Require(sceAudioOut2UserCreateEx(0x10000000, 0, &user) == 0 && user != 0, "a user was not created through the ex call");
    std::uint32_t contextAttributes = 1;
    std::uint32_t portAttributes = 1;
    Require(sceAudioOut2UserGetSupportedAttributes(user, &contextAttributes, &portAttributes) == 0, "a user made by the ex call was not known");
    Require(sceAudioOut2UserDestroy(user) == 0, "a user made by the ex call was not destroyed");

    AudioOut2ContextParam params{};
    Require(sceAudioOut2ContextResetParam(&params) == 0, "the context parameters did not reset");
    std::size_t size = 0;
    Require(sceAudioOut2LoContextQueryMemory(&params, &size) == 0 && size == 0x5b1c, "the low-latency memory query returned the wrong size");
    size = 0;
    Require(sceAudioOut2LoContextQueryMemoryEx(&params, nullptr, &size) == 0 && size == 0x5b1c, "the extended memory query returned the wrong size");
    Require(sceAudioOut2LoContextQueryMemory(nullptr, &size) == errorInvalidParam, "a null-parameter memory query was not refused");
    params.max_object_ports = 0x81;
    Require(sceAudioOut2LoContextQueryMemoryEx(&params, nullptr, &size) == errorInvalidParam, "an oversized object port count was accepted");
    params.max_object_ports = 0;
    params.max_ports = 0x21;
    Require(sceAudioOut2LoContextQueryMemoryEx(&params, nullptr, &size) == errorInvalidParam, "an oversized port count was accepted");
    params.max_ports = 8;
    params.num_grains = 0x180;
    Require(sceAudioOut2LoContextQueryMemoryEx(&params, nullptr, &size) == errorInvalidGrains, "a misaligned grain count was accepted");
    params.num_grains = 0x80;
    Require(sceAudioOut2LoContextQueryMemoryEx(&params, nullptr, &size) == errorInvalidGrains, "an undersized grain count was accepted");
    params.num_grains = 256;
    params.queue_depth = 0x11;
    Require(sceAudioOut2LoContextQueryMemoryEx(&params, nullptr, &size) == errorInvalidParam, "an oversized queue depth was accepted");
    const std::uint32_t extension[2] = {0, 0};
    Require(sceAudioOut2LoContextQueryMemoryEx(&params, extension, &size) == 0, "an extension-relaxed queue depth was refused");
    params.queue_depth = 0x41;
    Require(sceAudioOut2LoContextQueryMemoryEx(&params, extension, &size) == errorInvalidParam, "a queue depth above the extended bound was accepted");
    params.queue_depth = 1;
    const std::uint32_t badExtension[2] = {0x16, 0};
    Require(sceAudioOut2LoContextQueryMemoryEx(&params, badExtension, &size) == errorInvalidParam, "an oversized extension count was accepted");

    AudioOut2ContextHandle context = 0;
    Require(sceAudioOut2LoContextCreate(&params, nullptr, 0, &context) == errorInvalidParam, "a context was created over a null buffer");
    Require(sceAudioOut2LoContextCreateEx(nullptr, nullptr, &context, 0, &context) == errorInvalidParam, "a context was created over null parameters");
    Require(sceAudioOut2LoContextCreateEx(&params, nullptr, nullptr, 0, &context) == errorInvalidParam, "a context was created over a null buffer through the ex call");
    Require(sceAudioOut2LoContextCreateEx(&params, nullptr, &context, 0, nullptr) == errorInvalidParam, "a context was created over a null output");
    context = 0;
    Require(sceAudioOut2LoContextCreateEx(&params, nullptr, &context, 1, &context) == errorInvalidParam && context == static_cast<AudioOut2ContextHandle>(-1), "a short buffer was not refused with the sentinel handle");
    const std::uint32_t boundExtension[2] = {0, 1};
    Require(sceAudioOut2LoContextCreateEx(&params, boundExtension, &context, 0x20000, &context) == errorInvalidParam, "an extension over the object port bound was accepted");
    std::vector<std::byte> buffer(0x20000);
    Require(sceAudioOut2LoContextCreateEx(&params, extension, buffer.data(), buffer.size(), &context) == 0 && context != 0 && context != static_cast<AudioOut2ContextHandle>(-1), "a context was not created through the ex call");

    AudioOut2PortHandle port = 0;
    Require(CreatePort(context, &port) == 0, "a port was not created on the extended context");
    AudioOut2PortHandle exPort = 0;
    Require(sceAudioOut2PortCreateEx(context, nullptr, &exPort) == errorInvalidArgument, "a port was created through the ex call over null parameters");
    AudioOut2PortParam portParams{};
    portParams.data_format = formatStereoFloat;
    portParams.sampling_freq = 48000;
    Require(sceAudioOut2LoPortCreateEx(context, &portParams, &exPort) == 0 && exPort != 0, "a port was not created through the low-latency ex call");

    float volume[2] = {0.5f, 0.25f};
    AudioOut2Attribute setVolume{};
    setVolume.attribute_id = 1;
    setVolume.value = volume;
    setVolume.value_size = sizeof(volume);
    Require(sceAudioOut2LoPortSetAttributesEx(exPort, &setVolume, 1) == 0, "the extended attribute set was refused");

    float readBack[2] = {0.0f, 0.0f};
    AudioOut2Attribute getVolume{};
    getVolume.attribute_id = 1;
    getVolume.value = readBack;
    getVolume.value_size = sizeof(readBack);
    Require(sceAudioOut2LoPortGetAttributes(exPort, &getVolume, 1) == 0 && readBack[0] == 0.5f && readBack[1] == 0.25f, "the volume attribute did not round trip");
    auto* unreadableAttributes = reinterpret_cast<AudioOut2Attribute*>(0x10);
    errno = 73;
    Require(sceAudioOut2LoPortGetAttributes(exPort, unreadableAttributes, 1) == errorInvalidParam, "an unreadable port attribute record was accepted");
    Require(errno == 73, "an unreadable port attribute record changed host errno");
    Require(sceAudioOut2LoPortGetAttributes(portSentinel, unreadableAttributes, 1) == errorInvalidHandle, "an unreadable port attribute record hid an invalid handle");
    Require(sceAudioOut2LoPortGetAttributes(exPort, unreadableAttributes, 0) == 0, "an empty port attribute read touched its pointer");
    Require(sceAudioOut2LoPortGetAttributes(exPort, nullptr, 1) == errorInvalidParam, "a null port attribute record was accepted");
    void* readOnlyValue = AllocatePage(true);
    Require(readOnlyValue != nullptr, "a read-only port attribute output page was not mapped");
    getVolume.value = readOnlyValue;
    errno = 73;
    Require(sceAudioOut2LoPortGetAttributes(exPort, &getVolume, 1) == errorInvalidParam, "an unwritable port attribute value was accepted");
    Require(errno == 73, "an unwritable port attribute value changed host errno");
    FreePage(readOnlyValue);
    getVolume.value = readBack;
    volume[0] = 0.75f;
    volume[1] = 0.125f;
    Require(sceAudioOut2PortSetAttributesEx(exPort, &setVolume, 1) == 0, "the attribute set through sceAudioOut2PortSetAttributesEx was refused");
    Require(sceAudioOut2LoPortGetAttributes(exPort, &getVolume, 1) == 0 && readBack[0] == 0.75f && readBack[1] == 0.125f, "the volume set through sceAudioOut2PortSetAttributesEx did not round trip");
    Require(sceAudioOut2PortSetAttributesEx(portSentinel, &setVolume, 1) == errorInvalidHandle, "sceAudioOut2PortSetAttributesEx accepted a dead port");
    AudioOut2Attribute badAttribute{};
    badAttribute.attribute_id = 5;
    badAttribute.value = readBack;
    badAttribute.value_size = sizeof(readBack);
    Require(sceAudioOut2LoPortGetAttributes(exPort, &badAttribute, 1) == errorInvalidParam, "an unreadable port attribute was accepted");
    badAttribute.attribute_id = 9;
    Require(sceAudioOut2LoPortGetAttributes(portSentinel, &getVolume, 1) == errorInvalidHandle, "a port attribute read on a dead port was not refused");

    Require(sceAudioOut2LoContextGetAttributes(context, &badAttribute, 0) == 0, "an empty context attribute read was refused");
    Require(sceAudioOut2LoContextGetAttributes(context, &badAttribute, 1) == errorInvalidParam, "an unknown context attribute was accepted");
    badAttribute.attribute_id = 0x1e;
    Require(sceAudioOut2LoContextGetAttributes(context + 8, &badAttribute, 1) == errorInvalidParam, "a context attribute read on a dead context was not refused");

    Require(sceAudioOut2PortDestroy(port) == 0, "a live port was not destroyed");
    Require(sceAudioOut2PortDestroy(exPort) == 0, "a live extended port was not destroyed");
    Require(sceAudioOut2Terminate() == 0, "terminate with a live context did not return 0");
    Require(sceAudioOut2ContextGetQueueLevel(context, nullptr, nullptr) == errorInvalidHandle, "a context survived terminate");
    Require(sceAudioOut2Terminate() == 0, "a repeated terminate did not return 0");
}

}

void BadGuestPointersAreRefused() {
    constexpr int errorInvalidArgument = static_cast<int>(0x80260502);
    AudioOut2ContextParam params{};
    Require(sceAudioOut2ContextResetParam(&params) == 0, "the context parameters did not reset");
    void* unreadable = AllocatePage(false);
    Require(unreadable != nullptr, "an unreadable page was not mapped");
    AudioOut2ContextHandle context = 0x6B6B;
    Require(sceAudioOut2ContextCreate(static_cast<const AudioOut2ContextParam*>(unreadable), nullptr, 0, &context) == errorInvalidArgument, "unreadable parameters were not refused");
    Require(context == 0x6B6B, "a refused context creation wrote a handle");
    FreePage(unreadable);

    void* readOnly = AllocatePage(true);
    Require(readOnly != nullptr, "a read-only page was not mapped");
    Require(sceAudioOut2ContextCreate(&params, nullptr, 0, static_cast<AudioOut2ContextHandle*>(readOnly)) == errorInvalidArgument, "an unwritable handle slot was not refused");
    FreePage(readOnly);

    const auto live = CreateContext();
    Require(sceAudioOut2ContextDestroy(live) == 0, "a context created after the refusals was not destroyed");
}

void OversizedGrainIsRefused() {
    constexpr int errorInvalidArgument = static_cast<int>(0x80260502);
    AudioOut2ContextParam params{};
    Require(sceAudioOut2ContextResetParam(&params) == 0, "the context parameters did not reset");
    constexpr AudioOut2ContextHandle sentinel = 0x6B6B;
    AudioOut2ContextHandle context = sentinel;
    params.num_grains = 0xFFFFFFFFu;
    Require(sceAudioOut2ContextCreate(&params, nullptr, 0, &context) == errorInvalidArgument, "a grain of 4 GiB frames was not refused");
    Require(context == sentinel, "a refused context creation wrote a handle");
    params.num_grains = 12001;
    Require(sceAudioOut2ContextCreate(&params, nullptr, 0, &context) == errorInvalidArgument, "a grain past the queue cap was not refused");
    Require(context == sentinel, "a refused context creation wrote a handle");
    params.num_grains = 12000;
    Require(sceAudioOut2ContextCreate(&params, nullptr, 0, &context) == 0 && context != sentinel, "a grain at the queue cap was refused");
    Require(sceAudioOut2ContextDestroy(context) == 0, "a context at the queue cap was not destroyed");
}

int main() {
    const auto path = std::filesystem::temp_directory_path() / ("anyps5_audio_out2_context_handles-" + std::to_string(std::random_device{}()) + ".raw");
    std::filesystem::remove(path);
    SetEnvironment("SDL_AUDIODRIVER", "disk");
    SetEnvironment("SDL_DISKAUDIOFILE", path.string());
    FirmwareExSignatures();
    ContextMemoryNullSize();
    ContextMemoryQuery();
    OversizedGrainIsRefused();
    BadGuestPointersAreRefused();
    UncreatedContextIsInvalid();
    ContextAttributesNullList();
    QueueLevelRejectsUnwritableOutputs();
    DestroyedContextIsInvalid();
    std::filesystem::remove(path);
    std::puts("audio out2 context handles: PASS");
    return 0;
}
