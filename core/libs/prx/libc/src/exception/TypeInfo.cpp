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

struct SiTypeRecord { const void* const* vtable; const char* name; const std::type_info* base; };
struct PointerTypeRecord { const void* const* vtable; const char* name; unsigned flags; const std::type_info* pointee; };

}

namespace {
const std::type_info* AsTypeInfo(const LibcException::SiTypeRecord& record) {
    return reinterpret_cast<const std::type_info*>(&record);
}
}

extern "C" {
extern LibcException::SiTypeRecord _ZTISt9type_info_nid_postfix;
extern LibcException::SiTypeRecord _ZTIN10__cxxabiv116__shim_type_infoE_nid_postfix;
extern LibcException::SiTypeRecord _ZTIN10__cxxabiv116__enum_type_infoE_nid_postfix;
extern LibcException::SiTypeRecord _ZTIN10__cxxabiv117__array_type_infoE_nid_postfix;
extern LibcException::SiTypeRecord _ZTIN10__cxxabiv117__class_type_infoE_nid_postfix;
extern LibcException::SiTypeRecord _ZTIN10__cxxabiv117__pbase_type_infoE_nid_postfix;
extern LibcException::SiTypeRecord _ZTIN10__cxxabiv119__pointer_type_infoE_nid_postfix;
extern LibcException::SiTypeRecord _ZTIN10__cxxabiv120__function_type_infoE_nid_postfix;
extern LibcException::SiTypeRecord _ZTIN10__cxxabiv120__si_class_type_infoE_nid_postfix;
extern LibcException::SiTypeRecord _ZTIN10__cxxabiv121__vmi_class_type_infoE_nid_postfix;
extern LibcException::SiTypeRecord _ZTIN10__cxxabiv123__fundamental_type_infoE_nid_postfix;
extern LibcException::SiTypeRecord _ZTIN10__cxxabiv129__pointer_to_member_type_infoE_nid_postfix;
LibcException::TypeInfoVtable _ZTVN10__cxxabiv117__class_type_infoE_nid_postfix {
    0,
    AsTypeInfo(_ZTIN10__cxxabiv117__class_type_infoE_nid_postfix)
};
LibcException::TypeInfoVtable _ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix {
    0,
    AsTypeInfo(_ZTIN10__cxxabiv120__si_class_type_infoE_nid_postfix)
};
LibcException::TypeInfoVtable _ZTVN10__cxxabiv121__vmi_class_type_infoE_nid_postfix {
    0,
    AsTypeInfo(_ZTIN10__cxxabiv121__vmi_class_type_infoE_nid_postfix)
};
LibcException::TypeInfoVtable _ZTVN10__cxxabiv119__pointer_type_infoE_nid_postfix {
    0,
    AsTypeInfo(_ZTIN10__cxxabiv119__pointer_type_infoE_nid_postfix),
    LibcException::DestroyTypeInfo,
    LibcException::DeleteTypeInfo,
    LibcException::IsPointerTypeInfoTrue
};
LibcException::TypeInfoVtable _ZTVN10__cxxabiv120__function_type_infoE_nid_postfix {
    0,
    AsTypeInfo(_ZTIN10__cxxabiv120__function_type_infoE_nid_postfix),
    LibcException::DestroyTypeInfo,
    LibcException::DeleteTypeInfo,
    LibcException::IsPointerTypeInfo,
    LibcException::IsFunctionTypeInfoTrue
};
LibcException::TypeInfoVtable _ZTVN10__cxxabiv123__fundamental_type_infoE_nid_postfix {
    0,
    AsTypeInfo(_ZTIN10__cxxabiv123__fundamental_type_infoE_nid_postfix)
};
LibcException::TypeInfoVtable _ZTVN10__cxxabiv116__shim_type_infoE_nid_postfix {
    0,
    AsTypeInfo(_ZTIN10__cxxabiv116__shim_type_infoE_nid_postfix)
};
LibcException::TypeInfoVtable _ZTVN10__cxxabiv116__enum_type_infoE_nid_postfix {
    0,
    AsTypeInfo(_ZTIN10__cxxabiv116__enum_type_infoE_nid_postfix)
};
LibcException::TypeInfoVtable _ZTVN10__cxxabiv117__array_type_infoE_nid_postfix {
    0,
    AsTypeInfo(_ZTIN10__cxxabiv117__array_type_infoE_nid_postfix)
};
LibcException::TypeInfoVtable _ZTVN10__cxxabiv117__pbase_type_infoE_nid_postfix {
    0,
    AsTypeInfo(_ZTIN10__cxxabiv117__pbase_type_infoE_nid_postfix)
};
LibcException::TypeInfoVtable _ZTVN10__cxxabiv129__pointer_to_member_type_infoE_nid_postfix {
    0,
    AsTypeInfo(_ZTIN10__cxxabiv129__pointer_to_member_type_infoE_nid_postfix)
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

namespace {
const void* const* ClassVtable() {
    return reinterpret_cast<const void* const*>(&_ZTVN10__cxxabiv117__class_type_infoE_nid_postfix) + 2;
}
const void* const* SingleVtable() {
    return reinterpret_cast<const void* const*>(&_ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix) + 2;
}
const void* const* PointerVtable() {
    return reinterpret_cast<const void* const*>(&_ZTVN10__cxxabiv119__pointer_type_infoE_nid_postfix) + 2;
}
const std::type_info* AsTypeInfo(const FundamentalTypeRecord& record) {
    return reinterpret_cast<const std::type_info*>(&record);
}
}

extern "C" {
LibcException::SiTypeRecord _ZTISt9type_info_nid_postfix {ClassVtable(), "St9type_info", nullptr};
LibcException::SiTypeRecord _ZTIN10__cxxabiv116__shim_type_infoE_nid_postfix {SingleVtable(), "N10__cxxabiv116__shim_type_infoE", AsTypeInfo(_ZTISt9type_info_nid_postfix)};
LibcException::SiTypeRecord _ZTIN10__cxxabiv116__enum_type_infoE_nid_postfix {SingleVtable(), "N10__cxxabiv116__enum_type_infoE", AsTypeInfo(_ZTIN10__cxxabiv116__shim_type_infoE_nid_postfix)};
LibcException::SiTypeRecord _ZTIN10__cxxabiv117__array_type_infoE_nid_postfix {SingleVtable(), "N10__cxxabiv117__array_type_infoE", AsTypeInfo(_ZTIN10__cxxabiv116__shim_type_infoE_nid_postfix)};
LibcException::SiTypeRecord _ZTIN10__cxxabiv117__class_type_infoE_nid_postfix {SingleVtable(), "N10__cxxabiv117__class_type_infoE", AsTypeInfo(_ZTIN10__cxxabiv116__shim_type_infoE_nid_postfix)};
LibcException::SiTypeRecord _ZTIN10__cxxabiv117__pbase_type_infoE_nid_postfix {SingleVtable(), "N10__cxxabiv117__pbase_type_infoE", AsTypeInfo(_ZTIN10__cxxabiv116__shim_type_infoE_nid_postfix)};
LibcException::SiTypeRecord _ZTIN10__cxxabiv119__pointer_type_infoE_nid_postfix {SingleVtable(), "N10__cxxabiv119__pointer_type_infoE", AsTypeInfo(_ZTIN10__cxxabiv117__pbase_type_infoE_nid_postfix)};
LibcException::SiTypeRecord _ZTIN10__cxxabiv120__function_type_infoE_nid_postfix {SingleVtable(), "N10__cxxabiv120__function_type_infoE", AsTypeInfo(_ZTIN10__cxxabiv116__shim_type_infoE_nid_postfix)};
LibcException::SiTypeRecord _ZTIN10__cxxabiv120__si_class_type_infoE_nid_postfix {SingleVtable(), "N10__cxxabiv120__si_class_type_infoE", AsTypeInfo(_ZTIN10__cxxabiv117__class_type_infoE_nid_postfix)};
LibcException::SiTypeRecord _ZTIN10__cxxabiv121__vmi_class_type_infoE_nid_postfix {SingleVtable(), "N10__cxxabiv121__vmi_class_type_infoE", AsTypeInfo(_ZTIN10__cxxabiv117__class_type_infoE_nid_postfix)};
LibcException::SiTypeRecord _ZTIN10__cxxabiv123__fundamental_type_infoE_nid_postfix {SingleVtable(), "N10__cxxabiv123__fundamental_type_infoE", AsTypeInfo(_ZTIN10__cxxabiv116__shim_type_infoE_nid_postfix)};
LibcException::SiTypeRecord _ZTIN10__cxxabiv129__pointer_to_member_type_infoE_nid_postfix {SingleVtable(), "N10__cxxabiv129__pointer_to_member_type_infoE", AsTypeInfo(_ZTIN10__cxxabiv117__pbase_type_infoE_nid_postfix)};

LibcException::PointerTypeRecord _ZTIPv_nid_postfix {PointerVtable(), "Pv", 0, AsTypeInfo(_ZTIv_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPw_nid_postfix {PointerVtable(), "Pw", 0, AsTypeInfo(_ZTIw_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPb_nid_postfix {PointerVtable(), "Pb", 0, AsTypeInfo(_ZTIb_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPc_nid_postfix {PointerVtable(), "Pc", 0, AsTypeInfo(_ZTIc_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPa_nid_postfix {PointerVtable(), "Pa", 0, AsTypeInfo(_ZTIa_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPh_nid_postfix {PointerVtable(), "Ph", 0, AsTypeInfo(_ZTIh_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPs_nid_postfix {PointerVtable(), "Ps", 0, AsTypeInfo(_ZTIs_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPt_nid_postfix {PointerVtable(), "Pt", 0, AsTypeInfo(_ZTIt_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPi_nid_postfix {PointerVtable(), "Pi", 0, AsTypeInfo(_ZTIi_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPj_nid_postfix {PointerVtable(), "Pj", 0, AsTypeInfo(_ZTIj_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPl_nid_postfix {PointerVtable(), "Pl", 0, AsTypeInfo(_ZTIl_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPm_nid_postfix {PointerVtable(), "Pm", 0, AsTypeInfo(_ZTIm_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPx_nid_postfix {PointerVtable(), "Px", 0, AsTypeInfo(_ZTIx_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPy_nid_postfix {PointerVtable(), "Py", 0, AsTypeInfo(_ZTIy_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPn_nid_postfix {PointerVtable(), "Pn", 0, AsTypeInfo(_ZTIn_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPo_nid_postfix {PointerVtable(), "Po", 0, AsTypeInfo(_ZTIo_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPf_nid_postfix {PointerVtable(), "Pf", 0, AsTypeInfo(_ZTIf_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPd_nid_postfix {PointerVtable(), "Pd", 0, AsTypeInfo(_ZTId_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPe_nid_postfix {PointerVtable(), "Pe", 0, AsTypeInfo(_ZTIe_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPg_nid_postfix {PointerVtable(), "Pg", 0, AsTypeInfo(_ZTIg_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPDn_nid_postfix {PointerVtable(), "PDn", 0, AsTypeInfo(_ZTIDn_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPDs_nid_postfix {PointerVtable(), "PDs", 0, AsTypeInfo(_ZTIDs_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPDi_nid_postfix {PointerVtable(), "PDi", 0, AsTypeInfo(_ZTIDi_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPDu_nid_postfix {PointerVtable(), "PDu", 0, AsTypeInfo(_ZTIDu_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPDh_nid_postfix {PointerVtable(), "PDh", 0, AsTypeInfo(_ZTIDh_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPKv_nid_postfix {PointerVtable(), "PKv", 1, AsTypeInfo(_ZTIv_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPKw_nid_postfix {PointerVtable(), "PKw", 1, AsTypeInfo(_ZTIw_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPKb_nid_postfix {PointerVtable(), "PKb", 1, AsTypeInfo(_ZTIb_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPKc_nid_postfix {PointerVtable(), "PKc", 1, AsTypeInfo(_ZTIc_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPKa_nid_postfix {PointerVtable(), "PKa", 1, AsTypeInfo(_ZTIa_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPKh_nid_postfix {PointerVtable(), "PKh", 1, AsTypeInfo(_ZTIh_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPKs_nid_postfix {PointerVtable(), "PKs", 1, AsTypeInfo(_ZTIs_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPKt_nid_postfix {PointerVtable(), "PKt", 1, AsTypeInfo(_ZTIt_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPKi_nid_postfix {PointerVtable(), "PKi", 1, AsTypeInfo(_ZTIi_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPKj_nid_postfix {PointerVtable(), "PKj", 1, AsTypeInfo(_ZTIj_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPKl_nid_postfix {PointerVtable(), "PKl", 1, AsTypeInfo(_ZTIl_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPKm_nid_postfix {PointerVtable(), "PKm", 1, AsTypeInfo(_ZTIm_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPKx_nid_postfix {PointerVtable(), "PKx", 1, AsTypeInfo(_ZTIx_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPKy_nid_postfix {PointerVtable(), "PKy", 1, AsTypeInfo(_ZTIy_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPKn_nid_postfix {PointerVtable(), "PKn", 1, AsTypeInfo(_ZTIn_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPKo_nid_postfix {PointerVtable(), "PKo", 1, AsTypeInfo(_ZTIo_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPKf_nid_postfix {PointerVtable(), "PKf", 1, AsTypeInfo(_ZTIf_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPKd_nid_postfix {PointerVtable(), "PKd", 1, AsTypeInfo(_ZTId_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPKe_nid_postfix {PointerVtable(), "PKe", 1, AsTypeInfo(_ZTIe_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPKg_nid_postfix {PointerVtable(), "PKg", 1, AsTypeInfo(_ZTIg_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPKDn_nid_postfix {PointerVtable(), "PKDn", 1, AsTypeInfo(_ZTIDn_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPKDs_nid_postfix {PointerVtable(), "PKDs", 1, AsTypeInfo(_ZTIDs_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPKDi_nid_postfix {PointerVtable(), "PKDi", 1, AsTypeInfo(_ZTIDi_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPKDu_nid_postfix {PointerVtable(), "PKDu", 1, AsTypeInfo(_ZTIDu_nid_postfix)};
LibcException::PointerTypeRecord _ZTIPKDh_nid_postfix {PointerVtable(), "PKDh", 1, AsTypeInfo(_ZTIDh_nid_postfix)};
}
