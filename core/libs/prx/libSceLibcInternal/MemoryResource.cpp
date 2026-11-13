#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <cstddef>

namespace LibcException {
struct TypeInfoVtable;
}

extern "C" {
extern LibcException::TypeInfoVtable _ZTVN10__cxxabiv117__class_type_infoE_nid_postfix;
extern LibcException::TypeInfoVtable _ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix;
}

namespace {

struct MemoryResource {
    const void* vtable;
};

struct ClassTypeInfo {
    const void* vtable;
    const char* name;
};

struct SingleTypeInfo {
    const void* vtable;
    const char* name;
    const void* base;
};

struct MemoryResourceVtable {
    std::ptrdiff_t offset;
    const SingleTypeInfo* type;
    void (APS5_VABI* destroy)(MemoryResource*);
    void (APS5_VABI* deleteObject)(MemoryResource*);
    void* (APS5_VABI* allocate)(MemoryResource*, std::size_t, std::size_t);
    void (APS5_VABI* deallocate)(MemoryResource*, void*, std::size_t, std::size_t);
    bool (APS5_VABI* isEqual)(const MemoryResource*, const MemoryResource*);
};

constexpr std::size_t DefaultNewAlignment = 32;

const void* TypeInfoVptr(LibcException::TypeInfoVtable& vtable) {
    return reinterpret_cast<const unsigned char*>(&vtable) + 2 * sizeof(void*);
}

const ClassTypeInfo MemoryResourceType {TypeInfoVptr(_ZTVN10__cxxabiv117__class_type_infoE_nid_postfix), "NSt3pmr15memory_resourceE"};
const SingleTypeInfo IdentityEqualResourceType {TypeInfoVptr(_ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix), "NSt3pmr24_Identity_equal_resourceE", &MemoryResourceType};
const SingleTypeInfo NewDeleteResourceType {TypeInfoVptr(_ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix), "NSt3pmr33_Aligned_new_delete_resource_implE", &IdentityEqualResourceType};
const SingleTypeInfo NullResourceType {TypeInfoVptr(_ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix), "ZNSt3pmr20null_memory_resourceEvE14_Null_resource", &IdentityEqualResourceType};

void APS5_VABI DestroyPmrResource_nid_no_patch(MemoryResource*) {}

void APS5_VABI DeletePmrResource_nid_no_patch(MemoryResource* self) {
    _ZdlPv_nid_postfix(self);
}

bool APS5_VABI IsSamePmrResource_nid_no_patch(const MemoryResource* self, const MemoryResource* other) {
    return self == other;
}

void* APS5_VABI NewDeletePmrAllocate_nid_no_patch(MemoryResource*, std::size_t bytes, std::size_t alignment) {
    if (alignment > DefaultNewAlignment) return _ZnwmSt11align_val_t_nid_postfix(bytes, alignment);
    return _Znwm_nid_postfix(bytes);
}

void APS5_VABI NewDeletePmrDeallocate_nid_no_patch(MemoryResource*, void* pointer, std::size_t bytes, std::size_t alignment) {
    if (alignment > DefaultNewAlignment)
        _ZdlPvmSt11align_val_t_nid_postfix(pointer, bytes, alignment);
    else
        _ZdlPvm_nid_postfix(pointer, bytes);
}

void* APS5_VABI NullPmrAllocate_nid_no_patch(MemoryResource*, std::size_t, std::size_t) {
    _ZSt11_Xbad_allocv_nid_postfix();
}

void APS5_VABI NullPmrDeallocate_nid_no_patch(MemoryResource*, void*, std::size_t, std::size_t) {}

const MemoryResourceVtable NewDeleteVtable {0, &NewDeleteResourceType, DestroyPmrResource_nid_no_patch, DeletePmrResource_nid_no_patch, NewDeletePmrAllocate_nid_no_patch, NewDeletePmrDeallocate_nid_no_patch, IsSamePmrResource_nid_no_patch};
const MemoryResourceVtable NullVtable {0, &NullResourceType, DestroyPmrResource_nid_no_patch, DeletePmrResource_nid_no_patch, NullPmrAllocate_nid_no_patch, NullPmrDeallocate_nid_no_patch, IsSamePmrResource_nid_no_patch};

MemoryResource NewDeleteResource {&NewDeleteVtable.destroy};
MemoryResource NullResource {&NullVtable.destroy};

std::atomic<MemoryResource*> DefaultResource {nullptr};

MemoryResource* OrFallback(MemoryResource* resource, MemoryResource* fallback) {
    return resource != nullptr ? resource : fallback;
}

}

extern "C" {

void* APS5_VABI sceLibcInternalPmrNewDeleteResource_nid_no_patch() noexcept {
    return &NewDeleteResource;
}
APS5_EXPORT("9G32u5RRYxE", sceLibcInternalPmrNewDeleteResource_nid_no_patch);

void* APS5_VABI sceLibcInternalPmrGetDefaultResource_nid_no_patch() noexcept {
    return OrFallback(DefaultResource.load(), &NewDeleteResource);
}
APS5_EXPORT("d2u38zs4Pe8", sceLibcInternalPmrGetDefaultResource_nid_no_patch);

void* APS5_VABI sceLibcInternalPmrNullMemoryResource_nid_no_patch() noexcept {
    return &NullResource;
}
APS5_EXPORT("eWMGI7B7Lyc", sceLibcInternalPmrNullMemoryResource_nid_no_patch);

void* APS5_VABI sceLibcInternalPmrSetDefaultResource_nid_no_patch(void* resource) noexcept {
    return OrFallback(DefaultResource.exchange(static_cast<MemoryResource*>(resource)), &NewDeleteResource);
}
APS5_EXPORT("TKYsv0jdvRw", sceLibcInternalPmrSetDefaultResource_nid_no_patch);

}
