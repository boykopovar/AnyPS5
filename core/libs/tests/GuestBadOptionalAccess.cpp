#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <typeinfo>

struct TypeRecord { const void* const* vtable; const char* name; const TypeRecord* base; };
struct FundamentalRecord { const void* const* vtable; const char* name; };
struct Object { const void* vtable; const char* message; };
struct Vtable { std::ptrdiff_t offset; const TypeRecord* type; const void* destroy; const void* deleteObject; const char* (APS5_VABI *what)(const Object*); };

extern "C" {
extern const TypeRecord _ZTISt9exception_nid_postfix;
extern const TypeRecord _ZTISt19bad_optional_access_nid_postfix;
extern const Vtable _ZTVSt19bad_optional_access_nid_postfix;
extern const FundamentalRecord _ZTIi_nid_postfix;
extern const FundamentalRecord _ZTIj_nid_postfix;
extern const FundamentalRecord _ZTIf_nid_postfix;
extern const FundamentalRecord _ZTIDn_nid_postfix;
extern const void* const _ZTVN10__cxxabiv123__fundamental_type_infoE_nid_postfix[];
void APS5_VABI _ZNSt19bad_optional_accessC1Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt19bad_optional_accessD1Ev_nid_postfix(Object*);
[[noreturn]] void APS5_VABI _ZSt26_Throw_bad_optional_accessv_nid_postfix();
void* APS5_VABI __cxa_allocate_exception_nid_postfix(std::size_t);
[[noreturn]] void APS5_VABI __cxa_throw_nid_postfix(void*, const void*, void (*)(void*));
const void* APS5_VABI __cxa_current_exception_type_nid_postfix();
}

static void Require(bool value) { if (!value) std::abort(); }

static void TestFundamentalRecords() {
    const auto* const address = reinterpret_cast<const void* const*>(_ZTVN10__cxxabiv123__fundamental_type_infoE_nid_postfix) + 2;
    const FundamentalRecord* records[] = {&_ZTIi_nid_postfix, &_ZTIj_nid_postfix, &_ZTIf_nid_postfix, &_ZTIDn_nid_postfix};
    for (const FundamentalRecord* record : records)
        Require(record->vtable == address);
    Require(std::strcmp(_ZTIi_nid_postfix.name, "i") == 0 && std::strcmp(_ZTIDn_nid_postfix.name, "Dn") == 0);
    const auto* recordType = static_cast<const std::type_info*>(_ZTIi_nid_postfix.vtable[-1]);
    Require(std::strcmp(recordType->name(), "N10__cxxabiv123__fundamental_type_infoE") == 0);

    using CatchType = bool (APS5_VABI*)(const void*, const void*, void**, unsigned);
    const auto catchType = reinterpret_cast<CatchType>(_ZTIi_nid_postfix.vtable[4]);
    int thrownValue = 0;
    void* thrownObject = &thrownValue;
    Require(catchType(&_ZTIi_nid_postfix, &_ZTIi_nid_postfix, &thrownObject, 0));
    Require(!catchType(&_ZTIi_nid_postfix, &_ZTIj_nid_postfix, &thrownObject, 0));
    Require(!catchType(&_ZTIf_nid_postfix, &_ZTIi_nid_postfix, &thrownObject, 0));
    Require(!catchType(&_ZTIDn_nid_postfix, &_ZTIi_nid_postfix, &thrownObject, 0));

    void* exception = __cxa_allocate_exception_nid_postfix(sizeof(int));
    *static_cast<int*>(exception) = 42;
    bool caught = false;
    try {
        __cxa_throw_nid_postfix(exception, &_ZTIi_nid_postfix, nullptr);
    } catch (...) {
        caught = __cxa_current_exception_type_nid_postfix() == &_ZTIi_nid_postfix;
    }
    Require(caught);
}

int main() {
    const auto& type = _ZTISt19bad_optional_access_nid_postfix;
    Require(std::strcmp(type.name, "St19bad_optional_access") == 0);
    Require(type.base == &_ZTISt9exception_nid_postfix);
    const auto& table = _ZTVSt19bad_optional_access_nid_postfix;
    Require(table.offset == 0 && table.type == &type && table.what != nullptr);
    Object object{};
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
    TestFundamentalRecords();
    return 0;
}
