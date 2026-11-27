#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/JSDOMWrapperAbi.hpp"

#include <cstdint>

extern "C" {

NKWebKit::WebCoreAbi::Document* APS5_VABI _ZN7WebCore10JSDocument9toWrappedERN3JSC2VMENS1_7JSValueE(void* vm, std::uint64_t value);
NKWebKit::WebCoreAbi::Element* APS5_VABI _ZN7WebCore9JSElement9toWrappedERN3JSC2VMENS1_7JSValueE(void* vm, std::uint64_t value);
NKWebKit::WebCoreAbi::Event* APS5_VABI _ZN7WebCore7JSEvent9toWrappedERN3JSC2VMENS1_7JSValueE(void* vm, std::uint64_t value);
NKWebKit::WebCoreAbi::Node* APS5_VABI _ZN7WebCore6JSNode9toWrappedERN3JSC2VMENS1_7JSValueE(void* vm, std::uint64_t value);

}
