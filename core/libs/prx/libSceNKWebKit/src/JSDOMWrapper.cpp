#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/JSDOMWrapperExports.hpp"

#include <cerrno>
#include <stdexcept>
#include <string>

namespace {

using NKWebKit::WebCoreAbi::JSDocumentWrapperType;
using NKWebKit::WebCoreAbi::JSDOMWrapper;
using NKWebKit::WebCoreAbi::JSElementType;
using NKWebKit::WebCoreAbi::JSEventType;
using NKWebKit::WebCoreAbi::JSNodeType;
using NKWebKit::WebCoreAbi::JSNodeTypeMask;
using NKWebKit::WebCoreAbi::JSValueNotCellMask;

bool IsCell(std::uint64_t value) {
    return (value & JSValueNotCellMask) == 0;
}

JSDOMWrapper ReadWrapper(std::uint64_t value, const char* function) {
    const auto* wrapper = reinterpret_cast<const JSDOMWrapper*>(static_cast<std::uintptr_t>(value));
    if (!wrapper) throw std::invalid_argument(std::string(function) + ": null cell");
    JSDOMWrapper copy{};
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(&copy, wrapper, sizeof(JSDOMWrapper));
    errno = savedErrno;
    if (!ok) throw std::invalid_argument(std::string(function) + ": unreadable cell");
    return copy;
}

void* WrappedIfType(std::uint64_t value, std::uint8_t firstType, std::uint8_t lastType, const char* function) {
    if (!IsCell(value)) return nullptr;
    const JSDOMWrapper wrapper = ReadWrapper(value, function);
    if (wrapper.cell.type < firstType || wrapper.cell.type > lastType) return nullptr;
    return wrapper.wrapped;
}

}

extern "C" {

NKWebKit::WebCoreAbi::Document* APS5_VABI _ZN7WebCore10JSDocument9toWrappedERN3JSC2VMENS1_7JSValueE(void*, std::uint64_t value) {
    return static_cast<NKWebKit::WebCoreAbi::Document*>(WrappedIfType(value, JSDocumentWrapperType, JSDocumentWrapperType, __func__));
}

NKWebKit::WebCoreAbi::Element* APS5_VABI _ZN7WebCore9JSElement9toWrappedERN3JSC2VMENS1_7JSValueE(void*, std::uint64_t value) {
    return static_cast<NKWebKit::WebCoreAbi::Element*>(WrappedIfType(value, JSElementType, JSElementType, __func__));
}

NKWebKit::WebCoreAbi::Event* APS5_VABI _ZN7WebCore7JSEvent9toWrappedERN3JSC2VMENS1_7JSValueE(void*, std::uint64_t value) {
    return static_cast<NKWebKit::WebCoreAbi::Event*>(WrappedIfType(value, JSEventType, JSEventType, __func__));
}

NKWebKit::WebCoreAbi::Node* APS5_VABI _ZN7WebCore6JSNode9toWrappedERN3JSC2VMENS1_7JSValueE(void*, std::uint64_t value) {
    return static_cast<NKWebKit::WebCoreAbi::Node*>(WrappedIfType(value, JSNodeType, JSNodeType + JSNodeTypeMask, __func__));
}

}
