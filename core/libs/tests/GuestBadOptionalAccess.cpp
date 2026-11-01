#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <typeinfo>

struct Vtable { std::ptrdiff_t offset; const std::type_info* type; const void* destroy; const void* deleteObject; const char* (APS5_VABI *what)(const LibcException::ExceptionObject*); };

extern "C" {
extern __cxxabiv1::__class_type_info _ZTISt9exception_nid_postfix;
extern __cxxabiv1::__si_class_type_info _ZTISt19bad_optional_access_nid_postfix;
extern Vtable _ZTVSt19bad_optional_access_nid_postfix;
extern std::type_info _ZTIi_nid_postfix;
extern std::type_info _ZTIj_nid_postfix;
extern std::type_info _ZTIf_nid_postfix;
extern std::type_info _ZTIDn_nid_postfix;
extern void* const _ZTVN10__cxxabiv123__fundamental_type_infoE_nid_postfix[];
}

static void Require(bool value) { if (!value) std::abort(); }

static void* const* Vptr(const std::type_info& record) {
    return *reinterpret_cast<void* const* const*>(&record);
}

static void TestFundamentalRecords() {
    void* const* const fundamental = &_ZTVN10__cxxabiv123__fundamental_type_infoE_nid_postfix[2];
    const std::type_info* records[] = {&_ZTIi_nid_postfix, &_ZTIj_nid_postfix, &_ZTIf_nid_postfix, &_ZTIDn_nid_postfix};
    for (const std::type_info* record : records)
        Require(Vptr(*record) == fundamental);
    Require(std::strcmp(_ZTIi_nid_postfix.name(), "i") == 0 && std::strcmp(_ZTIDn_nid_postfix.name(), "Dn") == 0);
    const auto* recordType = static_cast<const std::type_info*>(Vptr(_ZTIi_nid_postfix)[-1]);
    Require(std::strcmp(recordType->name(), "N10__cxxabiv123__fundamental_type_infoE") == 0);

    using CatchType = bool (APS5_VABI*)(const std::type_info*, const std::type_info*, void**, unsigned);
    const auto catchType = reinterpret_cast<CatchType>(Vptr(_ZTIi_nid_postfix)[4]);
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
    Require(std::strcmp(type.name(), "St19bad_optional_access") == 0);
    Require(type.__base_type == &_ZTISt9exception_nid_postfix);
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
    TestFundamentalRecords();
    return 0;
}
