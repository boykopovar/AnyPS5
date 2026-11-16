#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <thread>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc CurrentThreadId check failed at line %u\n", location.line());
    std::abort();
}

int CurrentThreadId() {
    return _ZN3rtc15CurrentThreadIdEv_nid_postfix();
}

void TestPositiveAndStable() {
    const int first = CurrentThreadId();
    Require(first > 0);
    Require(CurrentThreadId() == first);
}

void TestDistinctPerThread() {
    const int mine = CurrentThreadId();
    int other = 0;
    std::thread worker([&other] { other = CurrentThreadId(); });
    worker.join();
    Require(other > 0);
    Require(other != mine);
}

void TestMatchesGuestThreadId() {
    Require(CurrentThreadId() == scePthreadGetthreadid());
    int other = 0;
    int otherGuest = 0;
    std::thread worker([&other, &otherGuest] {
        other = CurrentThreadId();
        otherGuest = scePthreadGetthreadid();
    });
    worker.join();
    Require(other == otherGuest);
}

}

int main() {
    TestPositiveAndStable();
    TestDistinctPerThread();
    TestMatchesGuestThreadId();
    std::puts("libwebrtc CurrentThreadId tests passed");
}
