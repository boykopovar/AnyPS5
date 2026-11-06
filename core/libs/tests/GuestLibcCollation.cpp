#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc collation: %s\n", message);
    std::abort();
}

int main() {
    const char abc[] = "abc";
    const char abd[] = "abd";
    const char ab[] = "ab";
    const char abcd[] = "abcd";
    const char embedded[] = "ab\0x";
    const char otherEmbedded[] = "ab\0y";

    Require(_Strcoll_nid_postfix(abc, abc + 3, abc, abc + 3) == 0, "equal strings compare equal");
    Require(_Strcoll_nid_postfix(abc, abc + 3, abd, abd + 3) < 0, "a smaller byte compares less");
    Require(_Strcoll_nid_postfix(abd, abd + 3, abc, abc + 3) > 0, "a larger byte compares greater");
    Require(_Strcoll_nid_postfix(ab, ab + 2, abc, abc + 3) < 0, "a prefix compares less");
    Require(_Strcoll_nid_postfix(abcd, abcd + 4, abc, abc + 3) > 0, "the longer string compares greater");
    Require(_Strcoll_nid_postfix(embedded, embedded + 4, otherEmbedded, otherEmbedded + 4) < 0,
        "a byte behind the terminator is compared");
    Require(_Strcoll_nid_postfix(embedded, embedded + 2, otherEmbedded, otherEmbedded + 4) < 0,
        "a shorter range compares less even when the ranges start alike");

    char destination[8] {};
    Require(_Strxfrm_nid_postfix(destination, destination + 8, abc, abc + 3) == 3, "the transform reports the source length");
    Require(std::memcmp(destination, abc, 3) == 0, "the transform copies the source");
    Require(destination[3] == 0, "the transform writes only the source");

    std::memset(destination, 0x5a, sizeof(destination));
    Require(_Strxfrm_nid_postfix(destination, destination + 3, abc, abc + 3) == 3, "an exact fit still reports the length");
    Require(std::memcmp(destination, abc, 3) == 0, "an exact fit copies the source");

    std::memset(destination, 0x5a, sizeof(destination));
    Require(_Strxfrm_nid_postfix(destination, destination + 2, abc, abc + 3) == 3, "a short buffer still reports the length");
    Require(destination[0] == 0x5a && destination[1] == 0x5a && destination[2] == 0x5a, "a short buffer is left alone");

    const char16_t wideAbc[] = u"abc";
    const char16_t wideAbd[] = u"abd";
    const char16_t wideAb[] = u"ab";
    const char16_t wideHigh[] = u"\u0100\u0100";

    Require(_Wcscoll_nid_postfix(wideAbc, wideAbc + 3, wideAbc, wideAbc + 3) == 0, "equal wide strings compare equal");
    Require(_Wcscoll_nid_postfix(wideAbc, wideAbc + 3, wideAbd, wideAbd + 3) < 0, "a smaller code unit compares less");
    Require(_Wcscoll_nid_postfix(wideAbd, wideAbd + 3, wideAbc, wideAbc + 3) > 0, "a larger code unit compares greater");
    Require(_Wcscoll_nid_postfix(wideAb, wideAb + 2, wideAbc, wideAbc + 3) < 0, "a wide prefix compares less");
    Require(_Wcscoll_nid_postfix(wideHigh, wideHigh + 2, wideAbc, wideAbc + 3) > 0,
        "the code unit is compared as a value, not byte by byte");

    char16_t wideDestination[8] {};
    Require(_Wcsxfrm_nid_postfix(wideDestination, wideDestination + 8, wideAbc, wideAbc + 3) == 3,
        "the wide transform reports the source length in code units");
    Require(std::memcmp(wideDestination, wideAbc, 3 * sizeof(char16_t)) == 0, "the wide transform copies the source");
    Require(wideDestination[3] == 0, "the wide transform writes only the source");

    for (char16_t& unit : wideDestination) unit = 0x5a5a;
    Require(_Wcsxfrm_nid_postfix(wideDestination, wideDestination + 2, wideAbc, wideAbc + 3) == 3,
        "a short wide buffer still reports the length");
    Require(wideDestination[0] == 0x5a5a && wideDestination[2] == 0x5a5a, "a short wide buffer is left alone");

    return 0;
}
