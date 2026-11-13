/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Derived from Source/WebCore/bridge/IdentifierRep.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceNKWebKit/include/IdentifierRepExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstring>
#include <mutex>
#include <new>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace {

using NKWebKit::WebCoreAbi::IdentifierRep;
using NKWebKit::WtfAbi::String;

struct StringKeyHash {
    std::size_t operator()(const String& key) const {
        return key.impl ? _ZNK3WTF10StringImpl14concurrentHashEv(key.impl) : 0;
    }
};

struct StringKeyEqual {
    bool operator()(const String& a, const String& b) const {
        return _ZN3WTF5equalEPKNS_10StringImplES2_(a.impl, b.impl);
    }
};

std::mutex& RegistryMutex() {
    static auto* mutex = new std::mutex;
    return *mutex;
}

std::unordered_set<const IdentifierRep*>& IdentifierSet() {
    static auto* identifierSet = new std::unordered_set<const IdentifierRep*>;
    return *identifierSet;
}

std::unordered_map<int, IdentifierRep*>& IntIdentifierMap() {
    static auto* intIdentifierMap = new std::unordered_map<int, IdentifierRep*>;
    return *intIdentifierMap;
}

std::unordered_map<String, IdentifierRep*, StringKeyHash, StringKeyEqual>& StringIdentifierMap() {
    static auto* stringIdentifierMap = new std::unordered_map<String, IdentifierRep*, StringKeyHash, StringKeyEqual>;
    return *stringIdentifierMap;
}

IdentifierRep* Allocate() {
    return new (_ZN3WTF10fastMallocEm(sizeof(IdentifierRep))) IdentifierRep{};
}

IdentifierRep* Create(int number) {
    auto* identifier = Allocate();
    identifier->value.number = number;
    identifier->isString = false;
    return identifier;
}

IdentifierRep* Create(const char* name) {
    auto* identifier = Allocate();
    identifier->value.string = _ZN3WTF10fastStrDupEPKc(name);
    identifier->isString = true;
    return identifier;
}

}

extern "C" {

IdentifierRep* APS5_VABI _ZN7WebCore13IdentifierRep3getEi(int intID) {
    std::lock_guard lock(RegistryMutex());
    if (intID == 0 || intID == -1) {
        static IdentifierRep* negativeOneAndZeroIdentifiers[2];
        auto*& identifier = negativeOneAndZeroIdentifiers[intID + 1];
        if (!identifier) identifier = Create(intID);
        return identifier;
    }
    auto& identifiers = IntIdentifierMap();
    if (auto found = identifiers.find(intID); found != identifiers.end()) return found->second;
    auto* identifier = Create(intID);
    identifiers.emplace(intID, identifier);
    IdentifierSet().insert(identifier);
    return identifier;
}

IdentifierRep* APS5_VABI _ZN7WebCore13IdentifierRep3getEPKc(const char* name) {
    if (!name) return nullptr;
    String string = _ZN3WTF6String26fromUTF8WithLatin1FallbackEPKhm(reinterpret_cast<const unsigned char*>(name), std::strlen(name));
    std::lock_guard lock(RegistryMutex());
    auto& identifiers = StringIdentifierMap();
    if (auto found = identifiers.find(string); found != identifiers.end()) return found->second;
    auto* identifier = Create(name);
    identifiers.emplace(std::move(string), identifier);
    IdentifierSet().insert(identifier);
    return identifier;
}

bool APS5_VABI _ZN7WebCore13IdentifierRep7isValidEPS0_(IdentifierRep* identifier) {
    std::lock_guard lock(RegistryMutex());
    return IdentifierSet().contains(identifier);
}

}
