#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"

#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "guest_pad_get_data_internal: %s\n", message);
    std::abort();
}

}

int main() {
    PadData data{};
    Require(scePadGetDataInternal(0, nullptr) == PAD_ERROR_INVALID_ARG, "null data must return PAD_ERROR_INVALID_ARG");
    Require(scePadGetDataInternal(0, &data) == PAD_ERROR_NOT_INITIALIZED, "pre-init read must return PAD_ERROR_NOT_INITIALIZED");
    Require(data.left_stick_x == 0x80 && data.left_stick_y == 0x80, "pre-init read must write neutral stick data");

    Require(scePadInit_nid_postfix() == PAD_OK, "scePadInit must succeed");
    data = {};
    data.left_stick_x = 0x5a;
    Require(scePadGetDataInternal(PAD_MAX_CONTROLLERS, &data) == PAD_ERROR_INVALID_ARG, "out-of-range port must return PAD_ERROR_INVALID_ARG");
    Require(data.left_stick_x == 0x5a, "out-of-range port error must not write neutral stick data");

    data = {};
    Require(scePadGetDataInternal(0, &data) == PAD_OK, "valid port read must succeed");
    Require(data.left_stick_x == 0x80 && data.left_stick_y == 0x80, "port 0 read must return valid stick center");
    Require(data.right_stick_x == 0x80 && data.right_stick_y == 0x80, "port 0 read must return valid right stick center");
    return 0;
}
