#include "prx/libSceHmd/include/Api.hpp"

#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "%s\n", what);
    std::abort();
}

} // namespace

int main() {
    const int first = sceHmdDistortionGetCompoundEyeCorrectionCommand();
    const int second = sceHmdDistortionGetCompoundEyeCorrectionCommand();
    Require(first < 0, "sceHmdDistortionGetCompoundEyeCorrectionCommand did not answer an error");
    Require(first == second, "sceHmdDistortionGetCompoundEyeCorrectionCommand did not answer the same error on a repeated call");
    return 0;
}
