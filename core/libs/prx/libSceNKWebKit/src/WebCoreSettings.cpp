// Derived from Source/WebCore/page/Settings.yaml@6aa3f91fdcd4ec16d4369ca7bdfb76eb9500fa31 and Source/WTF/Scripts/Preferences/WebPreferences.yaml, WebPreferencesDebug.yaml and WebPreferencesInternal.yaml at the same revision
/*
 * Copyright (C) 2017-2020 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/SettingsExports.hpp"

#include <cstddef>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace {

using NKWebKit::WebCoreAbi::OptionalBool;

struct BooleanSettings {
    bool imagesEnabled = true;
    bool showDebugBorders = false;
    bool showRepaintCounter = false;
    bool authorAndUserStylesEnabled = true;
    bool iceCandidateFilteringEnabled = true;
    bool scriptEnabled = false;
    bool webSecurityEnabled = true;
    bool needsSiteSpecificQuirks = false;
    bool webRTCEncryptionEnabled = true;
    std::optional<bool> imagesEnabledInspectorOverride;
    std::optional<bool> showDebugBordersInspectorOverride;
    std::optional<bool> showRepaintCounterInspectorOverride;
    std::optional<bool> authorAndUserStylesEnabledInspectorOverride;
    std::optional<bool> iceCandidateFilteringEnabledInspectorOverride;
};

constexpr std::size_t maximumObjects = 256;

std::mutex& StoreMutex() {
    static std::mutex mutex;
    return mutex;
}

std::unordered_map<const void*, BooleanSettings>& Store() {
    static std::unordered_map<const void*, BooleanSettings> store;
    return store;
}

[[noreturn]] void ThrowNull(const char* caller) {
    throw std::invalid_argument(std::string("WebCore::Settings::") + caller + ": null settings");
}

void Checked(const void* self, const char* caller) {
    if (!self) ThrowNull(caller);
}

BooleanSettings& Find(const void* self) {
    auto& store = Store();
    auto found = store.find(self);
    if (found != store.end()) return found->second;
    if (store.size() >= maximumObjects)
        throw std::length_error(std::string("WebCore::Settings: more than ") + std::to_string(maximumObjects) + " settings objects");
    return store.emplace(self, BooleanSettings()).first->second;
}

const BooleanSettings& Defaults() {
    static const BooleanSettings defaults;
    return defaults;
}

bool Read(const void* self, bool BooleanSettings::* field, std::optional<bool> BooleanSettings::* inspectorOverride, const char* caller) {
    Checked(self, caller);
    std::lock_guard<std::mutex> lock(StoreMutex());
    const auto& store = Store();
    auto found = store.find(self);
    if (found == store.end()) {
        if (inspectorOverride && Defaults().*inspectorOverride) return *(Defaults().*inspectorOverride);
        return Defaults().*field;
    }
    if (inspectorOverride && found->second.*inspectorOverride) return *(found->second.*inspectorOverride);
    return found->second.*field;
}

void Write(const void* self, bool BooleanSettings::* field, bool value, const char* caller) {
    Checked(self, caller);
    std::lock_guard<std::mutex> lock(StoreMutex());
    Find(self).*field = value;
}

void WriteInspectorOverride(const void* self, std::optional<bool> BooleanSettings::* inspectorOverride, OptionalBool value, const char* caller) {
    Checked(self, caller);
    std::lock_guard<std::mutex> lock(StoreMutex());
    Find(self).*inspectorOverride = value.hasValue ? std::optional<bool>(value.value) : std::nullopt;
}

}

extern "C" {

void APS5_VABI _ZN7WebCore8Settings16setImagesEnabledEb(void* self, bool imagesEnabled) {
    Write(self, &BooleanSettings::imagesEnabled, imagesEnabled, "setImagesEnabled");
}

void APS5_VABI _ZN7WebCore8Settings19setShowDebugBordersEb(void* self, bool showDebugBorders) {
    Write(self, &BooleanSettings::showDebugBorders, showDebugBorders, "setShowDebugBorders");
}

void APS5_VABI _ZN7WebCore8Settings21setShowRepaintCounterEb(void* self, bool showRepaintCounter) {
    Write(self, &BooleanSettings::showRepaintCounter, showRepaintCounter, "setShowRepaintCounter");
}

void APS5_VABI _ZN7WebCore8Settings29setAuthorAndUserStylesEnabledEb(void* self, bool authorAndUserStylesEnabled) {
    Write(self, &BooleanSettings::authorAndUserStylesEnabled, authorAndUserStylesEnabled, "setAuthorAndUserStylesEnabled");
}

void APS5_VABI _ZN7WebCore8Settings31setICECandidateFilteringEnabledEb(void* self, bool iceCandidateFilteringEnabled) {
    Write(self, &BooleanSettings::iceCandidateFilteringEnabled, iceCandidateFilteringEnabled, "setICECandidateFilteringEnabled");
}

void APS5_VABI _ZN7WebCore8Settings33setImagesEnabledInspectorOverrideEN3WTF8OptionalIbEE(void* self, OptionalBool imagesEnabledInspectorOverride) {
    WriteInspectorOverride(self, &BooleanSettings::imagesEnabledInspectorOverride, imagesEnabledInspectorOverride, "setImagesEnabledInspectorOverride");
}

void APS5_VABI _ZN7WebCore8Settings36setShowDebugBordersInspectorOverrideEN3WTF8OptionalIbEE(void* self, OptionalBool showDebugBordersInspectorOverride) {
    WriteInspectorOverride(self, &BooleanSettings::showDebugBordersInspectorOverride, showDebugBordersInspectorOverride, "setShowDebugBordersInspectorOverride");
}

void APS5_VABI _ZN7WebCore8Settings38setShowRepaintCounterInspectorOverrideEN3WTF8OptionalIbEE(void* self, OptionalBool showRepaintCounterInspectorOverride) {
    WriteInspectorOverride(self, &BooleanSettings::showRepaintCounterInspectorOverride, showRepaintCounterInspectorOverride, "setShowRepaintCounterInspectorOverride");
}

void APS5_VABI _ZN7WebCore8Settings46setAuthorAndUserStylesEnabledInspectorOverrideEN3WTF8OptionalIbEE(void* self, OptionalBool authorAndUserStylesEnabledInspectorOverride) {
    WriteInspectorOverride(self, &BooleanSettings::authorAndUserStylesEnabledInspectorOverride, authorAndUserStylesEnabledInspectorOverride, "setAuthorAndUserStylesEnabledInspectorOverride");
}

void APS5_VABI _ZN7WebCore8Settings48setICECandidateFilteringEnabledInspectorOverrideEN3WTF8OptionalIbEE(void* self, OptionalBool iceCandidateFilteringEnabledInspectorOverride) {
    WriteInspectorOverride(self, &BooleanSettings::iceCandidateFilteringEnabledInspectorOverride, iceCandidateFilteringEnabledInspectorOverride, "setICECandidateFilteringEnabledInspectorOverride");
}

bool APS5_VABI _ZNK7WebCore8Settings15isScriptEnabledEv(const void* self) {
    return Read(self, &BooleanSettings::scriptEnabled, nullptr, "isScriptEnabled");
}

bool APS5_VABI _ZNK7WebCore8Settings16areImagesEnabledEv(const void* self) {
    return Read(self, &BooleanSettings::imagesEnabled, &BooleanSettings::imagesEnabledInspectorOverride, "areImagesEnabled");
}

bool APS5_VABI _ZNK7WebCore8Settings16showDebugBordersEv(const void* self) {
    return Read(self, &BooleanSettings::showDebugBorders, &BooleanSettings::showDebugBordersInspectorOverride, "showDebugBorders");
}

bool APS5_VABI _ZNK7WebCore8Settings18showRepaintCounterEv(const void* self) {
    return Read(self, &BooleanSettings::showRepaintCounter, &BooleanSettings::showRepaintCounterInspectorOverride, "showRepaintCounter");
}

bool APS5_VABI _ZNK7WebCore8Settings18webSecurityEnabledEv(const void* self) {
    return Read(self, &BooleanSettings::webSecurityEnabled, nullptr, "webSecurityEnabled");
}

bool APS5_VABI _ZNK7WebCore8Settings23needsSiteSpecificQuirksEv(const void* self) {
    return Read(self, &BooleanSettings::needsSiteSpecificQuirks, nullptr, "needsSiteSpecificQuirks");
}

bool APS5_VABI _ZNK7WebCore8Settings23webRTCEncryptionEnabledEv(const void* self) {
    return Read(self, &BooleanSettings::webRTCEncryptionEnabled, nullptr, "webRTCEncryptionEnabled");
}

bool APS5_VABI _ZNK7WebCore8Settings26authorAndUserStylesEnabledEv(const void* self) {
    return Read(self, &BooleanSettings::authorAndUserStylesEnabled, &BooleanSettings::authorAndUserStylesEnabledInspectorOverride, "authorAndUserStylesEnabled");
}

bool APS5_VABI _ZNK7WebCore8Settings28iceCandidateFilteringEnabledEv(const void* self) {
    return Read(self, &BooleanSettings::iceCandidateFilteringEnabled, &BooleanSettings::iceCandidateFilteringEnabledInspectorOverride, "iceCandidateFilteringEnabled");
}

}
