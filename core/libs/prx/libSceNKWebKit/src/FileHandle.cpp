// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/FileHandle.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/FileHandle.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceNKWebKit/include/FileHandleExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdarg>
#include <new>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using NKWebKit::WebCoreAbi::FileHandle;
using NKWebKit::WtfAbi::String;

constexpr int InvalidHandle = -1;
constexpr int GuestEINTR = 4;
constexpr int GuestEINVAL = 22;
constexpr bool FileAccessPermissionAll = true;

FileHandle& Checked(FileHandle* self, const char* operation) {
    if (!self) throw std::invalid_argument(std::string("WebCore::FileHandle::") + operation + ": null this");
    return *self;
}

const String& CheckedPath(const String* path, const char* operation) {
    if (!path) throw std::invalid_argument(std::string("WebCore::FileHandle::") + operation + ": null path");
    return *path;
}

bool IsValid(const FileHandle& handle) {
    return handle.fileHandle != InvalidHandle;
}

void Construct(FileHandle* self, const String* path, int mode, unsigned lockMode, bool shouldLock) {
    auto& handle = Checked(self, "FileHandle");
    String copied = path ? *path : String();
    new (&handle.path) String(std::move(copied));
    handle.mode = mode;
    handle.fileHandle = InvalidHandle;
    handle.lockMode = lockMode;
    handle.shouldLock = shouldLock;
}

void ConstructMoved(FileHandle* self, FileHandle* other) {
    auto& source = Checked(other, "FileHandle");
    auto& handle = Checked(self, "FileHandle");
    new (&handle.path) String(std::move(source.path));
    handle.mode = source.mode;
    handle.fileHandle = std::exchange(source.fileHandle, InvalidHandle);
    handle.lockMode = 0;
    handle.shouldLock = false;
}

void Close(FileHandle& handle) {
    if (handle.shouldLock && IsValid(handle)) {
        _ZN3WTF14FileSystemImpl18unlockAndCloseFileEi(handle.fileHandle);
        return;
    }
    _ZN3WTF14FileSystemImpl9closeFileERi(handle.fileHandle);
}

bool Open(FileHandle& handle) {
    if (!handle.path.impl || !handle.path.impl->length) return false;
    if (!IsValid(handle)) {
        handle.fileHandle = handle.shouldLock
            ? _ZN3WTF14FileSystemImpl15openAndLockFileERKNS_6StringENS0_12FileOpenModeENS_9OptionSetINS0_12FileLockModeEEE(handle.path, handle.mode, handle.lockMode)
            : _ZN3WTF14FileSystemImpl8openFileERKNS_6StringENS0_12FileOpenModeENS0_20FileAccessPermissionEb(handle.path, handle.mode, FileAccessPermissionAll, false);
    }
    return IsValid(handle);
}

template <typename Transfer>
int Retry(Transfer transfer) {
    do {
        const auto transferred = transfer();
        if (transferred >= 0) return static_cast<int>(transferred);
    } while (*__error_nid_postfix() == GuestEINTR);
    return -1;
}

int RejectNegativeLength() {
    *__error_nid_postfix() = GuestEINVAL;
    return -1;
}

int Write(FileHandle& handle, const void* data, int length) {
    if (!Open(handle)) return -1;
    if (length < 0) return RejectNegativeLength();
    return Retry([&] { return write_nid_postfix(handle.fileHandle, static_cast<const char*>(data), static_cast<std::int64_t>(static_cast<std::size_t>(length))); });
}

}  // namespace

extern "C" {

void APS5_VABI _ZN7WebCore10FileHandleC1Ev(FileHandle* self) {
    Construct(self, nullptr, 0, 0, false);
}

void APS5_VABI _ZN7WebCore10FileHandleC2Ev(FileHandle* self) {
    Construct(self, nullptr, 0, 0, false);
}

void APS5_VABI _ZN7WebCore10FileHandleC1ERKN3WTF6StringENS1_14FileSystemImpl12FileOpenModeE(FileHandle* self, const String* path, int mode) {
    Construct(self, &CheckedPath(path, "FileHandle"), mode, 0, false);
}

void APS5_VABI _ZN7WebCore10FileHandleC2ERKN3WTF6StringENS1_14FileSystemImpl12FileOpenModeE(FileHandle* self, const String* path, int mode) {
    Construct(self, &CheckedPath(path, "FileHandle"), mode, 0, false);
}

void APS5_VABI _ZN7WebCore10FileHandleC1ERKN3WTF6StringENS1_14FileSystemImpl12FileOpenModeENS1_9OptionSetINS5_12FileLockModeEEE(FileHandle* self, const String* path, int mode, unsigned lockMode) {
    Construct(self, &CheckedPath(path, "FileHandle"), mode, lockMode, true);
}

void APS5_VABI _ZN7WebCore10FileHandleC2ERKN3WTF6StringENS1_14FileSystemImpl12FileOpenModeENS1_9OptionSetINS5_12FileLockModeEEE(FileHandle* self, const String* path, int mode, unsigned lockMode) {
    Construct(self, &CheckedPath(path, "FileHandle"), mode, lockMode, true);
}

void APS5_VABI _ZN7WebCore10FileHandleC1EOS0_(FileHandle* self, FileHandle* other) {
    ConstructMoved(self, other);
}

void APS5_VABI _ZN7WebCore10FileHandleC2EOS0_(FileHandle* self, FileHandle* other) {
    ConstructMoved(self, other);
}

void APS5_VABI _ZN7WebCore10FileHandleD1Ev(FileHandle* self) {
    auto& handle = Checked(self, "~FileHandle");
    Close(handle);
    handle.path.~String();
}

void APS5_VABI _ZN7WebCore10FileHandleD2Ev(FileHandle* self) {
    _ZN7WebCore10FileHandleD1Ev(self);
}

FileHandle& APS5_VABI _ZN7WebCore10FileHandleaSEOS0_(FileHandle* self, FileHandle* other) {
    auto& handle = Checked(self, "operator=");
    auto& source = Checked(other, "operator=");
    Close(handle);
    handle.path = std::move(source.path);
    handle.mode = source.mode;
    handle.fileHandle = std::exchange(source.fileHandle, InvalidHandle);
    handle.shouldLock = source.shouldLock;
    handle.lockMode = source.lockMode;
    return handle;
}

bool APS5_VABI _ZNK7WebCore10FileHandlecvbEv(const FileHandle* self) {
    return IsValid(Checked(const_cast<FileHandle*>(self), "operator bool"));
}

bool APS5_VABI _ZN7WebCore10FileHandle4openERKN3WTF6StringENS1_14FileSystemImpl12FileOpenModeE(FileHandle* self, const String* path, int mode) {
    auto& handle = Checked(self, "open");
    const auto& requested = CheckedPath(path, "open");
    if (IsValid(handle) && _ZN3WTF5equalEPKNS_10StringImplES2_(requested.impl, handle.path.impl) && mode == handle.mode) return true;
    Close(handle);
    handle.path = requested;
    handle.mode = mode;
    return Open(handle);
}

bool APS5_VABI _ZN7WebCore10FileHandle4openEv(FileHandle* self) {
    return Open(Checked(self, "open"));
}

int APS5_VABI _ZN7WebCore10FileHandle4readEPvi(FileHandle* self, void* data, int length) {
    auto& handle = Checked(self, "read");
    if (!Open(handle)) return -1;
    if (length < 0) return RejectNegativeLength();
    return Retry([&] { return read_nid_postfix(handle.fileHandle, data, static_cast<std::uint64_t>(static_cast<std::size_t>(length))); });
}

int APS5_VABI _ZN7WebCore10FileHandle5writeEPKvi(FileHandle* self, const void* data, int length) {
    return Write(Checked(self, "write"), data, length);
}

bool APS5_VABI _ZN7WebCore10FileHandle6printfEPKcz(FileHandle* self, const char* format, ...) {
    auto& handle = Checked(self, "printf");
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    __builtin_sysv_va_list preflight;
    __builtin_sysv_va_copy(preflight, args);
#else
    std::va_list args;
    va_start(args, format);
    std::va_list preflight;
    va_copy(preflight, args);
#endif
    const int length = vsnprintf_nid_postfix(nullptr, 0, format, reinterpret_cast<VaList*>(preflight));
    std::vector<char> buffer;
    if (length >= 0) {
        buffer.resize(static_cast<std::size_t>(length) + 1);
        vsnprintf_nid_postfix(buffer.data(), buffer.size(), format, reinterpret_cast<VaList*>(args));
    }
#ifdef _WIN32
    __builtin_sysv_va_end(preflight);
    __builtin_sysv_va_end(args);
#else
    va_end(preflight);
    va_end(args);
#endif
    if (length < 0) return false;
    return Write(handle, buffer.data(), length) >= 0;
}

void APS5_VABI _ZN7WebCore10FileHandle5closeEv(FileHandle* self) {
    Close(Checked(self, "close"));
}

}
