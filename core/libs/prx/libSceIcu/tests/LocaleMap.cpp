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
constexpr std::int32_t UsingFallback = -128;

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

void CheckGetLcid(const char* localeID, std::uint32_t expected) {
    const auto lcid = uloc_getLCID_nid_postfix(localeID);
    Require(lcid == expected, std::string("uloc_getLCID ") + (localeID ? localeID : "(null)") + ": got " +
                                  std::to_string(lcid) + ", expected " + std::to_string(expected));
}

void CheckGetLcidKeywords() {
    std::string variants = "de_DE";
    for (int i = 0; i < 80; ++i) variants += "_X";
    CheckGetLcid(variants.c_str(), 0x0407);
    CheckGetLcid((variants + "@collation=phonebook").c_str(), 0x0407);
    CheckGetLcid(("de_DE@collation=" + std::string(200, 'p')).c_str(), 0x0407);
}

void CheckLocaleForLcid() {
    char buffer[64];
    std::memset(buffer, 'x', sizeof(buffer));
    std::int32_t status = ZeroError;
    Require(uloc_getLocaleForLCID_nid_postfix(0x10407, buffer, sizeof(buffer), &status) == 25 && status == ZeroError &&
                std::strcmp(buffer, "de_DE@collation=phonebook") == 0,
            "uloc_getLocaleForLCID 0x10407: expected de_DE@collation=phonebook");

    std::memset(buffer, 'x', sizeof(buffer));
    status = ZeroError;
    Require(uloc_getLocaleForLCID_nid_postfix(0x0004, buffer, sizeof(buffer), &status) == 7 && status == ZeroError &&
                std::strcmp(buffer, "zh_Hans") == 0,
            "uloc_getLocaleForLCID 0x0004: expected zh_Hans");

    char exact[5];
    std::memset(exact, 'x', sizeof(exact));
    status = ZeroError;
    Require(uloc_getLocaleForLCID_nid_postfix(0x0409, exact, 5, &status) == 5 && status == NotTerminated &&
                std::memcmp(exact, "en_US", 5) == 0,
            "uloc_getLocaleForLCID exact capacity: expected an unterminated en_US");

    char small[2];
    std::memset(small, 'x', sizeof(small));
    status = ZeroError;
    Require(uloc_getLocaleForLCID_nid_postfix(0x0409, small, 2, &status) == 5 && status == BufferOverflow &&
                std::memcmp(small, "en", 2) == 0,
            "uloc_getLocaleForLCID small capacity: expected an overflow after en");

    status = ZeroError;
    Require(uloc_getLocaleForLCID_nid_postfix(0x0409, nullptr, 0, &status) == 5 && status == BufferOverflow,
            "uloc_getLocaleForLCID preflight: expected length 5 and an overflow");

    std::memset(buffer, 'x', sizeof(buffer));
    status = ZeroError;
    Require(uloc_getLocaleForLCID_nid_postfix(0x03ff, buffer, sizeof(buffer), &status) == 0 &&
                status == IllegalArgument && buffer[0] == 'x',
            "uloc_getLocaleForLCID unknown LCID: expected an illegal argument and an untouched buffer");

    std::memset(buffer, 'x', sizeof(buffer));
    status = IllegalArgument;
    Require(uloc_getLocaleForLCID_nid_postfix(0x0409, buffer, sizeof(buffer), &status) == 5 &&
                status == IllegalArgument && std::strcmp(buffer, "en_US") == 0,
            "uloc_getLocaleForLCID failed status: ICU still writes the locale and keeps the error");

    status = UsingFallback;
    Require(uloc_getLocaleForLCID_nid_postfix(0x0409, buffer, sizeof(buffer), &status) == 5 && status == UsingFallback,
            "uloc_getLocaleForLCID: a fallback warning must be kept");

    status = NotTerminated;
    Require(uloc_getLocaleForLCID_nid_postfix(0x0409, buffer, sizeof(buffer), &status) == 5 && status == ZeroError,
            "uloc_getLocaleForLCID: a fitting result must clear a not-terminated warning");

    status = ZeroError;
    Require(uloc_getLocaleForLCID_nid_postfix(0x0000, buffer, sizeof(buffer), &status) == 4 && status == ZeroError &&
                std::strcmp(buffer, "root") == 0,
            "uloc_getLocaleForLCID 0: expected root");
}

void CheckLocaleForLcidNullStatusThrows() {
    char buffer[8];
    try {
        uloc_getLocaleForLCID_nid_postfix(0x0409, buffer, sizeof(buffer), nullptr);
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, "uloc_getLocaleForLCID: a null status did not throw");
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
    CheckGetLcid(nullptr, 0x0000);
    CheckGetLcid("", 0x0000);
    CheckGetLcid("e", 0x0000);
    CheckGetLcid("en", 0x0009);
    CheckGetLcid("en_US", 0x0409);
    CheckGetLcid("en-US", 0x0000);
    CheckGetLcid("EN_us", 0x0000);
    CheckGetLcid("en_ZZ", 0x0009);
    CheckGetLcid("en_US_POSIX", 0x007f);
    CheckGetLcid("de__POSIX", 0x0007);
    CheckGetLcid("en_US.UTF-8", 0x0000);
    CheckGetLcid("xx_YY", 0x0000);
    CheckGetLcid("eng_USA", 0x0000);
    CheckGetLcid("root", 0x0000);
    CheckGetLcid("und_US", 0x0000);
    CheckGetLcid("es_ES", 0x0c0a);
    CheckGetLcid("es_ES@collation=traditional", 0x040a);
    CheckGetLcid("de_DE@collation=phonebook", 0x10407);
    CheckGetLcid("de@collation=phonebook", 0x10407);
    CheckGetLcid("de_DE@collation=PhoneBook", 0x0407);
    CheckGetLcid("de_DE@currency=EUR", 0x0407);
    CheckGetLcid("de_DE@currency=EUR;collation=phonebook", 0x10407);
    CheckGetLcid("de_DE@collation=", 0x0407);
    CheckGetLcid("de_DE@", 0x0407);
    CheckGetLcid("zh_CN@collation=stroke", 0x20804);
    CheckGetLcid("zh_CN@collation=pinyin", 0x0804);
    CheckGetLcid("ja_JP@calendar=japanese", 0x0411);
    CheckGetLcid("zh_Hans_CN", 0x0804);
    CheckGetLcid("zh_Hant_TW", 0x0404);
    CheckGetLcid("sr_Latn_RS", 0x241a);
    CheckGetLcid("sr_Cyrl_RS", 0x281a);
    CheckGetLcid("ckb_Arab_IQ", 0x0492);
    CheckGetLcid("quz_PE", 0x0c6b);
    CheckGetLcid("qu_PE", 0x0c6b);
    CheckGetLcid("prs_AF", 0x0000);
    CheckGetLcid("iw_IL", 0x040d);
    CheckGetLcid("in_ID", 0x0000);
    CheckGetLcidKeywords();
    CheckLocaleForLcid();
    CheckLocaleForLcidNullStatusThrows();
    CheckNullStatusThrows();
    std::puts("ICU locale map tests passed");
    return 0;
}
