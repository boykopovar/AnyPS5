#include "prx/libwebrtc/libwebrtc.hpp"
#include "Testing.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>

int main() {
    __builtin_cpu_init();
    WebRtcAvailableCpuFeatures features = _ZN6webrtc23GetAvailableCpuFeaturesEv_nid_postfix();
    Require(features.sse2);
    Require(features.sse2 == (__builtin_cpu_supports("sse2") != 0));
    Require(features.avx2 == (__builtin_cpu_supports("avx2") != 0 && __builtin_cpu_supports("bmi2") != 0));
    Require(!features.neon);
    WebRtcAvailableCpuFeatures again = _ZN6webrtc23GetAvailableCpuFeaturesEv_nid_postfix();
    Require(again.sse2 == features.sse2 && again.avx2 == features.avx2 && again.neon == features.neon);
    std::printf("sse2=%d avx2=%d neon=%d\n", features.sse2, features.avx2, features.neon);
    return 0;
}
