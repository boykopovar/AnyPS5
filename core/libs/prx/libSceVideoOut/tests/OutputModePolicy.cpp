#include "prx/libSceVideoOut/include/Output.hpp"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>

static void Require(bool value, const char* reason) {
    if (!value) {
        std::fprintf(stderr, "%s\n", reason);
        std::abort();
    }
}

int main() {
    constexpr int unsupported = static_cast<int>(0x80290016);
    constexpr int invalidValue = static_cast<int>(0x80290001);
    constexpr int unknownMode = static_cast<int>(0x8029001e);
    VideoOutOutputOptions options{};
    Require(sceVideoOutInitializeOutputOptions(&options) == 0, "output options initialization failed");
    for (int refresh : {-1, 0, 30, 59, 60, 119, 120, 144, 240}) {
        Require(ValidateVideoOutOutputMode_nid_no_patch(15, &options, refresh) == (refresh < 120 ? unsupported : 0), "mode 15 did not follow the display refresh threshold");
        Require(ValidateVideoOutOutputMode_nid_no_patch(15, nullptr, refresh) == (refresh < 120 ? unsupported : 0), "mode 15 without options did not follow the display refresh threshold");
        Require(ValidateVideoOutOutputMode_nid_no_patch(1, &options, refresh) == 0, "default mode depended on the display refresh rate");
    }
    for (std::uint64_t mode : {4, 7, 8, 12, 13, 14, 16, 17, 19}) {
        Require(ValidateVideoOutOutputMode_nid_no_patch(mode, &options, 240) == unsupported, "unimplemented output mode was accepted on a fast display");
    }
    Require(ValidateVideoOutOutputMode_nid_no_patch(0xd000000a, &options, 240) == unknownMode, "unknown output mode was accepted");
    VideoOutOutputOptions changed = options;
    changed.internalData[0] = 0;
    Require(ValidateVideoOutOutputMode_nid_no_patch(15, &changed, 120) == invalidValue, "mode 15 accepted invalid output options");
    Require(ValidateVideoOutOutputMode_nid_no_patch(15, &changed, 60) == unsupported, "invalid options changed unsupported-mode error precedence");
    changed = options;
    changed.internalData[3] = 0xffffffff;
    Require(ValidateVideoOutOutputMode_nid_no_patch(15, &changed, 120) == 0, "mode 15 rejected the free options word");
    bool thrown = false;
    try {
        ValidateVideoOutOutputMode_nid_no_patch(18, &options, 120);
    } catch (const std::runtime_error&) {
        thrown = true;
    }
    Require(thrown, "unsettled mode 18 did not throw");

}
