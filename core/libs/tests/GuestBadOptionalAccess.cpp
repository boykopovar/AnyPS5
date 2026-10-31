#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <typeinfo>

struct TypeRecord { const void* const* vtable; const char* name; const TypeRecord* base; };
struct FundamentalRecord { const void* const* vtable; const char* name; };
struct FundamentalVtable {
    std::ptrdiff_t offset;
    const std::type_info* type;
    const void* destroy;
    const void* deleteObject;
    const void* isPointer;
    const void* isFunction;
    bool (APS5_VABI *catchType)(const FundamentalRecord*, const FundamentalRecord*, void**, unsigned);
};

struct Vtable { std::ptrdiff_t offset; const TypeRecord* type; const void* destroy; const void* deleteObject; const char* (APS5_VABI *what)(const LibcException::ExceptionObject*); };

struct AbiRecord { const void* const* vtable; const char* name; const void* base; };
struct PointerRecord { const void* const* vtable; const char* name; unsigned flags; const void* pointee; };
struct AbiVtable {
    std::ptrdiff_t offset;
    const void* type;
    const void* destroy;
    const void* deleteObject;
    const void* isPointer;
    const void* isFunction;
    bool (APS5_VABI *catchType)(const void*, const void*, void**, unsigned);
};

extern "C" {
extern const TypeRecord _ZTISt9exception_nid_postfix;
extern const TypeRecord _ZTISt19bad_optional_access_nid_postfix;
extern const Vtable _ZTVSt19bad_optional_access_nid_postfix;
extern const FundamentalVtable _ZTVN10__cxxabiv123__fundamental_type_infoE_nid_postfix;
extern const FundamentalRecord _ZTIi_nid_postfix;
extern const FundamentalRecord _ZTIj_nid_postfix;
extern const FundamentalRecord _ZTIf_nid_postfix;
extern const FundamentalRecord _ZTIDn_nid_postfix;
extern const AbiVtable _ZTVN10__cxxabiv117__class_type_infoE_nid_postfix;
extern const AbiVtable _ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix;
extern const AbiVtable _ZTVN10__cxxabiv119__pointer_type_infoE_nid_postfix;
extern const AbiVtable _ZTVN10__cxxabiv117__pbase_type_infoE_nid_postfix;
extern const AbiVtable _ZTVN10__cxxabiv116__shim_type_infoE_nid_postfix;
extern const AbiRecord _ZTISt9type_info_nid_postfix;
extern const AbiRecord _ZTIN10__cxxabiv116__shim_type_infoE_nid_postfix;
extern const AbiRecord _ZTIN10__cxxabiv117__class_type_infoE_nid_postfix;
extern const AbiRecord _ZTIN10__cxxabiv120__si_class_type_infoE_nid_postfix;
extern const AbiRecord _ZTIN10__cxxabiv117__pbase_type_infoE_nid_postfix;
extern const AbiRecord _ZTIN10__cxxabiv119__pointer_type_infoE_nid_postfix;
extern const PointerRecord _ZTIPi_nid_postfix;
extern const PointerRecord _ZTIPKi_nid_postfix;
extern const PointerRecord _ZTIPf_nid_postfix;
extern const PointerRecord _ZTIPv_nid_postfix;
extern const PointerRecord _ZTIPKv_nid_postfix;
}

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    const auto& type = _ZTISt19bad_optional_access_nid_postfix;
    Require(std::strcmp(type.name, "St19bad_optional_access") == 0);
    Require(type.base == &_ZTISt9exception_nid_postfix);
    const auto& table = _ZTVSt19bad_optional_access_nid_postfix;
    Require(table.offset == 0 && table.type == &type && table.what != nullptr);
    LibcException::ExceptionObject object{};
    _ZNSt19bad_optional_accessC1Ev_nid_postfix(&object);

    Require(object.vtable == &table.destroy);
    Require(std::strcmp(table.what(&object), "bad_optional_access") == 0);
    _ZNSt19bad_optional_accessD1Ev_nid_postfix(&object);

    bool thrown = false;
    try {
        _ZSt26_Throw_bad_optional_accessv_nid_postfix();

    } catch (...) {
        thrown = true;
    }
    Require(thrown);
    const auto* const addressPoint = reinterpret_cast<const void* const*>(&_ZTVN10__cxxabiv123__fundamental_type_infoE_nid_postfix) + 2;
    Require(_ZTIi_nid_postfix.vtable == addressPoint && _ZTIf_nid_postfix.vtable == addressPoint &&
            _ZTIDn_nid_postfix.vtable == addressPoint);
    Require(std::strcmp(_ZTIi_nid_postfix.name, "i") == 0 && std::strcmp(_ZTIf_nid_postfix.name, "f") == 0 &&
            std::strcmp(_ZTIDn_nid_postfix.name, "Dn") == 0);
    const auto* kind = reinterpret_cast<const std::type_info*>(_ZTIi_nid_postfix.vtable[-1]);
    Require(std::strcmp(kind->name(), "N10__cxxabiv123__fundamental_type_infoE") == 0);
    void* thrownObject = nullptr;
    Require(_ZTVN10__cxxabiv123__fundamental_type_infoE_nid_postfix.catchType(&_ZTIi_nid_postfix, &_ZTIi_nid_postfix, &thrownObject, 0));
    Require(!_ZTVN10__cxxabiv123__fundamental_type_infoE_nid_postfix.catchType(&_ZTIi_nid_postfix, &_ZTIj_nid_postfix, &thrownObject, 0));
    Require(!_ZTVN10__cxxabiv123__fundamental_type_infoE_nid_postfix.catchType(&_ZTIf_nid_postfix, &_ZTIi_nid_postfix, &thrownObject, 0));
    Require(!_ZTVN10__cxxabiv123__fundamental_type_infoE_nid_postfix.catchType(&_ZTIDn_nid_postfix, &_ZTIi_nid_postfix, &thrownObject, 0));
    int* storage = static_cast<int*>(__cxa_allocate_exception_nid_postfix(sizeof(int)));
    Require(storage != nullptr);
    *storage = 42;
    bool caught = false;
    try {
        __cxa_throw_nid_postfix(storage, reinterpret_cast<std::type_info*>(const_cast<FundamentalRecord*>(&_ZTIi_nid_postfix)), nullptr);
    } catch (...) {
        caught = __cxa_current_exception_type_nid_postfix() == reinterpret_cast<const std::type_info*>(&_ZTIi_nid_postfix) && *storage == 42;
    }
    Require(caught);

    const auto* const classAddressPoint = reinterpret_cast<const void* const*>(&_ZTVN10__cxxabiv117__class_type_infoE_nid_postfix) + 2;
    const auto* const siAddressPoint = reinterpret_cast<const void* const*>(&_ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix) + 2;
    const auto* const pointerAddressPoint = reinterpret_cast<const void* const*>(&_ZTVN10__cxxabiv119__pointer_type_infoE_nid_postfix) + 2;
    Require(_ZTISt9type_info_nid_postfix.vtable == classAddressPoint && _ZTISt9type_info_nid_postfix.base == nullptr &&
            std::strcmp(_ZTISt9type_info_nid_postfix.name, "St9type_info") == 0);
    Require(_ZTIN10__cxxabiv116__shim_type_infoE_nid_postfix.vtable == siAddressPoint &&
            _ZTIN10__cxxabiv116__shim_type_infoE_nid_postfix.base == &_ZTISt9type_info_nid_postfix);
    Require(_ZTIN10__cxxabiv117__class_type_infoE_nid_postfix.base == &_ZTIN10__cxxabiv116__shim_type_infoE_nid_postfix);
    Require(_ZTIN10__cxxabiv120__si_class_type_infoE_nid_postfix.base == &_ZTIN10__cxxabiv117__class_type_infoE_nid_postfix);
    Require(_ZTIN10__cxxabiv119__pointer_type_infoE_nid_postfix.base ==
            reinterpret_cast<const void*>(&_ZTIN10__cxxabiv117__pbase_type_infoE_nid_postfix));
    Require(_ZTIPi_nid_postfix.vtable == pointerAddressPoint && _ZTIPKi_nid_postfix.vtable == pointerAddressPoint &&
            _ZTIPv_nid_postfix.vtable == pointerAddressPoint && _ZTIPKv_nid_postfix.vtable == pointerAddressPoint);
    Require(std::strcmp(_ZTIPi_nid_postfix.name, "Pi") == 0 && std::strcmp(_ZTIPKi_nid_postfix.name, "PKi") == 0);
    Require(_ZTIPi_nid_postfix.flags == 0 && _ZTIPKi_nid_postfix.flags == 1 && _ZTIPKv_nid_postfix.flags == 1);
    Require(_ZTIPi_nid_postfix.pointee == &_ZTIi_nid_postfix && _ZTIPKi_nid_postfix.pointee == &_ZTIi_nid_postfix);
    Require(_ZTIPi_nid_postfix.vtable[-1] == &_ZTIN10__cxxabiv119__pointer_type_infoE_nid_postfix);
    Require(_ZTVN10__cxxabiv119__pointer_type_infoE_nid_postfix.type == &_ZTIN10__cxxabiv119__pointer_type_infoE_nid_postfix);
    Require(_ZTVN10__cxxabiv117__pbase_type_infoE_nid_postfix.type ==
            reinterpret_cast<const void*>(&_ZTIN10__cxxabiv117__pbase_type_infoE_nid_postfix));

    void* pointerObject = nullptr;
    Require(_ZTVN10__cxxabiv119__pointer_type_infoE_nid_postfix.catchType(&_ZTIPKi_nid_postfix, &_ZTIPi_nid_postfix, &pointerObject, 0));
    Require(!_ZTVN10__cxxabiv119__pointer_type_infoE_nid_postfix.catchType(&_ZTIPi_nid_postfix, &_ZTIPKi_nid_postfix, &pointerObject, 0));
    Require(_ZTVN10__cxxabiv119__pointer_type_infoE_nid_postfix.catchType(&_ZTIPKv_nid_postfix, &_ZTIPi_nid_postfix, &pointerObject, 0));
    Require(_ZTVN10__cxxabiv119__pointer_type_infoE_nid_postfix.catchType(&_ZTIPv_nid_postfix, &_ZTIPi_nid_postfix, &pointerObject, 0));
    Require(!_ZTVN10__cxxabiv119__pointer_type_infoE_nid_postfix.catchType(&_ZTIPi_nid_postfix, &_ZTIPf_nid_postfix, &pointerObject, 0));
    Require(_ZTVN10__cxxabiv119__pointer_type_infoE_nid_postfix.catchType(&_ZTIPi_nid_postfix, &_ZTIDn_nid_postfix, &pointerObject, 0));
    return 0;
}
