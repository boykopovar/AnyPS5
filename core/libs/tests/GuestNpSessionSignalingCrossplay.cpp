#include "prx/libSceNpSessionSignalingCrossplay/libSceNpSessionSignalingCrossplay.hpp"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int ErrorInvalidArgument = static_cast<int>(0x80553D03u);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

} // namespace

int main() {
    std::array<std::uint8_t, 64> buffer{};
    void* p = buffer.data();
    for (const int expectedErrno : {0, 7, 42}) {
        errno = expectedErrno;
        Require(sceNpSessionSignalingCrossplayInitialize(nullptr) == ErrorInvalidArgument,
                "Initialize must reject a null parameter");
        Require(sceNpSessionSignalingCrossplayGetMemoryInfo(nullptr) == ErrorInvalidArgument,
                "GetMemoryInfo must reject a null info");
        Require(sceNpSessionSignalingCrossplayUnknown02(1, nullptr, 2, p, p) == ErrorInvalidArgument,
                "wypjyjGWaLM must reject a null second argument");
        Require(sceNpSessionSignalingCrossplayUnknown02(1, p, 2, nullptr, p) == ErrorInvalidArgument,
                "wypjyjGWaLM must reject a null fourth argument");
        Require(sceNpSessionSignalingCrossplayUnknown02(1, p, 2, p, nullptr) == ErrorInvalidArgument,
                "wypjyjGWaLM must reject a null fifth argument");
        Require(sceNpSessionSignalingCrossplayUnknown02(0, nullptr, 0, nullptr, nullptr) == ErrorInvalidArgument,
                "wypjyjGWaLM must reject all null pointers");
        Require(errno == expectedErrno, "The null checks must not change errno");
    }
    Require(buffer == std::array<std::uint8_t, 64>{}, "Rejected calls must not write");
}
