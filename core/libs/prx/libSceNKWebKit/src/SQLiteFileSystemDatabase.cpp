// Derived from Source/WebCore/platform/sql/SQLiteFileSystem.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from Source/WTF/wtf/posix/FileSystemPOSIX.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
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

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/SQLiteFileSystemDatabaseExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/src/BlobDataFileReferenceAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

using NKWebKit::WebCoreAbi::OptionalWallTime;
using NKWebKit::WtfAbi::CString;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringView;

constexpr std::array<const char*, 3> DatabaseFileSuffixes{"", "-wal", "-shm"};

CString FileSystemRepresentation(const String& path) {
    return _ZNK3WTF6String4utf8ENS_14ConversionModeE(&path, 0);
}

const char* Data(const CString& representation) {
    return representation.buffer ? reinterpret_cast<const char*>(representation.buffer + 1) : nullptr;
}

String AppendSuffix(const String& path, const char* suffix) {
    const unsigned suffixLength = static_cast<unsigned>(std::strlen(suffix));
    if (!suffixLength) return path;
    const StringView view = NKWebKit::WtfAbi::ViewOf(path.impl);
    void* characters = nullptr;
    auto* impl = NKWebKit::WtfAbi::CreateUninitialized(view.length + suffixLength, view.is8Bit, characters);
    if (view.is8Bit) {
        auto* output = static_cast<unsigned char*>(characters);
        for (unsigned index = 0; index < view.length; ++index)
            output[index] = static_cast<unsigned char>(NKWebKit::WtfAbi::CodeUnitAt(view, index));
        for (unsigned index = 0; index < suffixLength; ++index)
            output[view.length + index] = static_cast<unsigned char>(suffix[index]);
    } else {
        auto* output = static_cast<char16_t*>(characters);
        for (unsigned index = 0; index < view.length; ++index)
            output[index] = NKWebKit::WtfAbi::CodeUnitAt(view, index);
        for (unsigned index = 0; index < suffixLength; ++index)
            output[view.length + index] = static_cast<char16_t>(suffix[index]);
    }
    return String(impl);
}

}  // namespace

extern "C" {

bool APS5_VABI _ZN7WebCore16SQLiteFileSystem18deleteDatabaseFileERKN3WTF6StringE(const String& filePath) {
    if (!filePath.impl) return true;
    bool fileExists = false;
    for (const char* suffix : DatabaseFileSuffixes) {
        const String path = AppendSuffix(filePath, suffix);
        _ZN3WTF14FileSystemImpl10deleteFileERKNS_6StringE(path);
        fileExists |= _ZN3WTF14FileSystemImpl10fileExistsERKNS_6StringE(path);
    }
    return !fileExists;
}

OptionalWallTime APS5_VABI _ZN7WebCore16SQLiteFileSystem20databaseCreationTimeERKN3WTF6StringE(const String& fileName) {
    if (!fileName.impl || !fileName.impl->length) return OptionalWallTime{};
    const auto representation = FileSystemRepresentation(fileName);
    const char* data = Data(representation);
    if (!data) return OptionalWallTime{};
    FileStat status{};
    if (stat_nid_postfix(data, &status) != 0) return OptionalWallTime{};
    return OptionalWallTime{static_cast<double>(status.st_birthtim.tv_sec)};
}

}
