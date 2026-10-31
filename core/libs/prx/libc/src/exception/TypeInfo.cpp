#include "prx/libc/include/Api.hpp"
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
LibcException::TypeInfoVtable _ZTVN10__cxxabiv123__fundamental_type_infoE_nid_postfix {
    0,
    &typeid(__cxxabiv1::__fundamental_type_info)
};
}

namespace {
struct FundamentalTypeRecord { const void* const* vtable; const char* name; };
const void* const* FundamentalVtable() {
    return reinterpret_cast<const void* const*>(&_ZTVN10__cxxabiv123__fundamental_type_infoE_nid_postfix) + 2;
}
}

extern "C" {
FundamentalTypeRecord _ZTIv_nid_postfix {FundamentalVtable(), "v"};
FundamentalTypeRecord _ZTIw_nid_postfix {FundamentalVtable(), "w"};
FundamentalTypeRecord _ZTIb_nid_postfix {FundamentalVtable(), "b"};
FundamentalTypeRecord _ZTIc_nid_postfix {FundamentalVtable(), "c"};
FundamentalTypeRecord _ZTIa_nid_postfix {FundamentalVtable(), "a"};
FundamentalTypeRecord _ZTIh_nid_postfix {FundamentalVtable(), "h"};
FundamentalTypeRecord _ZTIs_nid_postfix {FundamentalVtable(), "s"};
FundamentalTypeRecord _ZTIt_nid_postfix {FundamentalVtable(), "t"};
FundamentalTypeRecord _ZTIi_nid_postfix {FundamentalVtable(), "i"};
FundamentalTypeRecord _ZTIj_nid_postfix {FundamentalVtable(), "j"};
FundamentalTypeRecord _ZTIl_nid_postfix {FundamentalVtable(), "l"};
FundamentalTypeRecord _ZTIm_nid_postfix {FundamentalVtable(), "m"};
FundamentalTypeRecord _ZTIx_nid_postfix {FundamentalVtable(), "x"};
FundamentalTypeRecord _ZTIy_nid_postfix {FundamentalVtable(), "y"};
FundamentalTypeRecord _ZTIn_nid_postfix {FundamentalVtable(), "n"};
FundamentalTypeRecord _ZTIo_nid_postfix {FundamentalVtable(), "o"};
FundamentalTypeRecord _ZTIf_nid_postfix {FundamentalVtable(), "f"};
FundamentalTypeRecord _ZTId_nid_postfix {FundamentalVtable(), "d"};
FundamentalTypeRecord _ZTIe_nid_postfix {FundamentalVtable(), "e"};
FundamentalTypeRecord _ZTIg_nid_postfix {FundamentalVtable(), "g"};
FundamentalTypeRecord _ZTIz_nid_postfix {FundamentalVtable(), "z"};
FundamentalTypeRecord _ZTIDn_nid_postfix {FundamentalVtable(), "Dn"};
FundamentalTypeRecord _ZTIDs_nid_postfix {FundamentalVtable(), "Ds"};
FundamentalTypeRecord _ZTIDi_nid_postfix {FundamentalVtable(), "Di"};
FundamentalTypeRecord _ZTIDu_nid_postfix {FundamentalVtable(), "Du"};
FundamentalTypeRecord _ZTIDh_nid_postfix {FundamentalVtable(), "Dh"};
}
