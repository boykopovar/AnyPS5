#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define ROOT_TYPES(X) X(9exception) X(8ios_base)
#define DERIVED_TYPES(X) \
    X(8bad_cast, 9exception) X(10bad_typeid, 9exception) X(9bad_alloc, 9exception) X(20bad_array_new_length, 9bad_alloc) \
    X(17bad_function_call, 9exception) X(12bad_weak_ptr, 9exception) X(18bad_variant_access, 9exception) \
    X(19bad_optional_access, 9exception) X(11logic_error, 9exception) X(13runtime_error, 9exception) \
    X(16invalid_argument, 11logic_error) X(12out_of_range, 11logic_error) X(12domain_error, 11logic_error) \
    X(12length_error, 11logic_error) X(12system_error, 13runtime_error) X(11regex_error, 13runtime_error) \
    X(11range_error, 13runtime_error) X(14overflow_error, 13runtime_error) X(15underflow_error, 13runtime_error)
#define VTABLE_TYPES(X) \
    X(9exception) X(8bad_cast) X(10bad_typeid) X(9bad_alloc) X(20bad_array_new_length) X(17bad_function_call) \
    X(12bad_weak_ptr) X(18bad_variant_access) X(19bad_optional_access) X(11logic_error) X(13runtime_error) \
    X(16invalid_argument) X(12out_of_range) X(12domain_error) X(12length_error) X(12system_error) \
    X(11regex_error) X(11range_error) X(14overflow_error) X(15underflow_error)

#define DECLARE_ROOT(type) extern LibcException::TypeRecord _ZTISt##type##_nid_postfix;
#define DECLARE_DERIVED(type, base) extern LibcException::TypeRecord _ZTISt##type##_nid_postfix;
#define DECLARE_VTABLE(type) extern LibcException::ExceptionVtable _ZTVSt##type##_nid_postfix;

extern "C" {
ROOT_TYPES(DECLARE_ROOT)
DERIVED_TYPES(DECLARE_DERIVED)
VTABLE_TYPES(DECLARE_VTABLE)
}

namespace {

void Require(bool condition, const char* name, int line) {
    if (condition) return;
    std::fprintf(stderr, "standard exception type record check failed for %s at line %d\n", name, line);
    std::exit(1);
}
#define REQUIRE(value) Require((value), name, __LINE__)

struct Derived { const char* name; const LibcException::TypeRecord* type; const LibcException::TypeRecord* base; };
struct Root { const char* name; const LibcException::TypeRecord* type; };
struct Vtable { const char* name; const LibcException::ExceptionVtable* table; const LibcException::TypeRecord* type; };

#define ROOT_ENTRY(type) {"St" #type, &_ZTISt##type##_nid_postfix},
#define DERIVED_ENTRY(type, base) {"St" #type, &_ZTISt##type##_nid_postfix, &_ZTISt##base##_nid_postfix},
#define VTABLE_ENTRY(type) {"St" #type, &_ZTVSt##type##_nid_postfix, &_ZTISt##type##_nid_postfix},

const Root roots[] = {ROOT_TYPES(ROOT_ENTRY)};
const Derived derived[] = {DERIVED_TYPES(DERIVED_ENTRY)};
const Vtable vtables[] = {VTABLE_TYPES(VTABLE_ENTRY)};

}

int main() {
    const void* const* rootVtable = roots[0].type->vtable;
    const void* const* derivedVtable = derived[0].type->vtable;
    for (const auto& entry : roots) {
        const char* name = entry.name;
        REQUIRE(std::strcmp(entry.type->name, name) == 0 && entry.type->base == nullptr && entry.type->vtable == rootVtable);
    }
    for (const auto& entry : derived) {
        const char* name = entry.name;
        REQUIRE(std::strcmp(entry.type->name, name) == 0);
        REQUIRE(entry.type->base == entry.base && entry.type->vtable == derivedVtable);
    }
    for (const auto& entry : vtables) {
        const char* name = entry.name;
        REQUIRE(entry.table->offset == 0 && entry.table->type == entry.type);
        REQUIRE(entry.table->destroy != nullptr && entry.table->deleteObject != nullptr && entry.table->what != nullptr);
    }
    const char* name = "type_info vtables";
    REQUIRE(rootVtable != nullptr && derivedVtable != nullptr && rootVtable != derivedVtable);
    std::puts("standard exception type records and vtables passed");
    return 0;
}
