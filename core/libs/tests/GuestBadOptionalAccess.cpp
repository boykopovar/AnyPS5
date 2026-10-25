#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdlib>
#include <cstring>

struct TypeRecord { const void* const* vtable; const char* name; const TypeRecord* base; };

struct Vtable { std::ptrdiff_t offset; const TypeRecord* type; const void* destroy; const void* deleteObject; const char* (APS5_VABI *what)(const LibcException::ExceptionObject*); };

extern "C" {
extern const TypeRecord _ZTISt9exception_nid_postfix;
extern const TypeRecord _ZTISt19bad_optional_access_nid_postfix;
extern const Vtable _ZTVSt19bad_optional_access_nid_postfix;

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
    return 0;
}
