#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "Testing.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <thread>

namespace {

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
