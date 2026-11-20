#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error("icu availability: " + message);
}

constexpr std::int32_t ZeroError = 0;
constexpr std::int32_t MissingResourceError = 2;

}

int main() {
    const auto locales = uloc_countAvailable_nid_postfix();
    Require(locales >= 0, "uloc_countAvailable returned a negative count");
    if (locales > 0) {
        Require(uloc_getAvailable_nid_postfix(0) != nullptr, "uloc_getAvailable returned null for index 0");
    }

    std::int32_t status = ZeroError;
    char maximized[64] = {};
    const auto length = uloc_addLikelySubtags_nid_postfix("en", maximized, 64, &status);
    Require(length >= 0, "uloc_addLikelySubtags returned a negative length");
    Require(status == ZeroError || status == MissingResourceError, "uloc_addLikelySubtags returned an unexpected status");
    if (status == ZeroError) {
        Require(std::string(maximized, static_cast<std::size_t>(length)) == "en_Latn_US",
            "uloc_addLikelySubtags did not maximize en to en_Latn_US");
    }

    status = ZeroError;
    char minimized[64] = {};
    const auto minimizedLength = uloc_minimizeSubtags_nid_postfix("en_Latn_US", minimized, 64, &status);
    Require(minimizedLength >= 0, "uloc_minimizeSubtags returned a negative length");
    Require(status == ZeroError || status == MissingResourceError, "uloc_minimizeSubtags returned an unexpected status");
    if (status == ZeroError) {
        Require(std::string(minimized, static_cast<std::size_t>(minimizedLength)) == "en",
            "uloc_minimizeSubtags did not minimize en_Latn_US to en");
    }

    status = ZeroError;
    char canonical[32] = {};
    const auto canonicalLength = uloc_getName_nid_postfix("EN-us", canonical, 32, &status);
    Require(status == ZeroError, "uloc_getName failed");
    Require(canonicalLength == 5, "uloc_getName returned the wrong length");
    Require(std::string(canonical, static_cast<std::size_t>(canonicalLength)) == "en_US",
        "uloc_getName did not canonicalize EN-us to en_US");

    status = ZeroError;
    char small[4] = {};
    Require(uloc_getName_nid_postfix("en_us", small, 4, &status) == 5, "uloc_getName did not report the required length");
    Require(status != ZeroError, "uloc_getName did not report the overflow");

    const std::uint16_t wide[] = {0x61, 0x62, 0x63};
    Require(ustr_hashUCharsN_nid_postfix(wide, 3) == 136518, "ustr_hashUCharsN hashed abc wrongly");
    Require(ustr_hashUCharsN_nid_postfix(wide, 0) == 0, "ustr_hashUCharsN hashed the empty string wrongly");
    Require(ustr_hashCharsN_nid_postfix("abc", 3) == 136518, "ustr_hashCharsN hashed abc wrongly");
    Require(ustr_hashCharsN_nid_postfix("", 0) == 0, "ustr_hashCharsN hashed the empty string wrongly");
    Require(ustr_hashICharsN_nid_postfix("abc", 3) == 136518, "ustr_hashICharsN hashed abc wrongly");
    Require(ustr_hashICharsN_nid_postfix("ABC", 3) == 136518, "ustr_hashICharsN ignored case");

    status = ZeroError;
    udata_setFileAccess_nid_postfix(0, &status);
    Require(status == ZeroError, "udata_setFileAccess failed");
    return 0;
}
