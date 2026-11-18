#include "prx/libSceAc3Enc/libSceAc3Enc.hpp"

#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

} // namespace

int main() {
    constexpr int invalidHandle = static_cast<int>(0x80E00001u);
    constexpr int notEncoding = static_cast<int>(0x80E00009u);
    for (int call = 0; call < 2; ++call) {
        Require(sceAc3EncDeleteEncoder() == invalidHandle);
        Require(sceAc3EncEncode() == invalidHandle);
        Require(sceAc3EncEncodeAsync() == invalidHandle);
        Require(sceAc3EncFlush() == invalidHandle);
        Require(sceAc3EncWaitEncode() == notEncoding);
        Require(sceAc3EncIsEncoding() == 0);
    }
    return 0;
}
