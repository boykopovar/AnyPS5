#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfAtomStringExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/include/WtfSymbolExports.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>

using NKWebKit::WtfAbi::RegisteredSymbolImpl;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::SymbolImpl;
using NKWebKit::WtfAbi::SymbolRegistry;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WtfSymbolRegistry check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename Exception, typename Function>
static bool Throws(Function function) {
    try {
        function();
    } catch (const Exception&) {
        return true;
    }
    return false;
}

static const unsigned char* Bytes(const char* text) {
    return reinterpret_cast<const unsigned char*>(text);
}

static std::string Text(const StringImpl* string) {
    std::string result;
    for (unsigned i = 0; i < string->length; ++i)
        result += static_cast<char>(string->hashAndFlags & 4 ? static_cast<const unsigned char*>(string->data)[i] : static_cast<const char16_t*>(string->data)[i]);
    return result;
}

static const SymbolImpl& Symbol(const String& string) {
    return *reinterpret_cast<const SymbolImpl*>(string.impl);
}

static void TestCreate() {
    String base = _ZN3WTF10StringImpl6createEPKhj(Bytes("description-of-a-symbol"), 23);
    {
        String symbol = _ZN3WTF10SymbolImpl6createERNS_10StringImplE(base.impl);
        const auto& value = Symbol(symbol);
        Require(symbol.impl != base.impl && symbol.impl->refCount == 2);
        Require((symbol.impl->hashAndFlags & 0xFF) == (4 | 32 | 2) && !(symbol.impl->hashAndFlags >> 8));
        Require(symbol.impl->data == base.impl->data && Text(symbol.impl) == "description-of-a-symbol");
        Require(value.owner == base.impl && base.impl->refCount == 4 && value.flags == 0);
        Require(value.hashForSymbolShiftedWithFlagCount & 0x80000000u);
        String other = _ZN3WTF10SymbolImpl6createERNS_10StringImplE(base.impl);
        Require(Symbol(other).hashForSymbolShiftedWithFlagCount != value.hashForSymbolShiftedWithFlagCount);
    }
    Require(base.impl->refCount == 2);

    String substring = NKWebKit::WtfAbi::SubstringSharingImpl(base, 2, 21);
    Require((substring.impl->hashAndFlags & 3) == 2);
    {
        String symbol = _ZN3WTF17PrivateSymbolImpl6createERNS_10StringImplE(substring.impl);
        Require(Symbol(symbol).owner == base.impl && Text(symbol.impl) == "scription-of-a-symbol");
        Require(Symbol(symbol).flags == NKWebKit::WtfAbi::SymbolFlagIsPrivate);
        String atom = _ZN3WTF14AtomStringImpl11addSlowCaseERNS_10StringImplE(symbol.impl);
        Require(atom.impl != symbol.impl && (atom.impl->hashAndFlags & 16) && Text(atom.impl) == "scription-of-a-symbol");
    }

    String wide = _ZN3WTF10StringImpl6createEPKDsj(u"中文", 2);
    String wideSymbol = _ZN3WTF10SymbolImpl6createERNS_10StringImplE(wide.impl);
    Require(!(wideSymbol.impl->hashAndFlags & 4) && wideSymbol.impl->length == 2);

    String null = _ZN3WTF10SymbolImpl16createNullSymbolEv();
    Require(!null.impl->length && (null.impl->hashAndFlags & 0xFF) == (4 | 32 | 2));
    Require(Symbol(null).flags == NKWebKit::WtfAbi::SymbolFlagIsNullSymbol && Symbol(null).owner->length == 0);

    Require(Throws<std::invalid_argument>([] { _ZN3WTF10SymbolImpl6createERNS_10StringImplE(nullptr); }));
    Require(Throws<std::invalid_argument>([] { _ZN3WTF17PrivateSymbolImpl6createERNS_10StringImplE(nullptr); }));
}

static void TestRegistry() {
    SymbolRegistry registry{nullptr, 0};
    SymbolRegistry privateRegistry{nullptr, 1};
    String key = _ZN3WTF10StringImpl6createEPKhj(Bytes("Symbol.key"), 10);
    String sameKey = _ZN3WTF10StringImpl6createEPKDsj(u"Symbol.key", 10);
    StringImpl* registered;
    {
        String first = _ZN3WTF14SymbolRegistry12symbolForKeyERKNS_6StringE(&registry, &key);
        String second = _ZN3WTF14SymbolRegistry12symbolForKeyERKNS_6StringE(&registry, &sameKey);
        Require(first.impl == second.impl && first.impl->refCount == 6);
        const auto& value = *reinterpret_cast<const RegisteredSymbolImpl*>(first.impl);
        Require(value.symbol.flags == NKWebKit::WtfAbi::SymbolFlagIsRegistered && value.symbolRegistry == &registry);
        Require(value.symbol.owner == key.impl && Text(first.impl) == "Symbol.key");
        String privateSymbol = _ZN3WTF14SymbolRegistry12symbolForKeyERKNS_6StringE(&privateRegistry, &key);
        Require(privateSymbol.impl != first.impl);
        Require(reinterpret_cast<const RegisteredSymbolImpl*>(privateSymbol.impl)->symbol.flags == (NKWebKit::WtfAbi::SymbolFlagIsRegistered | NKWebKit::WtfAbi::SymbolFlagIsPrivate));
        registered = first.impl;
    }
    Require(registered->refCount == 2 && key.impl->refCount == 6);
    String held = _ZN3WTF14SymbolRegistry12symbolForKeyERKNS_6StringE(&registry, &key);
    Require(held.impl == registered);
    _ZN3WTF14SymbolRegistryD1Ev(&registry);
    Require(held.impl->refCount == 2 && !reinterpret_cast<const RegisteredSymbolImpl*>(held.impl)->symbolRegistry);
    _ZN3WTF14SymbolRegistryD2Ev(&privateRegistry);
    Require(key.impl->refCount == 4);
    held = String();
    Require(key.impl->refCount == 2);

    SymbolRegistry fresh{nullptr, 0};
    String again = _ZN3WTF14SymbolRegistry12symbolForKeyERKNS_6StringE(&fresh, &key);
    Require(again.impl && again.impl->refCount == 4 && reinterpret_cast<const RegisteredSymbolImpl*>(again.impl)->symbolRegistry == &fresh);
    _ZN3WTF14SymbolRegistryD1Ev(&fresh);
    _ZN3WTF14SymbolRegistryD1Ev(&fresh);

    const String nullKey;
    Require(Throws<std::invalid_argument>([&] { _ZN3WTF14SymbolRegistry12symbolForKeyERKNS_6StringE(&fresh, &nullKey); }));
    Require(Throws<std::invalid_argument>([&] { _ZN3WTF14SymbolRegistry12symbolForKeyERKNS_6StringE(nullptr, &key); }));
    Require(Throws<std::invalid_argument>([] { _ZN3WTF14SymbolRegistryD1Ev(nullptr); }));
}

int main() {
    TestCreate();
    TestRegistry();
    return 0;
}
