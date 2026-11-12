#include "prx/libSceHmd/include/Api.hpp"

#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "%s\n", what);
    std::abort();
}

}

int main() {
    constexpr int expected = static_cast<int>(0x81110009u);
    for (int call = 0; call < 3; ++call) {
        const int result = sceHmdDistortionGetCompoundEyeCorrectionCommand();
        Require(result < 0, "sceHmdDistortionGetCompoundEyeCorrectionCommand did not answer an error");
        Require(result == expected, "sceHmdDistortionGetCompoundEyeCorrectionCommand did not answer 0x81110009");
    }
    return 0;
}
