#include "prx/libSceNKWebKit/include/PropertyDescriptorExports.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

using NKWebKit::JscAbi::PropertyDescriptor;

static_assert(sizeof(PropertyDescriptor) == 0x20);
static_assert(offsetof(PropertyDescriptor, value) == 0x0);
static_assert(offsetof(PropertyDescriptor, getter) == 0x8);
static_assert(offsetof(PropertyDescriptor, setter) == 0x10);
static_assert(offsetof(PropertyDescriptor, attributes) == 0x18);
static_assert(offsetof(PropertyDescriptor, seenAttributes) == 0x1c);

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "PropertyDescriptor check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

template <typename Call>
static bool Throws(Call call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

int main() {
    PropertyDescriptor descriptor{0x1234, 0x5678, 0x9abc, 0x40, 0x8};
    _ZN3JSC18PropertyDescriptor12setUndefinedEv(&descriptor);
    Require(descriptor.value == 0xa);
    Require(descriptor.attributes == 0xe);
    Require(descriptor.getter == 0x5678 && descriptor.setter == 0x9abc && descriptor.seenAttributes == 0x8);

    descriptor = {0, 0, 0, 0xff, 0};
    _ZN3JSC18PropertyDescriptor11setWritableEb(&descriptor, true);
    Require(descriptor.attributes == 0xfd && descriptor.seenAttributes == 0x1);
    Require(_ZNK3JSC18PropertyDescriptor8writableEv(&descriptor));
    _ZN3JSC18PropertyDescriptor11setWritableEb(&descriptor, false);
    Require(descriptor.attributes == 0xff && descriptor.seenAttributes == 0x1);
    Require(!_ZNK3JSC18PropertyDescriptor8writableEv(&descriptor));

    descriptor = {0, 0, 0, 0xff, 0x10};
    _ZN3JSC18PropertyDescriptor13setEnumerableEb(&descriptor, true);
    Require(descriptor.attributes == 0xfb && descriptor.seenAttributes == 0x12);
    Require(_ZNK3JSC18PropertyDescriptor10enumerableEv(&descriptor));
    _ZN3JSC18PropertyDescriptor13setEnumerableEb(&descriptor, false);
    Require(descriptor.attributes == 0xff && descriptor.seenAttributes == 0x12);
    Require(!_ZNK3JSC18PropertyDescriptor10enumerableEv(&descriptor));

    descriptor = {0, 0, 0, 0, 0};
    _ZN3JSC18PropertyDescriptor15setConfigurableEb(&descriptor, false);
    Require(descriptor.attributes == 0x8 && descriptor.seenAttributes == 0x4);
    _ZN3JSC18PropertyDescriptor15setConfigurableEb(&descriptor, true);
    Require(descriptor.attributes == 0 && descriptor.seenAttributes == 0x4);
    Require(descriptor.value == 0 && descriptor.getter == 0 && descriptor.setter == 0);

    descriptor = {0x77, 0, 0x33, 0x2 | 0x4, 0x7};
    _ZN3JSC18PropertyDescriptor9setGetterENS_7JSValueE(&descriptor, 0x1122334455667788ull);
    Require(descriptor.getter == 0x1122334455667788ull);
    Require(descriptor.attributes == (0x10 | 0x4) && descriptor.seenAttributes == 0x7);
    Require(descriptor.value == 0x77 && descriptor.setter == 0x33);

    descriptor = {0, 0x44, 0, 0x2, 0};
    _ZN3JSC18PropertyDescriptor9setSetterENS_7JSValueE(&descriptor, 0x8877665544332211ull);
    Require(descriptor.setter == 0x8877665544332211ull);
    Require(descriptor.attributes == 0x10 && descriptor.getter == 0x44 && descriptor.seenAttributes == 0);

    descriptor = {0, 0, 0, 0, 0};
    Require(!_ZNK3JSC18PropertyDescriptor16isDataDescriptorEv(&descriptor));
    Require(!_ZNK3JSC18PropertyDescriptor20isAccessorDescriptorEv(&descriptor));
    descriptor = {0, 0, 0, 0, 0x6};
    Require(!_ZNK3JSC18PropertyDescriptor16isDataDescriptorEv(&descriptor));
    descriptor = {0, 0, 0, 0, 0x1};
    Require(_ZNK3JSC18PropertyDescriptor16isDataDescriptorEv(&descriptor));
    descriptor = {0xa, 0, 0, 0, 0};
    Require(_ZNK3JSC18PropertyDescriptor16isDataDescriptorEv(&descriptor));
    descriptor = {0, 0x5, 0, 0, 0};
    Require(_ZNK3JSC18PropertyDescriptor20isAccessorDescriptorEv(&descriptor));
    Require(!_ZNK3JSC18PropertyDescriptor16isDataDescriptorEv(&descriptor));
    descriptor = {0, 0, 0x5, 0, 0};
    Require(_ZNK3JSC18PropertyDescriptor20isAccessorDescriptorEv(&descriptor));

    descriptor = {0, 0, 0, 0x2 | 0x4, 0};
    Require(!_ZNK3JSC18PropertyDescriptor8writableEv(&descriptor));
    Require(!_ZNK3JSC18PropertyDescriptor10enumerableEv(&descriptor));
    descriptor = {0, 0, 0, ~0x6u, 0};
    Require(_ZNK3JSC18PropertyDescriptor8writableEv(&descriptor));
    Require(_ZNK3JSC18PropertyDescriptor10enumerableEv(&descriptor));

    Require(Throws([] { _ZNK3JSC18PropertyDescriptor8writableEv(nullptr); }));
    Require(Throws([] { _ZNK3JSC18PropertyDescriptor16isDataDescriptorEv(nullptr); }));
    Require(Throws([] { _ZN3JSC18PropertyDescriptor12setUndefinedEv(nullptr); }));
    Require(Throws([] { _ZN3JSC18PropertyDescriptor9setGetterENS_7JSValueE(nullptr, 1); }));
    return 0;
}
