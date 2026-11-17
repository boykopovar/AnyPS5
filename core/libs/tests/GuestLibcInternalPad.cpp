#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <thread>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal _Pad: %s\n", message);
    std::abort();
}

template <typename Function>
bool Throws(Function function) {
    try {
        function();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

struct TestPad {
    unsigned (APS5_VABI* const* vtable)(GuestPad*);
    void* condition;
    void* mutex;
    bool started;
};

struct MockThread {
    std::thread worker;
    void* result = nullptr;
};

static const char* lastCreatedName = nullptr;
static const void* lastCreatedAttr = nullptr;

static int APS5_VABI MockPthreadCreate(void** thread, const void* attrSlot, void* (APS5_VABI* entry)(void*), void* arg, const char* name) {
    lastCreatedName = name;
    lastCreatedAttr = attrSlot;
    auto* handle = new MockThread{};
    handle->worker = std::thread([handle, entry, arg] { handle->result = entry(arg); });
    *thread = handle;
    return 0;
}

static void JoinMockThread(void* thread) {
    auto* handle = static_cast<MockThread*>(thread);
    handle->worker.join();
    delete handle;
}

static unsigned APS5_VABI TestPadGo(GuestPad* base) {
    _ZNSt4_Pad8_ReleaseEv_nid_postfix(base);
    return 0;
}

}

int main() {
    GuestPad pad{};
    _ZNSt4_PadC2Ev_nid_postfix(&pad);
    Require(pad.condition != nullptr && pad.mutex != nullptr && !pad.started, "the base-object constructor creates the mutex and condition");

    sceLibcInternalPadCompleteDestructor_nid_no_patch(&pad);
    Require(pad.condition == nullptr && pad.mutex == nullptr, "the complete-object destructor releases the mutex and condition");
    Require(!pad.started && pad.vtable == nullptr, "the destructor leaves the other members alone");

    GuestPad again{};
    _ZNSt4_PadC2Ev_nid_postfix(&again);
    sceLibcInternalPadCompleteDestructor_nid_no_patch(&again);
    _ZNSt4_PadC2Ev_nid_postfix(&again);
    Require(again.condition != nullptr && again.mutex != nullptr, "a destroyed pad can be constructed again");
    _ZNSt4_PadD2Ev_nid_postfix(&again);

    Require(Throws([&] { sceLibcInternalPadCompleteDestructor_nid_no_patch(&pad); }), "destroying a pad twice is rejected");
    Require(Throws([] { sceLibcInternalPadCompleteDestructor_nid_no_patch(nullptr); }), "a null pad is rejected");

    GuestPad namedPad{};
    sceLibcInternalPadConstructorWithName_nid_no_patch(&namedPad, "namedWorker");
    Require(namedPad.condition != nullptr && namedPad.mutex != nullptr && !namedPad.started, "named constructor creates mutex and condition");
    sceLibcInternalPadCompleteDestructor_nid_no_patch(&namedPad);
    Require(namedPad.condition == nullptr && namedPad.mutex == nullptr, "complete destructor cleans named pad");

    _ZNSt4_PadC2EPKc_nid_postfix(&namedPad, "libcNamedWorker");
    Require(namedPad.condition != nullptr && namedPad.mutex != nullptr && !namedPad.started, "libc named constructor creates mutex and condition");
    _ZNSt4_PadD2Ev_nid_postfix(&namedPad);
    Require(namedPad.condition == nullptr && namedPad.mutex == nullptr, "libc destructor cleans named pad");

    Require(Throws([] { sceLibcInternalPadConstructorWithName_nid_no_patch(nullptr, "fail"); }), "null pad in named constructor is rejected");
    Require(Throws([] { _ZNSt4_PadC2EPKc_nid_postfix(nullptr, "fail"); }), "null pad in libc named constructor is rejected");

    unsigned (APS5_VABI* vtable[1])(GuestPad*) = {TestPadGo};

    TestPad launchPad{};
    launchPad.vtable = vtable;
    sceLibcInternalPadConstructorWithName_nid_no_patch(&launchPad, "launchTest");

    void* thread = nullptr;
    RegisterPthreadCreate_nid_no_patch(nullptr);
    Require(Throws([&] { sceLibcInternalPadLaunchWithName_nid_no_patch(&launchPad, "unreg", &thread); }), "launch without registered pthread_create throws");
    Require(Throws([&] { sceLibcInternalPadLaunchWithName_nid_no_patch(nullptr, "nullPad", &thread); }), "null pad in launch throws");
    Require(Throws([&] { sceLibcInternalPadLaunchWithName_nid_no_patch(&launchPad, "nullThread", nullptr); }), "null thread slot in launch throws");

    RegisterPthreadCreate_nid_no_patch(MockPthreadCreate);

    sceLibcInternalPadLaunchWithName_nid_no_patch(&launchPad, "workerOne", &thread);
    Require(thread != nullptr, "thread created");
    Require(launchPad.started, "pad started after launch");
    Require(lastCreatedName != nullptr && std::strcmp(lastCreatedName, "workerOne") == 0, "thread name forwarded");
    Require(lastCreatedAttr == nullptr, "attributes null for name-only launch");
    JoinMockThread(thread);
    sceLibcInternalPadCompleteDestructor_nid_no_patch(&launchPad);

    TestPad attrPad{};
    attrPad.vtable = vtable;
    _ZNSt4_PadC2EPKc_nid_postfix(reinterpret_cast<GuestPad*>(&attrPad), "attrPadInit");

    int dummyAttr = 123;
    void* attrSlot = &dummyAttr;
    void* threadAttr = nullptr;
    Require(Throws([&] { sceLibcInternalPadLaunchWithNameAttr_nid_no_patch(nullptr, "nullPad", &attrSlot, &threadAttr); }), "null pad in launch with attr throws");
    Require(Throws([&] { sceLibcInternalPadLaunchWithNameAttr_nid_no_patch(&attrPad, "nullThread", &attrSlot, nullptr); }), "null thread in launch with attr throws");

    sceLibcInternalPadLaunchWithNameAttr_nid_no_patch(&attrPad, "workerAttr", &attrSlot, &threadAttr);
    Require(threadAttr != nullptr, "attr thread created");
    Require(attrPad.started, "attr pad started after launch");
    Require(lastCreatedName != nullptr && std::strcmp(lastCreatedName, "workerAttr") == 0, "thread name forwarded with attr");
    Require(lastCreatedAttr == &attrSlot, "attr slot forwarded");
    JoinMockThread(threadAttr);
    _ZNSt4_PadD2Ev_nid_postfix(reinterpret_cast<GuestPad*>(&attrPad));

    RegisterPthreadCreate_nid_no_patch(nullptr);

    std::puts("libc internal _Pad complete-object destructor and named lifecycle checks passed");
    return 0;
}
