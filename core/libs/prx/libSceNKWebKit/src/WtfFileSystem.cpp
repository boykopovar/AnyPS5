// Derived from Source/WTF/wtf/playstation/FileSystemPlayStation.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from Source/WTF/wtf/posix/FileSystemPOSIX.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from Source/WTF/wtf/FileSystem.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2007-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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

/*
 * Copyright (C) 2007-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2015 Canon Inc. All rights reserved.
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
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include "prx/libc/include/GuestFilesystemPrimitives.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel_sys/include/Api.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

using NKWebKit::WtfAbi::CString;
using NKWebKit::WtfAbi::String;

constexpr std::size_t PathMax = 1024;
constexpr int GuestEintr = 4;
constexpr int CopyBufferSize = 1 << 16;

CString FileSystemRepresentation(const String& path) {
    return _ZNK3WTF6String4utf8ENS_14ConversionModeE(&path, 0);
}

const char* Data(const CString& representation) {
    return representation.buffer ? reinterpret_cast<const char*>(representation.buffer + 1) : nullptr;
}

bool Status(const String& path, FileStat& status, bool followLinks) {
    const auto representation = FileSystemRepresentation(path);
    const char* data = Data(representation);
    if (!data) return false;
    return (followLinks ? stat_nid_postfix(data, &status) : lstat_nid_postfix(data, &status)) == 0;
}

bool IsDirectory(const FileStat& status) {
    return GuestFilesystem::TypeOfMode(status.st_mode) == GuestFilesystem::TypeDirectory;
}

int ReadFromFile(int handle, char* buffer, int length) {
    for (;;) {
        const auto bytesRead = read_nid_postfix(handle, buffer, static_cast<std::uint64_t>(length));
        if (bytesRead >= 0) return static_cast<int>(bytesRead);
        if (*__error_nid_postfix() != GuestEintr) return -1;
    }
}

bool WriteToFile(int handle, const char* data, int length) {
    int offset = 0;
    while (offset < length) {
        const auto bytesWritten = write_nid_postfix(handle, data + offset, length - offset);
        if (bytesWritten < 0) {
            if (*__error_nid_postfix() == GuestEintr) continue;
            return false;
        }
        if (bytesWritten == 0) return false;
        offset += static_cast<int>(bytesWritten);
    }
    return true;
}

struct FileCloser {
    int handle;
    ~FileCloser() { _ZN3WTF14FileSystemImpl9closeFileERi(handle); }
};

bool CopyFileContents(const String& targetPath, const String& linkPath) {
    FileStat status{};
    if (!Status(targetPath, status, true) || GuestFilesystem::TypeOfMode(status.st_mode) != GuestFilesystem::TypeRegular) return false;
    FileCloser source{_ZN3WTF14FileSystemImpl8openFileERKNS_6StringENS0_12FileOpenModeENS0_20FileAccessPermissionEb(targetPath, 0, true, false)};
    if (source.handle == -1) return false;
    FileCloser destination{_ZN3WTF14FileSystemImpl8openFileERKNS_6StringENS0_12FileOpenModeENS0_20FileAccessPermissionEb(linkPath, 1, true, true)};
    if (destination.handle == -1) return false;
    fchmod_nid_postfix(destination.handle, status.st_mode & 07777);
    std::array<char, CopyBufferSize> buffer{};
    for (;;) {
        const int bytesRead = ReadFromFile(source.handle, buffer.data(), CopyBufferSize);
        if (bytesRead < 0 || !WriteToFile(destination.handle, buffer.data(), bytesRead)) {
            _ZN3WTF14FileSystemImpl10deleteFileERKNS_6StringE(linkPath);
            return false;
        }
        if (bytesRead == 0) return true;
    }
}

}  // namespace

extern "C" {

bool APS5_VABI _ZN3WTF14FileSystemImpl10fileExistsERKNS_6StringE(const String& path) {
    FileStat status{};
    return Status(path, status, true);
}

bool APS5_VABI _ZN3WTF14FileSystemImpl10deleteFileERKNS_6StringE(const String& path) {
    FileStat status{};
    if (!Status(path, status, false) || IsDirectory(status)) return false;
    const auto representation = FileSystemRepresentation(path);
    return unlink_nid_postfix(Data(representation)) == 0;
}

bool APS5_VABI _ZN3WTF14FileSystemImpl20deleteEmptyDirectoryERKNS_6StringE(const String& path) {
    FileStat status{};
    if (!Status(path, status, false) || !IsDirectory(status)) return false;
    const auto representation = FileSystemRepresentation(path);
    return rmdir_nid_postfix(Data(representation)) == 0;
}

bool APS5_VABI _ZN3WTF14FileSystemImpl8moveFileERKNS_6StringES3_(const String& oldPath, const String& newPath) {
    const auto oldFilename = FileSystemRepresentation(oldPath);
    if (!Data(oldFilename)) return false;
    const auto newFilename = FileSystemRepresentation(newPath);
    if (!Data(newFilename)) return false;
    return RenameGuestPath_nid_no_patch(Data(oldFilename), Data(newFilename)) != -1;
}

String APS5_VABI _ZN3WTF14FileSystemImpl8realPathERKNS_6StringE(const String& filePath) {
    const auto representation = FileSystemRepresentation(filePath);
    char resolvedName[PathMax];
    const char* result = realpath_nid_postfix(Data(representation), resolvedName);
    return result ? _ZN3WTF6String8fromUTF8EPKh(reinterpret_cast<const unsigned char*>(result)) : filePath;
}

bool APS5_VABI _ZN3WTF14FileSystemImpl19filesHaveSameVolumeERKNS_6StringES3_(const String& fileA, const String& fileB) {
    if (!fileA.impl || !fileB.impl) return false;
    FileStat statusA{};
    FileStat statusB{};
    if (!Status(fileA, statusA, true) || !Status(fileB, statusB, true)) return false;
    return statusA.st_dev == statusB.st_dev;
}

bool APS5_VABI _ZN3WTF14FileSystemImpl18hardLinkOrCopyFileERKNS_6StringES3_(const String& targetPath, const String& linkPath) {
    const auto target = FileSystemRepresentation(targetPath);
    const auto link = FileSystemRepresentation(linkPath);
    const char* targetData = Data(target);
    const char* linkData = Data(link);
    if (!targetData || !linkData) return false;
    if (link_nid_postfix(targetData, linkData) == 0) return true;
    return CopyFileContents(targetPath, linkPath);
}

}
