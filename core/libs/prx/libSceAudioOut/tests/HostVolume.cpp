#include "prx/libScePad/include/HostVolume.hpp"
#include "prx/libScePad/include/HostVolumeSettings.hpp"

#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void defaultState() {
    HostVolume state;
    require(state.Volume() == 100, "the volume starts at 100");
    require(!state.Muted(), "the host volume starts unmuted");
}

void stepsDown() {
    HostVolume state;
    for (int expected = 95; expected >= 0; expected -= 5) {
        require(state.VolumeDown(), "a step down from above 0 changes the volume");
        require(state.Volume() == expected, "each step down removes 5");
    }
    require(state.Volume() == 0, "twenty steps reach 0");
}

void stepsStopAtZero() {
    HostVolume state;
    for (int step = 0; step < 20; ++step) state.VolumeDown();
    require(!state.VolumeDown(), "a step down at 0 reports no change");
    require(state.Volume() == 0, "the volume never goes below 0");
}

void stepsUp() {
    HostVolume state;
    for (int step = 0; step < 20; ++step) state.VolumeDown();
    for (int expected = 5; expected <= 100; expected += 5) {
        require(state.VolumeUp(), "a step up from below 100 changes the volume");
        require(state.Volume() == expected, "each step up adds 5");
    }
}

void stepsStopAtMaximum() {
    HostVolume state;
    require(!state.VolumeUp(), "a step up at 100 reports no change");
    require(state.Volume() == 100, "the volume never goes above 100");
}

void muteKeepsVolume() {
    HostVolume state;
    state.VolumeDown();
    state.VolumeDown();
    require(state.Volume() == 90, "two steps down give 90");
    require(state.ToggleMute(), "muting changes the state");
    require(state.Muted() && state.Volume() == 90, "muting leaves the volume alone");
    require(state.ToggleMute(), "unmuting changes the state");
    require(!state.Muted() && state.Volume() == 90, "unmuting restores the volume that was set");
}

void muteAtZeroAndMaximum() {
    HostVolume state;
    state.ToggleMute();
    require(state.Volume() == 100, "muting at 100 keeps 100");
    state.ToggleMute();
    for (int step = 0; step < 20; ++step) state.VolumeDown();
    state.ToggleMute();
    require(state.Muted() && state.Volume() == 0, "muting at 0 keeps 0");
    state.ToggleMute();
    require(!state.Muted() && state.Volume() == 0, "unmuting at 0 leaves 0");
}

void stepsEndMute() {
    HostVolume state;
    state.VolumeDown();
    state.ToggleMute();
    require(state.VolumeUp(), "a step up while muted changes the state");
    require(!state.Muted() && state.Volume() == 100, "a step up while muted unmutes and moves the volume");
    state.ToggleMute();
    require(state.VolumeDown(), "a step down while muted changes the state");
    require(!state.Muted() && state.Volume() == 95, "a step down while muted unmutes and moves the volume");
}

void stepsAtLimitsStillEndMute() {
    HostVolume top;
    top.ToggleMute();
    require(top.VolumeUp(), "a step up at 100 while muted reports the unmute");
    require(!top.Muted() && top.Volume() == 100, "a step up at 100 while muted only unmutes");
    require(!top.VolumeUp(), "a second step up at 100 reports no change");
    HostVolume bottom;
    for (int step = 0; step < 20; ++step) bottom.VolumeDown();
    bottom.ToggleMute();
    require(bottom.VolumeDown(), "a step down at 0 while muted reports the unmute");
    require(!bottom.Muted() && bottom.Volume() == 0, "a step down at 0 while muted only unmutes");
}

void setVolumeDirectly() {
    HostVolume state;
    require(state.SetVolume(35), "a new volume reports a change");
    require(state.Volume() == 35, "the volume takes the value set");
    require(!state.SetVolume(35), "the same volume reports no change");
    require(state.SetVolume(0) && state.Volume() == 0, "0 is accepted");
    require(state.SetVolume(100) && state.Volume() == 100, "100 is accepted");
    state.ToggleMute();
    state.SetVolume(40);
    require(state.Muted(), "setting the volume leaves the mute alone");
}

void setVolumeRefusesOutOfRange() {
    HostVolume state;
    state.SetVolume(60);
    for (const int value : {-1, 101, 1000, -100}) {
        bool refused = false;
        try {
            state.SetVolume(value);
        } catch (const std::invalid_argument&) {
            refused = true;
        }
        require(refused, "a volume outside 0 to 100 is refused");
        require(state.Volume() == 60, "a refused volume leaves the state as it was");
    }
}

void focusIgnoredByDefault() {
    HostVolume state;
    require(!state.MuteOnFocusLoss(), "muting on focus loss is off by default");
    require(!state.FocusChanged(false), "losing focus changes nothing with the option off");
    require(!state.FocusMuted(), "no focus mute with the option off");
    require(!state.FocusChanged(true), "gaining focus changes nothing with the option off");
}

void focusLossMutesAndGainRestores() {
    HostVolume state;
    state.SetMuteOnFocusLoss(true);
    require(!state.FocusMuted(), "a focused window is not focus muted");
    require(state.FocusChanged(false), "losing focus reports a change");
    require(state.FocusMuted(), "a window without focus is focus muted");
    require(!state.FocusChanged(false), "losing focus twice reports no second change");
    require(state.FocusChanged(true), "gaining focus reports a change");
    require(!state.FocusMuted(), "gaining focus ends the focus mute");
}

void focusMuteKeepsUserState() {
    HostVolume state;
    state.SetMuteOnFocusLoss(true);
    state.VolumeDown();
    state.ToggleMute();
    state.FocusChanged(false);
    state.FocusChanged(true);
    require(state.Muted() && state.Volume() == 95, "a focus round trip leaves the user's mute and volume");
    state.ToggleMute();
    state.FocusChanged(false);
    require(!state.Muted() && state.Volume() == 95, "the focus mute does not touch the muted flag");
}

void focusOptionTogglesWhileUnfocused() {
    HostVolume state;
    state.FocusChanged(false);
    require(!state.FocusMuted(), "an unfocused window is not muted with the option off");
    require(state.SetMuteOnFocusLoss(true), "turning the option on while unfocused mutes");
    require(state.FocusMuted(), "the unfocused window is muted once the option is on");
    require(state.SetMuteOnFocusLoss(false), "turning the option off ends the focus mute");
    require(!state.FocusMuted(), "the focus mute is gone with the option off");
}

void gainAtDefaultIsUnity() {
    HostVolume state;
    require(state.Gain() == HostVolume::UnityGain, "the default gain is exactly unity");
    require(HostVolume::UnityGain == 65536, "unity is 1.0 in 16.16 fixed point");
}

void gainFollowsVolume() {
    HostVolume state;
    state.SetVolume(50);
    require(state.Gain() == 16384, "50 percent is a quarter of the amplitude");
    state.SetVolume(0);
    require(state.Gain() == 0, "0 is silence");
    std::uint32_t previous = 0;
    for (int volume = 1; volume <= 100; ++volume) {
        state.SetVolume(volume);
        require(state.Gain() > previous, "the gain grows with every volume");
        require(state.Gain() <= HostVolume::UnityGain, "the gain never exceeds unity");
        previous = state.Gain();
    }
}

void gainIsZeroWhenSilent() {
    HostVolume state;
    state.ToggleMute();
    require(state.Gain() == 0 && state.Silent(), "a muted state has no gain");
    state.ToggleMute();
    require(state.Gain() == HostVolume::UnityGain && !state.Silent(), "unmuting restores the gain");
    state.SetMuteOnFocusLoss(true);
    state.FocusChanged(false);
    require(state.Gain() == 0 && state.Silent(), "a focus muted state has no gain");
    state.FocusChanged(true);
    require(state.Gain() == HostVolume::UnityGain, "gaining focus restores the gain");
}

bool refusedWith(void (*parse)(), const char* name) {
    try {
        parse();
    } catch (const std::runtime_error& error) {
        return std::string(error.what()).find(name) != std::string::npos;
    }
    return false;
}

void flagParsing() {
    require(!ParseHostVolumeFlag("ANYPS5_MUTE_ON_FOCUS_LOSS", "0"), "0 turns the flag off");
    require(ParseHostVolumeFlag("ANYPS5_MUTE_ON_FOCUS_LOSS", "1"), "1 turns the flag on");
    require(refusedWith([] { ParseHostVolumeFlag("ANYPS5_MUTE_ON_FOCUS_LOSS", ""); }, "ANYPS5_MUTE_ON_FOCUS_LOSS"), "an empty flag is refused and named");
    require(refusedWith([] { ParseHostVolumeFlag("ANYPS5_MUTE_ON_FOCUS_LOSS", "yes"); }, "ANYPS5_MUTE_ON_FOCUS_LOSS"), "yes is refused and named");
    require(refusedWith([] { ParseHostVolumeFlag("ANYPS5_MUTE_ON_FOCUS_LOSS", "true"); }, "ANYPS5_MUTE_ON_FOCUS_LOSS"), "true is refused and named");
    require(refusedWith([] { ParseHostVolumeFlag("ANYPS5_MUTE_ON_FOCUS_LOSS", "2"); }, "ANYPS5_MUTE_ON_FOCUS_LOSS"), "2 is refused and named");
    require(refusedWith([] { ParseHostVolumeFlag("ANYPS5_MUTE_ON_FOCUS_LOSS", " 1"); }, "ANYPS5_MUTE_ON_FOCUS_LOSS"), "a padded 1 is refused and named");
    require(refusedWith([] { ParseHostVolumeFlag("ANYPS5_MUTE_ON_FOCUS_LOSS", "01"); }, "ANYPS5_MUTE_ON_FOCUS_LOSS"), "01 is refused and named");
}

void levelParsing() {
    require(ParseHostVolumeLevel("ANYPS5_VOLUME", "0") == 0, "0 is a level");
    require(ParseHostVolumeLevel("ANYPS5_VOLUME", "100") == 100, "100 is a level");
    require(ParseHostVolumeLevel("ANYPS5_VOLUME", "37") == 37, "any whole number in range is a level");
    for (const char* bad : {"", "-1", "101", "50.5", "5 0", "abc", "+50", " 50", "50 ", "1e1", "99999999999999999999"}) {
        bool refused = false;
        try {
            ParseHostVolumeLevel("ANYPS5_VOLUME", bad);
        } catch (const std::runtime_error& error) {
            refused = std::string(error.what()).find("ANYPS5_VOLUME") != std::string::npos;
        }
        require(refused, "a level outside 0 to 100 or not a whole number is refused and named");
    }
}

void messages() {
    HostVolume state;
    require(HostVolumeMessage(state) == "Host volume: 100%", "the default message gives the level");
    state.VolumeDown();
    require(HostVolumeMessage(state) == "Host volume: 95%", "a step shows the new level");
    state.ToggleMute();
    require(HostVolumeMessage(state) == "Host volume: muted (volume 95%)", "a mute shows the level it will return to");
    state.ToggleMute();
    state.SetMuteOnFocusLoss(true);
    state.FocusChanged(false);
    require(HostVolumeMessage(state) == "Host volume: muted while the window has no focus (volume 95%)", "a focus mute says why");
    state.ToggleMute();
    require(HostVolumeMessage(state) == "Host volume: muted (volume 95%)", "the user's mute takes precedence in the message");
}

}

int main() try {
    defaultState();
    stepsDown();
    stepsStopAtZero();
    stepsUp();
    stepsStopAtMaximum();
    muteKeepsVolume();
    muteAtZeroAndMaximum();
    stepsEndMute();
    stepsAtLimitsStillEndMute();
    setVolumeDirectly();
    setVolumeRefusesOutOfRange();
    focusIgnoredByDefault();
    focusLossMutesAndGainRestores();
    focusMuteKeepsUserState();
    focusOptionTogglesWhileUnfocused();
    gainAtDefaultIsUnity();
    gainFollowsVolume();
    gainIsZeroWhenSilent();
    flagParsing();
    levelParsing();
    messages();
    std::puts("host volume tests passed");
    return 0;
} catch (const std::exception& error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
}
