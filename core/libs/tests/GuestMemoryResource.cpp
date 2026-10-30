#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/AsmMacros.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace LibcException {
struct TypeInfoVtable;
}

struct TypeRecord { const void* vtable; const char* name; const TypeRecord* base; };

using Thrower = void (APS5_VABI *)(const void*);
using Allocate = void* (APS5_VABI *)(void*, std::size_t, std::size_t);
using Deallocate = void (APS5_VABI *)(void*, void*, std::size_t, std::size_t);
using IsEqual = bool (APS5_VABI *)(const void*, const void*);
using Destroy = void (APS5_VABI *)(void*);

extern "C" {
extern LibcException::TypeInfoVtable _ZTVN10__cxxabiv117__class_type_infoE_nid_postfix;
extern LibcException::TypeInfoVtable _ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix;
extern const TypeRecord _ZTISt9bad_alloc_nid_postfix;
extern const TypeRecord _ZTISt8bad_cast_nid_postfix;

void APS5_VABI SceV2Catch(Thrower thrower, const void* argument);
void APS5_VABI AllocateOverAligned(const void* resource);

const void* SceV2CatchType = nullptr;
const void* SceV2OtherType = nullptr;
int SceV2Selector = 0;
}

asm(".text\n"
    APS5_ASM_FUNCTION("AllocateOverAligned")
    "    movl $64, %edx\n"
    "    movl $16, %esi\n"
    "    movq (%rdi), %rax\n"
    "    jmp *16(%rax)\n"
    APS5_ASM_FUNCTION_END("AllocateOverAligned"));

static unsigned failures = 0;

static void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
}

static char caught[128];

extern "C" void APS5_VABI SceV2Inspect(void* object) {
    const auto* vtable = *static_cast<const void* const* const*>(object);
    std::snprintf(caught, sizeof(caught), "%s", static_cast<const TypeRecord*>(vtable[-1])->name);
}

static const void* const* Vtable(const void* resource) {
    return *static_cast<const void* const* const*>(resource);
}

static const TypeRecord* TypeOf(const void* resource) {
    return static_cast<const TypeRecord*>(Vtable(resource)[-1]);
}

template<typename TFunction>
static TFunction Slot(const void* resource, std::size_t index) {
    return reinterpret_cast<TFunction>(const_cast<void*>(Vtable(resource)[index]));
}

static const void* TypeInfoVptr(LibcException::TypeInfoVtable& vtable) {
    return reinterpret_cast<const unsigned char*>(&vtable) + 2 * sizeof(void*);
}

static void CheckTypes(const void* resource, const char* name) {
    Require(reinterpret_cast<std::intptr_t>(Vtable(resource)[-2]) == 0, "offset to top is 0");
    const TypeRecord* type = TypeOf(resource);
    Require(std::strcmp(type->name, name) == 0, name);
    Require(type->vtable == TypeInfoVptr(_ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix), "resource type is a single-base class");
    const TypeRecord* identity = type->base;
    Require(std::strcmp(identity->name, "NSt8__sce_v23pmr24_Identity_equal_resourceE") == 0, "_Identity_equal_resource base");
    Require(identity->vtable == TypeInfoVptr(_ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix), "_Identity_equal_resource is a single-base class");
    const TypeRecord* root = identity->base;
    Require(std::strcmp(root->name, "NSt8__sce_v23pmr15memory_resourceE") == 0, "memory_resource root");
    Require(root->vtable == TypeInfoVptr(_ZTVN10__cxxabiv117__class_type_infoE_nid_postfix), "memory_resource has no base");
}

static void CheckThrows(Thrower thrower, const void* argument, const char* what) {
    SceV2CatchType = &_ZTISt9bad_alloc_nid_postfix;
    SceV2OtherType = &_ZTISt8bad_cast_nid_postfix;
    SceV2Selector = 0;
    caught[0] = 0;
    SceV2Catch(thrower, argument);
    Require(SceV2Selector == 2, what);
    Require(std::strcmp(caught, "St9bad_alloc") == 0, what);
}

static void CheckResources() {
    void* aligned = _Aligned_new_delete_resource_nid_postfix();
    void* unaligned = _Unaligned_new_delete_resource_nid_postfix();
    void* empty = null_memory_resource_nid_postfix();
    Require(aligned && unaligned && empty, "resources exist");
    Require(aligned != unaligned && aligned != empty && unaligned != empty, "resources are distinct");
    Require(aligned == _Aligned_new_delete_resource_nid_postfix() && empty == null_memory_resource_nid_postfix(), "resources are stable");
    CheckTypes(aligned, "NSt8__sce_v23pmr33_Aligned_new_delete_resource_implE");
    CheckTypes(unaligned, "NSt8__sce_v23pmr35_Unaligned_new_delete_resource_implE");
    CheckTypes(empty, "Z20null_memory_resourceE14_Null_resource");

    for (void* resource : {aligned, unaligned, empty}) {
        Require(Slot<IsEqual>(resource, 4)(resource, resource), "is_equal to itself");
        Slot<Destroy>(resource, 0)(resource);
    }
    Require(!Slot<IsEqual>(aligned, 4)(aligned, unaligned), "aligned is not unaligned");
    Require(!Slot<IsEqual>(unaligned, 4)(unaligned, empty), "unaligned is not null");
    Require(!Slot<IsEqual>(empty, 4)(empty, aligned), "null is not aligned");

    for (const std::size_t alignment : {std::size_t{1}, std::size_t{16}, std::size_t{32}, std::size_t{64}, std::size_t{4096}}) {
        auto* block = static_cast<unsigned char*>(Slot<Allocate>(aligned, 2)(aligned, 200, alignment));
        Require(block != nullptr && reinterpret_cast<std::uintptr_t>(block) % alignment == 0, "aligned allocate honours the alignment");
        std::memset(block, 0x5a, 200);
        Slot<Deallocate>(aligned, 3)(aligned, block, 200, alignment);
    }
    for (const std::size_t alignment : {std::size_t{1}, std::size_t{8}, std::size_t{32}}) {
        auto* block = static_cast<unsigned char*>(Slot<Allocate>(unaligned, 2)(unaligned, 48, alignment));
        Require(block != nullptr && reinterpret_cast<std::uintptr_t>(block) % alignment == 0, "unaligned allocate up to 32");
        std::memset(block, 0xa5, 48);
        Slot<Deallocate>(unaligned, 3)(unaligned, block, 48, alignment);
    }
    Slot<Deallocate>(empty, 3)(empty, nullptr, 0, 1);
    Slot<Deallocate>(empty, 3)(empty, aligned, 64, 8);

    CheckThrows(AllocateOverAligned, unaligned, "unaligned allocate over 32 throws bad_alloc");
    CheckThrows(reinterpret_cast<Thrower>(const_cast<void*>(Vtable(empty)[2])), empty, "null allocate throws bad_alloc");
}

static void CheckDefaults() {
    void* aligned = _Aligned_new_delete_resource_nid_postfix();
    void* unaligned = _Unaligned_new_delete_resource_nid_postfix();
    void* empty = null_memory_resource_nid_postfix();
    Require(_Aligned_get_default_resource_nid_postfix() == aligned, "aligned default starts as new_delete");
    Require(_Unaligned_get_default_resource_nid_postfix() == unaligned, "unaligned default starts as new_delete");
    Require(_Aligned_set_default_resource_nid_postfix(empty) == aligned, "first aligned set returns new_delete");
    Require(_Aligned_get_default_resource_nid_postfix() == empty && _Unaligned_get_default_resource_nid_postfix() == empty, "both share one default");
    Require(_Unaligned_set_default_resource_nid_postfix(aligned) == empty, "set returns the previous resource");
    Require(_Unaligned_get_default_resource_nid_postfix() == aligned, "unaligned set changes the shared default");
    Require(_Aligned_set_default_resource_nid_postfix(nullptr) == aligned, "reset returns the previous resource");
    Require(_Aligned_get_default_resource_nid_postfix() == aligned && _Unaligned_get_default_resource_nid_postfix() == unaligned, "null restores each new_delete");
    Require(_Unaligned_set_default_resource_nid_postfix(nullptr) == unaligned, "unaligned set from null returns unaligned new_delete");
    Require(_Aligned_set_default_resource_nid_postfix(nullptr) == aligned, "aligned set from null returns aligned new_delete");
}

int main() {
    std::array<void*, 10> api{};
    ApplicationHeapRegister_nid_no_patch(api.data());
    CheckResources();
    CheckDefaults();
    if (failures != 0) {
        std::fprintf(stderr, "%u failures\n", failures);
        return 1;
    }
}
