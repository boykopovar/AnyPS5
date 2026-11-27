/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE INC.
 * OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * Derived from Source/JavaScriptCore/inspector/ContentSearchUtilities.cpp and Source/WTF/wtf/StdLibExtras.h@e12a212a531a
 */

#include "prx/libSceNKWebKit/include/ContentSearchUtilitiesExports.hpp"

#include <stdexcept>

extern "C" {

WTF::TextPosition APS5_VABI _ZN9Inspector22ContentSearchUtilities22textPositionFromOffsetEmRKN3WTF6VectorImLm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(std::size_t offset, const NKWebKit::WtfAbi::Vector<std::size_t>& lineEndings) {
    if (!lineEndings.size) throw std::out_of_range("Inspector::ContentSearchUtilities::textPositionFromOffset: no line endings");
    std::size_t start = 0;
    std::size_t size = lineEndings.size;
    while (size > 1) {
        const std::size_t pos = (size - 1) >> 1;
        const std::size_t value = lineEndings[start + pos];
        if (value == offset) {
            start += pos;
            break;
        }
        if (offset < value) {
            size = pos;
        } else {
            size -= pos + 1;
            start += pos + 1;
        }
    }
    std::size_t lineIndex = start;
    if (offset >= lineEndings[lineIndex]) ++lineIndex;
    const std::size_t lineStart = lineIndex ? lineEndings[lineIndex - 1] : 0;
    return {static_cast<int>(lineIndex), static_cast<int>(offset - lineStart)};
}

}
