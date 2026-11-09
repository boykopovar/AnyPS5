#include "prx/libSceIme/include/PsnDialogNotice.hpp"
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool value, const char* what) {
    if (!value) {
        std::fprintf(stderr, "psn_dialog_notice: %s\n", what);
        std::abort();
    }
}

const char16_t* const kPsnText = u"This feature needs PlayStation Network, which AnyPS5 does not provide, so it is not available.";
const char16_t* const kGenericText = u"This system feature is not available in AnyPS5.";

ModalDialogState StateFor(std::uint32_t owner, std::size_t& chosen) {
    chosen = 7;
    return ModalDialogPoll_nid_postfix(chosen, owner);
}

bool ShownMessageIs(const char16_t* text) {
    ModalDialogView view;
    if (!ModalDialogReadView_nid_postfix(view)) return false;
    return view.message == text && view.buttons.size() == 1 && view.buttons[0].label == u"OK" && view.buttons[0].selected;
}

void Clear() {
    ModalDialogReset_nid_postfix(ModalDialogAnyOwner);
}

}

int main() {
    PsnDialogNoticeSetMode_nid_postfix(PsnDialogNoticeMode::Show);
    std::size_t chosen = 0;

    Require(!PsnDialogNoticeOfferAt_nid_postfix(PsnDialogNoticeKind::PsnFeature, 0), "an offer was shown without a presenter");
    Require(StateFor(ModalDialogAnyOwner, chosen) == ModalDialogState::Idle, "a refused offer left a modal dialog open");

    ModalDialogSetPresenter_nid_postfix(true);
    Require(PsnDialogNoticeOfferAt_nid_postfix(PsnDialogNoticeKind::PsnFeature, 0), "the first offer with a presenter was refused");
    Require(ModalDialogCapturesInput_nid_postfix(), "the notice does not capture input while it shows");
    Require(ShownMessageIs(kPsnText), "the PSN notice text or its OK button is wrong");
    Require(StateFor(ModalDialogOwnerPsnNotice, chosen) == ModalDialogState::Showing, "the notice is not owned by its library");
    Require(StateFor(ModalDialogOwnerErrorDialog, chosen) == ModalDialogState::Idle, "another owner sees the notice");

    Require(!PsnDialogNoticeOfferAt_nid_postfix(PsnDialogNoticeKind::GenericFeature, 2500), "an offer replaced a notice that is still showing");
    Require(ShownMessageIs(kPsnText), "a refused offer changed the notice that is showing");

    ModalDialogPublishInput_nid_postfix(ModalDialogInputKind::Confirm);
    Require(StateFor(ModalDialogOwnerPsnNotice, chosen) == ModalDialogState::Answered, "OK does not end the notice");
    Clear();

    Require(!PsnDialogNoticeOfferAt_nid_postfix(PsnDialogNoticeKind::PsnFeature, 1000), "a second notice came inside the two-second interval");
    Require(!PsnDialogNoticeOfferAt_nid_postfix(PsnDialogNoticeKind::GenericFeature, 1999), "a notice came one millisecond inside the interval");
    Require(PsnDialogNoticeOfferAt_nid_postfix(PsnDialogNoticeKind::GenericFeature, 2000), "a notice was refused at exactly the interval");
    Require(ShownMessageIs(kGenericText), "the generic notice text is wrong");
    Clear();

    Require(!PsnDialogNoticeOfferAt_nid_postfix(PsnDialogNoticeKind::PsnFeature, 2001), "the interval does not count from the last shown notice");
    Clear();

    ModalDialogSetPresenter_nid_postfix(false);
    Require(!PsnDialogNoticeOfferAt_nid_postfix(PsnDialogNoticeKind::PsnFeature, 9000), "an offer was shown without a presenter after a reset");
    ModalDialogSetPresenter_nid_postfix(true);
    Require(PsnDialogNoticeOfferAt_nid_postfix(PsnDialogNoticeKind::PsnFeature, 9001), "a refused offer started the interval");
    Clear();

    PsnDialogNoticeSetMode_nid_postfix(PsnDialogNoticeMode::Silent);
    Require(!PsnDialogNoticeOfferAt_nid_postfix(PsnDialogNoticeKind::PsnFeature, 20000), "silent mode showed a notice");
    Require(StateFor(ModalDialogAnyOwner, chosen) == ModalDialogState::Idle, "silent mode opened a modal dialog");
    Require(!ModalDialogCapturesInput_nid_postfix(), "silent mode captures input");

    PsnDialogNoticeSetMode_nid_postfix(PsnDialogNoticeMode::Show);
    Require(PsnDialogNoticeOfferAt_nid_postfix(PsnDialogNoticeKind::PsnFeature, 20000), "show mode is not restored after silent");
    Clear();
    ModalDialogSetPresenter_nid_postfix(false);
    return 0;
}
