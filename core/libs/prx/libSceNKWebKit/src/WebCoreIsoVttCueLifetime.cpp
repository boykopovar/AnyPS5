// Derived from Source/WebCore/platform/graphics/iso/ISOVTTCue.cpp@73a35b43be7ae44c084885bd3fa4b403e59723c8
/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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
#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/include/IsoBoxExports.hpp"
#include "prx/libSceNKWebKit/include/LegacyExports.hpp"

#include <memory>
#include <stdexcept>
#include <utility>

namespace {

using NKWebKit::IsoAbi::WebVTTCue;
using NKWebKit::MediaAbi::MediaTime;
using NKWebKit::WtfAbi::String;

void APS5_VABI DeleteCue(WebVTTCue* cue) {
    _ZN7WebCore12ISOWebVTTCueD2Ev(cue);
    _ZN7WebCore6ISOBoxdlEPv(cue);
}

bool APS5_VABI ParseCue(WebVTTCue*, void*, unsigned*) {
    NotImplemented_nid_no_patch("_ZN7WebCore12ISOWebVTTCue5parseERN3JSC8DataViewERj");
}

const void* const CueVtable[] = {
    nullptr,
    nullptr,
    reinterpret_cast<const void*>(&_ZN7WebCore12ISOWebVTTCueD1Ev),
    reinterpret_cast<const void*>(&DeleteCue),
    reinterpret_cast<const void*>(&ParseCue),
};

template <typename T>
T& Checked(T* argument) {
    if (!argument) throw std::invalid_argument("null WebCore::ISOWebVTTCue argument");
    return *argument;
}

}  // namespace

extern "C" {

void APS5_VABI _ZN7WebCore12ISOWebVTTCueC1ERKS0_(WebVTTCue* self, const WebVTTCue* source) {
    _ZN7WebCore12ISOWebVTTCueC2ERKS0_(self, source);
}

void APS5_VABI _ZN7WebCore12ISOWebVTTCueC2ERKS0_(WebVTTCue* self, const WebVTTCue* source) {
    auto* cue = std::construct_at(&Checked(self), Checked(source));
    cue->box.vtable = &CueVtable[2];
}

void APS5_VABI _ZN7WebCore12ISOWebVTTCueC1ERKN3WTF9MediaTimeES4_(WebVTTCue* self, const MediaTime* presentationTime, const MediaTime* duration) {
    _ZN7WebCore12ISOWebVTTCueC2ERKN3WTF9MediaTimeES4_(self, presentationTime, duration);
}

void APS5_VABI _ZN7WebCore12ISOWebVTTCueC2ERKN3WTF9MediaTimeES4_(WebVTTCue* self, const MediaTime* presentationTime, const MediaTime* duration) {
    const auto& start = Checked(presentationTime);
    const auto& length = Checked(duration);
    auto* cue = std::construct_at(&Checked(self));
    cue->box.vtable = &CueVtable[2];
    cue->presentationTime = start;
    cue->duration = length;
}

void APS5_VABI _ZN7WebCore12ISOWebVTTCueC1EON3WTF9MediaTimeES3_ONS1_6StringES5_S5_S5_S5_(WebVTTCue* self,
    MediaTime* presentationTime, MediaTime* duration, String* sourceID, String* identifier,
    String* originalStartTime, String* settings, String* cueText) {
    _ZN7WebCore12ISOWebVTTCueC2EON3WTF9MediaTimeES3_ONS1_6StringES5_S5_S5_S5_(
        self, presentationTime, duration, sourceID, identifier, originalStartTime, settings, cueText);
}

void APS5_VABI _ZN7WebCore12ISOWebVTTCueC2EON3WTF9MediaTimeES3_ONS1_6StringES5_S5_S5_S5_(WebVTTCue* self,
    MediaTime* presentationTime, MediaTime* duration, String* sourceID, String* identifier,
    String* originalStartTime, String* settings, String* cueText) {
    const auto& start = Checked(presentationTime);
    const auto& length = Checked(duration);
    auto& source = Checked(sourceID);
    auto& id = Checked(identifier);
    auto& original = Checked(originalStartTime);
    auto& options = Checked(settings);
    auto& text = Checked(cueText);
    auto* cue = std::construct_at(&Checked(self));
    cue->box.vtable = &CueVtable[2];
    cue->presentationTime = start;
    cue->duration = length;
    cue->sourceID = std::move(source);
    cue->identifier = std::move(id);
    cue->originalStartTime = std::move(original);
    cue->settings = std::move(options);
    cue->cueText = std::move(text);
}

WebVTTCue* APS5_VABI _ZN7WebCore12ISOWebVTTCueaSERKS0_(WebVTTCue* self, const WebVTTCue* source) {
    auto& cue = Checked(self);
    const auto& other = Checked(source);
    if (self != source) {
        auto copy = other;
        copy.box.vtable = cue.box.vtable;
        cue = std::move(copy);
    }
    return self;
}

void APS5_VABI _ZN7WebCore12ISOWebVTTCueD1Ev(WebVTTCue* self) {
    _ZN7WebCore12ISOWebVTTCueD2Ev(self);
}

void APS5_VABI _ZN7WebCore12ISOWebVTTCueD2Ev(WebVTTCue* self) {
    std::destroy_at(&Checked(self));
}

}
