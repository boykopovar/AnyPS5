#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdlib>
#include <cstring>

struct TypeRecord { const void* const* vtable; const char* name; const TypeRecord* base; };
struct Vtable { std::ptrdiff_t offset; const TypeRecord* type; const void* destroy; const void* deleteObject; const void* what; };

extern "C" {
extern const TypeRecord _ZTISt13runtime_error_nid_postfix;
extern const TypeRecord _ZTISt15underflow_error_nid_postfix;
extern const Vtable _ZTVSt13runtime_error_nid_postfix;
extern const Vtable _ZTVSt15underflow_error_nid_postfix;
}

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    const auto& type = _ZTISt15underflow_error_nid_postfix;
    const auto& runtimeType = _ZTISt13runtime_error_nid_postfix;
    Require(std::strcmp(type.name, "St15underflow_error") == 0);
    Require(type.base == &runtimeType && type.vtable == runtimeType.vtable);
    const auto& table = _ZTVSt15underflow_error_nid_postfix;
    const auto& runtimeTable = _ZTVSt13runtime_error_nid_postfix;
    Require(table.offset == 0 && table.type == &type);
    Require(table.destroy == runtimeTable.destroy && table.deleteObject == runtimeTable.deleteObject && table.what == runtimeTable.what);
    LibcException::ExceptionObject object{};
    _ZNSt13runtime_errorC1EPKc_nid_postfix(&object, "value too small");
    Require(object.message != nullptr && std::strcmp(object.message, "value too small") == 0);
    _ZNSt15underflow_errorD1Ev_nid_postfix(&object);
    Require(object.message == nullptr);
    _ZNSt15underflow_errorD1Ev_nid_postfix(&object);
    Require(object.message == nullptr);
    return 0;
}
