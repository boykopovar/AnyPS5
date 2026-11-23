// Derived from https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/sql/SQLiteFileSystem.cpp
/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

// Derived from https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WTF/wtf/posix/FileSystemPOSIX.cpp
/*
 * Copyright (C) 2007-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/LegacySQLiteFileSystemExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/src/BlobDataFileReferenceAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <cstddef>

namespace {

using NKWebKit::WebCoreAbi::OptionalWallTime;
using NKWebKit::WtfAbi::CString;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringView;

StringView ViewOf(const String& string) {
    return string.impl ? NKWebKit::WtfAbi::ViewOf(string.impl) : StringView{nullptr, 0, true};
}

void Append(void* characters, bool is8Bit, unsigned& position, char16_t unit) {
    if (is8Bit) static_cast<unsigned char*>(characters)[position] = static_cast<unsigned char>(unit);
    else static_cast<char16_t*>(characters)[position] = unit;
    ++position;
}

}  // namespace

extern "C" {

String APS5_VABI _ZN7WebCore16SQLiteFileSystem28appendDatabaseFileNameToPathERKN3WTF6StringES4_(const String& path, const String& fileName) {
    const StringView pathView = ViewOf(path);
    const StringView nameView = ViewOf(fileName);
    const bool separator = !pathView.length || NKWebKit::WtfAbi::CodeUnitAt(pathView, pathView.length - 1) != u'/';
    const bool is8Bit = pathView.is8Bit && nameView.is8Bit;
    void* characters = nullptr;
    String result(NKWebKit::WtfAbi::CreateUninitialized(pathView.length + (separator ? 1U : 0U) + nameView.length, is8Bit, characters));
    unsigned position = 0;
    for (unsigned index = 0; index < pathView.length; ++index) Append(characters, is8Bit, position, NKWebKit::WtfAbi::CodeUnitAt(pathView, index));
    if (separator) Append(characters, is8Bit, position, u'/');
    for (unsigned index = 0; index < nameView.length; ++index) Append(characters, is8Bit, position, NKWebKit::WtfAbi::CodeUnitAt(nameView, index));
    return result;
}

bool APS5_VABI _ZN7WebCore16SQLiteFileSystem28deleteEmptyDatabaseDirectoryERKN3WTF6StringE(const String& path) {
    return _ZN3WTF14FileSystemImpl20deleteEmptyDirectoryERKNS_6StringE(path);
}

OptionalWallTime APS5_VABI _ZN7WebCore16SQLiteFileSystem24databaseModificationTimeERKN3WTF6StringE(const String& fileName) {
    if (!fileName.impl) return OptionalWallTime{};
    const CString representation = _ZNK3WTF6String4utf8ENS_14ConversionModeE(&fileName, 0);
    const char* data = representation.buffer ? reinterpret_cast<const char*>(representation.buffer + 1) : nullptr;
    if (!data || !*data) return OptionalWallTime{};
    FileStat status{};
    if (stat_nid_postfix(data, &status) != 0) return OptionalWallTime{};
    return OptionalWallTime{static_cast<double>(status.st_mtim.tv_sec)};
}

}
