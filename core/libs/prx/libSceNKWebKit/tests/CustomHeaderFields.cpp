#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "Testing.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>

using NKWebKit::WebCoreAbi::CustomHeaderFields;
using NKWebKit::WebCoreAbi::HTTPHeaderField;
using NKWebKit::WtfAbi::String;

using Destructor = void(APS5_VABI*)(CustomHeaderFields*);

template <typename Action>
static bool ThrowsInvalidArgument(Action action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static String Latin1(const char* text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), static_cast<unsigned>(std::strlen(text)));
}

static bool Is(const String& string, const char* text) {
    return string.impl && string.impl->length == std::strlen(text) && std::memcmp(string.impl->data, text, string.impl->length) == 0;
}

static void Fill(CustomHeaderFields& fields) {
    HTTPHeaderField field{Latin1("X-Test"), Latin1("one")};
    fields.fields.append(field);
    fields.thirdPartyDomains.append(Latin1("*.example.com"));
    fields.thirdPartyDomains.append(Latin1("example.org"));
}

static void ConstructsEmpty() {
    for (auto construct : {&_ZN7WebCore18CustomHeaderFieldsC1Ev, &_ZN7WebCore18CustomHeaderFieldsC2Ev}) {
        alignas(CustomHeaderFields) unsigned char storage[sizeof(CustomHeaderFields)];
        std::memset(storage, 0xAB, sizeof(storage));
        auto* fields = reinterpret_cast<CustomHeaderFields*>(storage);
        construct(fields);
        Require(!fields->fields.buffer && fields->fields.size == 0 && fields->fields.capacity == 0);
        Require(!fields->thirdPartyDomains.buffer && fields->thirdPartyDomains.size == 0);
        _ZN7WebCore18CustomHeaderFieldsD2Ev(fields);
    }
}

static void CopiesIndependently() {
    CustomHeaderFields original;
    Fill(original);
    for (auto copy : {&_ZN7WebCore18CustomHeaderFieldsC1ERKS0_, &_ZN7WebCore18CustomHeaderFieldsC2ERKS0_}) {
        alignas(CustomHeaderFields) unsigned char storage[sizeof(CustomHeaderFields)];
        auto* copied = reinterpret_cast<CustomHeaderFields*>(storage);
        copy(copied, &original);
        Require(copied->fields.size == 1 && copied->fields.capacity == 1 && copied->fields.buffer != original.fields.buffer);
        Require(Is(copied->fields.buffer[0].name, "X-Test") && Is(copied->fields.buffer[0].value, "one"));
        Require(copied->fields.buffer[0].name.impl == original.fields.buffer[0].name.impl);
        Require(copied->thirdPartyDomains.size == 2 && Is(copied->thirdPartyDomains.buffer[1], "example.org"));
        const auto references = original.thirdPartyDomains.buffer[0].impl->refCount;
        reinterpret_cast<Destructor>(&_ZN7WebCore18CustomHeaderFieldsD1Ev)(copied);
        Require(original.thirdPartyDomains.buffer[0].impl->refCount == references - 2);
    }
    Require(Is(original.fields.buffer[0].value, "one") && original.thirdPartyDomains.size == 2);
}

static void AssignsAndRefuses() {
    CustomHeaderFields source;
    Fill(source);
    CustomHeaderFields target;
    target.thirdPartyDomains.append(Latin1("old.example"));
    auto& result = _ZN7WebCore18CustomHeaderFieldsaSERKS0_(&target, &source);
    Require(&result == &target && target.fields.size == 1 && target.thirdPartyDomains.size == 2);
    Require(Is(target.thirdPartyDomains.buffer[0], "*.example.com"));
    Require(&_ZN7WebCore18CustomHeaderFieldsaSERKS0_(&target, &target) == &target && target.thirdPartyDomains.size == 2);
    Require(ThrowsInvalidArgument([] { _ZN7WebCore18CustomHeaderFieldsC2Ev(nullptr); }));
    Require(ThrowsInvalidArgument([&] { _ZN7WebCore18CustomHeaderFieldsC2ERKS0_(&target, nullptr); }));
    Require(ThrowsInvalidArgument([&] { _ZN7WebCore18CustomHeaderFieldsaSERKS0_(nullptr, &source); }));
    Require(ThrowsInvalidArgument([] { _ZN7WebCore18CustomHeaderFieldsD2Ev(nullptr); }));
}

int main() {
    ConstructsEmpty();
    CopiesIndependently();
    AssignsAndRefuses();
    std::puts("CustomHeaderFields passed");
    return 0;
}
