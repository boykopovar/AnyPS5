#include "prx/libSceJsc/include/WtfStringCreateExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

using Impl = NKWebKit::WtfAbi::StringImpl;

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "StringImpl::destroy check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

constexpr unsigned Flag8Bit = 1u << 2;

Impl* FromJsc(Jsc::WtfAbi::String string) {
    return reinterpret_cast<Impl*>(string.Release());
}

Impl* Make(unsigned ownership, const void* data, unsigned length, std::size_t bytes = sizeof(Impl)) {
    auto* impl = static_cast<Impl*>(_ZN3WTF10fastMallocEm(bytes));
    Require(impl != nullptr);
    impl->refCount = 2;
    impl->length = length;
    impl->data = data;
    impl->hashAndFlags = Flag8Bit | ownership;
    return impl;
}

template<typename Error, typename Function> bool Throws(Function function) {
    try {
        function();
    } catch (const Error&) {
        return true;
    }
    return false;
}

void FreesLibSceJscStrings() {
    const unsigned char text[] = "destroyed";
    Impl* copied = FromJsc(_ZN3WTF10StringImpl6createEPKh(text));
    Require(copied != nullptr && copied->refCount == 2 && copied->length == 9);
    _ZN3WTF10StringImpl7destroyEPS0_(copied);

    static const unsigned char borrowed[] = "borrowed";
    Impl* view = FromJsc(_ZN3WTF10StringImpl20createWithoutCopyingEPKhj(borrowed, 8));
    Require(view != nullptr && view->data == borrowed);
    _ZN3WTF10StringImpl7destroyEPS0_(view);
    Require(std::memcmp(borrowed, "borrowed", 8) == 0);
}

void FreesOwnedBuffers() {
    auto* characters = static_cast<unsigned char*>(_ZN3WTF10fastMallocEm(4));
    std::memcpy(characters, "own!", 4);
    _ZN3WTF10StringImpl7destroyEPS0_(Make(1, characters, 4));
}

void ReleasesSubstringOwners() {
    const unsigned char text[] = "substring owner";
    Impl* owner = FromJsc(_ZN3WTF10StringImpl6createEPKh(text));
    owner->refCount += 2;
    Impl* shared = Make(2, owner->data, 9, sizeof(Impl) + sizeof(Impl*));
    *reinterpret_cast<Impl**>(reinterpret_cast<unsigned char*>(shared) + sizeof(Impl)) = owner;
    _ZN3WTF10StringImpl7destroyEPS0_(shared);
    Require(owner->refCount == 2);

    Impl* last = Make(2, owner->data, 9, sizeof(Impl) + sizeof(Impl*));
    *reinterpret_cast<Impl**>(reinterpret_cast<unsigned char*>(last) + sizeof(Impl)) = owner;
    _ZN3WTF10StringImpl7destroyEPS0_(last);
}

void RejectsUnmodeledStrings() {
    static const unsigned char characters[] = "x";
    Require(Throws<std::invalid_argument>([] { _ZN3WTF10StringImpl7destroyEPS0_(nullptr); }));
    for (const unsigned flag : {1u << 4, 1u << 5}) {
        Impl* impl = Make(0, characters, 1);
        impl->hashAndFlags |= flag;
        Require(Throws<std::logic_error>([impl] { _ZN3WTF10StringImpl7destroyEPS0_(impl); }));
        _ZN3WTF8fastFreeEPv(impl);
    }
    Impl* external = Make(3, characters, 1);
    Require(Throws<std::logic_error>([external] { _ZN3WTF10StringImpl7destroyEPS0_(external); }));
    _ZN3WTF8fastFreeEPv(external);
}

}

int main() {
    FreesLibSceJscStrings();
    FreesOwnedBuffers();
    ReleasesSubstringOwners();
    RejectsUnmodeledStrings();
    std::puts("StringImpl::destroy tests passed");
    return 0;
}
