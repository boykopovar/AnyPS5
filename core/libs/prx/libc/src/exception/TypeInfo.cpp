#include "prx/libc/include/exceptions/Runtime.hpp"
#include <cstring>
#include <stdexcept>

namespace LibcException {
namespace {

using ClassTypeInfo = __cxxabiv1::__class_type_info;

void APS5_VABI DestroyTypeInfo(std::type_info*) {}

void APS5_VABI DeleteTypeInfo(std::type_info*) {
    throw std::runtime_error("deleting guest RTTI is not supported");
}

bool APS5_VABI IsPointerTypeInfo(const std::type_info*) { return false; }
bool APS5_VABI IsPointerTypeInfoTrue(const std::type_info*) { return true; }
bool APS5_VABI IsFunctionTypeInfoTrue(const std::type_info*) { return true; }

bool APS5_VABI CatchTypeInfo(const std::type_info* self, const std::type_info* thrown, void** object, unsigned) {
    if (!self || !thrown || !object) throw std::invalid_argument("invalid RTTI catch arguments");
    return Match(self, thrown, *object);
}

bool APS5_VABI UpcastTypeInfo(const std::type_info* self, const ClassTypeInfo* base, void** object) {
    if (!self || !base || !object) throw std::invalid_argument("invalid RTTI upcast arguments");
    Search search {base};
    Bases(self, *object, search);
    if (search.count != 1) return false;
    *object = search.found;
    return true;
}

bool APS5_VABI UpcastTypeInfoResult(const ClassTypeInfo*, const ClassTypeInfo*, const void*, ClassTypeInfo::__upcast_result&) {
    throw std::runtime_error("RTTI __upcast_result is not supported");
}

bool APS5_VABI DynamicCastTypeInfo(const ClassTypeInfo*, std::ptrdiff_t, ClassTypeInfo::__sub_kind, const ClassTypeInfo*, const void*, const ClassTypeInfo*, const void*, ClassTypeInfo::__dyncast_result&) {
    throw std::runtime_error("RTTI __dyncast_result is not supported");
}

ClassTypeInfo::__sub_kind APS5_VABI FindPublicTypeInfo(const ClassTypeInfo* self, std::ptrdiff_t src2dst,
                                                       const void* object, const ClassTypeInfo* sourceType,
                                                       const void* source) {
    constexpr int notContained = 1;
    constexpr int containedPublic = 6;
    constexpr int containedMask = 4;
    constexpr int publicMask = 2;
    constexpr int virtualMask = 1;
    if (!self || !sourceType) throw std::invalid_argument("RTTI __do_find_public_src: null type");
    const auto* vtable = *reinterpret_cast<const std::type_info* const* const*>(self);
    const char* kind = vtable[-1]->name();
    if (std::strcmp(kind, "N10__cxxabiv120__si_class_type_infoE") == 0) {
        if (object == source && *self == *sourceType)
            return static_cast<ClassTypeInfo::__sub_kind>(containedPublic);
        const auto* single = static_cast<const __cxxabiv1::__si_class_type_info*>(self);
        return FindPublicTypeInfo(single->__base_type, src2dst, object, sourceType, source);
    }
    if (std::strcmp(kind, "N10__cxxabiv121__vmi_class_type_infoE") == 0) {
        if (object == source && *self == *sourceType)
            return static_cast<ClassTypeInfo::__sub_kind>(containedPublic);
    } else {
        return static_cast<ClassTypeInfo::__sub_kind>(object == source ? containedPublic : notContained);
    }

    const auto* multiple = static_cast<const __cxxabiv1::__vmi_class_type_info*>(self);
    for (unsigned index = multiple->__base_count; index-- > 0;) {
        const auto& base = multiple->__base_info[index];
        if ((base.__offset_flags & publicMask) == 0) continue;
        const bool isVirtual = (base.__offset_flags & virtualMask) != 0;
        if (isVirtual && src2dst == -3) continue;
        auto offset = base.__offset_flags >> 8;
        const void* baseObject = object;
        if (isVirtual) {
            if (!object) continue;
            const auto* addressPoint = *static_cast<const std::intptr_t* const*>(object);
            std::memcpy(&offset, reinterpret_cast<const unsigned char*>(addressPoint) + offset, sizeof(offset));
        }
        if (object) baseObject = static_cast<const unsigned char*>(object) + offset;
        const auto result = FindPublicTypeInfo(base.__base_type, src2dst, baseObject, sourceType, source);
        const int bits = static_cast<int>(result);
        if ((bits & containedMask) == 0) continue;
        return static_cast<ClassTypeInfo::__sub_kind>(bits | (isVirtual ? virtualMask : 0));
    }
    return static_cast<ClassTypeInfo::__sub_kind>(notContained);
}

}

struct TypeInfoVtable {
    std::ptrdiff_t offset;
    const std::type_info* type;
    decltype(&DestroyTypeInfo) destroy = DestroyTypeInfo;
    decltype(&DeleteTypeInfo) deleteObject = DeleteTypeInfo;
    decltype(&IsPointerTypeInfo) isPointer = IsPointerTypeInfo;
    decltype(&IsPointerTypeInfo) isFunction = IsPointerTypeInfo;
    decltype(&CatchTypeInfo) catchType = CatchTypeInfo;
    decltype(&UpcastTypeInfo) upcast = UpcastTypeInfo;
    decltype(&UpcastTypeInfoResult) upcastResult = UpcastTypeInfoResult;
    decltype(&DynamicCastTypeInfo) dynamicCast = DynamicCastTypeInfo;
    decltype(&FindPublicTypeInfo) findPublic = FindPublicTypeInfo;
};

static_assert(offsetof(TypeInfoVtable, destroy) == 2 * sizeof(void*));
static_assert(sizeof(TypeInfoVtable) == 11 * sizeof(void*));

}

extern "C" {
LibcException::TypeInfoVtable _ZTVN10__cxxabiv117__class_type_infoE_nid_postfix {0, &typeid(__cxxabiv1::__class_type_info)};
LibcException::TypeInfoVtable _ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix {0, &typeid(__cxxabiv1::__si_class_type_info)};
LibcException::TypeInfoVtable _ZTVN10__cxxabiv121__vmi_class_type_infoE_nid_postfix {0, &typeid(__cxxabiv1::__vmi_class_type_info)};
LibcException::TypeInfoVtable _ZTVN10__cxxabiv119__pointer_type_infoE_nid_postfix {
    0,
    &typeid(__cxxabiv1::__pointer_type_info),
    LibcException::DestroyTypeInfo,
    LibcException::DeleteTypeInfo,
    LibcException::IsPointerTypeInfoTrue
};
LibcException::TypeInfoVtable _ZTVN10__cxxabiv120__function_type_infoE_nid_postfix {
    0,
    &typeid(__cxxabiv1::__function_type_info),
    LibcException::DestroyTypeInfo,
    LibcException::DeleteTypeInfo,
    LibcException::IsPointerTypeInfo,
    LibcException::IsFunctionTypeInfoTrue
};
}
