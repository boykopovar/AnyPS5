#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <utility>

using NKWebKit::WebCoreAbi::LegacyProtectionSpaceBase;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "LegacyProtectionSpaceBase check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

namespace {

using Construct5 = void (APS5_VABI *)(LegacyProtectionSpaceBase*, const String*, std::int32_t, std::uint32_t, const String*, std::uint32_t);
using Construct0 = void (APS5_VABI *)(LegacyProtectionSpaceBase*);

StringImpl Make(const char* text) {
    return StringImpl{4, static_cast<unsigned>(std::strlen(text)), text, 4};
}

struct alignas(8) Storage {
    std::array<std::uint8_t, 40> bytes;

    explicit Storage(std::uint8_t fill) { bytes.fill(fill); }

    LegacyProtectionSpaceBase* Get() { return reinterpret_cast<LegacyProtectionSpaceBase*>(bytes.data()); }

    template <typename T>
    T Read(std::size_t offset) const {
        T value;
        std::memcpy(&value, bytes.data() + offset, sizeof(value));
        return value;
    }
};

StringImpl* EmptyStringImpl() {
    String empty = _ZN3WTF10StringImpl6createEPKhj(nullptr, 0);
    return empty.impl;
}

void RequireTail(const Storage& storage, std::uint8_t fill) {
    for (std::size_t i = 29; i < storage.bytes.size(); ++i) Require(storage.bytes[i] == fill);
}

void TestDefault(Construct0 construct) {
    StringImpl* empty = EmptyStringImpl();
    Require(empty && empty->length == 0);
    const unsigned before = empty->refCount;
    Storage storage(0xa5);
    construct(storage.Get());
    Require(storage.Read<StringImpl*>(0) == empty);
    Require(storage.Read<std::int32_t>(8) == 0);
    Require(storage.Read<std::uint32_t>(12) == 1);
    Require(storage.Read<StringImpl*>(16) == empty);
    Require(storage.Read<std::uint32_t>(24) == 1);
    Require(storage.bytes[28] == 0);
    RequireTail(storage, 0xa5);
    Require(empty->refCount == before + 4);
    Require(_ZNK7WebCore19ProtectionSpaceBase4portEv(storage.Get()) == 0);
    Require(_ZNK7WebCore19ProtectionSpaceBase10serverTypeEv(storage.Get()) == 1);
    Require(_ZNK7WebCore19ProtectionSpaceBase20authenticationSchemeEv(storage.Get()) == 1);
    Require(_ZNK7WebCore19ProtectionSpaceBase4hostEv(storage.Get()).impl == empty);
    Require(_ZNK7WebCore19ProtectionSpaceBase5realmEv(storage.Get()).impl == empty);
}

void TestValues(Construct5 construct) {
    StringImpl hostImpl = Make("example.com");
    StringImpl realmImpl = Make("Secure Area");
    String host(&hostImpl);
    String realm(&realmImpl);
    Storage storage(0x5a);
    construct(storage.Get(), &host, 8443, 6, &realm, 3);
    Require(storage.Read<StringImpl*>(0) == &hostImpl);
    Require(storage.Read<std::int32_t>(8) == 8443);
    Require(storage.Read<std::uint32_t>(12) == 6);
    Require(storage.Read<StringImpl*>(16) == &realmImpl);
    Require(storage.Read<std::uint32_t>(24) == 3);
    Require(storage.bytes[28] == 0);
    RequireTail(storage, 0x5a);
    Require(hostImpl.refCount == 6);
    Require(realmImpl.refCount == 6);
    Require(host.impl == &hostImpl && realm.impl == &realmImpl);

    const LegacyProtectionSpaceBase* space = storage.Get();
    Require(&_ZNK7WebCore19ProtectionSpaceBase4hostEv(space) == reinterpret_cast<const String*>(storage.bytes.data()));
    Require(&_ZNK7WebCore19ProtectionSpaceBase5realmEv(space) == reinterpret_cast<const String*>(storage.bytes.data() + 16));
    Require(_ZNK7WebCore19ProtectionSpaceBase4portEv(space) == 8443);
    Require(_ZNK7WebCore19ProtectionSpaceBase10serverTypeEv(space) == 6);
    Require(_ZNK7WebCore19ProtectionSpaceBase20authenticationSchemeEv(space) == 3);

    Storage other(0x00);
    construct(other.Get(), &host, -1, 8, &realm, 100);
    Require(_ZNK7WebCore19ProtectionSpaceBase4portEv(other.Get()) == -1);
    Require(_ZNK7WebCore19ProtectionSpaceBase10serverTypeEv(other.Get()) == 8);
    Require(_ZNK7WebCore19ProtectionSpaceBase20authenticationSchemeEv(other.Get()) == 100);
    Require(other.bytes[28] == 0);
    Require(hostImpl.refCount == 8);
    host.impl = nullptr;
    realm.impl = nullptr;
}

void TestEmptyInputs(Construct5 construct) {
    StringImpl* empty = EmptyStringImpl();
    StringImpl emptyHostImpl = Make("");
    StringImpl realmImpl = Make("realm");
    String emptyHost(&emptyHostImpl);
    String nullRealm;
    Storage storage(0xa5);
    construct(storage.Get(), &emptyHost, 80, 1, &nullRealm, 2);
    Require(storage.Read<StringImpl*>(0) == empty);
    Require(storage.Read<StringImpl*>(16) == empty);
    Require(emptyHostImpl.refCount == 4);

    String nullHost;
    String realm(&realmImpl);
    Storage swapped(0xa5);
    construct(swapped.Get(), &nullHost, 80, 1, &realm, 2);
    Require(swapped.Read<StringImpl*>(0) == empty);
    Require(swapped.Read<StringImpl*>(16) == &realmImpl);
    emptyHost.impl = nullptr;
    realm.impl = nullptr;
}

template <typename Function>
bool Throws(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void TestNull(Construct5 construct, Construct0 constructDefault) {
    StringImpl hostImpl = Make("host");
    String host(&hostImpl);
    Storage storage(0xa5);
    const Storage before = storage;
    Require(Throws([&] { construct(storage.Get(), nullptr, 1, 1, &host, 1); }));
    Require(Throws([&] { construct(storage.Get(), &host, 1, 1, nullptr, 1); }));
    Require(storage.bytes == before.bytes);
    Require(hostImpl.refCount == 4);
    Require(Throws([&] { construct(nullptr, &host, 1, 1, &host, 1); }));
    Require(hostImpl.refCount == 4);
    Require(Throws([&] { constructDefault(nullptr); }));
    Require(Throws([] { _ZNK7WebCore19ProtectionSpaceBase4hostEv(nullptr); }));
    Require(Throws([] { _ZNK7WebCore19ProtectionSpaceBase4portEv(nullptr); }));
    Require(Throws([] { _ZNK7WebCore19ProtectionSpaceBase10serverTypeEv(nullptr); }));
    Require(Throws([] { _ZNK7WebCore19ProtectionSpaceBase5realmEv(nullptr); }));
    Require(Throws([] { _ZNK7WebCore19ProtectionSpaceBase20authenticationSchemeEv(nullptr); }));
    host.impl = nullptr;
}

}

int main() {
    TestDefault(_ZN7WebCore19ProtectionSpaceBaseC1Ev);
    TestDefault(_ZN7WebCore19ProtectionSpaceBaseC2Ev);
    TestValues(_ZN7WebCore19ProtectionSpaceBaseC1ERKN3WTF6StringEiNS_25ProtectionSpaceServerTypeES4_NS_35ProtectionSpaceAuthenticationSchemeE);
    TestValues(_ZN7WebCore19ProtectionSpaceBaseC2ERKN3WTF6StringEiNS_25ProtectionSpaceServerTypeES4_NS_35ProtectionSpaceAuthenticationSchemeE);
    TestEmptyInputs(_ZN7WebCore19ProtectionSpaceBaseC1ERKN3WTF6StringEiNS_25ProtectionSpaceServerTypeES4_NS_35ProtectionSpaceAuthenticationSchemeE);
    TestEmptyInputs(_ZN7WebCore19ProtectionSpaceBaseC2ERKN3WTF6StringEiNS_25ProtectionSpaceServerTypeES4_NS_35ProtectionSpaceAuthenticationSchemeE);
    TestNull(_ZN7WebCore19ProtectionSpaceBaseC1ERKN3WTF6StringEiNS_25ProtectionSpaceServerTypeES4_NS_35ProtectionSpaceAuthenticationSchemeE, _ZN7WebCore19ProtectionSpaceBaseC1Ev);
    TestNull(_ZN7WebCore19ProtectionSpaceBaseC2ERKN3WTF6StringEiNS_25ProtectionSpaceServerTypeES4_NS_35ProtectionSpaceAuthenticationSchemeE, _ZN7WebCore19ProtectionSpaceBaseC2Ev);
    return 0;
}
