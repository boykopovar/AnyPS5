// Derived from Source/WebCore/page/SettingsBase.cpp@41412390dcd775cf7b24a32d6b864c9e386e4859
/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2011, 2012 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/SettingsBaseDefaultsExports.hpp"

using NKWebKit::WtfAbi::EmptyStringImpl;
using NKWebKit::WtfAbi::String;

extern "C" {

float APS5_VABI _ZN7WebCore12SettingsBase26defaultMinimumZoomFontSizeEv() {
    return 15;
}

bool APS5_VABI _ZN7WebCore12SettingsBase28defaultTextAutosizingEnabledEv() {
    return false;
}

bool APS5_VABI _ZN7WebCore12SettingsBase37defaultDownloadableBinaryFontsEnabledEv() {
    return true;
}

bool APS5_VABI _ZN7WebCore12SettingsBase35defaultContentChangeObserverEnabledEv() {
    return false;
}

const String& APS5_VABI _ZN7WebCore12SettingsBase48defaultMediaContentTypesRequiringHardwareSupportEv() {
    static const String value(EmptyStringImpl());
    return value;
}

}
