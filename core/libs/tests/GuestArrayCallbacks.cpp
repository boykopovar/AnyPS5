#include "prx/libc/include/Api.hpp"
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
    void* array = __cxa_vec_new_nid_postfix(Count, ElementBytes, Padding, nullptr, nullptr);
    require(array != nullptr, "default array allocation failed");
    checkCookie(array);
    reset(static_cast<std::byte*>(array));
    __cxa_vec_ctor_nid_postfix(array, Count, ElementBytes, construct, destroy);
    __cxa_vec_delete_nid_postfix(array, ElementBytes, Padding, destroy);

    require(constructed == Count && destroyed == Count, "default array deletion failed");
    __cxa_vec_delete_nid_postfix(nullptr, ElementBytes, Padding, destroy);

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
    checkElements();
    checkAllocation(false);
    checkAllocation(true);
    checkDefaultAllocation();
    checkRollback(false);
    checkRollback(true);
    checkCopyRollback();
    std::puts("guest array callback arguments and lifecycle: PASS");
}
