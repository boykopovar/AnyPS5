#include "prx/libSceMsgDialog/include/HostMessageBox.hpp"

#include <cstdio>

namespace {

bool correct = true;

void Check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "HOSTMSGBOX: %s\n", message);
        correct = false;
    }
}

}  // namespace

int main() {
    Check(!MsgDialog::HostMessageBoxEnabled(nullptr, false), "an unset variable enables the host box");
    Check(MsgDialog::HostMessageBoxEnabled("1", false), "1 does not enable the host box");
    Check(!MsgDialog::HostMessageBoxEnabled("0", false), "0 enables the host box");
    return correct ? 0 : 1;
}
