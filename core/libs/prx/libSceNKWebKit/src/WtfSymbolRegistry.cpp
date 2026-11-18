// Derived from https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/text/SymbolImpl.cpp
// Derived from https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/text/SymbolRegistry.cpp
/*
 * Copyright (C) 2016 Yusuke Suzuki <utatane.tea@gmail.com>.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (C) 2015 Yusuke Suzuki <utatane.tea@gmail.com>.
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceNKWebKit/include/WtfSymbolExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace NKWebKit {
void* Allocate(std::size_t size, std::size_t alignment = 16);
}

namespace {

using NKWebKit::WtfAbi::RegisteredSymbolImpl;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::SymbolImpl;
using NKWebKit::WtfAbi::SymbolRegistry;

constexpr unsigned Is8BitFlag = 4;
constexpr unsigned IsSymbolFlag = 32;
constexpr unsigned BufferSubstring = 2;
constexpr unsigned BufferOwnershipMask = 3;
constexpr std::uint8_t PrivateSymbolType = 1;

StringImpl* OwnerOf(StringImpl& rep) {
    if ((rep.hashAndFlags & BufferOwnershipMask) == BufferSubstring) return *reinterpret_cast<StringImpl**>(reinterpret_cast<unsigned char*>(&rep) + 24);
    return &rep;
}

void Retain(StringImpl* string) {
    string->refCount += 2;
}

void Initialize(SymbolImpl& symbol, const void* characters, unsigned length, bool is8Bit, StringImpl* owner, unsigned flags) {
    symbol.string = StringImpl{2, length, characters, (is8Bit ? Is8BitFlag : 0U) | IsSymbolFlag | BufferSubstring};
    symbol.owner = owner;
    symbol.hashForSymbolShiftedWithFlagCount = _ZN3WTF10SymbolImpl17nextHashForSymbolEv();
    symbol.flags = flags;
}

template<typename Symbol> Symbol* CreateFrom(StringImpl* rep, unsigned flags, const char* function) {
    if (!rep) throw std::invalid_argument(std::string(function) + " requires a string");
    StringImpl* owner = OwnerOf(*rep);
    if ((owner->hashAndFlags & BufferOwnershipMask) == BufferSubstring) throw std::logic_error(std::string(function) + " was given a substring whose owner is a substring");
    auto* symbol = static_cast<Symbol*>(NKWebKit::Allocate(sizeof(Symbol)));
    Retain(owner);
    Initialize(reinterpret_cast<SymbolImpl&>(*symbol), rep->data, rep->length, rep->hashAndFlags & Is8BitFlag, owner, flags);
    return symbol;
}

struct Key {
    std::u16string characters;
    bool operator==(const Key& other) const { return characters == other.characters; }
};

struct KeyHash {
    std::size_t operator()(const Key& key) const { return std::hash<std::u16string>()(key.characters); }
};

Key KeyOf(const StringImpl* string) {
    Key key;
    if (!string) return key;
    key.characters.reserve(string->length);
    for (unsigned i = 0; i < string->length; ++i) {
        key.characters.push_back(string->hashAndFlags & Is8BitFlag ? static_cast<const unsigned char*>(string->data)[i] : static_cast<const char16_t*>(string->data)[i]);
    }
    return key;
}

std::mutex& RegistriesLock() {
    static std::mutex lock;
    return lock;
}

std::unordered_map<const SymbolRegistry*, std::unordered_map<Key, RegisteredSymbolImpl*, KeyHash>>& Registries() {
    static auto* registries = new std::unordered_map<const SymbolRegistry*, std::unordered_map<Key, RegisteredSymbolImpl*, KeyHash>>();
    return *registries;
}

void DestroyRegistry(SymbolRegistry* self) {
    if (!self) throw std::invalid_argument("WTF::SymbolRegistry::~SymbolRegistry requires an object");
    std::vector<RegisteredSymbolImpl*> symbols;
    {
        std::lock_guard<std::mutex> locker(RegistriesLock());
        auto found = Registries().find(self);
        if (found == Registries().end()) return;
        for (auto& entry : found->second) symbols.push_back(entry.second);
        Registries().erase(found);
    }
    for (auto* symbol : symbols) symbol->symbolRegistry = nullptr;
    for (auto* symbol : symbols) NKWebKit::WtfAbi::Release(&symbol->symbol.string);
}

}  // namespace

namespace NKWebKit::WtfAbi {

void ForgetRegisteredSymbol(StringImpl* string) {
    auto& symbol = *reinterpret_cast<RegisteredSymbolImpl*>(string);
    if (!(symbol.symbol.flags & SymbolFlagIsRegistered) || !symbol.symbolRegistry) return;
    std::lock_guard<std::mutex> locker(RegistriesLock());
    auto found = Registries().find(static_cast<SymbolRegistry*>(symbol.symbolRegistry));
    if (found == Registries().end()) return;
    for (auto it = found->second.begin(); it != found->second.end(); ++it) {
        if (it->second == &symbol) {
            found->second.erase(it);
            return;
        }
    }
}

}  // namespace NKWebKit::WtfAbi

extern "C" {

String APS5_VABI _ZN3WTF10SymbolImpl6createERNS_10StringImplE(StringImpl* rep) {
    return String(&CreateFrom<SymbolImpl>(rep, 0, "WTF::SymbolImpl::create")->string);
}

String APS5_VABI _ZN3WTF10SymbolImpl16createNullSymbolEv() {
    auto* symbol = static_cast<SymbolImpl*>(NKWebKit::Allocate(sizeof(SymbolImpl)));
    StringImpl* empty = NKWebKit::WtfAbi::EmptyStringImpl();
    Initialize(*symbol, empty->data, 0, true, empty, NKWebKit::WtfAbi::SymbolFlagIsNullSymbol);
    return String(&symbol->string);
}

String APS5_VABI _ZN3WTF17PrivateSymbolImpl6createERNS_10StringImplE(StringImpl* rep) {
    return String(&CreateFrom<SymbolImpl>(rep, NKWebKit::WtfAbi::SymbolFlagIsPrivate, "WTF::PrivateSymbolImpl::create")->string);
}

String APS5_VABI _ZN3WTF14SymbolRegistry12symbolForKeyERKNS_6StringE(SymbolRegistry* self, const String* key) {
    if (!self || !key || !key->impl) throw std::invalid_argument("WTF::SymbolRegistry::symbolForKey requires a registry and a non-null key");
    Key lookup = KeyOf(key->impl);
    {
        std::lock_guard<std::mutex> locker(RegistriesLock());
        auto& table = Registries()[self];
        auto found = table.find(lookup);
        if (found != table.end()) {
            Retain(&found->second->symbol.string);
            return String(&found->second->symbol.string);
        }
    }
    unsigned flags = NKWebKit::WtfAbi::SymbolFlagIsRegistered;
    if (self->symbolType == PrivateSymbolType) flags |= NKWebKit::WtfAbi::SymbolFlagIsPrivate;
    auto* symbol = CreateFrom<RegisteredSymbolImpl>(key->impl, flags, "WTF::SymbolRegistry::symbolForKey");
    symbol->symbolRegistry = self;
    Retain(&symbol->symbol.string);
    {
        std::lock_guard<std::mutex> locker(RegistriesLock());
        Registries()[self].emplace(std::move(lookup), symbol);
    }
    return String(&symbol->symbol.string);
}

void APS5_VABI _ZN3WTF14SymbolRegistryD1Ev(SymbolRegistry* self) {
    DestroyRegistry(self);
}

void APS5_VABI _ZN3WTF14SymbolRegistryD2Ev(SymbolRegistry* self) {
    DestroyRegistry(self);
}

}
