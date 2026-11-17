#include "prx/libSceDtsEnc/libSceDtsEnc.hpp"
#include "Testing.hpp"

#include <cstdlib>

int main() {
    constexpr int invalidHandle = static_cast<int>(0x80FA0001u);
    constexpr int notEncoding = static_cast<int>(0x80FA0009u);
    for (int call = 0; call < 2; ++call) {
        Require(sceDtsEncDeleteEncoder() == invalidHandle);
        Require(sceDtsEncEncode() == invalidHandle);
        Require(sceDtsEncEncodeAsync() == invalidHandle);
        Require(sceDtsEncFlush() == invalidHandle);
        Require(sceDtsEncWaitEncode() == notEncoding);
        Require(sceDtsEncIsEncoding() == 0);
    }
    return 0;
}
