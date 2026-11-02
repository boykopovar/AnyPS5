#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc PlatformThread check failed at line %u\n", location.line());
    std::abort();
}

template<typename TException, typename TAction> void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const TException&) {
        return;
    }
    Require(false, location);
}

WebRtcPlatformThread Thread(void* handle, bool hasHandle, bool joinable) {
    WebRtcPlatformThread thread;
    std::memset(&thread, 0xA5, sizeof(thread));
    thread.handle = handle;
    thread.hasHandle = hasHandle;
    thread.joinable = joinable;
    return thread;
}

void TestFinalize() {
    int handle = 0;
    WebRtcPlatformThread empty = Thread(nullptr, false, true);
    _ZN3rtc14PlatformThread8FinalizeEv_nid_postfix(&empty);
    Require(!empty.hasHandle && empty.joinable);

    WebRtcPlatformThread detached = Thread(&handle, true, false);
    _ZN3rtc14PlatformThread8FinalizeEv_nid_postfix(&detached);
    Require(!detached.hasHandle && detached.handle == &handle && !detached.joinable);
    _ZN3rtc14PlatformThread8FinalizeEv_nid_postfix(&detached);
    Require(!detached.hasHandle);

    WebRtcPlatformThread joinable = Thread(&handle, true, true);
    RequireThrows<std::runtime_error>([&] { _ZN3rtc14PlatformThread8FinalizeEv_nid_postfix(&joinable); });
    Require(joinable.hasHandle && joinable.joinable);
    RequireThrows<std::runtime_error>([&] { _ZN3rtc14PlatformThreadD1Ev_nid_postfix(&joinable); });
}

void TestDestructor() {
    int handle = 0;
    WebRtcPlatformThread detached = Thread(&handle, true, false);
    _ZN3rtc14PlatformThreadD1Ev_nid_postfix(&detached);
    Require(!detached.hasHandle);
    WebRtcPlatformThread empty = Thread(nullptr, false, false);
    _ZN3rtc14PlatformThreadD1Ev_nid_postfix(&empty);
    Require(!empty.hasHandle);
}

void TestMoveAssignment() {
    int first = 0;
    int second = 0;
    WebRtcPlatformThread target = Thread(&first, true, false);
    WebRtcPlatformThread source = Thread(&second, true, true);
    Require(_ZN3rtc14PlatformThreadaSEOS0__nid_postfix(&target, &source) == &target);
    Require(target.handle == &second && target.hasHandle && target.joinable);
    Require(!source.hasHandle && source.joinable);

    WebRtcPlatformThread emptyTarget = Thread(nullptr, false, false);
    WebRtcPlatformThread emptySource = Thread(nullptr, false, true);
    _ZN3rtc14PlatformThreadaSEOS0__nid_postfix(&emptyTarget, &emptySource);
    Require(!emptyTarget.hasHandle && emptyTarget.joinable && !emptySource.hasHandle);

    WebRtcPlatformThread busy = Thread(&first, true, true);
    WebRtcPlatformThread other = Thread(&second, true, false);
    RequireThrows<std::runtime_error>([&] { _ZN3rtc14PlatformThreadaSEOS0__nid_postfix(&busy, &other); });
    Require(busy.handle == &first && busy.hasHandle && other.hasHandle);
}

void TestNullThreadsThrow() {
    WebRtcPlatformThread thread = Thread(nullptr, false, false);
    RequireThrows<std::invalid_argument>([] { _ZN3rtc14PlatformThread8FinalizeEv_nid_postfix(nullptr); });
    RequireThrows<std::invalid_argument>([] { _ZN3rtc14PlatformThreadD1Ev_nid_postfix(nullptr); });
    RequireThrows<std::invalid_argument>([&] { _ZN3rtc14PlatformThreadaSEOS0__nid_postfix(nullptr, &thread); });
    RequireThrows<std::invalid_argument>([&] { _ZN3rtc14PlatformThreadaSEOS0__nid_postfix(&thread, nullptr); });
}

}

int main() {
    TestFinalize();
    TestDestructor();
    TestMoveAssignment();
    TestNullThreadsThrow();
    return 0;
}
