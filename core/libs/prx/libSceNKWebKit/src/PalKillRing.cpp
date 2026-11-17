// Derived from Source/WebCore/PAL/pal/text/KillRing.cpp@e12a212a531aeddc11259e8515671cf5188e038d
/*
 * Copyright (C) 2010 Google Inc. All Rights Reserved.
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
 *
 * Derived from Source/WebCore/PAL/pal/text/KillRing.cpp@e12a212a531aeddc11259e8515671cf5188e038d
 */

#include "prx/libSceNKWebKit/include/PalKillRingExports.hpp"

extern "C" {

void APS5_VABI _ZN3PAL8KillRing6appendERKN3WTF6StringE(NKWebKit::PalAbi::KillRing*, const NKWebKit::WtfAbi::String*) {
}

void APS5_VABI _ZN3PAL8KillRing7prependERKN3WTF6StringE(NKWebKit::PalAbi::KillRing*, const NKWebKit::WtfAbi::String*) {
}

NKWebKit::WtfAbi::String APS5_VABI _ZN3PAL8KillRing4yankEv(NKWebKit::PalAbi::KillRing*) {
    return NKWebKit::WtfAbi::String();
}

void APS5_VABI _ZN3PAL8KillRing16startNewSequenceEv(NKWebKit::PalAbi::KillRing*) {
}

void APS5_VABI _ZN3PAL8KillRing16setToYankedStateEv(NKWebKit::PalAbi::KillRing*) {
}

}
