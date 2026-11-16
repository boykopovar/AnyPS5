#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdlib>
#include <cstring>

struct TypeRecord { const void* const* vtable; const char* name; const TypeRecord* base; };
struct Vtable {
    std::ptrdiff_t offset;
    const TypeRecord* type;
    void (APS5_VABI *destroy)(LibcException::ExceptionObject*);
    void (APS5_VABI *deleteObject)(LibcException::ExceptionObject*);
    const char* (APS5_VABI *what)(const LibcException::ExceptionObject*);
};

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
    Require(std::strcmp(table.what(&object), "value too small") == 0);
    Require(std::strcmp(_ZNKSt13runtime_error4whatEv_nid_postfix(&object), "value too small") == 0);
    _ZNSt15underflow_errorD1Ev_nid_postfix(&object);
    Require(object.message == nullptr);
    Require(std::strcmp(table.what(&object), "") == 0);
    _ZNSt15underflow_errorD1Ev_nid_postfix(&object);
    Require(object.message == nullptr);

    LibcException::ExceptionObject emptyMessage{};
    _ZNSt13runtime_errorC1EPKc_nid_postfix(&emptyMessage, "");
    Require(emptyMessage.message != nullptr && std::strcmp(table.what(&emptyMessage), "") == 0);
    _ZNSt15underflow_errorD1Ev_nid_postfix(&emptyMessage);
    Require(emptyMessage.message == nullptr);

    LibcException::ExceptionObject nullMessage{};
    _ZNSt13runtime_errorC1EPKc_nid_postfix(&nullMessage, nullptr);
    Require(nullMessage.message != nullptr && std::strcmp(table.what(&nullMessage), "") == 0);
    table.destroy(&nullMessage);
    Require(nullMessage.message == nullptr);

    auto* allocated = static_cast<LibcException::ExceptionObject*>(std::malloc(sizeof(LibcException::ExceptionObject)));
    Require(allocated != nullptr);
    *allocated = {};
    _ZNSt13runtime_errorC1EPKc_nid_postfix(allocated, "heap underflow");
    Require(std::strcmp(table.what(allocated), "heap underflow") == 0);
    table.deleteObject(allocated);
    return 0;
}
