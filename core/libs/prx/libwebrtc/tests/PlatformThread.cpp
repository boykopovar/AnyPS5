#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/include/General.hpp"

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

const int vtable = 0;
void* joined = nullptr;
int joins = 0;
int joinResult = 0;

int APS5_VABI RecordJoin(void* thread, void**) {
    joined = thread;
    ++joins;
    return joinResult;
}

void ResetJoins(int result) {
    joined = nullptr;
    joins = 0;
    joinResult = result;
}

WebRtcPlatformThread Thread(void* handle, bool hasHandle, bool joinable) {
    WebRtcPlatformThread thread;
    std::memset(&thread, 0xA5, sizeof(thread));
    thread.vtable = &vtable;
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

    ResetJoins(0);
    _ZN3rtc14PlatformThread8FinalizeEv_nid_postfix(&detached);
    Require(joins == 0 && detached.vtable == &vtable);
}

void TestJoin() {
    int handle = 0;
    ResetJoins(0);
    WebRtcPlatformThread joinable = Thread(&handle, true, true);
    _ZN3rtc14PlatformThread8FinalizeEv_nid_postfix(&joinable);
    Require(joins == 1 && joined == &handle && !joinable.hasHandle && joinable.joinable && joinable.vtable == &vtable);
    _ZN3rtc14PlatformThread8FinalizeEv_nid_postfix(&joinable);
    Require(joins == 1);

    ResetJoins(0);
    WebRtcPlatformThread destroyed = Thread(&handle, true, true);
    _ZN3rtc14PlatformThreadD1Ev_nid_postfix(&destroyed);
    Require(joins == 1 && joined == &handle && !destroyed.hasHandle);

    ResetJoins(3);
    WebRtcPlatformThread failing = Thread(&handle, true, true);
    RequireThrows<std::runtime_error>([&] { _ZN3rtc14PlatformThread8FinalizeEv_nid_postfix(&failing); });
    Require(joins == 1);
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

    Require(target.vtable == &vtable && source.vtable == &vtable);

    ResetJoins(0);
    WebRtcPlatformThread busy = Thread(&first, true, true);
    WebRtcPlatformThread other = Thread(&second, true, false);
    _ZN3rtc14PlatformThreadaSEOS0__nid_postfix(&busy, &other);
    Require(joins == 1 && joined == &first);
    Require(busy.handle == &second && busy.hasHandle && !busy.joinable && !other.hasHandle);
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
    RegisterPthreadJoin_nid_no_patch(RecordJoin);
    TestFinalize();
    TestJoin();
    TestDestructor();
    TestMoveAssignment();
    TestNullThreadsThrow();
    return 0;
}
