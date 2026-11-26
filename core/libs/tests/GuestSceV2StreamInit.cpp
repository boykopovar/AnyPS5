#include "prx/libc/include/Api.hpp"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <thread>
#include <vector>

namespace {

void Require(bool condition, const char* label) {
    if (condition) return;
    std::fprintf(stderr, "sce v2 stream init: %s\n", label);
    std::abort();
}

template <typename Worker>
void RunConcurrent(int thread_count, Worker&& worker) {
    std::atomic<int> ready{0};
    std::vector<std::thread> threads;
    threads.reserve(static_cast<std::size_t>(thread_count));
    for (int index = 0; index < thread_count; ++index) {
        threads.emplace_back([&, index] {
            ready.fetch_add(1, std::memory_order_acq_rel);
            while (ready.load(std::memory_order_acquire) < thread_count) {
            }
            worker(index);
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
}

}

int main() {
    std::int32_t& wide = _ZNSt8__sce_v26_Winit9_Init_cntE_nid_postfix;
    std::int32_t& narrow = _ZNSt8__sce_v28ios_base4Init9_Init_cntE_nid_postfix;
    std::uint64_t self[2] {};
    Require(wide == -1 && narrow == -1, "both counters start at -1");

    _ZNSt8__sce_v26_WinitC1Ev_nid_postfix(self);
    Require(wide == 1, "the first _Winit raises -1 to 1");
    _ZNSt8__sce_v26_WinitC2Ev_nid_postfix(self);
    Require(wide == 2, "a second _Winit counts up");
    _ZNSt8__sce_v26_WinitD1Ev_nid_postfix(self);
    _ZNSt8__sce_v26_WinitD2Ev_nid_postfix(self);
    Require(wide == 0, "the destructors count down to 0");
    _ZNSt8__sce_v26_WinitD1Ev_nid_postfix(self);
    Require(wide == -1, "a destructor below 0 keeps counting down");
    wide = -5;
    _ZNSt8__sce_v26_WinitC1Ev_nid_postfix(self);
    Require(wide == 1, "a negative count restarts at 1");
    Require(narrow == -1, "_Winit leaves ios_base::Init alone");

    Require(_ZNSt8__sce_v28ios_base4Init14_Init_cnt_funcEv_nid_postfix() == &narrow, "_Init_cnt_func returns the counter");
    _ZNSt8__sce_v28ios_base4Init10_Init_ctorEPS1__nid_postfix(self);
    Require(narrow == 1, "the first _Init_ctor raises -1 to 1");
    _ZNSt8__sce_v28ios_base4Init10_Init_ctorEPS1__nid_postfix(self);
    _ZNSt8__sce_v28ios_base4Init10_Init_dtorEPS1__nid_postfix(self);
    _ZNSt8__sce_v28ios_base4Init10_Init_dtorEPS1__nid_postfix(self);
    Require(narrow == 0, "_Init_dtor counts down to 0");
    _ZNSt8__sce_v28ios_base4Init10_Init_dtorEPS1__nid_postfix(self);
    Require(narrow == -1, "_Init_dtor below 0 keeps counting down");
    Require(wide == 1, "ios_base::Init leaves _Winit alone");

    constexpr int kThreads = 8;
    constexpr int kIterations = 25000;
    wide = 0;
    RunConcurrent(kThreads, [&](int) {
        std::uint64_t local_self[2] {};
        for (int i = 0; i < kIterations; ++i) {
            _ZNSt8__sce_v26_WinitC1Ev_nid_postfix(local_self);
            _ZNSt8__sce_v26_WinitC2Ev_nid_postfix(local_self);
        }
    });
    Require(wide == kThreads * kIterations * 2, "concurrent _Winit increments preserve exact count");

    wide = 1000000;
    RunConcurrent(kThreads, [&](int index) {
        std::uint64_t local_self[2] {};
        for (int i = 0; i < kIterations; ++i) {
            if ((index & 1) == 0) {
                _ZNSt8__sce_v26_WinitC1Ev_nid_postfix(local_self);
            } else {
                _ZNSt8__sce_v26_WinitD1Ev_nid_postfix(local_self);
            }
        }
    });
    Require(wide == 1000000, "concurrent _Winit increment and decrement preserve exact count");

    narrow = 1000000;
    RunConcurrent(kThreads, [&](int index) {
        std::uint64_t local_self[2] {};
        for (int i = 0; i < kIterations; ++i) {
            if ((index & 1) == 0) {
                _ZNSt8__sce_v28ios_base4Init10_Init_ctorEPS1__nid_postfix(local_self);
            } else {
                _ZNSt8__sce_v28ios_base4Init10_Init_dtorEPS1__nid_postfix(local_self);
            }
        }
    });
    Require(narrow == 1000000, "concurrent ios_base::Init increment and decrement preserve exact count");

    constexpr std::int32_t kInt32Max = std::numeric_limits<std::int32_t>::max();
    constexpr std::int32_t kInt32Min = std::numeric_limits<std::int32_t>::min();

    wide = kInt32Max;
    _ZNSt8__sce_v26_WinitC1Ev_nid_postfix(self);
    Require(wide == kInt32Min, "_WinitC1Ev wraps INT32_MAX to INT32_MIN");
    _ZNSt8__sce_v26_WinitD1Ev_nid_postfix(self);
    Require(wide == kInt32Max, "_WinitD1Ev wraps INT32_MIN to INT32_MAX");

    wide = kInt32Max;
    _ZNSt8__sce_v26_WinitC2Ev_nid_postfix(self);
    Require(wide == kInt32Min, "_WinitC2Ev wraps INT32_MAX to INT32_MIN");
    _ZNSt8__sce_v26_WinitD2Ev_nid_postfix(self);
    Require(wide == kInt32Max, "_WinitD2Ev wraps INT32_MIN to INT32_MAX");
    wide = kInt32Min;
    _ZNSt8__sce_v26_WinitC2Ev_nid_postfix(self);
    Require(wide == 1, "_WinitC2Ev resets INT32_MIN to 1");

    narrow = kInt32Max;
    _ZNSt8__sce_v28ios_base4Init10_Init_ctorEPS1__nid_postfix(self);
    Require(narrow == kInt32Min, "_Init_ctor wraps INT32_MAX to INT32_MIN");
    _ZNSt8__sce_v28ios_base4Init10_Init_dtorEPS1__nid_postfix(self);
    Require(narrow == kInt32Max, "_Init_dtor wraps INT32_MIN to INT32_MAX");
    narrow = kInt32Min;
    _ZNSt8__sce_v28ios_base4Init10_Init_ctorEPS1__nid_postfix(self);
    Require(narrow == 1, "_Init_ctor resets INT32_MIN to 1");

    return 0;
}
