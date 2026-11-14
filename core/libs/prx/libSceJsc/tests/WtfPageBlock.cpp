#include "prx/libSceJsc/libSceJsc.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF page block check failed at line %d\n", line);
        std::abort();
    }
}

void TestPageMask() {
    const std::size_t mask = _ZN3WTF8pageMaskEv();
    Check(mask == std::size_t{0xFFFFFFFFFFFFC000ull}, __LINE__);
    Check(~mask + 1 == 16384, __LINE__);
    Check(_ZN3WTF8pageMaskEv() == mask, __LINE__);
    Check((std::size_t{0x12345678} & mask) == 0x12344000, __LINE__);
}

} // namespace

int main() {
    TestPageMask();
    return 0;
}
