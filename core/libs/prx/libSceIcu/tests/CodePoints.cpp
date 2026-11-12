#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr std::int32_t NoBlock = 0;
constexpr std::int32_t BasicLatin = 1;
constexpr std::int32_t Cyrillic = 9;
constexpr std::int32_t Hebrew = 11;
constexpr std::int32_t Arabic = 12;

void ExpectInt(std::int64_t actual, std::int64_t expected, const char* what) {
    if (actual == expected) return;
    std::fprintf(stderr, "%s returned %lld, expected %lld\n", what, static_cast<long long>(actual), static_cast<long long>(expected));
    std::abort();
}

void TestCaseMapping() {
    ExpectInt(u_tolower_nid_postfix(u'A'), u'a', "u_tolower(A)");
    ExpectInt(u_tolower_nid_postfix(0x0130), u'i', "u_tolower(U+0130)");
    ExpectInt(u_tolower_nid_postfix(0x0391), 0x03B1, "u_tolower(U+0391)");
    ExpectInt(u_tolower_nid_postfix(0x10400), 0x10428, "u_tolower(U+10400)");
    ExpectInt(u_toupper_nid_postfix(u'a'), u'A', "u_toupper(a)");
    ExpectInt(u_toupper_nid_postfix(0x00DF), 0x00DF, "u_toupper(U+00DF)");
    ExpectInt(u_toupper_nid_postfix(0x01C6), 0x01C4, "u_toupper(U+01C6)");
    ExpectInt(u_totitle_nid_postfix(0x01C6), 0x01C5, "u_totitle(U+01C6)");
    ExpectInt(u_totitle_nid_postfix(u'a'), u'A', "u_totitle(a)");
    ExpectInt(u_tolower_nid_postfix(u'1'), u'1', "u_tolower(1)");
    ExpectInt(u_toupper_nid_postfix(-1), -1, "u_toupper(-1)");
    ExpectInt(u_totitle_nid_postfix(0x110000), 0x110000, "u_totitle(U+110000)");
}

void TestBlocks() {
    ExpectInt(ublock_getCode_nid_postfix(u'A'), BasicLatin, "ublock_getCode(A)");
    ExpectInt(ublock_getCode_nid_postfix(0x0410), Cyrillic, "ublock_getCode(U+0410)");
    ExpectInt(ublock_getCode_nid_postfix(0x05D0), Hebrew, "ublock_getCode(U+05D0)");
    ExpectInt(ublock_getCode_nid_postfix(0x0627), Arabic, "ublock_getCode(U+0627)");
    ExpectInt(ublock_getCode_nid_postfix(0x2FE0), NoBlock, "ublock_getCode(U+2FE0)");
    ExpectInt(ublock_getCode_nid_postfix(-1), NoBlock, "ublock_getCode(-1)");
}

}

int main() {
    TestCaseMapping();
    TestBlocks();
    return 0;
}
