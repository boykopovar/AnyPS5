#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <vector>

namespace {

using Classifier = std::int8_t (APS5_VABI*)(std::int32_t);

void Check(const char* name, Classifier classify, std::int32_t character, bool expected) {
    const int result = classify(character);
    if (result == (expected ? 1 : 0)) return;
    std::fprintf(stderr, "%s(0x%x) returned %d, expected %d\n", name, static_cast<unsigned>(character), result, expected ? 1 : 0);
    std::abort();
}

void Sweep(const char* name, Classifier classify, std::initializer_list<std::int32_t> members) {
    std::vector<bool> member(0x110000);
    for (const auto character : members) member[character] = true;
    for (std::int32_t character = -0x1000; character <= 0x110fff; ++character)
        Check(name, classify, character, character >= 0 && character <= 0x10ffff && member[character]);
    for (const auto character : members) {
        for (const std::uint32_t high : {0x110000u, 0x200000u, 0x1000000u, 0x80000000u, 0xffff0000u})
            Check(name, classify, static_cast<std::int32_t>(static_cast<std::uint32_t>(character) + high), false);
    }
    for (const auto character : {std::numeric_limits<std::int32_t>::min(), -1, 0x110000, std::numeric_limits<std::int32_t>::max()})
        Check(name, classify, character, false);
}

}

int main() {
    Sweep("u_isspace", u_isspace_nid_postfix, {
        0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x85, 0xa0, 0x1680,
        0x2000, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005, 0x2006, 0x2007, 0x2008, 0x2009, 0x200a,
        0x2028, 0x2029, 0x202f, 0x205f, 0x3000});
    Sweep("u_isblank", u_isblank_nid_postfix, {
        0x09, 0x20, 0xa0, 0x1680,
        0x2000, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005, 0x2006, 0x2007, 0x2008, 0x2009, 0x200a,
        0x202f, 0x205f, 0x3000});
    Sweep("u_isWhitespace", u_isWhitespace_nid_postfix, {
        0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x1680,
        0x2000, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005, 0x2006, 0x2008, 0x2009, 0x200a,
        0x2028, 0x2029, 0x205f, 0x3000});
    Sweep("u_isJavaSpaceChar", u_isJavaSpaceChar_nid_postfix, {
        0x20, 0xa0, 0x1680,
        0x2000, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005, 0x2006, 0x2007, 0x2008, 0x2009, 0x200a,
        0x2028, 0x2029, 0x202f, 0x205f, 0x3000});
    Sweep("u_isUWhiteSpace", u_isUWhiteSpace_nid_postfix, {
        0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x20, 0x85, 0xa0, 0x1680,
        0x2000, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005, 0x2006, 0x2007, 0x2008, 0x2009, 0x200a,
        0x2028, 0x2029, 0x202f, 0x205f, 0x3000});
    std::puts("ICU space classification tests passed");
}
