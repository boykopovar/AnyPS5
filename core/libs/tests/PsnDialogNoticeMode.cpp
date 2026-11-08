#include "prx/libSceIme/include/PsnDialogNotice.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

void Require(bool value, const char* what) {
    if (!value) {
        std::fprintf(stderr, "psn_dialog_notice_mode: %s\n", what);
        std::abort();
    }
}

void RefusesNamingVariable(const char* text) {
    try {
        ParsePsnDialogNoticeMode(text);
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        Require(message.find("ANYPS5_PSN_DIALOGS") != std::string::npos, "the refusal does not name the variable");
        Require(message.find(std::string("'") + text + "'") != std::string::npos, "the refusal does not name the value");
        return;
    }
    Require(false, "an invalid value was accepted");
}

}

void HostTextMatchesTheDialogMessages() {
    const auto psnRequest = PsnDialogNoticeRequest(PsnDialogNoticeKind::PsnFeature);
    const auto genericRequest = PsnDialogNoticeRequest(PsnDialogNoticeKind::GenericFeature);
    const auto matches = [](const std::u16string& wide, std::string_view narrow) {
        if (wide.size() != narrow.size()) return false;
        for (std::size_t index = 0; index < wide.size(); ++index) {
            if (wide[index] != static_cast<char16_t>(narrow[index])) return false;
        }
        return true;
    };
    Require(matches(psnRequest.message, PsnDialogNoticeHostText(PsnDialogNoticeKind::PsnFeature)), "the PSN host text differs from the dialog");
    Require(matches(genericRequest.message, PsnDialogNoticeHostText(PsnDialogNoticeKind::GenericFeature)), "the generic host text differs from the dialog");
}

int main() {
    HostTextMatchesTheDialogMessages();
    Require(ParsePsnDialogNoticeMode(nullptr) == PsnDialogNoticeMode::Silent, "an unset variable is not silent");
    Require(ParsePsnDialogNoticeMode("") == PsnDialogNoticeMode::Silent, "an empty value is not the same as unset");
    Require(ParsePsnDialogNoticeMode("silent") == PsnDialogNoticeMode::Silent, "silent is not silent");
    Require(ParsePsnDialogNoticeMode("show") == PsnDialogNoticeMode::Show, "show is not show");
    RefusesNamingVariable("Show");
    RefusesNamingVariable("SHOW");
    RefusesNamingVariable("show ");
    RefusesNamingVariable("1");
    RefusesNamingVariable("0");
    RefusesNamingVariable("on");
    RefusesNamingVariable("yes");
    return 0;
}
