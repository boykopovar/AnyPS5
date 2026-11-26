#include "prx/libSceFios2/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

static void Require(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "guest_fios2_passthru_handles check failed at line %d\n", line);
        std::abort();
    }
}

static std::uint32_t Bits(SceFiosFH handle) {
    return static_cast<std::uint32_t>(handle);
}

int main() {
    Require(sceFiosFilenoToFH(-1) == 0, __LINE__);
    Require(sceFiosFHToFileno(0) == -1, __LINE__);

    const SceFiosFH first = sceFiosFilenoToFH(5);
    Require(Bits(first) == 0x7fff8000u, __LINE__);
    Require(sceFiosFHToFileno(first) == 5, __LINE__);
    Require(sceFiosFilenoToFH(5) == first, __LINE__);

    const SceFiosFH second = sceFiosFilenoToFH(9);
    Require(Bits(second) == 0x7fff8001u, __LINE__);
    Require(sceFiosFHToFileno(second) == 9, __LINE__);

    sceFiosDeallocatePassthruFH(first);
    Require(sceFiosFHToFileno(first) == -1, __LINE__);
    Require(sceFiosFHToFileno(second) == 9, __LINE__);
    const SceFiosFH reused = sceFiosFilenoToFH(11);
    Require(reused == first && sceFiosFHToFileno(reused) == 11, __LINE__);

    Require(sceFiosFHToFileno(static_cast<SceFiosFH>(0x7fff80ffu)) == -1, __LINE__);
    Require(sceFiosFHToFileno(static_cast<SceFiosFH>(0x7fff8100u)) == -1, __LINE__);
    Require(sceFiosFHToFileno(static_cast<SceFiosFH>(0x00000001u)) == -1, __LINE__);
    sceFiosDeallocatePassthruFH(0);
    sceFiosDeallocatePassthruFH(static_cast<SceFiosFH>(0x7fff8100u));
    Require(sceFiosFHToFileno(reused) == 11 && sceFiosFHToFileno(second) == 9, __LINE__);

    for (int descriptor = 100; descriptor < 354; ++descriptor) {
        const SceFiosFH handle = sceFiosFilenoToFH(descriptor);
        Require(Bits(handle) == 0x7fff8000u + static_cast<std::uint32_t>(descriptor - 98), __LINE__);
    }
    Require(sceFiosFilenoToFH(354) == 0, __LINE__);
    Require(sceFiosFilenoToFH(9) == second, __LINE__);
    Require(sceFiosFHToFileno(static_cast<SceFiosFH>(0x7fff80ffu)) == 353, __LINE__);
    sceFiosDeallocatePassthruFH(static_cast<SceFiosFH>(0x7fff8010u));
    Require(Bits(sceFiosFilenoToFH(354)) == 0x7fff8010u, __LINE__);
    return 0;
}
