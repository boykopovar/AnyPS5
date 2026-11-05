// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/WTF/wtf/Language.cpp
/*
 * Copyright (C) 2010, 2013, 2016, 2017 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstddef>
#include <mutex>
#include <new>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringVector;

using Observer = void (APS5_VABI *)(void*);

struct ObserverEntry {
    void* context;
    Observer observer;
};

std::mutex languagesLock;

StringVector& PreferredLanguagesOverride() {
    static auto* languages = new StringVector;
    return *languages;
}

std::vector<ObserverEntry>& ObserverMap() {
    static auto* map = new std::vector<ObserverEntry>;
    return *map;
}

void CheckContext(void* context) {
    if (!context || context == reinterpret_cast<void*>(-1)) throw std::invalid_argument("invalid WTF language change observer context");
}

bool IsRegistered(void* context) {
    for (const auto& entry : ObserverMap()) {
        if (entry.context == context) return true;
    }
    return false;
}

unsigned Length(const String& string) {
    return string.impl ? string.impl->length : 0;
}

char16_t Character(const String& string, unsigned index) {
    return string.impl->hashAndFlags & 4 ? static_cast<char16_t>(static_cast<const unsigned char*>(string.impl->data)[index]) : static_cast<const char16_t*>(string.impl->data)[index];
}

String Lowercase(const String& string) {
    return _ZNK3WTF6String23convertToASCIILowercaseEv(&string);
}

String ReplaceAt(const String& string, unsigned index, char16_t value) {
    if (string.impl->hashAndFlags & 4) {
        unsigned char* data;
        String result = _ZN3WTF10StringImpl19createUninitializedEjRPh(string.impl->length, data);
        for (unsigned i = 0; i < string.impl->length; ++i) data[i] = static_cast<unsigned char>(Character(string, i));
        data[index] = static_cast<unsigned char>(value);
        return result;
    }
    char16_t* data;
    String result = _ZN3WTF10StringImpl19createUninitializedEjRPDs(string.impl->length, data);
    for (unsigned i = 0; i < string.impl->length; ++i) data[i] = Character(string, i);
    data[index] = value;
    return result;
}

String CanonicalLanguageIdentifier(const String& languageCode) {
    String lowercase = Lowercase(languageCode);
    if (Length(lowercase) >= 3 && Character(lowercase, 2) == '_') return ReplaceAt(lowercase, 2, '-');
    return lowercase;
}

StringVector Copy(const StringVector& source) {
    StringVector result;
    if (!source.size) return result;
    result.buffer = static_cast<String*>(_ZN3WTF10fastMallocEm(std::size_t{source.size} * sizeof(String)));
    result.capacity = source.size;
    for (; result.size < source.size; ++result.size) new (&result.buffer[result.size]) String(source.buffer[result.size]);
    return result;
}

void Swap(StringVector& a, StringVector& b) {
    std::swap(a.buffer, b.buffer);
    std::swap(a.capacity, b.capacity);
    std::swap(a.size, b.size);
}

}

NKWebKit::WtfAbi::StringVector::~StringVector() {
    for (unsigned i = 0; i < size; ++i) buffer[i].~String();
    if (buffer) _ZN3WTF8fastFreeEPv(buffer);
}

extern "C" void APS5_VABI _ZN3WTF17languageDidChangeEv() {
    const std::vector<ObserverEntry> observers = ObserverMap();
    for (const auto& observer : observers) {
        if (!IsRegistered(observer.context)) continue;
        if (!observer.observer) throw std::invalid_argument("null WTF language change observer");
        observer.observer(observer.context);
    }
}

extern "C" void APS5_VABI _ZN3WTF25addLanguageChangeObserverEPvPFvS0_E(void* context, Observer observer) {
    CheckContext(context);
    auto& map = ObserverMap();
    for (auto& entry : map) {
        if (entry.context == context) {
            entry.observer = observer;
            return;
        }
    }
    map.push_back({context, observer});
}

extern "C" NKWebKit::WtfAbi::String APS5_VABI _ZN3WTF28displayNameForLanguageLocaleERKNS_6StringE(const String* localeName) {
    if (!localeName) throw std::invalid_argument("null WTF string");
    return *localeName;
}

extern "C" void APS5_VABI _ZN3WTF28removeLanguageChangeObserverEPv(void* context) {
    CheckContext(context);
    auto& map = ObserverMap();
    for (auto entry = map.begin(); entry != map.end(); ++entry) {
        if (entry->context == context) {
            map.erase(entry);
            return;
        }
    }
}

extern "C" void APS5_VABI _ZN3WTF30overrideUserPreferredLanguagesERKNS_6VectorINS_6StringELm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEE(const StringVector* languages) {
    if (!languages) throw std::invalid_argument("null WTF string vector");
    {
        std::lock_guard<std::mutex> locker(languagesLock);
        StringVector copy = Copy(*languages);
        Swap(copy, PreferredLanguagesOverride());
    }
    _ZN3WTF17languageDidChangeEv();
}

extern "C" NKWebKit::WtfAbi::StringVector APS5_VABI _ZN3WTF30userPreferredLanguagesOverrideEv() {
    std::lock_guard<std::mutex> locker(languagesLock);
    return Copy(PreferredLanguagesOverride());
}

extern "C" std::size_t APS5_VABI _ZN3WTF33indexOfBestMatchingLanguageInListERKNS_6StringERKNS_6VectorIS0_Lm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEERb(const String* language, const StringVector* languageList, bool* exactMatch) {
    if (!language || !languageList || !exactMatch) throw std::invalid_argument("null WTF language match argument");
    String lowercaseLanguage = Lowercase(*language);
    const String* languageWithoutLocaleMatch = nullptr;
    const String* languageMatchButNotLocale = nullptr;
    std::size_t languageWithoutLocaleMatchIndex = 0;
    std::size_t languageMatchButNotLocaleMatchIndex = 0;
    const unsigned languageLength = Length(lowercaseLanguage);
    const bool canMatchLanguageOnly = languageLength == 2 || (languageLength >= 3 && Character(lowercaseLanguage, 2) == '-');

    for (std::size_t i = 0; i < languageList->size; ++i) {
        String canonicalizedLanguageFromList = CanonicalLanguageIdentifier(languageList->buffer[i]);

        if (_ZN3WTF5equalEPKNS_10StringImplES2_(lowercaseLanguage.impl, canonicalizedLanguageFromList.impl)) {
            *exactMatch = true;
            return i;
        }

        const unsigned canonicalizedLength = Length(canonicalizedLanguageFromList);
        if (canMatchLanguageOnly && canonicalizedLength >= 2) {
            if (Character(lowercaseLanguage, 0) == Character(canonicalizedLanguageFromList, 0) && Character(lowercaseLanguage, 1) == Character(canonicalizedLanguageFromList, 1)) {
                if (!languageWithoutLocaleMatch && canonicalizedLength == 2) {
                    languageWithoutLocaleMatch = &languageList->buffer[i];
                    languageWithoutLocaleMatchIndex = i;
                }
                if (!languageMatchButNotLocale && canonicalizedLength >= 3) {
                    languageMatchButNotLocale = &languageList->buffer[i];
                    languageMatchButNotLocaleMatchIndex = i;
                }
            }
        }
    }

    *exactMatch = false;

    if (languageWithoutLocaleMatch) return languageWithoutLocaleMatchIndex;

    if (languageMatchButNotLocale) return languageMatchButNotLocaleMatchIndex;

    return languageList->size;
}
