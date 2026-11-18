#pragma once

#include "prx/libSceNKWebKit/src/PropertyDescriptorAbi.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>

extern "C" {

bool APS5_VABI _ZNK3JSC18PropertyDescriptor8writableEv(const NKWebKit::JscAbi::PropertyDescriptor* self);
bool APS5_VABI _ZNK3JSC18PropertyDescriptor10enumerableEv(const NKWebKit::JscAbi::PropertyDescriptor* self);
bool APS5_VABI _ZNK3JSC18PropertyDescriptor16isDataDescriptorEv(const NKWebKit::JscAbi::PropertyDescriptor* self);
bool APS5_VABI _ZNK3JSC18PropertyDescriptor20isAccessorDescriptorEv(const NKWebKit::JscAbi::PropertyDescriptor* self);
void APS5_VABI _ZN3JSC18PropertyDescriptor11setWritableEb(NKWebKit::JscAbi::PropertyDescriptor* self, bool writable);
void APS5_VABI _ZN3JSC18PropertyDescriptor13setEnumerableEb(NKWebKit::JscAbi::PropertyDescriptor* self, bool enumerable);
void APS5_VABI _ZN3JSC18PropertyDescriptor15setConfigurableEb(NKWebKit::JscAbi::PropertyDescriptor* self, bool configurable);
void APS5_VABI _ZN3JSC18PropertyDescriptor12setUndefinedEv(NKWebKit::JscAbi::PropertyDescriptor* self);
void APS5_VABI _ZN3JSC18PropertyDescriptor9setGetterENS_7JSValueE(NKWebKit::JscAbi::PropertyDescriptor* self, std::uint64_t getter);
void APS5_VABI _ZN3JSC18PropertyDescriptor9setSetterENS_7JSValueE(NKWebKit::JscAbi::PropertyDescriptor* self, std::uint64_t setter);

}
