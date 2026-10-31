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

extern "C" {
extern const TypeRecord _ZTISt9exception_nid_postfix;
extern const TypeRecord _ZTISt19bad_optional_access_nid_postfix;
extern const Vtable _ZTVSt19bad_optional_access_nid_postfix;
extern const FundamentalVtable _ZTVN10__cxxabiv123__fundamental_type_infoE_nid_postfix;
extern const FundamentalRecord _ZTIi_nid_postfix;
extern const FundamentalRecord _ZTIj_nid_postfix;
extern const FundamentalRecord _ZTIf_nid_postfix;
extern const FundamentalRecord _ZTIDn_nid_postfix;
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
    return 0;
}
