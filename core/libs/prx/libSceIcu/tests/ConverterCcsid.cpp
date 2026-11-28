#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error("icu ccsid: " + message);
}

constexpr std::int32_t ZeroError = 0;

}

int main() {
    // ICU does not validate the platform: UCNV_UNKNOWN (-1) and UCNV_IBM (0) are both legal.
    for (const std::int32_t platform : {-1, 0}) {
        std::int32_t status = ZeroError;
        UConverter* converter = ucnv_openCCSID_nid_postfix(367, platform, &status);
        if (converter != nullptr) {
            Require(status <= ZeroError, "a converter was returned with an error status");
        }
    }

    std::int32_t status = ZeroError;
    Require(ucnv_openCCSID_nid_postfix(367, 0, nullptr) == nullptr, "a null status was accepted");

    status = 5;
    Require(ucnv_openCCSID_nid_postfix(367, 0, &status) == nullptr, "a failing status was ignored");
    Require(status == 5, "a failing status was overwritten");

    status = ZeroError;
    UConverter* usAscii = ucnv_openCCSID_nid_postfix(367, 0, &status);
    if (usAscii != nullptr) {
        Require(status == ZeroError, "a converter was returned with an error status");
    }
    return 0;
}
