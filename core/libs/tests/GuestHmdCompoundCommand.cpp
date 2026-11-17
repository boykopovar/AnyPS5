#include "prx/libSceHmd/include/Api.hpp"
#include "Testing.hpp"

#include <cstdio>
#include <cstdlib>

int main() {
    const int first = sceHmdDistortionGetCompoundEyeCorrectionCommand();
    const int second = sceHmdDistortionGetCompoundEyeCorrectionCommand();
    Require(first < 0, "sceHmdDistortionGetCompoundEyeCorrectionCommand did not answer an error");
    Require(first == second, "sceHmdDistortionGetCompoundEyeCorrectionCommand did not answer the same error on a repeated call");
    return 0;
}
