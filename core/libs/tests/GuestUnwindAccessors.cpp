#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/exceptions/Runtime.hpp"
#include "prx/libc/include/exceptions/Unwind.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <typeinfo>

static void Require(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "unwind accessor check failed at line %d\n", line);
        std::abort();
    }
}
#define Check(value) Require((value), __LINE__)

constexpr int RegisterCount = 17;
constexpr int InstructionPointer = 16;

static std::uintptr_t Pattern(int index) {
    return 0x1000u + static_cast<std::uintptr_t>(index) * 0x111u;
}

namespace {

#ifndef _LIBCPP_VERSION
using TestExceptionPointer = std::exception_ptr;
#else
using TestExceptionPointer = GuestExceptionPointer;
#endif

constexpr std::size_t VecCount = 4;
constexpr std::size_t VecElementBytes = 16;
constexpr std::size_t VecPadding = 16;
constexpr std::size_t VecTotalBytes = VecCount * VecElementBytes + VecPadding;
alignas(16) std::array<std::byte, VecTotalBytes> vecStorage{};
std::size_t vecConstructed = 0;
std::size_t vecDestroyed = 0;
std::size_t vecAllocated = 0;
std::size_t vecDeallocated = 0;
std::size_t vecThrowConstructAt = VecCount + 1;
std::size_t vecThrowDestroyAt = VecCount + 1;
void* vecLastDeallocPtr = nullptr;
std::size_t vecLastDeallocSize = 0;

void ResetVecState() {
    vecStorage.fill(std::byte{0});
    vecConstructed = 0;
    vecDestroyed = 0;
    vecAllocated = 0;
    vecDeallocated = 0;
    vecThrowConstructAt = VecCount + 1;
    vecThrowDestroyAt = VecCount + 1;
    vecLastDeallocPtr = nullptr;
    vecLastDeallocSize = 0;
}

void APS5_VABI VecConstruct(void* pointer) {
    auto* base = vecStorage.data() + VecPadding;
    Check(vecConstructed < VecCount && pointer == base + vecConstructed * VecElementBytes);
    if (vecConstructed == vecThrowConstructAt) throw 31;
    std::memset(pointer, static_cast<int>(++vecConstructed), VecElementBytes);
}

void APS5_VABI VecDestroy(void* pointer) {
    auto* base = vecStorage.data() + VecPadding;
    Check(vecDestroyed < vecConstructed && pointer == base + (vecConstructed - vecDestroyed - 1) * VecElementBytes);
    Check(*static_cast<std::byte*>(pointer) == std::byte(vecConstructed - vecDestroyed));
    ++vecDestroyed;
    if (vecDestroyed == vecThrowDestroyAt) throw 47;
}

void* APS5_VABI VecAlloc(std::size_t bytes) {
    Check(bytes == VecTotalBytes);
    ++vecAllocated;
    return vecStorage.data();
}

void APS5_VABI VecDealloc2(void* pointer) {
    Check(pointer == vecStorage.data());
    Check(vecDestroyed == vecConstructed);
    ++vecDeallocated;
    vecLastDeallocPtr = pointer;
}

void APS5_VABI VecDealloc3(void* pointer, std::size_t bytes) {
    Check(bytes == VecTotalBytes);
    vecLastDeallocSize = bytes;
    VecDealloc2(pointer);
}

template<typename TAction>
void ExpectIntException(TAction action, int expected) {
    bool caught = false;
    try {
        action();
    } catch (int value) {
        Check(value == expected);
        caught = true;
    }
    Check(caught);
}

void CheckExceptionPointerType() {
    Check(_ZNKSt15__exception_ptr13exception_ptr20__cxa_exception_typeEv_nid_postfix(nullptr) == nullptr);
    TestExceptionPointer empty(nullptr);
    Check(_ZNKSt15__exception_ptr13exception_ptr6_M_getEv_nid_postfix(&empty) == nullptr);
    Check(_ZNKSt15__exception_ptr13exception_ptr20__cxa_exception_typeEv_nid_postfix(&empty) == nullptr);

    void* object = __cxa_allocate_exception_nid_postfix(sizeof(int));
    Check(object != nullptr);
    *static_cast<int*>(object) = 19;
    LibcException::FromObject(object)->type = const_cast<std::type_info*>(&typeid(int));

    TestExceptionPointer live(nullptr);
    _ZNSt15__exception_ptr13exception_ptrC1EPv_nid_postfix(&live, object);
    Check(_ZNKSt15__exception_ptr13exception_ptr6_M_getEv_nid_postfix(&live) == object);
    Check(_ZNKSt15__exception_ptr13exception_ptr20__cxa_exception_typeEv_nid_postfix(&live) == &typeid(int));

    _ZNSt15__exception_ptr13exception_ptr10_M_releaseEv_nid_postfix(&live);
    Check(_ZNKSt15__exception_ptr13exception_ptr6_M_getEv_nid_postfix(&live) == nullptr);
    Check(_ZNKSt15__exception_ptr13exception_ptr20__cxa_exception_typeEv_nid_postfix(&live) == nullptr);
    __cxa_free_exception_nid_postfix(object);
}

void CheckArrayNullDealloc() {
    for (bool sized : {false, true}) {
        ResetVecState();
        vecThrowConstructAt = 2;
        ExpectIntException([&] {
            if (sized) __cxa_vec_new3_nid_postfix(VecCount, VecElementBytes, VecPadding, VecConstruct, VecDestroy, VecAlloc, nullptr);
            else __cxa_vec_new2_nid_postfix(VecCount, VecElementBytes, VecPadding, VecConstruct, VecDestroy, VecAlloc, nullptr);
        }, 31);
        Check(vecConstructed == 2 && vecDestroyed == 2 && vecAllocated == 1 && vecDeallocated == 0);

        ResetVecState();
        vecThrowConstructAt = 2;
        ExpectIntException([&] {
            if (sized) __cxa_vec_new3_nid_postfix(VecCount, VecElementBytes, VecPadding, VecConstruct, VecDestroy, VecAlloc, VecDealloc3);
            else __cxa_vec_new2_nid_postfix(VecCount, VecElementBytes, VecPadding, VecConstruct, VecDestroy, VecAlloc, VecDealloc2);
        }, 31);
        Check(vecConstructed == 2 && vecDestroyed == 2 && vecAllocated == 1 && vecDeallocated == 1);
        Check(vecLastDeallocPtr == vecStorage.data());
        if (sized) Check(vecLastDeallocSize == VecTotalBytes);

        ResetVecState();
        void* array = sized
            ? __cxa_vec_new3_nid_postfix(VecCount, VecElementBytes, VecPadding, VecConstruct, VecDestroy, VecAlloc, nullptr)
            : __cxa_vec_new2_nid_postfix(VecCount, VecElementBytes, VecPadding, VecConstruct, VecDestroy, VecAlloc, nullptr);
        Check(array == vecStorage.data() + VecPadding && vecConstructed == VecCount && vecAllocated == 1);
        std::size_t cookie = 0;
        std::memcpy(&cookie, static_cast<const std::byte*>(array) - sizeof(cookie), sizeof(cookie));
        Check(cookie == VecCount);
        if (sized) __cxa_vec_delete3_nid_postfix(array, VecElementBytes, VecPadding, VecDestroy, nullptr);
        else __cxa_vec_delete2_nid_postfix(array, VecElementBytes, VecPadding, VecDestroy, nullptr);
        Check(vecDestroyed == VecCount && vecDeallocated == 0);

        ResetVecState();
        array = sized
            ? __cxa_vec_new3_nid_postfix(VecCount, VecElementBytes, VecPadding, VecConstruct, VecDestroy, VecAlloc, nullptr)
            : __cxa_vec_new2_nid_postfix(VecCount, VecElementBytes, VecPadding, VecConstruct, VecDestroy, VecAlloc, nullptr);
        Check(array == vecStorage.data() + VecPadding && vecConstructed == VecCount);
        vecThrowDestroyAt = 1;
        ExpectIntException([&] {
            if (sized) __cxa_vec_delete3_nid_postfix(array, VecElementBytes, VecPadding, VecDestroy, nullptr);
            else __cxa_vec_delete2_nid_postfix(array, VecElementBytes, VecPadding, VecDestroy, nullptr);
        }, 47);
        Check(vecDestroyed == VecCount && vecDeallocated == 0);

        ResetVecState();
        array = sized
            ? __cxa_vec_new3_nid_postfix(VecCount, VecElementBytes, VecPadding, VecConstruct, VecDestroy, VecAlloc, VecDealloc3)
            : __cxa_vec_new2_nid_postfix(VecCount, VecElementBytes, VecPadding, VecConstruct, VecDestroy, VecAlloc, VecDealloc2);
        vecThrowDestroyAt = 1;
        ExpectIntException([&] {
            if (sized) __cxa_vec_delete3_nid_postfix(array, VecElementBytes, VecPadding, VecDestroy, VecDealloc3);
            else __cxa_vec_delete2_nid_postfix(array, VecElementBytes, VecPadding, VecDestroy, VecDealloc2);
        }, 47);
        Check(vecDestroyed == VecCount && vecDeallocated == 1 && vecLastDeallocPtr == vecStorage.data());
        if (sized) Check(vecLastDeallocSize == VecTotalBytes);
    }
}

}

int main() {
    {
        _Unwind_Context context{};
        for (int index = 0; index < RegisterCount; ++index) {
            Check(_Unwind_GetGR_nid_postfix(&context, index) == 0);
            _Unwind_SetGR_nid_postfix(&context, index, Pattern(index));
            for (int other = 0; other < RegisterCount; ++other)
                Check(_Unwind_GetGR_nid_postfix(&context, other) == (other <= index ? Pattern(other) : 0));
        }
        for (int index = 0; index < RegisterCount; ++index) Check(_Unwind_GetGR_nid_postfix(&context, index) == Pattern(index));
    }
    {
        _Unwind_Context context{};
        _Unwind_SetGR_nid_postfix(&context, 3, 0x33);
        _Unwind_SetIP_nid_postfix(&context, 0x4000);
        Check(_Unwind_GetIP_nid_postfix(&context) == 0x4000);
        Check(_Unwind_GetGR_nid_postfix(&context, InstructionPointer) == 0x4000);
        Check(_Unwind_GetGR_nid_postfix(&context, 3) == 0x33);
        _Unwind_SetGR_nid_postfix(&context, InstructionPointer, 0x5000);
        Check(_Unwind_GetIP_nid_postfix(&context) == 0x5000);
    }
    {
        _Unwind_Context context{};
        _Unwind_SetIP_nid_postfix(&context, 0x6000);
        int before = 99;
        Check(_Unwind_GetIPInfo_nid_postfix(&context, &before) == 0x6000 && before == 0);
        context.signalFrame = true;
        before = 99;
        Check(_Unwind_GetIPInfo_nid_postfix(&context, &before) == 0x6000 && before == 1);
        Check(_Unwind_GetIPInfo_nid_postfix(&context, nullptr) == 0x6000);
        _Unwind_SetIP_nid_postfix(&context, 0x7000);
        Check(_Unwind_GetIPInfo_nid_postfix(&context, &before) == 0x7000);
    }
    {
        _Unwind_Context context{};
        context.cfa = 0x8100;
        context.lsda = 0x8200;
        context.region = 0x8300;
        context.dataBase = 0x8400;
        context.textBase = 0x8500;
        Check(_Unwind_GetCFA_nid_postfix(&context) == 0x8100);
        Check(_Unwind_GetLanguageSpecificData_nid_postfix(&context) == 0x8200);
        Check(_Unwind_GetRegionStart_nid_postfix(&context) == 0x8300);
        Check(_Unwind_GetDataRelBase_nid_postfix(&context) == 0x8400);
        Check(_Unwind_GetTextRelBase_nid_postfix(&context) == 0x8500);
        for (int index = 0; index < RegisterCount; ++index) Check(_Unwind_GetGR_nid_postfix(&context, index) == 0);
    }
    CheckExceptionPointerType();
    CheckArrayNullDealloc();
    return 0;
}
