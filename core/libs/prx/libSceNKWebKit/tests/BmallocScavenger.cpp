#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "bmalloc scavenger check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

}

int main() {
    auto* block = static_cast<unsigned char*>(_ZN7bmalloc3api15mallocOutOfLineEmNS_8HeapKindE(64, 0));
    Require(block != nullptr);
    std::memset(block, 0x5a, 64);

    alignas(16) unsigned char scavenger[64] = {};
    _ZN7bmalloc6IsoTLS8scavengeEv();
    _ZN7bmalloc9Scavenger8scheduleEm(scavenger, 0);
    _ZN7bmalloc9Scavenger8scheduleEm(scavenger, 1u << 20);
    _ZN7bmalloc9Scavenger29scheduleIfUnderMemoryPressureEm(scavenger, 0);
    _ZN7bmalloc9Scavenger29scheduleIfUnderMemoryPressureEm(scavenger, 1u << 20);
    _ZN7bmalloc9Scavenger8scheduleEm(nullptr, 4096);
    _ZN7bmalloc9Scavenger29scheduleIfUnderMemoryPressureEm(nullptr, 4096);

    for (unsigned char byte : scavenger) Require(byte == 0);
    for (int i = 0; i < 64; ++i) Require(block[i] == 0x5a);
    _ZN7bmalloc3api13freeOutOfLineEPvNS_8HeapKindE(block, 0);
    return 0;
}
