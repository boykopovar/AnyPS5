#include "prx/libSceIme/include/ImeInput.hpp"
#include "prx/libSceIme/include/ImeKeyboardView.hpp"
#include "prx/libScePad/include/HostVolume.hpp"
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "FAIL %s\n", what);
    std::exit(1);
}

std::vector<ImeHostPad> g_received;

void Receive(const ImeHostPad& pad) {
    g_received.push_back(pad);
}

bool Provide(ImeKeyboardView& view) {
    view.text = u"Name";
    view.caret = 2;
    view.rows = 6;
    view.columns = 10;
    view.keys.push_back({2, 4, 1, 1, u"g", ImeKeySymbol::None, {}, true});
    return true;
}

void VerifyHostVolumeFocusMute() {
    HostVolume mutedFocus;
    Require(!mutedFocus.SetMuteOnFocusLoss(true), "enabling mute on focus loss while focused does not notify");
    Require(mutedFocus.ToggleMute(), "manual mute activates");
    Require(mutedFocus.Silent() && mutedFocus.Gain() == 0, "manual mute silences output");

    Require(!mutedFocus.FocusChanged(false), "losing focus while manually muted suppresses change notification");
    Require(!mutedFocus.Focused() && mutedFocus.FocusMuted() && mutedFocus.Silent() && mutedFocus.Gain() == 0, "losing focus while muted updates focus state and stays silent");

    Require(!mutedFocus.FocusChanged(true), "regaining focus while manually muted suppresses change notification");
    Require(mutedFocus.Focused() && !mutedFocus.FocusMuted() && mutedFocus.Silent() && mutedFocus.Gain() == 0, "regaining focus while muted updates focus state and stays silent");

    HostVolume unfocusedMuted;
    Require(unfocusedMuted.ToggleMute(), "manual mute activates before focus loss");
    Require(!unfocusedMuted.FocusChanged(false), "losing focus with mute on focus loss disabled stays silent");
    Require(!unfocusedMuted.SetMuteOnFocusLoss(true), "enabling mute on focus loss while unfocused and manually muted suppresses change notification");
    Require(unfocusedMuted.MuteOnFocusLoss() && unfocusedMuted.FocusMuted() && unfocusedMuted.Silent() && unfocusedMuted.Gain() == 0, "enabling mute on focus loss while muted updates focus-mute state");
    Require(!unfocusedMuted.SetMuteOnFocusLoss(false), "disabling mute on focus loss while unfocused and manually muted suppresses change notification");
    Require(!unfocusedMuted.MuteOnFocusLoss() && !unfocusedMuted.FocusMuted() && unfocusedMuted.Silent() && unfocusedMuted.Gain() == 0, "disabling mute on focus loss while muted clears focus-mute state");

    HostVolume unmutedFocus;
    Require(!unmutedFocus.SetMuteOnFocusLoss(true), "enabling mute on focus loss while focused and unmuted does not notify");
    Require(unmutedFocus.FocusChanged(false), "losing focus while unmuted notifies");
    Require(unmutedFocus.Gain() == 0 && unmutedFocus.Silent() && unmutedFocus.FocusMuted() && !unmutedFocus.Focused(), "losing focus while unmuted zeroes gain");
    Require(unmutedFocus.FocusChanged(true), "regaining focus while unmuted notifies");
    Require(unmutedFocus.Gain() == HostVolume::UnityGain && !unmutedFocus.Silent() && !unmutedFocus.FocusMuted() && unmutedFocus.Focused(), "regaining focus while unmuted restores unity gain");

    Require(!unmutedFocus.SetMuteOnFocusLoss(false), "disabling mute on focus loss while focused does not notify");
    Require(!unmutedFocus.FocusChanged(false), "losing focus with mute on focus loss disabled does not notify");
    Require(unmutedFocus.SetMuteOnFocusLoss(true), "enabling mute on focus loss while unfocused and unmuted notifies");
    Require(unmutedFocus.Gain() == 0 && unmutedFocus.Silent() && unmutedFocus.FocusMuted(), "enabling mute on focus loss while unfocused and unmuted zeroes gain");
    Require(unmutedFocus.SetMuteOnFocusLoss(false), "disabling mute on focus loss while unfocused and unmuted notifies");
    Require(unmutedFocus.Gain() == HostVolume::UnityGain && !unmutedFocus.Silent() && !unmutedFocus.FocusMuted(), "disabling mute on focus loss while unfocused and unmuted restores unity gain");
}

}

int main() {
    Require(!ImeCapturesPad_nid_postfix(), "no sink: the pad is not captured");
    ImePublishPad_nid_postfix({0x4000, 128, 128, 0});
    ImeKeyboardView view;
    Require(!ImeReadKeyboardView_nid_postfix(view), "no provider: no keyboard view");

    ImeSetHostPadSink_nid_no_patch(&Receive);
    Require(ImeCapturesPad_nid_postfix(), "a sink captures the pad");
    Require(!ImeCapturesKeyboard_nid_postfix(), "a pad sink does not capture the keyboard");
    ImePublishPad_nid_postfix({0x4000 | 0x0200, 255, 0, 200});
    Require(g_received.size() == 1, "a published pad reaches the sink once");
    Require(g_received[0].buttons == (0x4000u | 0x0200u) && g_received[0].leftX == 255 && g_received[0].leftY == 0 && g_received[0].l2 == 200, "the sink sees the published buttons, stick and trigger");

    ImeSetKeyboardViewProvider_nid_no_patch(&Provide);
    Require(ImeReadKeyboardView_nid_postfix(view), "a provider fills the keyboard view");
    Require(view.text == u"Name" && view.caret == 2 && view.rows == 6 && view.columns == 10 && view.keys.size() == 1 && view.keys[0].selected && view.keys[0].label == u"g", "the view is the provider's");

    ImeSetHostPadSink_nid_no_patch(nullptr);
    ImeSetKeyboardViewProvider_nid_no_patch(nullptr);
    Require(!ImeCapturesPad_nid_postfix(), "a cleared sink releases the pad");
    ImePublishPad_nid_postfix({0x4000, 128, 128, 0});
    Require(g_received.size() == 1, "a released pad reaches no sink");
    ImeKeyboardView cleared;
    Require(!ImeReadKeyboardView_nid_postfix(cleared), "a cleared provider gives no view");
    VerifyHostVolumeFocusMute();
    std::printf("ime pad route tests passed\n");
    return 0;
}
