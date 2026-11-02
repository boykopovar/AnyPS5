#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <typeinfo>

struct Vtable { std::ptrdiff_t offset; const std::type_info* type; const void* destroy; const void* deleteObject; const char* (APS5_VABI *what)(const LibcException::ExceptionObject*); };

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

struct SiTypeRecord { const void* const* vtable; const char* name; const std::type_info* base; };
struct PointerTypeRecord { const void* const* vtable; const char* name; unsigned flags; const std::type_info* pointee; };

extern "C" {
extern __cxxabiv1::__class_type_info _ZTISt9exception_nid_postfix;
extern __cxxabiv1::__si_class_type_info _ZTISt19bad_optional_access_nid_postfix;
extern Vtable _ZTVSt19bad_optional_access_nid_postfix;
extern std::type_info _ZTIi_nid_postfix;
extern std::type_info _ZTIj_nid_postfix;
extern std::type_info _ZTIf_nid_postfix;
extern std::type_info _ZTIDn_nid_postfix;
extern void* const _ZTVN10__cxxabiv123__fundamental_type_infoE_nid_postfix[];
extern SiTypeRecord _ZTISt9type_info_nid_postfix;
extern SiTypeRecord _ZTIN10__cxxabiv116__shim_type_infoE_nid_postfix;
extern SiTypeRecord _ZTIN10__cxxabiv117__class_type_infoE_nid_postfix;
extern SiTypeRecord _ZTIN10__cxxabiv117__pbase_type_infoE_nid_postfix;
extern SiTypeRecord _ZTIN10__cxxabiv119__pointer_type_infoE_nid_postfix;
extern SiTypeRecord _ZTIN10__cxxabiv120__si_class_type_infoE_nid_postfix;
extern PointerTypeRecord _ZTIPi_nid_postfix;
extern PointerTypeRecord _ZTIPKi_nid_postfix;
extern PointerTypeRecord _ZTIPf_nid_postfix;
extern PointerTypeRecord _ZTIPv_nid_postfix;
extern PointerTypeRecord _ZTIPKv_nid_postfix;
extern void* const _ZTVN10__cxxabiv117__class_type_infoE_nid_postfix[];
extern void* const _ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix[];
extern void* const _ZTVN10__cxxabiv119__pointer_type_infoE_nid_postfix[];
extern void* const _ZTVN10__cxxabiv117__pbase_type_infoE_nid_postfix[];
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

static void TestAbiAndPointerRecords() {
    const void* const* classAddressPoint = _ZTVN10__cxxabiv117__class_type_infoE_nid_postfix + 2;
    const void* const* siAddressPoint = _ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix + 2;
    const void* const* pointerAddressPoint = _ZTVN10__cxxabiv119__pointer_type_infoE_nid_postfix + 2;
    Require(_ZTISt9type_info_nid_postfix.vtable == classAddressPoint && _ZTISt9type_info_nid_postfix.base == nullptr &&
            std::strcmp(_ZTISt9type_info_nid_postfix.name, "St9type_info") == 0);
    Require(_ZTIN10__cxxabiv116__shim_type_infoE_nid_postfix.vtable == siAddressPoint &&
            _ZTIN10__cxxabiv116__shim_type_infoE_nid_postfix.base ==
                    reinterpret_cast<const std::type_info*>(&_ZTISt9type_info_nid_postfix));
    Require(_ZTIN10__cxxabiv117__class_type_infoE_nid_postfix.base ==
            reinterpret_cast<const std::type_info*>(&_ZTIN10__cxxabiv116__shim_type_infoE_nid_postfix));
    Require(_ZTIN10__cxxabiv120__si_class_type_infoE_nid_postfix.base ==
            reinterpret_cast<const std::type_info*>(&_ZTIN10__cxxabiv117__class_type_infoE_nid_postfix));
    Require(_ZTIN10__cxxabiv119__pointer_type_infoE_nid_postfix.base ==
            reinterpret_cast<const std::type_info*>(&_ZTIN10__cxxabiv117__pbase_type_infoE_nid_postfix));
    Require(_ZTIPi_nid_postfix.vtable == pointerAddressPoint && _ZTIPKi_nid_postfix.vtable == pointerAddressPoint &&
            _ZTIPv_nid_postfix.vtable == pointerAddressPoint && _ZTIPKv_nid_postfix.vtable == pointerAddressPoint);
    Require(std::strcmp(_ZTIPi_nid_postfix.name, "Pi") == 0 && std::strcmp(_ZTIPKi_nid_postfix.name, "PKi") == 0);
    Require(_ZTIPi_nid_postfix.flags == 0 && _ZTIPKi_nid_postfix.flags == 1 && _ZTIPKv_nid_postfix.flags == 1);
    Require(_ZTIPi_nid_postfix.pointee == &_ZTIi_nid_postfix && _ZTIPKi_nid_postfix.pointee == &_ZTIi_nid_postfix);
    Require(_ZTIPi_nid_postfix.vtable[-1] == reinterpret_cast<const void*>(&_ZTIN10__cxxabiv119__pointer_type_infoE_nid_postfix));
    Require(_ZTVN10__cxxabiv119__pointer_type_infoE_nid_postfix[1] ==
            reinterpret_cast<const void*>(&_ZTIN10__cxxabiv119__pointer_type_infoE_nid_postfix));
    Require(_ZTVN10__cxxabiv117__pbase_type_infoE_nid_postfix[1] ==
            reinterpret_cast<const void*>(&_ZTIN10__cxxabiv117__pbase_type_infoE_nid_postfix));

    using CatchType = bool (APS5_VABI*)(const std::type_info*, const std::type_info*, void**, unsigned);
    const auto catchType = reinterpret_cast<CatchType>(_ZTIPi_nid_postfix.vtable[4]);
    const auto* pi = reinterpret_cast<const std::type_info*>(&_ZTIPi_nid_postfix);
    const auto* pki = reinterpret_cast<const std::type_info*>(&_ZTIPKi_nid_postfix);
    const auto* pf = reinterpret_cast<const std::type_info*>(&_ZTIPf_nid_postfix);
    const auto* pv = reinterpret_cast<const std::type_info*>(&_ZTIPv_nid_postfix);
    const auto* pkv = reinterpret_cast<const std::type_info*>(&_ZTIPKv_nid_postfix);
    void* pointerObject = nullptr;
    Require(catchType(pki, pi, &pointerObject, 0));
    Require(!catchType(pi, pki, &pointerObject, 0));
    Require(catchType(pkv, pi, &pointerObject, 0));
    Require(catchType(pv, pi, &pointerObject, 0));
    Require(!catchType(pi, pf, &pointerObject, 0));
    Require(catchType(pi, &_ZTIDn_nid_postfix, &pointerObject, 0));
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
    TestAbiAndPointerRecords();
    return 0;
}
