#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <thread>

namespace {

void* APS5_VABI TestAllocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI TestFree(void* pointer) { std::free(pointer); }
void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI TestReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc rtc::ThreadManager check failed at line %u\n", location.line());
    std::abort();
}

void* FakeThread(std::uintptr_t value) {
    return reinterpret_cast<void*>(value);
}

void TestInstanceIsOneManager() {
    WebRtcThreadManager* manager = _ZN3rtc13ThreadManager8InstanceEv_nid_postfix();
    Require(manager != nullptr);
    Require(_ZN3rtc13ThreadManager8InstanceEv_nid_postfix() == manager);
    WebRtcThreadManager* other = nullptr;
    std::thread worker([&other] { other = _ZN3rtc13ThreadManager8InstanceEv_nid_postfix(); });
    worker.join();
    Require(other == manager);
}

void TestNoThreadIsCurrentAtFirst() {
    WebRtcThreadManager* manager = _ZN3rtc13ThreadManager8InstanceEv_nid_postfix();
    Require(_ZN3rtc13ThreadManager13CurrentThreadEv_nid_postfix(manager) == nullptr);
    Require(_ZN3rtc6Thread7CurrentEv_nid_postfix() == nullptr);
    Require(!_ZNK3rtc6Thread9IsCurrentEv_nid_postfix(FakeThread(0x1000)));
    Require(_ZNK3rtc6Thread9IsCurrentEv_nid_postfix(nullptr));
}

void TestCurrentThreadReadsThePerThreadKey() {
    WebRtcThreadManager* manager = _ZN3rtc13ThreadManager8InstanceEv_nid_postfix();
    void* const mine = FakeThread(0x1000);
    Require(pthread_setspecific_nid_postfix(manager->key, mine) == 0);
    Require(_ZN3rtc13ThreadManager13CurrentThreadEv_nid_postfix(manager) == mine);
    Require(_ZN3rtc6Thread7CurrentEv_nid_postfix() == mine);
    Require(_ZNK3rtc6Thread9IsCurrentEv_nid_postfix(mine));
    Require(!_ZNK3rtc6Thread9IsCurrentEv_nid_postfix(FakeThread(0x2000)));

    void* seenBefore = mine;
    void* seenAfter = nullptr;
    bool otherIsCurrent = false;
    bool mineIsCurrent = true;
    std::thread worker([&] {
        seenBefore = _ZN3rtc6Thread7CurrentEv_nid_postfix();
        Require(pthread_setspecific_nid_postfix(manager->key, FakeThread(0x2000)) == 0);
        seenAfter = _ZN3rtc6Thread7CurrentEv_nid_postfix();
        otherIsCurrent = _ZNK3rtc6Thread9IsCurrentEv_nid_postfix(FakeThread(0x2000));
        mineIsCurrent = _ZNK3rtc6Thread9IsCurrentEv_nid_postfix(mine);
    });
    worker.join();
    Require(seenBefore == nullptr && seenAfter == FakeThread(0x2000));
    Require(otherIsCurrent && !mineIsCurrent);
    Require(_ZN3rtc6Thread7CurrentEv_nid_postfix() == mine);

    Require(pthread_setspecific_nid_postfix(manager->key, nullptr) == 0);
    Require(_ZN3rtc6Thread7CurrentEv_nid_postfix() == nullptr);
}

void TestNullManagerThrows() {
    try {
        _ZN3rtc13ThreadManager13CurrentThreadEv_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false);
}

}

int main() {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    TestInstanceIsOneManager();
    TestNoThreadIsCurrentAtFirst();
    TestCurrentThreadReadsThePerThreadKey();
    TestNullManagerThrows();
    return 0;
}
