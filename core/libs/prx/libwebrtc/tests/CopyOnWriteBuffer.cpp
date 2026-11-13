#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>
#include <unordered_map>

namespace {

std::unordered_map<void*, std::size_t> allocations;
std::size_t allocated = 0;
std::size_t freed = 0;
bool failArray = false;

void Require(bool condition) {
    if (!condition) throw std::runtime_error("CopyOnWriteBuffer assertion failed");
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    if (failArray && size != 32) throw std::bad_alloc();
    auto* pointer = std::malloc(size);
    if (pointer) {
        allocations.emplace(pointer, size);
        ++allocated;
        std::memset(pointer, 0xa5, size);
    }
    return pointer;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    Require(allocations.erase(pointer) == 1);
    ++freed;
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

void Destroy(WebRtcCopyOnWriteBuffer& buffer) {
    _ZN3rtc17CopyOnWriteBufferD1Ev_nid_postfix(&buffer);
}

void TestConstructors() {
    WebRtcCopyOnWriteBuffer buffer;
    _ZN3rtc17CopyOnWriteBufferC1Em_nid_postfix(&buffer, 7);
    Require(buffer.offset == 0 && buffer.size == 7);
    Require(buffer.buffer->size == 7 && buffer.buffer->capacity == 7 && buffer.buffer->refCount == 1);
    for (std::size_t i = 0; i < 7; ++i) Require(buffer.buffer->data[i] == 0xa5);
    Destroy(buffer);
    for (auto sizes : {std::pair<std::size_t, std::size_t>{7, 3}, {7, 12}, {0, 12}, {0, 0}}) {
        _ZN3rtc17CopyOnWriteBufferC1Emm_nid_postfix(&buffer, sizes.first, sizes.second);
        Require(buffer.offset == 0 && buffer.size == sizes.first);
        if (sizes.first || sizes.second) {
            Require(buffer.buffer->size == sizes.first);
            Require(buffer.buffer->capacity == (sizes.first > sizes.second ? sizes.first : sizes.second));
            Require(buffer.buffer->refCount == 1);
        } else Require(buffer.buffer == nullptr);
        Destroy(buffer);
    }
    _ZN3rtc17CopyOnWriteBufferC1Em_nid_postfix(&buffer, 0);
    Require(buffer.buffer == nullptr && buffer.offset == 0 && buffer.size == 0);
    WebRtcCopyOnWriteBuffer copy;
    _ZN3rtc17CopyOnWriteBufferC1ERKS0__nid_postfix(&copy, &buffer);
    Require(copy.buffer == nullptr && copy.offset == 0 && copy.size == 0);
    Destroy(copy);
    Destroy(buffer);
}

void TestSharing() {
    for (bool originalFirst : {false, true}) {
        WebRtcCopyOnWriteBuffer original, copy;
        _ZN3rtc17CopyOnWriteBufferC1Em_nid_postfix(&original, 8);
        auto* backing = original.buffer;
        const auto before = freed;
        _ZN3rtc17CopyOnWriteBufferC1ERKS0__nid_postfix(&copy, &original);
        Require(copy.buffer == backing && backing->refCount == 2 && copy.size == 8 && copy.offset == 0);
        Destroy(originalFirst ? original : copy);
        Require(backing->refCount == 1 && freed == before);
        Destroy(originalFirst ? copy : original);
        Require(freed == before + 2);
    }
}

void TestDetach() {
    WebRtcCopyOnWriteBuffer original, copy;
    _ZN3rtc17CopyOnWriteBufferC1Emm_nid_postfix(&original, 8, 12);
    for (int i = 0; i < 8; ++i) original.buffer->data[i] = static_cast<std::uint8_t>(i + 10);
    auto* backing = original.buffer;
    const auto before = allocated;
    _ZN3rtc17CopyOnWriteBuffer24UnshareAndEnsureCapacityEm_nid_postfix(&original, 12);
    Require(original.buffer == backing && allocated == before);
    original.offset = 2;
    original.size = 4;
    _ZN3rtc17CopyOnWriteBuffer24UnshareAndEnsureCapacityEm_nid_postfix(&original, 10);
    Require(original.buffer == backing && original.offset == 2 && allocated == before);
    original.offset = 0;
    original.size = 8;
    _ZN3rtc17CopyOnWriteBuffer24UnshareAndEnsureCapacityEm_nid_postfix(&original, 20);
    Require(original.buffer != backing && original.buffer->capacity == 20 && original.buffer->size == 8);
    Require(original.size == 8 && original.offset == 0 && original.buffer->refCount == 1);
    for (int i = 0; i < 8; ++i) Require(original.buffer->data[i] == i + 10);
    _ZN3rtc17CopyOnWriteBufferC1ERKS0__nid_postfix(&copy, &original);
    backing = original.buffer;
    _ZN3rtc17CopyOnWriteBuffer24UnshareAndEnsureCapacityEm_nid_postfix(&copy, 3);
    Require(copy.buffer != backing && copy.buffer->data != backing->data && copy.buffer->capacity == 8);
    Require(copy.buffer->refCount == 1 && backing->refCount == 1);
    Require(std::memcmp(copy.buffer->data, backing->data, 8) == 0);
    copy.buffer->data[0] = 99;
    Require(original.buffer->data[0] == 10);
    Destroy(copy);
    _ZN3rtc17CopyOnWriteBufferC1ERKS0__nid_postfix(&copy, &original);
    copy.offset = 3;
    copy.size = 4;
    _ZN3rtc17CopyOnWriteBuffer24UnshareAndEnsureCapacityEm_nid_postfix(&copy, 10);
    Require(copy.offset == 0 && copy.size == 4 && copy.buffer->size == 4 && copy.buffer->capacity == 10);
    Require(copy.buffer->refCount == 1 && original.buffer->refCount == 1);
    for (int i = 0; i < 4; ++i) Require(copy.buffer->data[i] == i + 13);
    Destroy(copy);
    Destroy(original);
}

void TestAllocationFailure() {
    WebRtcCopyOnWriteBuffer buffer;
    const auto before = allocations.size();
    failArray = true;
    bool threw = false;
    try {
        _ZN3rtc17CopyOnWriteBufferC1Em_nid_postfix(&buffer, 7);
    } catch (const std::bad_alloc&) { threw = true; }
    failArray = false;
    Require(threw && allocations.size() == before);
}

void TestGuestBacking() {
    auto* backing = new (_Znwm_nid_postfix(32)) WebRtcRefCountedBuffer{6, 9,
        static_cast<std::uint8_t*>(_Znam_nid_postfix(9)), 1};
    WebRtcCopyOnWriteBuffer original{backing, 2, 3}, copy;
    _ZN3rtc17CopyOnWriteBufferC1ERKS0__nid_postfix(&copy, &original);
    Require(copy.buffer == backing && backing->refCount == 2 && copy.offset == 2 && copy.size == 3);
    Destroy(original);
    Require(backing->refCount == 1);
    Destroy(copy);
    WebRtcCopyOnWriteBuffer exported;
    _ZN3rtc17CopyOnWriteBufferC1Em_nid_postfix(&exported, 5);
    backing = exported.buffer;
    backing->refCount.fetch_add(1, std::memory_order_relaxed);
    Destroy(exported);
    Require(backing->refCount.fetch_sub(1, std::memory_order_acq_rel) == 1);
    _ZdaPv_nid_postfix(backing->data);
    backing->~WebRtcRefCountedBuffer();
    _ZdlPvm_nid_postfix(backing, 32);
}

}

int main() {
    void* api[10] = {reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch), reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch), reinterpret_cast<void*>(&PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
    try {
        TestConstructors();
        TestSharing();
        TestDetach();
        TestGuestBacking();
        TestAllocationFailure();
        Require(allocations.empty() && allocated == freed);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "CopyOnWriteBuffer exception: %s\n", error.what());
        return 1;
    }
}
