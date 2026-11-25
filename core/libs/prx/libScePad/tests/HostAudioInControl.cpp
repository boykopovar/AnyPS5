#include "prx/libSceAudioIn/AudioInCapture.hpp"
#include "prx/libScePad/include/HostAudioInControl.hpp"
#include "prx/libScePad/include/HostVolumeControl.hpp"
#include "prx/libScePad/include/Pad.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

extern "C" int APS5_VABI libScePadUnknown_oARLr0ca9N8_nid_no_patch(int handle, std::uint8_t* out);

static void Require(bool value) {
    if (!value) std::abort();
}

static void SetEnvVar(const char* name, const char* value) {
#ifdef _WIN32
    Require(_putenv_s(name, value != nullptr ? value : "") == 0);
#else
    if (value != nullptr) {
        Require(setenv(name, value, 1) == 0);
    } else {
        Require(unsetenv(name) == 0);
    }
#endif
}

struct EnvVarGuard {
    const char* name;
    std::optional<std::string> previous;

    explicit EnvVarGuard(const char* envName) : name(envName) {
        const char* current = std::getenv(name);
        if (current != nullptr) {
            previous = current;
        }
    }

    ~EnvVarGuard() {
        SetEnvVar(name, previous.has_value() ? previous->c_str() : nullptr);
    }
};

static void TestFocusMuteState() {
    HostVolumeConfigure_nid_no_patch(nullptr, nullptr);
    HostAudioInConfigure_nid_no_patch(nullptr);
    std::uint64_t state = HostAudioInMuteState_nid_no_patch();
    Require((state & 1u) == 0);
    const std::uint64_t initialGeneration = state >> 1;

    HostAudioInConfigure_nid_no_patch("1");
    state = HostAudioInMuteState_nid_no_patch();
    Require((state & 1u) == 0 && (state >> 1) == initialGeneration);

    HostVolumeFocus_nid_no_patch(false, nullptr);
    state = HostAudioInMuteState_nid_no_patch();
    Require((state & 1u) != 0 && (state >> 1) == initialGeneration + 1);

    HostVolumeFocus_nid_no_patch(false, nullptr);
    Require(HostAudioInMuteState_nid_no_patch() == state);

    HostVolumeFocus_nid_no_patch(true, nullptr);
    state = HostAudioInMuteState_nid_no_patch();
    Require((state & 1u) == 0 && (state >> 1) == initialGeneration + 2);

    HostVolumeFocus_nid_no_patch(false, nullptr);
    state = HostAudioInMuteState_nid_no_patch();
    Require((state & 1u) != 0 && (state >> 1) == initialGeneration + 3);
    HostAudioInConfigure_nid_no_patch("0");
    state = HostAudioInMuteState_nid_no_patch();
    Require((state & 1u) == 0 && (state >> 1) == initialGeneration + 4);

    bool rejected = false;
    try {
        HostAudioInConfigure_nid_no_patch("true");
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    Require(rejected && HostAudioInMuteState_nid_no_patch() == state);

    rejected = false;
    try {
        HostAudioInConfigure_nid_no_patch("");
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    Require(rejected && HostAudioInMuteState_nid_no_patch() == state);
    HostVolumeFocus_nid_no_patch(true, nullptr);
    HostAudioInConfigure_nid_no_patch(nullptr);
}

static void TestAudioMuteTracksFocusIndependentlyFromHostVolume() {
    HostVolumeConfigure_nid_no_patch("100", "1");
    HostAudioInConfigure_nid_no_patch("0");
    const std::uint64_t initial = HostAudioInMuteState_nid_no_patch();

    HostVolumeApply_nid_no_patch(HostVolumeCommand::ToggleMute, nullptr);
    Require(HostAudioInMuteState_nid_no_patch() == initial);

    HostAudioInConfigure_nid_no_patch("1");
    const std::uint64_t enabled = HostAudioInMuteState_nid_no_patch();
    Require((enabled & 1u) == 0 && (enabled >> 1) == (initial >> 1));

    HostVolumeFocus_nid_no_patch(false, nullptr);
    const std::uint64_t unfocused = HostAudioInMuteState_nid_no_patch();
    Require((unfocused & 1u) != 0 && (unfocused >> 1) == (enabled >> 1) + 1);

    HostVolumeApply_nid_no_patch(HostVolumeCommand::ToggleMute, nullptr);
    Require(HostAudioInMuteState_nid_no_patch() == unfocused);

    HostVolumeFocus_nid_no_patch(true, nullptr);
    const std::uint64_t focused = HostAudioInMuteState_nid_no_patch();
    Require((focused & 1u) == 0 && (focused >> 1) == (unfocused >> 1) + 1);

    HostAudioInConfigure_nid_no_patch(nullptr);
    HostVolumeConfigure_nid_no_patch(nullptr, nullptr);
}

static void TestConfigureFromEnvironmentAtomicity() {
    EnvVarGuard volumeGuard("ANYPS5_VOLUME");
    EnvVarGuard muteGuard("ANYPS5_MUTE_ON_FOCUS_LOSS");
    EnvVarGuard audioInGuard("ANYPS5_AUDIO_IN_MUTE_ON_FOCUS_LOSS");

    HostVolumeConfigure_nid_no_patch("40", "1");
    HostAudioInConfigure_nid_no_patch("1");
    HostVolumeFocus_nid_no_patch(false, nullptr);

    const std::uint32_t initialGain = HostVolumeGain_nid_no_patch();
    const bool initialFocus = HostVolumeFocused_nid_no_patch();
    const std::uint64_t initialAudioInMuteState = HostAudioInMuteState_nid_no_patch();
    Require(!initialFocus);
    Require((initialAudioInMuteState & 1u) != 0);

    SetEnvVar("ANYPS5_VOLUME", "80");
    SetEnvVar("ANYPS5_MUTE_ON_FOCUS_LOSS", "0");
    SetEnvVar("ANYPS5_AUDIO_IN_MUTE_ON_FOCUS_LOSS", "invalid");

    bool threw = false;
    try {
        HostVolumeConfigureFromEnvironment_nid_no_patch();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
    Require(HostVolumeGain_nid_no_patch() == initialGain);
    Require(HostVolumeFocused_nid_no_patch() == initialFocus);
    Require(HostAudioInMuteState_nid_no_patch() == initialAudioInMuteState);

    SetEnvVar("ANYPS5_AUDIO_IN_MUTE_ON_FOCUS_LOSS", "1");
    HostVolumeConfigureFromEnvironment_nid_no_patch();

    const std::uint32_t gain80 = HostVolumeGain_nid_no_patch();
    const std::uint64_t configuredAudioInMuteState = HostAudioInMuteState_nid_no_patch();
    Require(gain80 > 0 && gain80 < HostVolume::UnityGain);
    Require(HostVolumeFocused_nid_no_patch());
    Require((configuredAudioInMuteState & 1u) == 0);
    Require((configuredAudioInMuteState >> 1) == (initialAudioInMuteState >> 1) + 1);

    HostVolumeFocus_nid_no_patch(false, nullptr);
    Require(HostVolumeGain_nid_no_patch() == gain80);
    Require(!HostVolumeFocused_nid_no_patch());
    const std::uint64_t unfocusedAudioInMuteState = HostAudioInMuteState_nid_no_patch();
    Require((unfocusedAudioInMuteState & 1u) != 0);
    Require((unfocusedAudioInMuteState >> 1) == (configuredAudioInMuteState >> 1) + 1);

    HostVolumeFocus_nid_no_patch(true, nullptr);
    HostVolumeConfigure_nid_no_patch(nullptr, nullptr);
    HostAudioInConfigure_nid_no_patch(nullptr);
}

static void TestCapturedSampleFilter() {
    std::array<std::uint8_t, 4> samples{0x11, 0x22, 0x33, 0x44};
    Require(audioin::detail::filterCapturedSamples(samples.data(), samples.size(), 2, false) == 2);
    Require(samples == std::array<std::uint8_t, 4>{0x11, 0x22, 0, 0});

    samples = {0x11, 0x22, 0x33, 0x44};
    Require(audioin::detail::filterCapturedSamples(samples.data(), samples.size(), samples.size(), true) == 0);
    Require(samples == std::array<std::uint8_t, 4>{0, 0, 0, 0});

    samples = {0x11, 0x22, 0x33, 0x44};
    Require(audioin::detail::filterCapturedSamples(samples.data(), samples.size(), samples.size() + 1, false) == samples.size());
    Require(samples == std::array<std::uint8_t, 4>{0x11, 0x22, 0x33, 0x44});
}

static void TestDiscardQueuedAudioBelowThreshold() {
    std::array<std::uint8_t, 8> discarded{};
    std::size_t calls = 0;
    audioin::detail::discardQueuedAudio(0, discarded.data(), discarded.size(),
        [&calls](std::uint8_t*, std::uint32_t) {
            ++calls;
            return 0u;
        });
    Require(calls == 0);
}

static void TestDiscardQueuedAudioInChunks() {
    std::array<std::uint8_t, 4> discarded{};
    std::array<std::uint32_t, 3> requested{};
    std::size_t calls = 0;
    audioin::detail::discardQueuedAudio(10, discarded.data(), discarded.size(),
        [&requested, &calls](std::uint8_t*, std::uint32_t chunk) {
            requested[calls++] = chunk;
            return chunk;
        });
    Require(calls == 3);
    Require(requested == std::array<std::uint32_t, 3>{4, 4, 2});
}

static void TestDiscardQueuedAudioContinuesAfterPartialRead() {
    std::array<std::uint8_t, 4> discarded{};
    std::array<std::uint32_t, 3> requested{};
    std::size_t calls = 0;
    audioin::detail::discardQueuedAudio(10, discarded.data(), discarded.size(),
        [&requested, &calls](std::uint8_t*, std::uint32_t chunk) {
            requested[calls++] = chunk;
            return calls == 1 ? 2u : chunk;
        });
    Require(calls == 3);
    Require(requested == std::array<std::uint32_t, 3>{4, 4, 4});
}

static void TestDiscardQueuedAudioStopsOnNoProgress() {
    std::array<std::uint8_t, 4> discarded{};
    std::size_t calls = 0;
    audioin::detail::discardQueuedAudio(10, discarded.data(), discarded.size(),
        [&calls](std::uint8_t*, std::uint32_t) {
            ++calls;
            return 0u;
        });
    Require(calls == 1);
}

static void TestCaptureMuteGenerations() {
    audioin::detail::CaptureMuteGenerations generations;
    Require(!generations.Observe(4, 8));
    Require(!generations.Observe(4, 8));
    Require(generations.Observe(4, 9));
    Require(!generations.Observe(4, 9));
    Require(!generations.Observe(5, 9));
    Require(generations.Observe(5, 10));
    Require(!generations.Observe(4, 9));
    Require(generations.Observe(4, 10));
    generations.Forget(4);
    Require(!generations.Observe(4, 10));
    Require(!generations.Observe(5, 10));
}

static void TestPadUnknownOarlr0ca9n8() {
    std::uint8_t out = 0xAA;
    Require(libScePadUnknown_oARLr0ca9N8_nid_no_patch(0, nullptr) == PAD_ERROR_NOT_INITIALIZED);
    Require(libScePadUnknown_oARLr0ca9N8_nid_no_patch(1, &out) == PAD_ERROR_NOT_INITIALIZED && out == 0xAA);

    Require(scePadInit_nid_postfix() == PAD_OK);

    Require(libScePadUnknown_oARLr0ca9N8_nid_no_patch(0, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(libScePadUnknown_oARLr0ca9N8_nid_no_patch(1, nullptr) == PAD_ERROR_INVALID_ARG);

    out = 0xAA;
    Require(libScePadUnknown_oARLr0ca9N8_nid_no_patch(0, &out) == PAD_ERROR_INVALID_HANDLE && out == 0xAA);
    Require(libScePadUnknown_oARLr0ca9N8_nid_no_patch(-1, &out) == PAD_ERROR_INVALID_HANDLE && out == 0xAA);
    Require(libScePadUnknown_oARLr0ca9N8_nid_no_patch(12345, &out) == PAD_ERROR_INVALID_HANDLE && out == 0xAA);

    const int standardHandle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(standardHandle > 0);
    const int specialHandle = scePadOpen_nid_postfix(200, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(specialHandle > 0);

    errno = 71;
    Require(libScePadUnknown_oARLr0ca9N8_nid_no_patch(standardHandle, reinterpret_cast<std::uint8_t*>(0x10)) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);

#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const std::size_t pageSize = info.dwPageSize;
    void* page = VirtualAlloc(nullptr, pageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READONLY);
    Require(page != nullptr);
#else
    const long size = sysconf(_SC_PAGESIZE);
    Require(size > 0);
    const std::size_t pageSize = static_cast<std::size_t>(size);
    void* page = mmap(nullptr, pageSize, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(page != MAP_FAILED);
#endif

    Require(libScePadUnknown_oARLr0ca9N8_nid_no_patch(standardHandle, static_cast<std::uint8_t*>(page)) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);

#ifdef _WIN32
    Require(VirtualFree(page, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(page, pageSize) == 0);
#endif

    out = 0xAA;
    Require(libScePadUnknown_oARLr0ca9N8_nid_no_patch(standardHandle, &out) == PAD_OK && out == 0);
    out = 0xAA;
    Require(libScePadUnknown_oARLr0ca9N8_nid_no_patch(specialHandle, &out) == PAD_OK && out == 0);

    Require(scePadClose_nid_postfix(standardHandle) == PAD_OK);
    out = 0xAA;
    Require(libScePadUnknown_oARLr0ca9N8_nid_no_patch(standardHandle, &out) == PAD_ERROR_INVALID_HANDLE && out == 0xAA);

    Require(scePadClose_nid_postfix(specialHandle) == PAD_OK);
    out = 0xAA;
    Require(libScePadUnknown_oARLr0ca9N8_nid_no_patch(specialHandle, &out) == PAD_ERROR_INVALID_HANDLE && out == 0xAA);
}

int main() {
    TestFocusMuteState();
    TestCapturedSampleFilter();
    TestAudioMuteTracksFocusIndependentlyFromHostVolume();
    TestConfigureFromEnvironmentAtomicity();
    TestCaptureMuteGenerations();
    TestDiscardQueuedAudioBelowThreshold();
    TestDiscardQueuedAudioInChunks();
    TestDiscardQueuedAudioContinuesAfterPartialRead();
    TestDiscardQueuedAudioStopsOnNoProgress();
    TestPadUnknownOarlr0ca9n8();
    return 0;
}
