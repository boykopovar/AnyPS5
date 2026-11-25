#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "DNS check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

}

int main() {
    alignas(8) unsigned char nullString[8] = {};
    alignas(8) unsigned char before[8] = {};
    _ZN7WebCore11prefetchDNSERKN3WTF6StringE(nullString);
    Require(std::memcmp(nullString, before, sizeof(before)) == 0);

    alignas(8) unsigned char hostString[8];
    std::memset(hostString, 0x7c, sizeof(hostString));
    _ZN7WebCore11prefetchDNSERKN3WTF6StringE(hostString);
    for (unsigned char byte : hostString) Require(byte == 0x7c);

    _ZN7WebCore14stopResolveDNSEm(0);
    _ZN7WebCore14stopResolveDNSEm(1);
    _ZN7WebCore14stopResolveDNSEm(UINT64_MAX);
    return 0;
}
