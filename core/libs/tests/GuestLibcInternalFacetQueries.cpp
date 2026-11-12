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
    Require(!sceLibcInternalPending3_039_nid_no_patch(facet), "codecvt<wchar_t>::do_always_noconv must be false");
    Require(sceLibcInternalPending3_038_nid_no_patch(facet) == 6, "codecvt<wchar_t>::do_max_length must be 6");
    Require(sceLibcInternalPending3_037_nid_no_patch(facet) == 0, "codecvt<wchar_t>::do_encoding must be 0");
    Require(!sceLibcInternalPending3_023_nid_no_patch(facet), "codecvt<char16_t>::do_always_noconv must be false");
    Require(sceLibcInternalPending3_021_nid_no_patch(facet) == 0, "codecvt<char16_t>::do_encoding must be 0");
    Require(!sceLibcInternalPending3_016_nid_no_patch(facet), "codecvt<char32_t>::do_always_noconv must be false");
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
    Require(sceLibcInternalPending4_007_nid_no_patch(facet, name, locale) == -1, "messages<char>::do_open must return -1");
    Require(sceLibcInternalPending4_013_nid_no_patch(facet, name, locale) == -1, "messages<wchar_t>::do_open must return -1");
    sceLibcInternalPending4_008_nid_no_patch(facet, 0);
    sceLibcInternalPending4_014_nid_no_patch(facet, -1);
    Require(std::memcmp(facet, untouched, sizeof(facet)) == 0, "a messages call wrote the facet");
}

}

int main() {
    CheckCodecvtQueries();
    CheckMessagesQueries();
    return 0;
}
