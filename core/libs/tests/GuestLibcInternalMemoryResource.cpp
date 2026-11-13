#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace LibcException {
struct TypeInfoVtable;
}

extern "C" {
extern LibcException::TypeInfoVtable _ZTVN10__cxxabiv117__class_type_infoE_nid_postfix;
extern LibcException::TypeInfoVtable _ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix;
}

namespace {

struct TypeRecord {
    const void* vtable;
    const char* name;
    const TypeRecord* base;
};

using Allocate = void* (APS5_VABI*)(void*, std::size_t, std::size_t);
using Deallocate = void (APS5_VABI*)(void*, void*, std::size_t, std::size_t);
using IsEqual = bool (APS5_VABI*)(const void*, const void*);
using Destroy = void (APS5_VABI*)(void*);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libSceLibcInternal memory resource: %s\n", message);
    std::abort();
}

const void* const* Vtable(const void* resource) {
    return *static_cast<const void* const* const*>(resource);
}

const TypeRecord* TypeOf(const void* resource) {
    return static_cast<const TypeRecord*>(Vtable(resource)[-1]);
}

template<typename TFunction>
TFunction Slot(const void* resource, std::size_t index) {
    return reinterpret_cast<TFunction>(const_cast<void*>(Vtable(resource)[index]));
}

const void* TypeInfoVptr(LibcException::TypeInfoVtable& vtable) {
    return reinterpret_cast<const unsigned char*>(&vtable) + 2 * sizeof(void*);
}

void CheckTypes(const void* resource, const char* name) {
    Require(reinterpret_cast<std::intptr_t>(Vtable(resource)[-2]) == 0, "offset to top is 0");
    const TypeRecord* type = TypeOf(resource);
    Require(std::strcmp(type->name, name) == 0, name);
    Require(type->vtable == TypeInfoVptr(_ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix), "resource type is a single-base class");
    const TypeRecord* identity = type->base;
    Require(std::strcmp(identity->name, "NSt3pmr24_Identity_equal_resourceE") == 0, "_Identity_equal_resource base");
    Require(identity->vtable == TypeInfoVptr(_ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix), "_Identity_equal_resource is a single-base class");
    const TypeRecord* root = identity->base;
    Require(std::strcmp(root->name, "NSt3pmr15memory_resourceE") == 0, "memory_resource root");
    Require(root->vtable == TypeInfoVptr(_ZTVN10__cxxabiv117__class_type_infoE_nid_postfix), "memory_resource has no base");
}

}

int main() {
    std::array<void*, 10> api{};
    ApplicationHeapRegister_nid_no_patch(api.data());

    void* newDelete = sceLibcInternalPmrNewDeleteResource_nid_no_patch();
    void* nullRes = sceLibcInternalPmrNullMemoryResource_nid_no_patch();
    void* aligned = _Aligned_new_delete_resource_nid_postfix();
    void* unaligned = _Unaligned_new_delete_resource_nid_postfix();
    void* libcNull = null_memory_resource_nid_postfix();

    Require(newDelete != nullptr, "new_delete_resource returned null");
    Require(nullRes != nullptr, "null_memory_resource returned null");
    Require(newDelete == sceLibcInternalPmrNewDeleteResource_nid_no_patch(), "new_delete_resource is stable");
    Require(nullRes == sceLibcInternalPmrNullMemoryResource_nid_no_patch(), "null_memory_resource is stable");
    Require(newDelete != nullRes, "new_delete and null resources must be distinct");
    Require(newDelete != aligned && newDelete != unaligned && newDelete != libcNull, "standard new_delete is distinct from __sce_v2 family");
    Require(nullRes != aligned && nullRes != unaligned && nullRes != libcNull, "standard null_memory_resource is distinct from __sce_v2 family");

    CheckTypes(newDelete, "NSt3pmr33_Aligned_new_delete_resource_implE");
    CheckTypes(nullRes, "ZNSt3pmr20null_memory_resourceEvE14_Null_resource");

    for (void* resource : {newDelete, nullRes}) {
        Require(Slot<IsEqual>(resource, 4)(resource, resource), "is_equal to itself");
        Slot<Destroy>(resource, 0)(resource);
    }
    Require(!Slot<IsEqual>(newDelete, 4)(newDelete, nullRes), "new_delete is not null_memory_resource");
    Require(!Slot<IsEqual>(nullRes, 4)(nullRes, newDelete), "null_memory_resource is not new_delete");
    Require(!Slot<IsEqual>(newDelete, 4)(newDelete, aligned), "standard new_delete is not __sce_v2 aligned");
    Require(!Slot<IsEqual>(nullRes, 4)(nullRes, libcNull), "standard null is not __sce_v2 null");

    for (const std::size_t alignment : {std::size_t{1}, std::size_t{16}, std::size_t{32}, std::size_t{64}, std::size_t{4096}}) {
        auto* block = static_cast<unsigned char*>(Slot<Allocate>(newDelete, 2)(newDelete, 200, alignment));
        Require(block != nullptr && reinterpret_cast<std::uintptr_t>(block) % alignment == 0, "new_delete allocate honours alignment");
        std::memset(block, 0x5a, 200);
        Slot<Deallocate>(newDelete, 3)(newDelete, block, 200, alignment);
    }
    Slot<Deallocate>(nullRes, 3)(nullRes, nullptr, 0, 1);
    Slot<Deallocate>(nullRes, 3)(nullRes, newDelete, 64, 8);

    void* defaultRes = sceLibcInternalPmrGetDefaultResource_nid_no_patch();
    Require(defaultRes == newDelete, "initial default resource matches new_delete_resource");
    Require(_Aligned_get_default_resource_nid_postfix() == aligned, "initial aligned default matches aligned new_delete");
    Require(_Unaligned_get_default_resource_nid_postfix() == unaligned, "initial unaligned default matches unaligned new_delete");

    void* previousDefault = sceLibcInternalPmrSetDefaultResource_nid_no_patch(nullRes);
    Require(previousDefault == newDelete, "set_default_resource returned previous default");
    Require(sceLibcInternalPmrGetDefaultResource_nid_no_patch() == nullRes, "get_default_resource reflects update");
    Require(_Aligned_get_default_resource_nid_postfix() == aligned, "standard set_default_resource does not affect _Aligned_ default");
    Require(_Unaligned_get_default_resource_nid_postfix() == unaligned, "standard set_default_resource does not affect _Unaligned_ default");

    Require(_Aligned_set_default_resource_nid_postfix(libcNull) == aligned, "_Aligned_set_default_resource returned previous aligned default");
    Require(_Aligned_get_default_resource_nid_postfix() == libcNull, "_Aligned_get_default_resource reflects update");
    Require(sceLibcInternalPmrGetDefaultResource_nid_no_patch() == nullRes, "_Aligned_set_default_resource does not affect standard default");

    void* previousFromNull = sceLibcInternalPmrSetDefaultResource_nid_no_patch(nullptr);
    Require(previousFromNull == nullRes, "set_default_resource with null returns previous default");
    Require(sceLibcInternalPmrGetDefaultResource_nid_no_patch() == newDelete, "set_default_resource with null resets default to new_delete");
    Require(_Aligned_get_default_resource_nid_postfix() == libcNull, "standard reset does not affect _Aligned_ default");

    Require(_Aligned_set_default_resource_nid_postfix(nullptr) == libcNull, "_Aligned_set_default_resource with null returns previous default");
    Require(_Aligned_get_default_resource_nid_postfix() == aligned, "_Aligned_get_default_resource restored");

    return 0;
}
