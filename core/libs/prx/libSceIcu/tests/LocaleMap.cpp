#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

constexpr std::int32_t ZeroError = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t BufferOverflow = 15;
constexpr std::int32_t NotTerminated = -124;

void Require(bool condition, const std::string& message) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", message.c_str());
    std::abort();
}

void CheckPosix(std::uint32_t hostID, const char* expected) {
    char buffer[64];
    std::memset(buffer, 'x', sizeof(buffer));
    std::int32_t status = ZeroError;
    const auto length = uprv_convertToPosix_nid_postfix(hostID, buffer, sizeof(buffer), &status);
    Require(status == ZeroError, "LCID " + std::to_string(hostID) + ": status " + std::to_string(status));
    Require(length == static_cast<std::int32_t>(std::strlen(expected)) && std::strcmp(buffer, expected) == 0,
            "LCID " + std::to_string(hostID) + ": got \"" + buffer + "\", expected \"" + expected + "\"");
}

void CheckPosixCapacity() {
    char exact[5];
    std::memset(exact, 'x', sizeof(exact));
    std::int32_t status = ZeroError;
    Require(uprv_convertToPosix_nid_postfix(0x0409, exact, 5, &status) == 5, "exact capacity: wrong length");
    Require(status == NotTerminated && std::memcmp(exact, "en_US", 5) == 0, "exact capacity: expected an unterminated en_US");

    char small[3];
    std::memset(small, 'x', sizeof(small));
    status = ZeroError;
    Require(uprv_convertToPosix_nid_postfix(0x0409, small, 3, &status) == 5, "small capacity: wrong length");
    Require(status == BufferOverflow && std::memcmp(small, "en_", 3) == 0, "small capacity: expected an overflow after en_");

    status = ZeroError;
    Require(uprv_convertToPosix_nid_postfix(0x0409, nullptr, 0, &status) == 5 && status == BufferOverflow,
            "preflight: expected length 5 and an overflow");

    status = NotTerminated;
    char room[8];
    Require(uprv_convertToPosix_nid_postfix(0x0409, room, sizeof(room), &status) == 5 && status == ZeroError,
            "a fitting result must clear a not-terminated warning");

    status = ZeroError;
    Require(uprv_convertToPosix_nid_postfix(0x03ff, room, sizeof(room), &status) == 0 && status == IllegalArgument,
            "an unknown LCID must be an illegal argument");
}

void CheckLcid(const char* language, const char* posix, std::uint32_t expected) {
    std::int32_t status = ZeroError;
    const auto lcid = uprv_convertToLCID_nid_postfix(language, posix, &status);
    Require(lcid == expected, std::string("locale ") + posix + ": got LCID " + std::to_string(lcid) + ", expected " +
                                  std::to_string(expected));
}

void CheckPlatform() {
    std::int32_t status = ZeroError;
    Require(uprv_convertToLCIDPlatform_nid_postfix("en_US", &status) == 0 && status == ZeroError,
            "the platform lookup must not consult the host");
    status = IllegalArgument;
    Require(uprv_convertToLCIDPlatform_nid_postfix("en_US", &status) == 0 && status == IllegalArgument,
            "a failed status must be kept");
}

void CheckNullStatusThrows() {
    try {
        uprv_convertToLCID_nid_postfix("en", "en_US", nullptr);
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, "a null status did not throw");
}

}

int main() {
    CheckPosix(0x0409, "en_US");
    CheckPosix(0x0411, "ja_JP");
    CheckPosix(0x0407, "de_DE");
    CheckPosix(0x0492, "ckb_Arab_IQ");
    CheckPosix(0x0000, "root");
    CheckPosixCapacity();
    CheckLcid("en", "en_US", 0x0409);
    CheckLcid("ja", "ja_JP", 0x0411);
    CheckLcid("de", "de_DE", 0x0407);
    CheckLcid("x", "x", 0x0000);
    CheckPlatform();
    CheckNullStatusThrows();
    std::puts("ICU locale map tests passed");
    return 0;
}
