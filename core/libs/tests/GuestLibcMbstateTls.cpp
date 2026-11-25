#include "prx/libc/include/Api.hpp"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

constexpr auto Invalid = static_cast<std::size_t>(-1);
constexpr auto Incomplete = static_cast<std::size_t>(-2);

static void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "failed: %s\n", what);
    std::abort();
}

static void CheckGetpmbstateMainThread() {
    auto* pmb1 = _Getpmbstate_nid_postfix();
    auto* pmb2 = _Getpmbstate_nid_postfix();
    Require(pmb1 != nullptr, "pmb1 is non-null");
    Require(pmb2 != nullptr, "pmb2 is non-null");
    Require(pmb1 == pmb2, "pmb1 == pmb2 on main thread");

    auto* pwc1 = _Getpwcstate_nid_postfix();
    auto* pwc2 = _Getpwcstate_nid_postfix();
    Require(pwc1 != nullptr, "pwc1 is non-null");
    Require(pwc2 != nullptr, "pwc2 is non-null");
    Require(pwc1 == pwc2, "pwc1 == pwc2 on main thread");

    Require(pmb1 != pwc1, "pmb1 != pwc1 distinct state objects");

    mbstate_t zero{};
    Require(std::memcmp(pmb1, &zero, sizeof(mbstate_t)) == 0, "pmb1 zero-initialized");
    Require(std::memcmp(pwc1, &zero, sizeof(mbstate_t)) == 0, "pwc1 zero-initialized");

    *reinterpret_cast<unsigned char*>(pmb1) = 0x42;
    Require(std::memcmp(pwc1, &zero, sizeof(mbstate_t)) == 0, "mutating pmb1 does not change pwc1");
}

static void CheckGetpmbstateThreadLocal() {
    std::atomic<bool> t_pmb_zero{false};
    std::atomic<bool> t_pwc_zero{false};

    auto* main_pmb = _Getpmbstate_nid_postfix();
    auto* main_pwc = _Getpwcstate_nid_postfix();
    *reinterpret_cast<unsigned char*>(main_pmb) = 0x99;

    std::thread t([&] {
        auto* tp = _Getpmbstate_nid_postfix();
        auto* tw = _Getpwcstate_nid_postfix();
        Require(tp != nullptr, "thread pmb non-null");
        Require(tw != nullptr, "thread pwc non-null");
        Require(tp != tw, "thread pmb != thread pwc");
        Require(tp != main_pmb, "thread pmb != main pmb");
        Require(tw != main_pwc, "thread pwc != main pwc");

        mbstate_t zero{};
        t_pmb_zero.store(std::memcmp(tp, &zero, sizeof(mbstate_t)) == 0);
        t_pwc_zero.store(std::memcmp(tw, &zero, sizeof(mbstate_t)) == 0);
    });

    t.join();

    Require(t_pmb_zero.load(), "thread pmb initially zero");
    Require(t_pwc_zero.load(), "thread pwc initially zero");
}

static void CheckMbrtowcThreadLocal() {
    std::atomic<int> stage{0};
    std::atomic<std::size_t> t1_final_result{0};
    std::atomic<std::uint16_t> t1_final_w{0};
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);

    std::thread t1([&] {
        std::uint16_t w1 = 0;
        auto r1 = mbrtowc_nid_postfix(&w1, "\xe2", 1, nullptr);
        Require(r1 == Incomplete, "t1 first byte incomplete");
        stage.store(1);
        while (stage.load() < 2 && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }
        Require(stage.load() >= 2, "t1 timed out waiting for t2");

        auto r2 = mbrtowc_nid_postfix(&w1, "\x82\xac", 2, nullptr);
        t1_final_result.store(r2);
        t1_final_w.store(w1);
    });

    std::thread t2([&] {
        while (stage.load() < 1 && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }
        Require(stage.load() >= 1, "t2 timed out waiting for t1");

        std::uint16_t w2 = 0;
        *__error_nid_postfix() = 0;
        auto r1 = mbrtowc_nid_postfix(&w2, "\x82\xac", 2, nullptr);
        Require(r1 == Invalid, "t2 first call invalid");
        Require(*__error_nid_postfix() == 86, "t2 errno EILSEQ");

        std::uint16_t w3 = 0;
        *__error_nid_postfix() = 0;
        auto r2 = mbrtowc_nid_postfix(&w3, "\xe2\x82\xac", 3, nullptr);
        Require(r2 == 3, "t2 full decode returns 3");
        Require(w3 == 0x20ac, "t2 full decode U+20AC");
        Require(*__error_nid_postfix() == 0, "t2 full decode errno 0");

        std::uint16_t w4 = 0;
        auto r3 = mbrtowc_nid_postfix(&w4, "\xe2", 1, nullptr);
        Require(r3 == Incomplete, "t2 partial incomplete");

        stage.store(2);
    });

    t1.join();
    t2.join();

    Require(t1_final_result.load() == 2, "t1 completes pending char");
    Require(t1_final_w.load() == 0x20ac, "t1 final char U+20AC");
}

static void CheckWcrtombNullState() {
    char buf[8]{};
    auto r = wcrtomb_nid_postfix(buf, 0x20ac, nullptr);
    Require(r == 3, "wcrtomb returns 3");
    Require(std::memcmp(buf, "\xe2\x82\xac", 3) == 0, "wcrtomb encodes UTF-8");
}

int main() {
    CheckGetpmbstateMainThread();
    CheckGetpmbstateThreadLocal();
    CheckMbrtowcThreadLocal();
    CheckWcrtombNullState();
    return 0;
}
