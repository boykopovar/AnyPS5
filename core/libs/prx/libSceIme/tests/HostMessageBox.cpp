#include "prx/libSceIme/include/HostMessageBox.hpp"

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
    Check(!HostBox::HostMessageBoxEnabled(nullptr, false), "an unset variable enables the host box");
    Check(HostBox::HostMessageBoxEnabled("1", false), "1 does not enable the host box");
    Check(!HostBox::HostMessageBoxEnabled("0", false), "0 enables the host box");
    Check(!HostBox::HostMessageBoxEnabled("yes", false), "a value other than 1 enables the host box");
    Check(!HostBox::HostMessageBoxEnabled("1", true), "the host box is enabled on Apple builds");

    Check(HostBox::HostMessageLabel(u"Yes") == "Yes", "an ASCII label is changed");
    Check(HostBox::HostMessageLabel(u"AÑ") == "A?", "a non-ASCII label is not replaced by ?");

    Check(HostBox::IndexOfHostButton(0, 2, 1) == 0u, "the first button is not index 0");
    Check(HostBox::IndexOfHostButton(1, 2, 1) == 1u, "the second button is not index 1");
    Check(!HostBox::IndexOfHostButton(2, 2, 1).has_value(), "an out-of-range button is accepted");
    Check(!HostBox::IndexOfHostButton(-1, 2, 1).has_value(), "a closed box without a choice is accepted");
    return correct ? 0 : 1;
}
