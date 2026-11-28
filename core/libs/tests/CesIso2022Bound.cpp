#include "prx/libSceCesCs/Export.cpp"
#include <cstdint>
#include <cstdio>

int main() {
    Iso2022State state;
    state.set = Iso2022Set::Jis0208;
    const uint8_t bound[2] = {0x7E, 0x7F};
    uint32_t length = 0;
    uint32_t code = 0;
    const int result = decodeIso2022(PROFILES[0], state, bound, 2, length, code);
    if (result != CES_ERROR_UNASSIGNED_CODE || length != 1) {
        std::printf("FAIL: result=0x%08X length=%u\n", static_cast<unsigned>(result), static_cast<unsigned>(length));
        return 1;
    }
    std::printf("PASS: result=0x%08X length=%u\n", static_cast<unsigned>(result), static_cast<unsigned>(length));
    return 0;
}
