/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */
#include "prx/libSceNKWebKit/include/WebCoreSwitchExports.hpp"

#include <atomic>

namespace {

using NKWebKit::WebCoreAbi::FontCascadeCodePath;

std::atomic<FontCascadeCodePath> codePath{FontCascadeCodePath::Auto};
std::atomic<bool> shouldUseFontSmoothing{true};

}  // namespace

namespace NKWebKit {

WebCoreAbi::FontCascadeCodePath CurrentFontCascadeCodePath() {
    return codePath.load(std::memory_order_relaxed);
}

}  // namespace NKWebKit

extern "C" {

void APS5_VABI _ZN7WebCore11FontCascade11setCodePathENS0_8CodePathE(FontCascadeCodePath path) {
    codePath.store(path, std::memory_order_relaxed);
}

void APS5_VABI _ZN7WebCore11FontCascade21setShouldUseSmoothingEb(bool shouldUseSmoothing) {
    shouldUseFontSmoothing.store(shouldUseSmoothing, std::memory_order_relaxed);
}

bool APS5_VABI _ZN7WebCore11FontCascade18shouldUseSmoothingEv() {
    return shouldUseFontSmoothing.load(std::memory_order_relaxed);
}

}
