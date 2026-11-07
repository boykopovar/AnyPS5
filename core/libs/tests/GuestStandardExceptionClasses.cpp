#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

struct TypeRecord { const void* const* vtable; const char* name; const TypeRecord* base; };
struct Object { const void* vtable; const char* message; };
using Destroy = void (APS5_VABI*)(Object*);
using What = const char* (APS5_VABI*)(const Object*);
struct Vtable { std::ptrdiff_t offset; const TypeRecord* type; const void* destroy; const void* deleteObject; What what; };

extern "C" {
extern const TypeRecord _ZTISt9exception_nid_postfix;
extern const Vtable _ZTVSt9exception_nid_postfix;
void APS5_VABI _ZNSt9exceptionD0Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt9exceptionD1Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt9exceptionD2Ev_nid_postfix(Object*);
const char* APS5_VABI _ZNKSt9exception4whatEv_nid_postfix(const Object*);
extern const TypeRecord _ZTISt9bad_alloc_nid_postfix;
extern const Vtable _ZTVSt9bad_alloc_nid_postfix;
void APS5_VABI _ZNSt9bad_allocD0Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt9bad_allocD1Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt9bad_allocD2Ev_nid_postfix(Object*);
const char* APS5_VABI _ZNKSt9bad_alloc4whatEv_nid_postfix(const Object*);
extern const TypeRecord _ZTISt20bad_array_new_length_nid_postfix;
extern const Vtable _ZTVSt20bad_array_new_length_nid_postfix;
void APS5_VABI _ZNSt20bad_array_new_lengthD0Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt20bad_array_new_lengthD1Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt20bad_array_new_lengthD2Ev_nid_postfix(Object*);
const char* APS5_VABI _ZNKSt20bad_array_new_length4whatEv_nid_postfix(const Object*);
extern const TypeRecord _ZTISt8bad_cast_nid_postfix;
extern const Vtable _ZTVSt8bad_cast_nid_postfix;
void APS5_VABI _ZNSt8bad_castD0Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt8bad_castD1Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt8bad_castD2Ev_nid_postfix(Object*);
const char* APS5_VABI _ZNKSt8bad_cast4whatEv_nid_postfix(const Object*);
extern const TypeRecord _ZTISt10bad_typeid_nid_postfix;
extern const Vtable _ZTVSt10bad_typeid_nid_postfix;
void APS5_VABI _ZNSt10bad_typeidD0Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt10bad_typeidD1Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt10bad_typeidD2Ev_nid_postfix(Object*);
const char* APS5_VABI _ZNKSt10bad_typeid4whatEv_nid_postfix(const Object*);
extern const TypeRecord _ZTISt17bad_function_call_nid_postfix;
extern const Vtable _ZTVSt17bad_function_call_nid_postfix;
void APS5_VABI _ZNSt17bad_function_callD0Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt17bad_function_callD1Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt17bad_function_callD2Ev_nid_postfix(Object*);
const char* APS5_VABI _ZNKSt17bad_function_call4whatEv_nid_postfix(const Object*);
extern const TypeRecord _ZTISt11logic_error_nid_postfix;
extern const Vtable _ZTVSt11logic_error_nid_postfix;
void APS5_VABI _ZNSt11logic_errorD0Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt11logic_errorD1Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt11logic_errorD2Ev_nid_postfix(Object*);
const char* APS5_VABI _ZNKSt11logic_error4whatEv_nid_postfix(const Object*);
extern const TypeRecord _ZTISt12domain_error_nid_postfix;
extern const Vtable _ZTVSt12domain_error_nid_postfix;
void APS5_VABI _ZNSt12domain_errorD0Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt12domain_errorD1Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt12domain_errorD2Ev_nid_postfix(Object*);
const char* APS5_VABI _ZNKSt12domain_error4whatEv_nid_postfix(const Object*);
extern const TypeRecord _ZTISt16invalid_argument_nid_postfix;
extern const Vtable _ZTVSt16invalid_argument_nid_postfix;
void APS5_VABI _ZNSt16invalid_argumentD0Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt16invalid_argumentD1Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt16invalid_argumentD2Ev_nid_postfix(Object*);
const char* APS5_VABI _ZNKSt16invalid_argument4whatEv_nid_postfix(const Object*);
extern const TypeRecord _ZTISt12length_error_nid_postfix;
extern const Vtable _ZTVSt12length_error_nid_postfix;
void APS5_VABI _ZNSt12length_errorD0Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt12length_errorD1Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt12length_errorD2Ev_nid_postfix(Object*);
const char* APS5_VABI _ZNKSt12length_error4whatEv_nid_postfix(const Object*);
extern const TypeRecord _ZTISt12out_of_range_nid_postfix;
extern const Vtable _ZTVSt12out_of_range_nid_postfix;
void APS5_VABI _ZNSt12out_of_rangeD0Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt12out_of_rangeD1Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt12out_of_rangeD2Ev_nid_postfix(Object*);
const char* APS5_VABI _ZNKSt12out_of_range4whatEv_nid_postfix(const Object*);
extern const TypeRecord _ZTISt13runtime_error_nid_postfix;
extern const Vtable _ZTVSt13runtime_error_nid_postfix;
void APS5_VABI _ZNSt13runtime_errorD0Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt13runtime_errorD1Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt13runtime_errorD2Ev_nid_postfix(Object*);
const char* APS5_VABI _ZNKSt13runtime_error4whatEv_nid_postfix(const Object*);
extern const TypeRecord _ZTISt12bad_weak_ptr_nid_postfix;
extern const Vtable _ZTVSt12bad_weak_ptr_nid_postfix;
void APS5_VABI _ZNSt12bad_weak_ptrD0Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt12bad_weak_ptrD1Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt12bad_weak_ptrD2Ev_nid_postfix(Object*);
const char* APS5_VABI _ZNKSt12bad_weak_ptr4whatEv_nid_postfix(const Object*);
void APS5_VABI _ZNSt9exceptionC1Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt9exceptionC2Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt9bad_allocC1Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt9bad_allocC2Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt20bad_array_new_lengthC1Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt20bad_array_new_lengthC2Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt8bad_castC1Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt8bad_castC2Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt10bad_typeidC1Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt10bad_typeidC2Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt17bad_function_callC1Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt17bad_function_callC2Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt11logic_errorC1EPKc_nid_postfix(Object*, const char*);
void APS5_VABI _ZNSt11logic_errorC2EPKc_nid_postfix(Object*, const char*);
void APS5_VABI _ZNSt11logic_errorC1ERKS__nid_postfix(Object*, const Object*);
void APS5_VABI _ZNSt11logic_errorC2ERKS__nid_postfix(Object*, const Object*);
Object* APS5_VABI _ZNSt11logic_erroraSERKS__nid_postfix(Object*, const Object*);
void APS5_VABI _ZNSt12domain_errorC1EPKc_nid_postfix(Object*, const char*);
void APS5_VABI _ZNSt12domain_errorC2EPKc_nid_postfix(Object*, const char*);
void APS5_VABI _ZNSt12domain_errorC1ERKS__nid_postfix(Object*, const Object*);
void APS5_VABI _ZNSt12domain_errorC2ERKS__nid_postfix(Object*, const Object*);
Object* APS5_VABI _ZNSt12domain_erroraSERKS__nid_postfix(Object*, const Object*);
void APS5_VABI _ZNSt16invalid_argumentC1EPKc_nid_postfix(Object*, const char*);
void APS5_VABI _ZNSt16invalid_argumentC2EPKc_nid_postfix(Object*, const char*);
void APS5_VABI _ZNSt16invalid_argumentC1ERKS__nid_postfix(Object*, const Object*);
void APS5_VABI _ZNSt16invalid_argumentC2ERKS__nid_postfix(Object*, const Object*);
Object* APS5_VABI _ZNSt16invalid_argumentaSERKS__nid_postfix(Object*, const Object*);
void APS5_VABI _ZNSt12length_errorC1EPKc_nid_postfix(Object*, const char*);
void APS5_VABI _ZNSt12length_errorC2EPKc_nid_postfix(Object*, const char*);
void APS5_VABI _ZNSt12length_errorC1ERKS__nid_postfix(Object*, const Object*);
void APS5_VABI _ZNSt12length_errorC2ERKS__nid_postfix(Object*, const Object*);
Object* APS5_VABI _ZNSt12length_erroraSERKS__nid_postfix(Object*, const Object*);
void APS5_VABI _ZNSt12out_of_rangeC1EPKc_nid_postfix(Object*, const char*);
void APS5_VABI _ZNSt12out_of_rangeC2EPKc_nid_postfix(Object*, const char*);
void APS5_VABI _ZNSt12out_of_rangeC1ERKS__nid_postfix(Object*, const Object*);
void APS5_VABI _ZNSt12out_of_rangeC2ERKS__nid_postfix(Object*, const Object*);
Object* APS5_VABI _ZNSt12out_of_rangeaSERKS__nid_postfix(Object*, const Object*);
void APS5_VABI _ZNSt13runtime_errorC1EPKc_nid_postfix(Object*, const char*);
void APS5_VABI _ZNSt13runtime_errorC2EPKc_nid_postfix(Object*, const char*);
void APS5_VABI _ZNSt13runtime_errorC1ERKS__nid_postfix(Object*, const Object*);
void APS5_VABI _ZNSt13runtime_errorC2ERKS__nid_postfix(Object*, const Object*);
Object* APS5_VABI _ZNSt13runtime_erroraSERKS__nid_postfix(Object*, const Object*);
[[noreturn]] void APS5_VABI _ZSt11_Xbad_allocv_nid_postfix();
[[noreturn]] void APS5_VABI _ZSt13_Xrange_errorPKc_nid_postfix(const char*);
[[noreturn]] void APS5_VABI _ZSt14_Xlength_errorPKc_nid_postfix(const char*);
[[noreturn]] void APS5_VABI _ZSt18_Xinvalid_argumentPKc_nid_postfix(const char*);
[[noreturn]] void APS5_VABI _ZSt19_Throw_bad_weak_ptrv_nid_postfix();
[[noreturn]] void APS5_VABI _ZSt19_Xbad_function_callv_nid_postfix();
[[noreturn]] void APS5_VABI _ZSt26__throw_bad_variant_accessv_nid_postfix();
[[noreturn]] void APS5_VABI __cxa_bad_cast_nid_postfix();
[[noreturn]] void APS5_VABI __cxa_bad_typeid_nid_postfix();
[[noreturn]] void APS5_VABI __cxa_throw_bad_array_new_length_nid_postfix();
}

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::abort();
}

static void CheckType(const TypeRecord& type, const Vtable& table, const char* name, const TypeRecord* base) {
    Require(std::strcmp(type.name, name) == 0 && type.base == base);
    Require(table.offset == 0 && table.type == &type && table.what != nullptr);
}

static void CheckPlain(const TypeRecord& type, const Vtable& table, const char* name, const TypeRecord* base,
    void (APS5_VABI* c1)(Object*), void (APS5_VABI* c2)(Object*), Destroy d0, Destroy d1, Destroy d2, What what) {
    CheckType(type, table, name, base);
    Object object{};
    c1(&object);
    Require(object.vtable == &table.destroy);
    const char* text = what(&object);
    Require(text != nullptr && text[0] != 0 && std::strcmp(table.what(&object), text) == 0);
    d1(&object);
    Object base_object{};
    c2(&base_object);
    Require(base_object.vtable == &table.destroy);
    d2(&base_object);
    auto* heap = static_cast<Object*>(std::malloc(sizeof(Object)));
    Require(heap != nullptr);
    c1(heap);
    d0(heap);
}

static void CheckMessage(const TypeRecord& type, const Vtable& table, const char* name, const TypeRecord* base,
    void (APS5_VABI* c1)(Object*, const char*), void (APS5_VABI* c2)(Object*, const char*),
    void (APS5_VABI* copy1)(Object*, const Object*), void (APS5_VABI* copy2)(Object*, const Object*),
    Object* (APS5_VABI* assign)(Object*, const Object*), Destroy d0, Destroy d1, Destroy d2, What what) {
    CheckType(type, table, name, base);
    char text[] = "first message";
    Object first{};
    c1(&first, text);
    text[0] = 'X';
    Require(first.vtable == &table.destroy && std::strcmp(what(&first), "first message") == 0);
    Require(std::strcmp(table.what(&first), "first message") == 0);
    Object copy{};
    copy1(&copy, &first);
    Require(copy.vtable == &table.destroy && std::strcmp(what(&copy), "first message") == 0);
    Object second{};
    c2(&second, "second message");
    Require(assign(&copy, &second) == &copy && std::strcmp(what(&copy), "second message") == 0);
    Require(assign(&copy, &copy) == &copy && std::strcmp(what(&copy), "second message") == 0);
    Require(std::strcmp(what(&first), "first message") == 0);
    Object base_copy{};
    copy2(&base_copy, &second);
    Require(std::strcmp(what(&base_copy), "second message") == 0);
    d2(&base_copy);
    d1(&copy);
    d1(&second);
    d2(&first);
    auto* heap = static_cast<Object*>(std::malloc(sizeof(Object)));
    Require(heap != nullptr);
    c1(heap, "heap message");
    Require(std::strcmp(what(heap), "heap message") == 0);
    d0(heap);
}

template <typename TCall>
static bool Throws(TCall call) {
    try {
        call();
    } catch (...) {
        return true;
    }
    return false;
}

int main() {
    CheckPlain(_ZTISt9exception_nid_postfix, _ZTVSt9exception_nid_postfix, "St9exception", nullptr, _ZNSt9exceptionC1Ev_nid_postfix, _ZNSt9exceptionC2Ev_nid_postfix,
        _ZNSt9exceptionD0Ev_nid_postfix, _ZNSt9exceptionD1Ev_nid_postfix, _ZNSt9exceptionD2Ev_nid_postfix, _ZNKSt9exception4whatEv_nid_postfix);
    CheckPlain(_ZTISt9bad_alloc_nid_postfix, _ZTVSt9bad_alloc_nid_postfix, "St9bad_alloc", &_ZTISt9exception_nid_postfix, _ZNSt9bad_allocC1Ev_nid_postfix, _ZNSt9bad_allocC2Ev_nid_postfix,
        _ZNSt9bad_allocD0Ev_nid_postfix, _ZNSt9bad_allocD1Ev_nid_postfix, _ZNSt9bad_allocD2Ev_nid_postfix, _ZNKSt9bad_alloc4whatEv_nid_postfix);
    CheckPlain(_ZTISt20bad_array_new_length_nid_postfix, _ZTVSt20bad_array_new_length_nid_postfix, "St20bad_array_new_length", &_ZTISt9bad_alloc_nid_postfix, _ZNSt20bad_array_new_lengthC1Ev_nid_postfix, _ZNSt20bad_array_new_lengthC2Ev_nid_postfix,
        _ZNSt20bad_array_new_lengthD0Ev_nid_postfix, _ZNSt20bad_array_new_lengthD1Ev_nid_postfix, _ZNSt20bad_array_new_lengthD2Ev_nid_postfix, _ZNKSt20bad_array_new_length4whatEv_nid_postfix);
    CheckPlain(_ZTISt8bad_cast_nid_postfix, _ZTVSt8bad_cast_nid_postfix, "St8bad_cast", &_ZTISt9exception_nid_postfix, _ZNSt8bad_castC1Ev_nid_postfix, _ZNSt8bad_castC2Ev_nid_postfix,
        _ZNSt8bad_castD0Ev_nid_postfix, _ZNSt8bad_castD1Ev_nid_postfix, _ZNSt8bad_castD2Ev_nid_postfix, _ZNKSt8bad_cast4whatEv_nid_postfix);
    CheckPlain(_ZTISt10bad_typeid_nid_postfix, _ZTVSt10bad_typeid_nid_postfix, "St10bad_typeid", &_ZTISt9exception_nid_postfix, _ZNSt10bad_typeidC1Ev_nid_postfix, _ZNSt10bad_typeidC2Ev_nid_postfix,
        _ZNSt10bad_typeidD0Ev_nid_postfix, _ZNSt10bad_typeidD1Ev_nid_postfix, _ZNSt10bad_typeidD2Ev_nid_postfix, _ZNKSt10bad_typeid4whatEv_nid_postfix);
    CheckPlain(_ZTISt17bad_function_call_nid_postfix, _ZTVSt17bad_function_call_nid_postfix, "St17bad_function_call", &_ZTISt9exception_nid_postfix, _ZNSt17bad_function_callC1Ev_nid_postfix, _ZNSt17bad_function_callC2Ev_nid_postfix,
        _ZNSt17bad_function_callD0Ev_nid_postfix, _ZNSt17bad_function_callD1Ev_nid_postfix, _ZNSt17bad_function_callD2Ev_nid_postfix, _ZNKSt17bad_function_call4whatEv_nid_postfix);
    CheckMessage(_ZTISt11logic_error_nid_postfix, _ZTVSt11logic_error_nid_postfix, "St11logic_error", &_ZTISt9exception_nid_postfix, _ZNSt11logic_errorC1EPKc_nid_postfix, _ZNSt11logic_errorC2EPKc_nid_postfix,
        _ZNSt11logic_errorC1ERKS__nid_postfix, _ZNSt11logic_errorC2ERKS__nid_postfix, _ZNSt11logic_erroraSERKS__nid_postfix,
        _ZNSt11logic_errorD0Ev_nid_postfix, _ZNSt11logic_errorD1Ev_nid_postfix, _ZNSt11logic_errorD2Ev_nid_postfix, _ZNKSt11logic_error4whatEv_nid_postfix);
    CheckMessage(_ZTISt12domain_error_nid_postfix, _ZTVSt12domain_error_nid_postfix, "St12domain_error", &_ZTISt11logic_error_nid_postfix, _ZNSt12domain_errorC1EPKc_nid_postfix, _ZNSt12domain_errorC2EPKc_nid_postfix,
        _ZNSt12domain_errorC1ERKS__nid_postfix, _ZNSt12domain_errorC2ERKS__nid_postfix, _ZNSt12domain_erroraSERKS__nid_postfix,
        _ZNSt12domain_errorD0Ev_nid_postfix, _ZNSt12domain_errorD1Ev_nid_postfix, _ZNSt12domain_errorD2Ev_nid_postfix, _ZNKSt12domain_error4whatEv_nid_postfix);
    CheckMessage(_ZTISt16invalid_argument_nid_postfix, _ZTVSt16invalid_argument_nid_postfix, "St16invalid_argument", &_ZTISt11logic_error_nid_postfix, _ZNSt16invalid_argumentC1EPKc_nid_postfix, _ZNSt16invalid_argumentC2EPKc_nid_postfix,
        _ZNSt16invalid_argumentC1ERKS__nid_postfix, _ZNSt16invalid_argumentC2ERKS__nid_postfix, _ZNSt16invalid_argumentaSERKS__nid_postfix,
        _ZNSt16invalid_argumentD0Ev_nid_postfix, _ZNSt16invalid_argumentD1Ev_nid_postfix, _ZNSt16invalid_argumentD2Ev_nid_postfix, _ZNKSt16invalid_argument4whatEv_nid_postfix);
    CheckMessage(_ZTISt12length_error_nid_postfix, _ZTVSt12length_error_nid_postfix, "St12length_error", &_ZTISt11logic_error_nid_postfix, _ZNSt12length_errorC1EPKc_nid_postfix, _ZNSt12length_errorC2EPKc_nid_postfix,
        _ZNSt12length_errorC1ERKS__nid_postfix, _ZNSt12length_errorC2ERKS__nid_postfix, _ZNSt12length_erroraSERKS__nid_postfix,
        _ZNSt12length_errorD0Ev_nid_postfix, _ZNSt12length_errorD1Ev_nid_postfix, _ZNSt12length_errorD2Ev_nid_postfix, _ZNKSt12length_error4whatEv_nid_postfix);
    CheckMessage(_ZTISt12out_of_range_nid_postfix, _ZTVSt12out_of_range_nid_postfix, "St12out_of_range", &_ZTISt11logic_error_nid_postfix, _ZNSt12out_of_rangeC1EPKc_nid_postfix, _ZNSt12out_of_rangeC2EPKc_nid_postfix,
        _ZNSt12out_of_rangeC1ERKS__nid_postfix, _ZNSt12out_of_rangeC2ERKS__nid_postfix, _ZNSt12out_of_rangeaSERKS__nid_postfix,
        _ZNSt12out_of_rangeD0Ev_nid_postfix, _ZNSt12out_of_rangeD1Ev_nid_postfix, _ZNSt12out_of_rangeD2Ev_nid_postfix, _ZNKSt12out_of_range4whatEv_nid_postfix);
    CheckMessage(_ZTISt13runtime_error_nid_postfix, _ZTVSt13runtime_error_nid_postfix, "St13runtime_error", &_ZTISt9exception_nid_postfix, _ZNSt13runtime_errorC1EPKc_nid_postfix, _ZNSt13runtime_errorC2EPKc_nid_postfix,
        _ZNSt13runtime_errorC1ERKS__nid_postfix, _ZNSt13runtime_errorC2ERKS__nid_postfix, _ZNSt13runtime_erroraSERKS__nid_postfix,
        _ZNSt13runtime_errorD0Ev_nid_postfix, _ZNSt13runtime_errorD1Ev_nid_postfix, _ZNSt13runtime_errorD2Ev_nid_postfix, _ZNKSt13runtime_error4whatEv_nid_postfix);
    CheckType(_ZTISt12bad_weak_ptr_nid_postfix, _ZTVSt12bad_weak_ptr_nid_postfix, "St12bad_weak_ptr", &_ZTISt9exception_nid_postfix);
    Object weak{};
    weak.vtable = &_ZTVSt12bad_weak_ptr_nid_postfix.destroy;
    Require(_ZNKSt12bad_weak_ptr4whatEv_nid_postfix(&weak) != nullptr && _ZNKSt12bad_weak_ptr4whatEv_nid_postfix(&weak)[0] != 0);
    _ZNSt12bad_weak_ptrD1Ev_nid_postfix(&weak);
    _ZNSt12bad_weak_ptrD2Ev_nid_postfix(&weak);
    auto* heapWeak = static_cast<Object*>(std::malloc(sizeof(Object)));
    Require(heapWeak != nullptr);
    heapWeak->vtable = &_ZTVSt12bad_weak_ptr_nid_postfix.destroy;
    _ZNSt12bad_weak_ptrD0Ev_nid_postfix(heapWeak);
    Require(Throws([] { _ZSt11_Xbad_allocv_nid_postfix(); }));
    Require(Throws([] { _ZSt13_Xrange_errorPKc_nid_postfix("thrown"); }));
    Require(Throws([] { _ZSt14_Xlength_errorPKc_nid_postfix("thrown"); }));
    Require(Throws([] { _ZSt18_Xinvalid_argumentPKc_nid_postfix("thrown"); }));
    Require(Throws([] { _ZSt19_Throw_bad_weak_ptrv_nid_postfix(); }));
    Require(Throws([] { _ZSt19_Xbad_function_callv_nid_postfix(); }));
    Require(Throws([] { _ZSt26__throw_bad_variant_accessv_nid_postfix(); }));
    Require(Throws([] { __cxa_bad_cast_nid_postfix(); }));
    Require(Throws([] { __cxa_bad_typeid_nid_postfix(); }));
    Require(Throws([] { __cxa_throw_bad_array_new_length_nid_postfix(); }));
    return 0;
}
