// Derived from Source/JavaScriptCore/yarr/RegularExpression.cpp@c55364d53f971f63a709976f168d932d558b8909
/*
 * Copyright (C) 2004, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2011 Peter Varga (pvarga@webkit.org), University of Szeged
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

#include "prx/libSceNKWebKit/include/YarrRegularExpressionExports.hpp"

#include <stdexcept>

using Jsc::YarrAbi::RegularExpression;
using Jsc::YarrAbi::RegularExpressionPrivate;

extern "C" {

void APS5_VABI _ZN3JSC4Yarr17RegularExpressionD1Ev(RegularExpression* self) {
    if (!self) throw std::invalid_argument("JSC::Yarr::RegularExpression::~RegularExpression: null this");
    JscRegularExpressionRelease_nid_no_patch(self->d);
}

void APS5_VABI _ZN3JSC4Yarr17RegularExpressionD2Ev(RegularExpression* self) {
    _ZN3JSC4Yarr17RegularExpressionD1Ev(self);
}

RegularExpression* APS5_VABI _ZN3JSC4Yarr17RegularExpressionaSERKS1_(RegularExpression* self, const RegularExpression* other) {
    if (!self) throw std::invalid_argument("JSC::Yarr::RegularExpression::operator=: null this");
    if (!other) throw std::invalid_argument("JSC::Yarr::RegularExpression::operator=: null source");
    RegularExpressionPrivate* const incoming = other->d;
    if (incoming) ++incoming->refCount;
    RegularExpressionPrivate* const previous = self->d;
    self->d = incoming;
    JscRegularExpressionRelease_nid_no_patch(previous);
    return self;
}

}
