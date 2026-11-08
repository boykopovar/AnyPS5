#include "prx/libkernel/System/include/HostNotify.hpp"
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool value, const char* what) {
    if (!value) {
        std::fprintf(stderr, "host_notify: %s\n", what);
        std::abort();
    }
}

void RefusesValue(const char* text) {
    Require(!ParseHostNotifyMode(text).has_value(), "an invalid value was accepted");
    Require(HostNotifyModeFromEnvironment(text) == HostNotifyMode::Off, "an invalid value did not stay off");
}

}

int main() {
    Require(ParseHostNotifyMode(nullptr) == HostNotifyMode::Off, "an unset variable is not off");
    Require(ParseHostNotifyMode("") == HostNotifyMode::Off, "an empty value is not off");
    Require(ParseHostNotifyMode("0") == HostNotifyMode::Off, "0 is not off");
    Require(ParseHostNotifyMode("1") == HostNotifyMode::On, "1 is not on");
    RefusesValue("true");
    RefusesValue("on");
    RefusesValue("yes");
    RefusesValue("2");
    RefusesValue(" 1");
    RefusesValue("1 ");
    RefusesValue("ON");
    return 0;
}
