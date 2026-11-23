#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <thread>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc webrtc::SmoothingFilterImpl check failed at line %u\n", location.line());
    std::abort();
}

using AddSample = void (APS5_VABI*)(WebRtcSmoothingFilterImpl*, float);
using GetAverage = WebRtcOptionalFloat (APS5_VABI*)(WebRtcSmoothingFilterImpl*);
using SetTimeConstantMs = bool (APS5_VABI*)(WebRtcSmoothingFilterImpl*, int);
using Destroy = void (APS5_VABI*)(WebRtcSmoothingFilterImpl*);

template<class TFunction>
TFunction Slot(const WebRtcSmoothingFilterImpl& filter, int slot) {
    return reinterpret_cast<TFunction>(const_cast<void*>(filter.vtable[slot]));
}

void Add(WebRtcSmoothingFilterImpl& filter, float sample) {
    Slot<AddSample>(filter, 2)(&filter, sample);
}

WebRtcOptionalFloat Average(WebRtcSmoothingFilterImpl& filter) {
    return Slot<GetAverage>(filter, 3)(&filter);
}

bool SetTimeConstant(WebRtcSmoothingFilterImpl& filter, int timeConstantMilliseconds) {
    return Slot<SetTimeConstantMs>(filter, 4)(&filter, timeConstantMilliseconds);
}

void WaitAMillisecond() {
    const std::int64_t start = _ZN3rtc10TimeMillisEv_nid_postfix();
    while (_ZN3rtc10TimeMillisEv_nid_postfix() < start + 2) std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

void TestDestructorWritesNothing() {
    static unsigned char object[64];
    std::memset(object, 0xA5, sizeof(object));
    _ZN6webrtc19SmoothingFilterImplD1Ev_nid_postfix(object);
    for (unsigned char byte : object)
        Require(byte == 0xA5);
}

void TestConstructorWithoutInitialization() {
    WebRtcSmoothingFilterImpl filter{};
    filter.hasInitEndTime = true;
    _ZN6webrtc19SmoothingFilterImplC1Ei_nid_postfix(&filter, 0);
    Require(filter.vtable != nullptr);
    Require(filter.initTimeMilliseconds == 0);
    Require(filter.initFactor == 0.0f && filter.initConst == 0.0f && filter.alpha == 0.0f);
    Require(!filter.hasInitEndTime);
    Require(!Average(filter).engaged);
    Require(!SetTimeConstant(filter, 10));
}

void TestConstructorFactors() {
    WebRtcSmoothingFilterImpl one{};
    _ZN6webrtc19SmoothingFilterImplC1Ei_nid_postfix(&one, 1);
    Require(one.initFactor == 1.0f && one.initConst == 0.0f);
    Require(one.alpha == std::exp(-1.0f));

    WebRtcSmoothingFilterImpl hundred{};
    _ZN6webrtc19SmoothingFilterImplC1Ei_nid_postfix(&hundred, 100);
    Require(hundred.initTimeMilliseconds == 100);
    Require(std::fabs(hundred.initFactor - std::pow(100.0f, -0.01f)) <= 1e-6f);
    Require(std::fabs(hundred.initConst - (100.0f - std::pow(100.0f, 0.99f))) <= 1e-4f);
    Require(std::fabs(hundred.alpha - std::exp(-0.01f)) <= 1e-6f);
}

void TestFirstSampleAndTimeConstant() {
    WebRtcSmoothingFilterImpl filter{};
    _ZN6webrtc19SmoothingFilterImplC1Ei_nid_postfix(&filter, 0);
    const std::int64_t before = _ZN3rtc10TimeMillisEv_nid_postfix();
    Add(filter, 5.0f);
    const std::int64_t after = _ZN3rtc10TimeMillisEv_nid_postfix();
    Require(filter.hasInitEndTime);
    Require(filter.state == 5.0f && filter.lastSample == 5.0f);
    Require(filter.lastStateTimeMilliseconds >= before && filter.lastStateTimeMilliseconds <= after);
    Require(filter.initEndTimeMilliseconds == filter.lastStateTimeMilliseconds);
    const auto average = Average(filter);
    Require(average.engaged && average.storage.value == 5.0f);
    Require(SetTimeConstant(filter, 10));
    Require(filter.alpha == std::exp(-0.1f));
}

void TestTimeConstantRefusedDuringInitialization() {
    WebRtcSmoothingFilterImpl filter{};
    _ZN6webrtc19SmoothingFilterImplC1Ei_nid_postfix(&filter, 100000);
    const float alpha = filter.alpha;
    Add(filter, 2.0f);
    Require(filter.initEndTimeMilliseconds == filter.lastStateTimeMilliseconds + 100000);
    Require(!SetTimeConstant(filter, 10));
    Require(filter.alpha == alpha);
    const auto average = Average(filter);
    Require(average.engaged && average.storage.value == 2.0f);
}

void TestZeroAlphaFollowsTheLastSample() {
    WebRtcSmoothingFilterImpl filter{};
    _ZN6webrtc19SmoothingFilterImplC1Ei_nid_postfix(&filter, 0);
    Add(filter, 5.0f);
    WaitAMillisecond();
    Add(filter, 9.0f);
    Require(filter.state == 5.0f && filter.lastSample == 9.0f);
    WaitAMillisecond();
    const auto average = Average(filter);
    Require(average.engaged && average.storage.value == 9.0f);
}

void TestDeletingDestructor() {
    auto* filter = static_cast<WebRtcSmoothingFilterImpl*>(_Znwm_nid_postfix(sizeof(WebRtcSmoothingFilterImpl)));
    _ZN6webrtc19SmoothingFilterImplC1Ei_nid_postfix(filter, 20);
    Slot<Destroy>(*filter, 1)(filter);
}

}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    TestDestructorWritesNothing();
    TestConstructorWithoutInitialization();
    TestConstructorFactors();
    TestFirstSampleAndTimeConstant();
    TestTimeConstantRefusedDuringInitialization();
    TestZeroAlphaFollowsTheLastSample();
    TestDeletingDestructor();
    return 0;
}
