#include "prx/libSceNKWebKit/include/PropertyDescriptorExports.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>

using NKWebKit::JscAbi::PropertyDescriptor;

namespace {

template <typename Descriptor>
Descriptor* RequireDescriptor(Descriptor* self, const char* name) {
    if (self == nullptr) throw std::invalid_argument(std::string(name) + ": null object");
    return self;
}

void SetAttribute(PropertyDescriptor* self, std::uint32_t attribute, bool clear, std::uint32_t present) {
    if (clear)
        self->attributes &= ~attribute;
    else
        self->attributes |= attribute;
    self->seenAttributes |= present;
}

void SetAccessor(PropertyDescriptor* self) {
    self->attributes |= NKWebKit::JscAbi::Accessor;
    self->attributes &= ~static_cast<std::uint32_t>(NKWebKit::JscAbi::ReadOnly);
}

}

extern "C" {

bool APS5_VABI _ZNK3JSC18PropertyDescriptor8writableEv(const PropertyDescriptor* self) {
    return !(RequireDescriptor(self, __func__)->attributes & NKWebKit::JscAbi::ReadOnly);
}

bool APS5_VABI _ZNK3JSC18PropertyDescriptor10enumerableEv(const PropertyDescriptor* self) {
    return !(RequireDescriptor(self, __func__)->attributes & NKWebKit::JscAbi::DontEnum);
}

bool APS5_VABI _ZNK3JSC18PropertyDescriptor16isDataDescriptorEv(const PropertyDescriptor* self) {
    RequireDescriptor(self, __func__);
    return self->value != 0 || (self->seenAttributes & NKWebKit::JscAbi::WritablePresent);
}

bool APS5_VABI _ZNK3JSC18PropertyDescriptor20isAccessorDescriptorEv(const PropertyDescriptor* self) {
    RequireDescriptor(self, __func__);
    return self->getter != 0 || self->setter != 0;
}

void APS5_VABI _ZN3JSC18PropertyDescriptor11setWritableEb(PropertyDescriptor* self, bool writable) {
    SetAttribute(RequireDescriptor(self, __func__), NKWebKit::JscAbi::ReadOnly, writable, NKWebKit::JscAbi::WritablePresent);
}

void APS5_VABI _ZN3JSC18PropertyDescriptor13setEnumerableEb(PropertyDescriptor* self, bool enumerable) {
    SetAttribute(RequireDescriptor(self, __func__), NKWebKit::JscAbi::DontEnum, enumerable, NKWebKit::JscAbi::EnumerablePresent);
}

void APS5_VABI _ZN3JSC18PropertyDescriptor15setConfigurableEb(PropertyDescriptor* self, bool configurable) {
    SetAttribute(RequireDescriptor(self, __func__), NKWebKit::JscAbi::DontDelete, configurable, NKWebKit::JscAbi::ConfigurablePresent);
}

void APS5_VABI _ZN3JSC18PropertyDescriptor12setUndefinedEv(PropertyDescriptor* self) {
    RequireDescriptor(self, __func__);
    self->value = NKWebKit::JscAbi::EncodedJSUndefined;
    self->attributes = NKWebKit::JscAbi::ReadOnly | NKWebKit::JscAbi::DontDelete | NKWebKit::JscAbi::DontEnum;
}

void APS5_VABI _ZN3JSC18PropertyDescriptor9setGetterENS_7JSValueE(PropertyDescriptor* self, std::uint64_t getter) {
    RequireDescriptor(self, __func__)->getter = getter;
    SetAccessor(self);
}

void APS5_VABI _ZN3JSC18PropertyDescriptor9setSetterENS_7JSValueE(PropertyDescriptor* self, std::uint64_t setter) {
    RequireDescriptor(self, __func__)->setter = setter;
    SetAccessor(self);
}

}
