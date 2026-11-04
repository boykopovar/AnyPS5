#include "prx/libScePad/include/HostVolumeControl.hpp"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void setEnvironment(const char* name, const char* value) {
#ifdef _WIN32
    require(_putenv_s(name, value != nullptr ? value : "") == 0, "set the environment variable");
#else
    if (value != nullptr) require(::setenv(name, value, 1) == 0, "set the environment variable");
    else require(::unsetenv(name) == 0, "unset the environment variable");
#endif
}

bool refusesNaming(const char* volume, const char* flag, const char* name) {
    try {
        HostVolumeConfigure_nid_no_patch(volume, flag);
    } catch (const std::runtime_error& error) {
        return std::string(error.what()).find(name) != std::string::npos;
    }
    return false;
}

void defaultIsUnity() {
    HostVolumeConfigure_nid_no_patch(nullptr, nullptr);
    require(HostVolumeGain_nid_no_patch() == HostVolume::UnityGain, "the shared gain starts at exactly unity");
}

void commandsPublishTheGain() {
    HostVolumeConfigure_nid_no_patch(nullptr, nullptr);
    HostVolume after;
    require(!HostVolumeApply_nid_no_patch(HostVolumeCommand::VolumeUp, &after), "a step up at 100 changes nothing");
    require(HostVolumeGain_nid_no_patch() == HostVolume::UnityGain, "a step that changes nothing keeps unity");
    require(HostVolumeApply_nid_no_patch(HostVolumeCommand::VolumeDown, &after), "a step down changes the volume");
    require(after.Volume() == 95 && HostVolumeGain_nid_no_patch() == after.Gain(), "the gain follows the volume");
    require(HostVolumeApply_nid_no_patch(HostVolumeCommand::ToggleMute, &after), "mute changes the state");
    require(after.Muted() && HostVolumeGain_nid_no_patch() == 0, "mute publishes a zero gain");
    require(HostVolumeApply_nid_no_patch(HostVolumeCommand::ToggleMute, nullptr), "unmute works without an out parameter");
    HostVolumeApply_nid_no_patch(HostVolumeCommand::VolumeUp, &after);
    require(HostVolumeGain_nid_no_patch() == HostVolume::UnityGain, "stepping back to 100 returns to exactly unity");
}

void focusOnlyActsWhenEnabled() {
    HostVolumeConfigure_nid_no_patch(nullptr, nullptr);
    HostVolume after;
    require(!HostVolumeFocus_nid_no_patch(false, &after), "losing focus changes nothing by default");
    require(HostVolumeGain_nid_no_patch() == HostVolume::UnityGain, "the gain stays at unity without the option");
    HostVolumeFocus_nid_no_patch(true, nullptr);
    HostVolumeConfigure_nid_no_patch(nullptr, "1");
    require(HostVolumeGain_nid_no_patch() == HostVolume::UnityGain, "the option alone does not mute a focused window");
    require(HostVolumeFocus_nid_no_patch(false, &after), "losing focus with the option on changes the state");
    require(after.FocusMuted() && HostVolumeGain_nid_no_patch() == 0, "a window without focus is silent");
    require(HostVolumeFocus_nid_no_patch(true, &after), "gaining focus changes the state");
    require(!after.FocusMuted() && HostVolumeGain_nid_no_patch() == HostVolume::UnityGain, "gaining focus restores the gain");
}

void configureTakesInitialValues() {
    HostVolumeConfigure_nid_no_patch("40", "0");
    HostVolume after;
    HostVolumeApply_nid_no_patch(HostVolumeCommand::VolumeUp, &after);
    require(after.Volume() == 45, "the initial volume is the one configured");
    require(!after.MuteOnFocusLoss(), "the flag 0 leaves the option off");
    HostVolumeConfigure_nid_no_patch("0", nullptr);
    require(HostVolumeGain_nid_no_patch() == 0, "an initial volume of 0 is silent");
    HostVolumeConfigure_nid_no_patch(nullptr, nullptr);
    require(HostVolumeGain_nid_no_patch() == HostVolume::UnityGain, "configuring without values returns to the default");
}

void invalidValuesLeaveTheStateAlone() {
    HostVolumeConfigure_nid_no_patch("30", nullptr);
    const auto gain = HostVolumeGain_nid_no_patch();
    require(refusesNaming("101", nullptr, "ANYPS5_VOLUME"), "an out-of-range volume is refused and named");
    require(refusesNaming("loud", nullptr, "ANYPS5_VOLUME"), "a non-numeric volume is refused and named");
    require(refusesNaming(nullptr, "yes", "ANYPS5_MUTE_ON_FOCUS_LOSS"), "an invalid flag is refused and named");
    require(refusesNaming("50", "2", "ANYPS5_MUTE_ON_FOCUS_LOSS"), "an invalid flag next to a valid volume is refused and named");
    require(HostVolumeGain_nid_no_patch() == gain, "a refused configuration leaves the gain as it was");
    HostVolume after;
    HostVolumeApply_nid_no_patch(HostVolumeCommand::VolumeUp, &after);
    require(after.Volume() == 35, "a refused configuration leaves the volume as it was");
    HostVolumeConfigure_nid_no_patch(nullptr, nullptr);
}

void environmentIsRead() {
    setEnvironment("ANYPS5_VOLUME", nullptr);
    setEnvironment("ANYPS5_MUTE_ON_FOCUS_LOSS", nullptr);
    HostVolumeConfigureFromEnvironment_nid_no_patch();
    require(HostVolumeGain_nid_no_patch() == HostVolume::UnityGain, "unset variables keep the default");
    HostVolume after;
    HostVolumeFocus_nid_no_patch(false, &after);
    require(!after.FocusMuted(), "an unset ANYPS5_MUTE_ON_FOCUS_LOSS leaves the option off");
    HostVolumeFocus_nid_no_patch(true, nullptr);
    setEnvironment("ANYPS5_VOLUME", "60");
    setEnvironment("ANYPS5_MUTE_ON_FOCUS_LOSS", "1");
    HostVolumeConfigureFromEnvironment_nid_no_patch();
    HostVolumeFocus_nid_no_patch(false, &after);
    require(after.FocusMuted() && after.Volume() == 60, "both variables are applied");
    setEnvironment("ANYPS5_MUTE_ON_FOCUS_LOSS", "maybe");
    bool refused = false;
    try {
        HostVolumeConfigureFromEnvironment_nid_no_patch();
    } catch (const std::runtime_error& error) {
        refused = std::string(error.what()).find("ANYPS5_MUTE_ON_FOCUS_LOSS") != std::string::npos;
    }
    require(refused, "an invalid variable from the environment is refused and named");
    setEnvironment("ANYPS5_VOLUME", nullptr);
    setEnvironment("ANYPS5_MUTE_ON_FOCUS_LOSS", nullptr);
    HostVolumeConfigureFromEnvironment_nid_no_patch();
}

void readersSeeOnlyValidGains() {
    HostVolumeConfigure_nid_no_patch(nullptr, nullptr);
    std::atomic<bool> stop{false};
    std::atomic<bool> bad{false};
    std::thread reader([&] {
        while (!stop.load()) {
            const auto gain = HostVolumeGain_nid_no_patch();
            if (gain > HostVolume::UnityGain) bad.store(true);
        }
    });
    for (int round = 0; round < 20000; ++round) {
        HostVolumeApply_nid_no_patch(round % 3 == 0 ? HostVolumeCommand::ToggleMute : round % 3 == 1 ? HostVolumeCommand::VolumeDown : HostVolumeCommand::VolumeUp, nullptr);
    }
    stop.store(true);
    reader.join();
    require(!bad.load(), "a reader never sees a gain above unity while commands run");
}

}

int main() try {
    defaultIsUnity();
    commandsPublishTheGain();
    focusOnlyActsWhenEnabled();
    configureTakesInitialValues();
    invalidValuesLeaveTheStateAlone();
    environmentIsRead();
    readersSeeOnlyValidGains();
    std::puts("host volume state tests passed");
    return 0;
} catch (const std::exception& error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
}
