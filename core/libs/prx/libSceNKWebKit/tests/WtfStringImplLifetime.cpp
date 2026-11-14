#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringBuffer;
using NKWebKit::WtfAbi::StringImpl;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WtfStringImplLifetime check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename Exception, typename Action>
static bool Throws(Action action) {
    try {
        action();
    } catch (const Exception&) {
        return true;
    }
    return false;
}

static String Latin1(const char* text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), static_cast<unsigned>(std::strlen(text)));
}

static StringImpl* Detach(String& string) {
    auto* impl = string.impl;
    string.impl = nullptr;
    return impl;
}

static void AdoptsLatin1Buffers() {
    auto* data = static_cast<unsigned char*>(_ZN3WTF10fastMallocEm(3));
    std::memcpy(data, "abc", 3);
    StringBuffer<unsigned char> buffer{3, data};
    auto adopted = _ZN3WTF10StringImpl5adoptEONS_12StringBufferIhEE(buffer);
    Require(adopted.impl && adopted.impl->refCount == 2 && adopted.impl->length == 3);
    Require(adopted.impl->data == data && adopted.impl->hashAndFlags == 5);
    Require(!buffer.data && buffer.length == 3);
    Require(std::memcmp(adopted.impl->data, "abc", 3) == 0);
}

static void AdoptsUtf16Buffers() {
    auto* data = static_cast<char16_t*>(_ZN3WTF10fastMallocEm(4));
    data[0] = u'é';
    data[1] = u'x';
    StringBuffer<char16_t> buffer{2, data};
    auto adopted = _ZN3WTF10StringImpl5adoptEONS_12StringBufferIDsEE(buffer);
    Require(adopted.impl && adopted.impl->length == 2 && adopted.impl->data == data);
    Require(adopted.impl->hashAndFlags == 1 && !buffer.data && buffer.length == 2);
}

static void EmptyBuffersBecomeTheEmptyString() {
    auto empty = _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(""), 0);
    StringBuffer<unsigned char> none{0, nullptr};
    auto fromNone = _ZN3WTF10StringImpl5adoptEONS_12StringBufferIhEE(none);
    Require(fromNone.impl == empty.impl);
    auto* kept = static_cast<char16_t*>(_ZN3WTF10fastMallocEm(2));
    StringBuffer<char16_t> shrunk{0, kept};
    auto fromShrunk = _ZN3WTF10StringImpl5adoptEONS_12StringBufferIDsEE(shrunk);
    Require(fromShrunk.impl == empty.impl && shrunk.data == kept);
    _ZN3WTF8fastFreeEPv(kept);
    StringBuffer<unsigned char> broken{2, nullptr};
    Require(Throws<std::invalid_argument>([&] { _ZN3WTF10StringImpl5adoptEONS_12StringBufferIhEE(broken); }));
}

static void DestroysEveryOwnershipItCreates() {
    auto internal = Latin1("internal storage");
    _ZN3WTF10StringImpl7destroyEPS0_(Detach(internal));
    auto* data = static_cast<unsigned char*>(_ZN3WTF10fastMallocEm(5));
    std::memcpy(data, "owned", 5);
    StringBuffer<unsigned char> buffer{5, data};
    auto owned = _ZN3WTF10StringImpl5adoptEONS_12StringBufferIhEE(buffer);
    _ZN3WTF10StringImpl7destroyEPS0_(Detach(owned));
    auto parent = Latin1("abcdefghijklmnopqrstuvwx");
    auto shared = _ZNK3WTF6String20substringSharingImplEjj(&parent, 1, 20);
    Require((shared.impl->hashAndFlags & 3) == 2 && parent.impl->refCount == 4);
    _ZN3WTF10StringImpl7destroyEPS0_(Detach(shared));
    Require(parent.impl->refCount == 2);
}

static void RefusesStringsItCannotDestroy() {
    Require(Throws<std::invalid_argument>([] { _ZN3WTF10StringImpl7destroyEPS0_(nullptr); }));
    auto empty = _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(""), 0);
    Require(Throws<std::invalid_argument>([&] { _ZN3WTF10StringImpl7destroyEPS0_(empty.impl); }));
    for (const unsigned flags : {4U | 16U, 4U | 32U, 4U | 3U}) {
        auto plain = Latin1("not an ordinary string");
        auto* impl = plain.impl;
        const auto original = impl->hashAndFlags;
        impl->hashAndFlags = flags;
        Require(Throws<std::runtime_error>([&] { _ZN3WTF10StringImpl7destroyEPS0_(impl); }));
        impl->hashAndFlags = original;
    }
}

int main() {
    AdoptsLatin1Buffers();
    AdoptsUtf16Buffers();
    EmptyBuffersBecomeTheEmptyString();
    DestroysEveryOwnershipItCreates();
    RefusesStringsItCannotDestroy();
    std::puts("WtfStringImplLifetime passed");
    return 0;
}
