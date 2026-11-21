#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using ElementCallback = void (APS5_VABI *)(void*);
using CopyCallback = void (APS5_VABI *)(void*, void*);
using AllocateCallback = void* (APS5_VABI *)(std::size_t);
using FreeCallback = void (APS5_VABI *)(void*);
using SizedFreeCallback = void (APS5_VABI *)(void*, std::size_t);

namespace {

constexpr std::size_t Count = 4;
constexpr std::size_t ElementBytes = 16;
constexpr std::size_t Padding = 16;
constexpr std::size_t TotalBytes = Count * ElementBytes + Padding;
alignas(16) std::array<std::byte, TotalBytes> storage{};
alignas(16) std::array<std::byte, Count * ElementBytes> source{};
std::byte* destination = nullptr;
std::size_t constructed = 0;
std::size_t copied = 0;
std::size_t destroyed = 0;
std::size_t allocated = 0;
std::size_t freed = 0;
bool failAllocation = false;
bool failConstruct = false;
bool failCopy = false;
bool failDestroy = false;

unsigned heapAllocations = 0;
unsigned heapReleases = 0;
unsigned heapHandlerCalls = 0;
bool heapFailNext = false;
std::size_t heapLastBytes = 0;
void* heapLastAllocation = nullptr;
void* heapLastRelease = nullptr;

void* APS5_VABI HeapAllocate(std::size_t bytes) {
    ++heapAllocations;
    heapLastBytes = bytes;
    if (heapFailNext) {
        heapFailNext = false;
        return nullptr;
    }
    heapLastAllocation = std::malloc(bytes);
    return heapLastAllocation;
}

void APS5_VABI HeapRelease(void* pointer) {
    ++heapReleases;
    heapLastRelease = pointer;
    std::free(pointer);
}

void* APS5_VABI HeapCalloc(std::size_t count, std::size_t bytes) { return std::calloc(count, bytes); }
void* APS5_VABI HeapReallocate(void* pointer, std::size_t bytes) { return std::realloc(pointer, bytes); }
void* APS5_VABI HeapAlign(std::size_t, std::size_t bytes) { return std::malloc(bytes); }
void* APS5_VABI HeapRealign(void* pointer, std::size_t bytes, std::size_t) { return std::realloc(pointer, bytes); }
int APS5_VABI HeapPosixAlign(void** pointer, std::size_t, std::size_t bytes) {
    *pointer = std::malloc(bytes);
    return *pointer != nullptr ? 0 : 12;
}

void APS5_VABI HeapNewHandler() { ++heapHandlerCalls; }

void ResetHeap() {
    heapAllocations = heapReleases = heapHandlerCalls = 0;
    heapLastBytes = 0;
    heapLastAllocation = heapLastRelease = nullptr;
    heapFailNext = false;
}

void InitializeHeap() {
    std::array<void*, 10> api{};
    api[0] = reinterpret_cast<void*>(&HeapAllocate);
    api[1] = reinterpret_cast<void*>(&HeapRelease);
    api[2] = reinterpret_cast<void*>(&HeapCalloc);
    api[3] = reinterpret_cast<void*>(&HeapReallocate);
    api[4] = reinterpret_cast<void*>(&HeapAlign);
    api[5] = reinterpret_cast<void*>(&HeapRealign);
    api[6] = reinterpret_cast<void*>(&HeapPosixAlign);
    ApplicationHeapRegister_nid_no_patch(api.data());
}

void require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

void reset(std::byte* first) {
    destination = first;
    constructed = copied = destroyed = allocated = freed = 0;
    failAllocation = false;
    failConstruct = failCopy = failDestroy = false;
}

void APS5_VABI construct(void* pointer) {
    require(constructed < Count && pointer == destination + constructed * ElementBytes, "constructor received the wrong element");
    if (failConstruct && constructed == 2) throw 73;
    std::memset(pointer, static_cast<int>(++constructed), ElementBytes);
}

void APS5_VABI copy(void* target, void* original) {
    require(copied < Count && target == destination + copied * ElementBytes, "copy constructor received the wrong destination");
    require(original == source.data() + copied * ElementBytes, "copy constructor received the wrong source");
    if (failCopy && copied == 2) throw 73;
    std::memcpy(target, original, ElementBytes);
    ++copied;
}

void APS5_VABI destroy(void* pointer) {
    const auto count = copied != 0 ? copied : constructed;
    require(destroyed < count && pointer == destination + (count - destroyed - 1) * ElementBytes, "destructor received the wrong element or order");
    require(*static_cast<std::byte*>(pointer) == std::byte(count - destroyed), "destructor received the wrong element contents");
    ++destroyed;
    if (failDestroy && destroyed == 1) throw 73;
}

void* APS5_VABI allocate(std::size_t bytes) {
    require(bytes == TotalBytes, "allocator received the wrong size");
    ++allocated;
    return failAllocation ? nullptr : storage.data();
}

void APS5_VABI release(void* pointer) {
    require(pointer == storage.data(), "deallocator received the wrong storage");
    require(destroyed == constructed, "deallocator ran before element destruction finished");
    ++freed;
}

void APS5_VABI releaseSized(void* pointer, std::size_t bytes) {
    require(bytes == TotalBytes, "sized deallocator received the wrong size");
    release(pointer);
}

void checkCookie(const void* array) {
    std::size_t count = 0;
    std::memcpy(&count, static_cast<const std::byte*>(array) - sizeof(count), sizeof(count));
    require(count == Count, "array cookie has the wrong count");
}

void checkElements() {
    reset(source.data());
    __cxa_vec_ctor_nid_postfix(source.data(), Count, ElementBytes, construct, destroy);

    require(constructed == Count, "constructor count is wrong");
    __cxa_vec_dtor_nid_postfix(source.data(), Count, ElementBytes, destroy);

    require(destroyed == Count, "destructor count is wrong");
    reset(storage.data());
    __cxa_vec_cctor_nid_postfix(storage.data(), source.data(), Count, ElementBytes, copy, destroy);

    require(copied == Count && std::memcmp(storage.data(), source.data(), source.size()) == 0, "copy construction failed");
    __cxa_vec_cleanup_nid_postfix(storage.data(), Count, ElementBytes, destroy);

    require(destroyed == Count, "cleanup count is wrong");
    __cxa_vec_ctor_nid_postfix(nullptr, Count, ElementBytes, nullptr, nullptr);
    __cxa_vec_cctor_nid_postfix(nullptr, nullptr, Count, ElementBytes, nullptr, nullptr);
    __cxa_vec_dtor_nid_postfix(nullptr, Count, ElementBytes, nullptr);
    __cxa_vec_cleanup_nid_postfix(nullptr, Count, ElementBytes, nullptr);

}

void checkAllocation(bool sized) {
    reset(storage.data() + Padding);
    void* array = sized
        ? __cxa_vec_new3_nid_postfix(Count, ElementBytes, Padding, construct, destroy, allocate, releaseSized)
        : __cxa_vec_new2_nid_postfix(Count, ElementBytes, Padding, construct, destroy, allocate, release);

    require(array == destination && constructed == Count && allocated == 1, "array allocation failed");
    checkCookie(array);
    if (sized) __cxa_vec_delete3_nid_postfix(array, ElementBytes, Padding, destroy, releaseSized);
    else __cxa_vec_delete2_nid_postfix(array, ElementBytes, Padding, destroy, release);
    require(destroyed == Count && freed == 1, "array deletion failed");
    reset(storage.data() + Padding);
    failAllocation = true;
    array = sized
        ? __cxa_vec_new3_nid_postfix(Count, ElementBytes, Padding, construct, destroy, allocate, releaseSized)
        : __cxa_vec_new2_nid_postfix(Count, ElementBytes, Padding, construct, destroy, allocate, release);

    require(array == nullptr && allocated == 1 && constructed == 0 && freed == 0, "failed allocation invoked element callbacks");
    if (sized) __cxa_vec_delete3_nid_postfix(nullptr, ElementBytes, Padding, destroy, releaseSized);
    else __cxa_vec_delete2_nid_postfix(nullptr, ElementBytes, Padding, destroy, release);
    require(freed == 0, "null array invoked deallocation");
}

void checkDefaultAllocation() {
    ResetHeap();
    void* control = _Znam_nid_postfix(TotalBytes);
    require(control == heapLastAllocation && heapAllocations == 1 && heapLastBytes == TotalBytes, "guest operator new control reaches replacement allocator");
    _ZdaPv_nid_postfix(control);
    require(heapReleases == 1 && heapLastRelease == control, "guest operator delete control reaches replacement deallocator");
    ResetHeap();
    void* array = __cxa_vec_new_nid_postfix(Count, ElementBytes, Padding, nullptr, nullptr);
    require(array != nullptr, "default array returned a nonnull block");
    checkCookie(array);
    std::fprintf(stderr, "array guest heap stage=default_new qualified=1 heapAllocations=%u expectedBytes=%zu actualBytes=%zu\n", heapAllocations, TotalBytes, heapLastBytes);
    require(heapAllocations == 1 && heapLastBytes == TotalBytes, "default array uses the application allocator");
    require(array == static_cast<std::byte*>(heapLastAllocation) + Padding, "default array returns guest allocation after cookie");
    checkCookie(array);
    reset(static_cast<std::byte*>(array));
    __cxa_vec_ctor_nid_postfix(array, Count, ElementBytes, construct, destroy);
    __cxa_vec_delete_nid_postfix(array, ElementBytes, Padding, destroy);
    require(constructed == Count && destroyed == Count, "default array deletion failed");
    require(heapReleases == 1 && heapLastRelease == heapLastAllocation, "default array uses the application deallocator");
    __cxa_vec_delete_nid_postfix(nullptr, ElementBytes, Padding, destroy);
    require(heapReleases == 1, "null default array does not deallocate");

    ResetHeap();
    array = __cxa_vec_new_nid_postfix(0, ElementBytes, 0, nullptr, nullptr);
    require(array == heapLastAllocation && heapAllocations == 1 && heapLastBytes == 1, "empty default array uses operator new zero normalization");
    __cxa_vec_delete_nid_postfix(array, ElementBytes, 0, nullptr);
    require(heapReleases == 1 && heapLastRelease == heapLastAllocation, "empty default array uses application deallocation");

    ResetHeap();
    heapFailNext = true;
    const auto oldHandler = _ZSt15set_new_handlerPFvvE_nid_postfix(HeapNewHandler);
    array = __cxa_vec_new_nid_postfix(Count, ElementBytes, Padding, nullptr, nullptr);
    _ZSt15set_new_handlerPFvvE_nid_postfix(oldHandler);
    require(heapAllocations == 2 && heapHandlerCalls == 1, "default array retries through the guest new handler");
    __cxa_vec_delete_nid_postfix(array, ElementBytes, Padding, nullptr);
    require(heapReleases == 1, "retried default array is released once");
}

template<typename TAction>
void expectException(TAction action) {
    bool caught = false;
    try { action(); }
    catch (int value) { require(value == 73, "callback exception changed"); caught = true; }
    require(caught, "callback exception was swallowed");
}

void checkRollback(bool sized) {
    reset(storage.data() + Padding);
    failConstruct = true;
    expectException([&] {
        if (sized) __cxa_vec_new3_nid_postfix(Count, ElementBytes, Padding, construct, destroy, allocate, releaseSized);
        else __cxa_vec_new2_nid_postfix(Count, ElementBytes, Padding, construct, destroy, allocate, release);
    });
    require(constructed == 2 && destroyed == 2 && allocated == 1 && freed == 1, "constructor rollback failed");
    reset(storage.data() + Padding);
    void* array = sized
        ? __cxa_vec_new3_nid_postfix(Count, ElementBytes, Padding, construct, destroy, allocate, releaseSized)
        : __cxa_vec_new2_nid_postfix(Count, ElementBytes, Padding, construct, destroy, allocate, release);

    failDestroy = true;
    expectException([&] {
        if (sized) __cxa_vec_delete3_nid_postfix(array, ElementBytes, Padding, destroy, releaseSized);
        else __cxa_vec_delete2_nid_postfix(array, ElementBytes, Padding, destroy, release);
    });
    require(constructed == Count && destroyed == Count && allocated == 1 && freed == 1, "destructor rollback failed");
}

void APS5_VABI ConstructDefault(void* pointer) {
    if (constructed == 0) destination = static_cast<std::byte*>(pointer);
    construct(pointer);
}

void checkDefaultRollback() {
    ResetHeap();
    reset(nullptr);
    failConstruct = true;
    expectException([] {
        __cxa_vec_new_nid_postfix(Count, ElementBytes, Padding,
            ConstructDefault, destroy);
    });
    require(constructed == 2 && destroyed == 2 && heapAllocations == 1 && heapReleases == 1, "default constructor failure frees the guest block");

    ResetHeap();
    void* array = __cxa_vec_new_nid_postfix(Count, ElementBytes, Padding, nullptr, nullptr);
    reset(static_cast<std::byte*>(array));
    __cxa_vec_ctor_nid_postfix(array, Count, ElementBytes, construct, destroy);
    failDestroy = true;
    expectException([&] { __cxa_vec_delete_nid_postfix(array, ElementBytes, Padding, destroy); });
    require(destroyed == Count && heapAllocations == 1 && heapReleases == 1, "default destructor failure frees the guest block");
}

void checkCopyRollback() {
    reset(storage.data());
    failCopy = true;
    expectException([] {
        __cxa_vec_cctor_nid_postfix(storage.data(), source.data(), Count, ElementBytes, copy, destroy);

    });
    require(copied == 2 && destroyed == 2, "copy constructor rollback failed");
}

}

int main() {
    InitializeHeap();
    checkElements();
    checkAllocation(false);
    checkAllocation(true);
    checkDefaultAllocation();
    checkRollback(false);
    checkRollback(true);
    checkCopyRollback();
    checkDefaultRollback();
    std::puts("guest array callback arguments and lifecycle: PASS");
}
