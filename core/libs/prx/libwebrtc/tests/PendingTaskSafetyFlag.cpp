#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <unordered_map>

namespace {

std::unordered_map<void*, std::size_t> allocations;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc PendingTaskSafetyFlag check failed at line %u\n", location.line());
    std::abort();
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    auto* pointer = std::malloc(size);
    allocations.emplace(pointer, size);
    return pointer;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    Require(allocations.erase(pointer) == 1);
    std::free(pointer);
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t size) { return Allocate_nid_no_patch(size); }
void* APS5_VABI Realign_nid_no_patch(void* pointer, std::size_t size, std::size_t) { return Reallocate_nid_no_patch(pointer, size); }
int APS5_VABI PosixAlign_nid_no_patch(void** pointer, std::size_t alignment, std::size_t size) {
    *pointer = Align_nid_no_patch(alignment, size);
    return *pointer ? 0 : 12;
}

template<typename TAction> void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

void TestAlive() {
    WebRtcPendingTaskSafetyFlag flag{1, true};
    Require(_ZNK6webrtc21PendingTaskSafetyFlag5aliveEv_nid_postfix(&flag));
    _ZN6webrtc21PendingTaskSafetyFlag11SetNotAliveEv_nid_postfix(&flag);
    Require(!flag.alive);
    Require(flag.refCount == 1);
    Require(!_ZNK6webrtc21PendingTaskSafetyFlag5aliveEv_nid_postfix(&flag));
    _ZN6webrtc21PendingTaskSafetyFlag11SetNotAliveEv_nid_postfix(&flag);
    Require(!_ZNK6webrtc21PendingTaskSafetyFlag5aliveEv_nid_postfix(&flag));
}

void TestCreate() {
    auto* sentinel = reinterpret_cast<WebRtcPendingTaskSafetyFlag*>(0x1);
    WebRtcPendingTaskSafetyFlag* first = sentinel;
    Require(_ZN6webrtc21PendingTaskSafetyFlag6CreateEv_nid_postfix(&first) == &first);
    Require(first && first != sentinel);
    Require(allocations.size() == 1);
    Require(allocations.at(first) == sizeof(WebRtcPendingTaskSafetyFlag));
    Require(first->refCount == 1);
    Require(first->alive);
    Require(_ZNK6webrtc21PendingTaskSafetyFlag5aliveEv_nid_postfix(first));

    WebRtcPendingTaskSafetyFlag* second = nullptr;
    _ZN6webrtc21PendingTaskSafetyFlag6CreateEv_nid_postfix(&second);
    Require(second && second != first);
    _ZN6webrtc21PendingTaskSafetyFlag11SetNotAliveEv_nid_postfix(first);
    Require(!_ZNK6webrtc21PendingTaskSafetyFlag5aliveEv_nid_postfix(first));
    Require(_ZNK6webrtc21PendingTaskSafetyFlag5aliveEv_nid_postfix(second));
    Require(second->refCount == 1);

    _ZdlPv_nid_postfix(first);
    _ZdlPv_nid_postfix(second);
    Require(allocations.empty());
}

void TestNullFlag() {
    RequireInvalidArgument([] { _ZN6webrtc21PendingTaskSafetyFlag6CreateEv_nid_postfix(nullptr); });
    RequireInvalidArgument([] { _ZN6webrtc21PendingTaskSafetyFlag11SetNotAliveEv_nid_postfix(nullptr); });
    RequireInvalidArgument([] { _ZNK6webrtc21PendingTaskSafetyFlag5aliveEv_nid_postfix(nullptr); });
}

}

int main() {
    void* api[10] = {reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch), reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch), reinterpret_cast<void*>(&PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
    TestAlive();
    TestCreate();
    TestNullFlag();
    Require(allocations.empty());
    std::puts("libwebrtc PendingTaskSafetyFlag tests passed");
}
