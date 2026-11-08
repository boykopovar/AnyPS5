#include "prx/libSceErrorDialog/include/ErrorDialogRequest.hpp"
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool value, const char* what) {
    if (!value) {
        std::fprintf(stderr, "error_dialog_request: %s\n", what);
        std::abort();
    }
}

}

int main() {
    const ModalDialogRequest request = ErrorDialog::BuildRequest(static_cast<std::int32_t>(0x80020001u));
    Require(request.message == u"An error occurred.\nError code: 0x80020001", "the error code is not in the message");
    Require(request.buttons.size() == 1 && request.buttons[0] == u"OK", "the dialog does not have one OK button");
    Require(request.focus == 0 && request.cancel == 0, "the OK button is not focused and chosen by a cancel");
    Require(ErrorDialog::BuildRequest(0).message == u"An error occurred.\nError code: 0x00000000", "a zero code is not padded");
    Require(ErrorDialog::BuildRequest(-1).message == u"An error occurred.\nError code: 0xFFFFFFFF", "a negative code is not shown as unsigned");
    Require(ErrorDialog::BuildRequest(0x1AB).message == u"An error occurred.\nError code: 0x000001AB", "hex digits are not uppercase");
}
