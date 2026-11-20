/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
 * Copyright (C) 2008, 2011, 2014-2016 Apple Inc. All Rights Reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceNKWebKit/include/ScrollbarValueExports.hpp"

extern "C" void APS5_VABI _ZN7WebCore14ScrollableArea32computeScrollbarValueAndOverhangEfffRfS1_(float currentPosition, float totalSize, float visibleSize, float& scrollbarValue, float& overhangAmount) {
    scrollbarValue = 0;
    overhangAmount = 0;
    float maximum = totalSize - visibleSize;

    if (currentPosition < 0) {
        scrollbarValue = 0;
        overhangAmount = -currentPosition;
    } else if (visibleSize + currentPosition > totalSize) {
        scrollbarValue = 1;
        overhangAmount = currentPosition + visibleSize - totalSize;
    } else {
        if (maximum > 0)
            scrollbarValue = currentPosition / maximum;
        else
            scrollbarValue = 0;
    }
}
