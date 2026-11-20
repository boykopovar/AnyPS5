#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "LibcInternal facet queries: %s\n", message);
        std::abort();
    }
}

void CheckCodecvtQueries() {
    unsigned char facet[0x20];
    unsigned char untouched[sizeof(facet)];
    std::memset(facet, 0xa5, sizeof(facet));
    std::memcpy(untouched, facet, sizeof(facet));
    Require(!_ZNKSt7codecvtIwc9_MbstatetE16do_always_noconvEv(facet), "codecvt<wchar_t>::do_always_noconv must be false");
    Require(_ZNKSt7codecvtIwc9_MbstatetE13do_max_lengthEv(facet) == 6, "codecvt<wchar_t>::do_max_length must be 6");
    Require(_ZNKSt7codecvtIwc9_MbstatetE11do_encodingEv(facet) == 0, "codecvt<wchar_t>::do_encoding must be 0");
    Require(!_ZNKSt7codecvtIDsc9_MbstatetE16do_always_noconvEv(facet), "codecvt<char16_t>::do_always_noconv must be false");
    Require(_ZNKSt7codecvtIDsc9_MbstatetE11do_encodingEv(facet) == 0, "codecvt<char16_t>::do_encoding must be 0");
    Require(!_ZNKSt7codecvtIDic9_MbstatetE16do_always_noconvEv(facet), "codecvt<char32_t>::do_always_noconv must be false");
    Require(std::memcmp(facet, untouched, sizeof(facet)) == 0, "a codecvt query wrote the facet");
}

void CheckMessagesQueries() {
    unsigned char facet[0x20];
    unsigned char name[0x20];
    unsigned char locale[0x10];
    std::memset(facet, 0x5a, sizeof(facet));
    std::memset(name, 0x5a, sizeof(name));
    std::memset(locale, 0x5a, sizeof(locale));
    unsigned char untouched[sizeof(facet)];
    std::memcpy(untouched, facet, sizeof(facet));
    Require(_ZNKSt8messagesIcE7do_openERKSsRKSt6locale(facet, name, locale) == -1, "messages<char>::do_open must return -1");
    Require(_ZNKSt8messagesIwE7do_openERKSsRKSt6locale(facet, name, locale) == -1, "messages<wchar_t>::do_open must return -1");
    _ZNKSt8messagesIcE8do_closeEi(facet, 0);
    _ZNKSt8messagesIwE8do_closeEi(facet, -1);
    Require(std::memcmp(facet, untouched, sizeof(facet)) == 0, "a messages call wrote the facet");
}

}

int main() {
    CheckCodecvtQueries();
    CheckMessagesQueries();
    return 0;
}
