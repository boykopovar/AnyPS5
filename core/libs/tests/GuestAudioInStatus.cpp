#include "prx/libSceAudioIn/include/AudioIn.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>

static void Require(bool value) { if (!value) std::abort(); }

namespace {

constexpr int user = 0x10000000;
constexpr int invalidHandle = static_cast<int>(0x80260101);
constexpr int invalidPointer = static_cast<int>(0x80260105);
constexpr int invalidState = static_cast<int>(0x80260109);

void TestStatusQuery() {
    Require(sceAudioInInit() == 0);
    Require(sceAudioInGetHandleStatusInfo(0, nullptr) == invalidHandle);
    Require(sceAudioInGetHandleStatusInfo(9999, nullptr) == invalidHandle);

    int port = sceAudioInOpen(user, 0, 0, 256, 48000, 2);
    Require(port > 0);
    Require(sceAudioInGetHandleStatusInfo(port, nullptr) == invalidPointer);
    Require(sceAudioInClose(port) == 0);
    Require(sceAudioInGetHandleStatusInfo(port, nullptr) == invalidState);
}

} // namespace

int main() {
    TestStatusQuery();
    return 0;
}
