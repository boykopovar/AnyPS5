#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
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

const ClassTypeInfo MemoryResourceType {TypeInfoVptr(_ZTVN10__cxxabiv117__class_type_infoE_nid_postfix), "NSt8__sce_v23pmr15memory_resourceE"};
const SingleTypeInfo IdentityEqualResourceType {TypeInfoVptr(_ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix), "NSt8__sce_v23pmr24_Identity_equal_resourceE", &MemoryResourceType};
const SingleTypeInfo AlignedNewDeleteResourceType {TypeInfoVptr(_ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix), "NSt8__sce_v23pmr33_Aligned_new_delete_resource_implE", &IdentityEqualResourceType};
const SingleTypeInfo UnalignedNewDeleteResourceType {TypeInfoVptr(_ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix), "NSt8__sce_v23pmr35_Unaligned_new_delete_resource_implE", &IdentityEqualResourceType};
const SingleTypeInfo NullResourceType {TypeInfoVptr(_ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix), "Z20null_memory_resourceE14_Null_resource", &IdentityEqualResourceType};

void APS5_VABI DestroyResource(MemoryResource*) {}

void APS5_VABI DeleteResource(MemoryResource* self) {
    _ZdlPv_nid_postfix(self);
}

bool APS5_VABI IsSameResource(const MemoryResource* self, const MemoryResource* other) {
    return self == other;
}

void* APS5_VABI AlignedAllocate(MemoryResource*, std::size_t bytes, std::size_t alignment) {
    if (alignment > DefaultNewAlignment) return _ZnwmSt11align_val_t_nid_postfix(bytes, alignment);
    return _Znwm_nid_postfix(bytes);
}

void APS5_VABI AlignedDeallocate(MemoryResource*, void* pointer, std::size_t bytes, std::size_t alignment) {
    if (alignment > DefaultNewAlignment)
        _ZdlPvmSt11align_val_t_nid_postfix(pointer, bytes, alignment);
    else
        _ZdlPvm_nid_postfix(pointer, bytes);
}

void* APS5_VABI UnalignedAllocate(MemoryResource*, std::size_t bytes, std::size_t alignment) {
    if (alignment > DefaultNewAlignment) _ZNSt8__sce_v211_Xbad_allocEv_nid_postfix();
    return _Znwm_nid_postfix(bytes);
}

void APS5_VABI UnalignedDeallocate(MemoryResource*, void* pointer, std::size_t bytes, std::size_t) {
    _ZdlPvm_nid_postfix(pointer, bytes);
}

void* APS5_VABI NullAllocate(MemoryResource*, std::size_t, std::size_t) {
    _ZNSt8__sce_v211_Xbad_allocEv_nid_postfix();
}

void APS5_VABI NullDeallocate(MemoryResource*, void*, std::size_t, std::size_t) {}

const MemoryResourceVtable AlignedNewDeleteVtable {0, &AlignedNewDeleteResourceType, DestroyResource, DeleteResource, AlignedAllocate, AlignedDeallocate, IsSameResource};
const MemoryResourceVtable UnalignedNewDeleteVtable {0, &UnalignedNewDeleteResourceType, DestroyResource, DeleteResource, UnalignedAllocate, UnalignedDeallocate, IsSameResource};
const MemoryResourceVtable NullVtable {0, &NullResourceType, DestroyResource, DeleteResource, NullAllocate, NullDeallocate, IsSameResource};

MemoryResource AlignedNewDeleteResource {&AlignedNewDeleteVtable.destroy};
MemoryResource UnalignedNewDeleteResource {&UnalignedNewDeleteVtable.destroy};
MemoryResource NullResource {&NullVtable.destroy};

std::atomic<MemoryResource*> DefaultResource {nullptr};

MemoryResource* OrFallback(MemoryResource* resource, MemoryResource* fallback) {
    return resource != nullptr ? resource : fallback;
}

}

extern "C" {

void* APS5_VABI null_memory_resource_nid_postfix() {
    return &NullResource;
}

void* APS5_VABI _Aligned_new_delete_resource_nid_postfix() {
    return &AlignedNewDeleteResource;
}

void* APS5_VABI _Unaligned_new_delete_resource_nid_postfix() {
    return &UnalignedNewDeleteResource;
}

void* APS5_VABI _Aligned_get_default_resource_nid_postfix() {
    return OrFallback(DefaultResource.load(), &AlignedNewDeleteResource);
}

void* APS5_VABI _Unaligned_get_default_resource_nid_postfix() {
    return OrFallback(DefaultResource.load(), &UnalignedNewDeleteResource);
}

void* APS5_VABI _Aligned_set_default_resource_nid_postfix(void* resource) {
    return OrFallback(DefaultResource.exchange(static_cast<MemoryResource*>(resource)), &AlignedNewDeleteResource);
}

void* APS5_VABI _Unaligned_set_default_resource_nid_postfix(void* resource) {
    return OrFallback(DefaultResource.exchange(static_cast<MemoryResource*>(resource)), &UnalignedNewDeleteResource);
}

}
