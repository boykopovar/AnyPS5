#include "prx/libc/include/exceptions/Runtime.hpp"
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

ClassTypeInfo::__sub_kind APS5_VABI FindPublicTypeInfo(const ClassTypeInfo*, std::ptrdiff_t, const void*, const ClassTypeInfo*, const void*) {
    throw std::runtime_error("RTTI __do_find_public_src is not supported");
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
