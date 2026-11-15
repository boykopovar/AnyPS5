#include "prx/libwebrtc/libwebrtc.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc ExpFilter check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

void Reset(WebRtcExpFilter& filter, float alpha) {
    _ZN3rtc9ExpFilter5ResetEf_nid_postfix(&filter, alpha);
}

float Apply(WebRtcExpFilter& filter, float exponent, float sample) {
    return _ZN3rtc9ExpFilter5ApplyEff_nid_postfix(&filter, exponent, sample);
}

void TestReset() {
    WebRtcExpFilter filter {0.25f, 7.0f, 3.0f};
    Reset(filter, 0.5f);
    Require(filter.alpha == 0.5f);
    Require(filter.filtered == WebRtcExpFilterValueUndefined);
    Require(filter.max == 3.0f);
}

void TestApply() {
    WebRtcExpFilter filter {0.0f, 0.0f, WebRtcExpFilterValueUndefined};
    Reset(filter, 0.5f);
    Require(Apply(filter, 1.0f, 4.0f) == 4.0f);
    Require(Apply(filter, 1.0f, 8.0f) == 6.0f);
    Require(Apply(filter, 2.0f, 10.0f) == 9.0f);
    Require(filter.filtered == 9.0f);
    float alpha = std::pow(0.5f, 0.5f);
    float expected = alpha * 9.0f + (1 - alpha) * 1.0f;
    Require(Apply(filter, 0.5f, 1.0f) == expected);
    Require(Apply(filter, 1.0f, 1000.0f) > 400.0f);
}

void TestUndefinedOutputRestarts() {
    WebRtcExpFilter filter {0.0f, 0.0f, WebRtcExpFilterValueUndefined};
    Reset(filter, 0.5f);
    Require(Apply(filter, 1.0f, 0.0f) == 0.0f);
    Require(Apply(filter, 1.0f, -2.0f) == -1.0f);
    Require(Apply(filter, 1.0f, 10.0f) == 10.0f);
}

void TestMax() {
    WebRtcExpFilter filter {0.0f, 0.0f, 5.0f};
    Reset(filter, 0.5f);
    Require(Apply(filter, 1.0f, 9.0f) == 5.0f);
    Require(Apply(filter, 1.0f, 1.0f) == 3.0f);
    Require(Apply(filter, 1.0f, 5.0f) == 4.0f);
    Require(filter.max == 5.0f);
}

void TestNullFilter() {
    RequireInvalidArgument([] { _ZN3rtc9ExpFilter5ResetEf_nid_postfix(nullptr, 0.5f); });
    RequireInvalidArgument([] { _ZN3rtc9ExpFilter5ApplyEff_nid_postfix(nullptr, 1.0f, 1.0f); });
}

}

int main() {
    TestReset();
    TestApply();
    TestUndefinedOutputRestarts();
    TestMax();
    TestNullFilter();
    std::puts("libwebrtc ExpFilter tests passed");
}
