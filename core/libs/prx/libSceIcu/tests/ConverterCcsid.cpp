#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error("icu ccsid: " + message);
}

constexpr std::int32_t ZeroError = 0;
constexpr std::int32_t IllegalArgument = 1;

}

int main() {
    std::int32_t status = ZeroError;
    Require(ucnv_openCCSID_nid_postfix(367, 7, &status) == nullptr, "an invalid platform was accepted");
    Require(status == IllegalArgument, "an invalid platform did not report an illegal argument");

    status = ZeroError;
    Require(ucnv_openCCSID_nid_postfix(367, -1, &status) == nullptr, "a negative platform was accepted");
    Require(status == IllegalArgument, "a negative platform did not report an illegal argument");

    status = ZeroError;
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
