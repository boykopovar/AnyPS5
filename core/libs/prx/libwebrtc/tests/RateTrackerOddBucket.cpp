#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cfenv>
#include <cfloat>
#include <cstdio>
#include <limits>
#include <stdexcept>

#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ != 0)
#error "Normal floating-point evaluation is required"
#endif
#if FLT_EVAL_METHOD != 0
#error "Binary64 evaluation is required"
#endif
static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559);
static_assert(std::numeric_limits<double>::radix == 2 && std::numeric_limits<double>::digits == 53);

namespace {
std::int64_t timeMilliseconds = 0;
bool constructed = false;
bool destroyed = false;
std::int64_t APS5_VABI ReadTime(const WebRtcRateTracker*) { return timeMilliseconds; }

struct Tracker {
    WebRtcRateTracker object{};
    std::array<void*, 3> table{};
    Tracker() {
        _ZN3rtc11RateTrackerC1Elm_nid_postfix(&object, 3, 1);
        constructed = true;
        table = {object.vtable[0], object.vtable[1], reinterpret_cast<void*>(&ReadTime)};
        object.vtable = table.data();
    }
    ~Tracker() {
        _ZN3rtc11RateTrackerD1Ev_nid_postfix(&object);
        destroyed = true;
    }
    void CheckRate(std::int64_t time, double expected) {
        timeMilliseconds = time;
        const auto actual = _ZNK3rtc11RateTracker22ComputeRateForIntervalEl_nid_postfix(&object, 3);
        std::printf("time=%lld actual=%.17g expected=%.17g\n", static_cast<long long>(time), actual, expected);
        if (actual != expected) throw std::runtime_error("odd-width exact rate mismatch");
    }
    void AddSamples(std::int64_t count) { _ZN3rtc11RateTracker10AddSamplesEl_nid_postfix(&object, count); }
};
}

int main() {
    bool passed = false;
    try {
        std::array<void*, 10> heap{};
        ApplicationHeapRegister_nid_no_patch(heap.data());
        if (std::fesetround(FE_TONEAREST) != 0 || std::fegetround() != FE_TONEAREST)
            throw std::runtime_error("FE_TONEAREST unavailable");
        Tracker tracker;
        tracker.CheckRate(0, 0.0);
        tracker.AddSamples(1);
        tracker.CheckRate(0, 0.0);
        tracker.CheckRate(1, 0.0);
        tracker.CheckRate(2, 0.0);
        tracker.CheckRate(3, 1000.0 / 3.0);
        tracker.CheckRate(4, 1000.0 / 3.0);
        tracker.CheckRate(5, 0.0);
        tracker.CheckRate(6, 0.0);
        tracker.AddSamples(7);
        tracker.CheckRate(9, 7000.0 / 3.0);
        passed = true;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
    }
    std::printf("constructed=%d direct_D1_cleanup=%d result=%s\n", constructed, destroyed,
        passed && constructed && destroyed ? "PASS" : "FAIL");
    return passed && constructed && destroyed ? 0 : 1;
}
