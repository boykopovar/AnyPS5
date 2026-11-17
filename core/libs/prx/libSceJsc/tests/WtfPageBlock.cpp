#include "prx/libSceJsc/libSceJsc.hpp"
#include "Testing.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

void TestPageMask() {
    const std::size_t mask = _ZN3WTF8pageMaskEv();
    Require(mask == std::size_t{0xFFFFFFFFFFFFC000ull});
    Require(~mask + 1 == 16384);
    Require(_ZN3WTF8pageMaskEv() == mask);
    Require((std::size_t{0x12345678} & mask) == 0x12344000);
}

} // namespace

int main() {
    TestPageMask();
    return 0;
}
