// Derived from Source/WebCore/platform/network/BlobDataFileReference.cpp@e12a212a531aeddc11259e8515671cf5188e038d
// Derived from Source/WTF/wtf/FileSystem.cpp@e12a212a531aeddc11259e8515671cf5188e038d
/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestFilesystemPrimitives.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceNKWebKit/include/BlobDataFileReferenceExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using NKWebKit::WebCoreAbi::BlobDataFileReference;
using NKWebKit::WebCoreAbi::BlobDataFileReferenceRef;
using NKWebKit::WebCoreAbi::OptionalWallTime;
using NKWebKit::WtfAbi::CString;
using NKWebKit::WtfAbi::String;

struct FileMetadata {
    double modificationTime;
    long long length;
};

BlobDataFileReference& Checked(BlobDataFileReference* self, const char* caller) {
    if (!self) throw std::invalid_argument(std::string("WebCore::BlobDataFileReference::") + caller + ": null this");
    return *self;
}

const String& CheckedPath(const String* path, const char* caller) {
    if (!path) throw std::invalid_argument(std::string("WebCore::BlobDataFileReference::") + caller + ": null path");
    return *path;
}

bool IsNull(const String& string) {
    return !string.impl;
}

bool IsEmpty(const String& string) {
    return !string.impl || !string.impl->length;
}

CString FileSystemRepresentation(const String& path) {
    return _ZNK3WTF6String4utf8ENS_14ConversionModeE(&path, 0);
}

const char* Data(const CString& representation) {
    return representation.buffer ? reinterpret_cast<const char*>(representation.buffer + 1) : nullptr;
}

double ModificationSeconds(const KernelTimespec& time) {
    auto seconds = static_cast<std::int64_t>(time.tv_sec);
    if (seconds < 0 && time.tv_nsec > 0) ++seconds;
    return static_cast<double>(seconds);
}

std::optional<FileMetadata> FileMetadataFollowingSymlinks(const String& path) {
    if (IsEmpty(path)) return std::nullopt;
    const auto representation = FileSystemRepresentation(path);
    const char* data = Data(representation);
    if (!data) return std::nullopt;
    FileStat status{};
    if (stat_nid_postfix(data, &status) != 0) return std::nullopt;
    const bool regular = GuestFilesystem::TypeOfMode(status.st_mode) == GuestFilesystem::TypeRegular;
    return FileMetadata{ModificationSeconds(status.st_mtim), regular ? static_cast<long long>(status.st_size) : 0};
}

bool DeleteFile(const String& path) {
    const auto representation = FileSystemRepresentation(path);
    const char* data = Data(representation);
    if (!data) return false;
    FileStat status{};
    if (lstat_nid_postfix(data, &status) != 0) return false;
    if (GuestFilesystem::TypeOfMode(status.st_mode) == GuestFilesystem::TypeDirectory) return false;
    return unlink_nid_postfix(data) == 0;
}

const void* const Vtable[] = {
    nullptr,
    nullptr,
    reinterpret_cast<const void*>(&_ZN7WebCore21BlobDataFileReferenceD1Ev),
    reinterpret_cast<const void*>(&_ZN7WebCore21BlobDataFileReferenceD0Ev),
    reinterpret_cast<const void*>(&_ZN7WebCore21BlobDataFileReference20prepareForFileAccessEv),
    reinterpret_cast<const void*>(&_ZN7WebCore21BlobDataFileReference16revokeFileAccessEv),
};

void Construct(BlobDataFileReference* self, const String* path, const String* replacementPath) {
    auto& object = Checked(self, "BlobDataFileReference");
    const auto& source = CheckedPath(path, "BlobDataFileReference");
    const auto& replacement = CheckedPath(replacementPath, "BlobDataFileReference");
    object.vtable = &Vtable[2];
    object.refCount = 1;
    new (&object.path) String(source);
    new (&object.replacementPath) String(replacement);
    object.size = 0;
    object.expectedModificationTime = std::numeric_limits<double>::quiet_NaN();
}

}  // namespace

extern "C" {

void APS5_VABI _ZN7WebCore21BlobDataFileReferenceC1ERKN3WTF6StringES4_(BlobDataFileReference* self, const String* path, const String* replacementPath) {
    Construct(self, path, replacementPath);
}

void APS5_VABI _ZN7WebCore21BlobDataFileReferenceC2ERKN3WTF6StringES4_(BlobDataFileReference* self, const String* path, const String* replacementPath) {
    Construct(self, path, replacementPath);
}

void APS5_VABI _ZN7WebCore21BlobDataFileReferenceD1Ev(BlobDataFileReference* self) {
    auto& object = Checked(self, "~BlobDataFileReference");
    object.vtable = &Vtable[2];
    if (!IsNull(object.replacementPath)) DeleteFile(object.replacementPath);
    object.replacementPath.~String();
    object.path.~String();
}

void APS5_VABI _ZN7WebCore21BlobDataFileReferenceD2Ev(BlobDataFileReference* self) {
    _ZN7WebCore21BlobDataFileReferenceD1Ev(self);
}

void APS5_VABI _ZN7WebCore21BlobDataFileReferenceD0Ev(BlobDataFileReference* self) {
    _ZN7WebCore21BlobDataFileReferenceD1Ev(self);
    _ZdlPv_nid_postfix(self);
}

BlobDataFileReferenceRef APS5_VABI _ZN7WebCore21BlobDataFileReference6createERKN3WTF6StringES4_(const String* path, const String* replacementPath) {
    CheckedPath(path, "create");
    CheckedPath(replacementPath, "create");
    auto* object = static_cast<BlobDataFileReference*>(_Znwm_nid_postfix(sizeof(BlobDataFileReference)));
    Construct(object, path, replacementPath);
    return BlobDataFileReferenceRef(object);
}

const String* APS5_VABI _ZN7WebCore21BlobDataFileReference4pathEv(BlobDataFileReference* self) {
    auto& object = Checked(self, "path");
    return IsNull(object.replacementPath) ? &object.path : &object.replacementPath;
}

OptionalWallTime APS5_VABI _ZN7WebCore21BlobDataFileReference24expectedModificationTimeEv(BlobDataFileReference* self) {
    const double time = Checked(self, "expectedModificationTime").expectedModificationTime;
    if (std::isnan(time)) return OptionalWallTime();
    return OptionalWallTime(time);
}

void APS5_VABI _ZN7WebCore21BlobDataFileReference26startTrackingModificationsEv(BlobDataFileReference* self) {
    auto& object = Checked(self, "startTrackingModifications");
    auto metadata = FileMetadataFollowingSymlinks(object.path);
    if (!metadata) return;
    object.expectedModificationTime = metadata->modificationTime;
    if (!IsNull(object.replacementPath)) {
        metadata = FileMetadataFollowingSymlinks(object.replacementPath);
        if (!metadata) return;
    }
    object.size = static_cast<std::uint64_t>(metadata->length);
}

void APS5_VABI _ZN7WebCore21BlobDataFileReference20prepareForFileAccessEv(BlobDataFileReference* self) {
    Checked(self, "prepareForFileAccess");
}

void APS5_VABI _ZN7WebCore21BlobDataFileReference16revokeFileAccessEv(BlobDataFileReference* self) {
    Checked(self, "revokeFileAccess");
}

}
