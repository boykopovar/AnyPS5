// Derived from Source/WTF/wtf/StackBounds.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 *  Copyright (C) 2003-2019 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include <stdexcept>
#include <string>
#ifndef _WIN32
#include <unistd.h>
#endif

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <algorithm>
#include <type_traits>
#include <vector>

namespace NKWebKit {
void FillRandomBytes(void* buffer, std::size_t bytes);
std::size_t PageSize();
int NumberOfProcessorCores();
std::size_t RamSize();
}  // namespace NKWebKit

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

static_assert(std::is_trivially_copyable_v<StringImpl>);
static_assert(std::is_trivially_copyable_v<WTF::MappedFileData>);

bool g_configPermanentlyFrozen = false;

template<typename T>
T ReadGuestValue(const T* pointer, const char* function) {
    T value{};
    if (!GuestCopyIn_nid_no_patch(&value, pointer, sizeof(T)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

void RequireReadableGuestBytes(const void* pointer, std::size_t size, const char* function) {
    if (!size) return;
    if (!pointer) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(pointer, size);
    if (!GuestReadable_nid_no_patch(pointer, size))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

void RequireWritableGuestBytes(void* pointer, std::size_t size, const char* function) {
    if (!size) return;
    if (!pointer) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    bool ok = false;
    {
        const GuestArena::HostWrite guard(pointer, size);
        ok = guard.Open() && GuestWritable_nid_no_patch(pointer, size);
    }
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

template<typename T>
bool TryPublishGuestValue(T* pointer, const T& value, const T& previous) {
    if (GuestCopyOut_nid_no_patch(pointer, &value, sizeof(T))) return true;
    static_cast<void>(GuestCopyOut_nid_no_patch(pointer, &previous, sizeof(T)));
    return false;
}

String CopyGuestStringToHost(const String* source, const char* function) {
    StringImpl* const impl = ReadGuestValue(reinterpret_cast<StringImpl* const*>(source), function);
    if (!impl) return String{};
    const StringImpl header = ReadGuestValue(impl, function);
    if (!header.length) return String(NKWebKit::WtfAbi::EmptyStringImpl());
    if (!header.data) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    const bool is8Bit = (header.hashAndFlags & 4u) != 0;
    const std::size_t byteLength = static_cast<std::size_t>(header.length) * (is8Bit ? 1u : sizeof(char16_t));
    RequireReadableGuestBytes(header.data, byteLength, function);
    if (is8Bit) {
        std::vector<unsigned char> narrow(header.length);
        if (!GuestCopyIn_nid_no_patch(narrow.data(), header.data, byteLength))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        return _ZN3WTF10StringImpl6createEPKhj(narrow.data(), header.length);
    }
    std::vector<char16_t> wide(header.length);
    if (!GuestCopyIn_nid_no_patch(wide.data(), header.data, byteLength))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return _ZN3WTF10StringImpl6createEPKDsj(wide.data(), header.length);
}

struct MappedDataGuard {
    WTF::MappedFileData mapped{};

    ~MappedDataGuard() {
        if (mapped.data) {
            try {
                munmap_nid_postfix(mapped.data, mapped.size);
            } catch (...) {
            }
        }
    }
};

void PublishMappedResult(WTF::MappedFileData* self, const WTF::MappedFileData& previousSelf, MappedDataGuard& mapped, bool* success, bool ok, bool previousSuccess, const char* function) {
    if (!TryPublishGuestValue(self, mapped.mapped, previousSelf))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (!TryPublishGuestValue(success, ok, previousSuccess)) {
        const bool restoredSelf = GuestCopyOut_nid_no_patch(self, &previousSelf, sizeof(previousSelf));
        if (!restoredSelf) mapped.mapped = {};
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    }
    mapped.mapped = {};
}

}  // namespace

extern "C" {
int APS5_VABI _ZN3WTF22numberOfProcessorCoresEv() {
    return NKWebKit::NumberOfProcessorCores();
}

std::uint64_t APS5_VABI _ZN3WTF7ramSizeEv() {
    return NKWebKit::RamSize();
}

void APS5_VABI _ZN3WTF29cryptographicallyRandomValuesEPvm(void* buffer, std::size_t length) {
    if (!buffer || length == 0) return;
    RequireWritableGuestBytes(buffer, length, __func__);
    std::uint8_t chunk[4096];
    auto* const destination = static_cast<std::uint8_t*>(buffer);
    std::size_t offset = 0;
    while (offset < length) {
        const std::size_t step = std::min<std::size_t>(sizeof(chunk), length - offset);
        NKWebKit::FillRandomBytes(chunk, step);
        if (!GuestCopyOut_nid_no_patch(destination + offset, chunk, step))
            NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        offset += step;
    }
}

WTF::StackBounds APS5_VABI _ZN3WTF11StackBounds32currentThreadStackBoundsInternalEv() {
    PthreadAttr attr = nullptr;
    if (const int error = pthread_attr_init_nid_postfix(&attr))
        throw std::runtime_error("currentThreadStackBoundsInternal: pthread_attr_init failed with " + std::to_string(error));
    void* bound = nullptr;
    std::size_t stackSize = 0;
    int error = pthread_attr_get_np_nid_postfix(pthread_self_nid_postfix(), &attr);
    if (!error) error = pthread_attr_getstack_nid_postfix(&attr, &bound, &stackSize);
    pthread_attr_destroy_nid_postfix(&attr);
    if (error)
        throw std::runtime_error("currentThreadStackBoundsInternal: reading the thread stack failed with " + std::to_string(error));
    if (!bound || !stackSize) throw std::runtime_error("currentThreadStackBoundsInternal: the thread has no stack record");
    return WTF::StackBounds{static_cast<char*>(bound) + stackSize, bound};
}

std::size_t APS5_VABI _ZN3WTF8pageSizeEv() {
    return NKWebKit::PageSize();
}

void APS5_VABI _ZN3WTF6Config17permanentlyFreezeEv() {
    g_configPermanentlyFrozen = true;
}

int APS5_VABI _ZN3WTF18constantTimeMemcmpEPKvS1_m(const void* voidA, const void* voidB, std::size_t length) {
    if (length == 0) return 0;
    RequireReadableGuestBytes(voidA, length, __func__);
    RequireReadableGuestBytes(voidB, length, __func__);
    std::uint8_t bufA[4096];
    std::uint8_t bufB[4096];
    const auto* const a = static_cast<const std::uint8_t*>(voidA);
    const auto* const b = static_cast<const std::uint8_t*>(voidB);
    std::uint8_t result = 0;
    std::size_t offset = 0;
    while (offset < length) {
        const std::size_t step = std::min<std::size_t>(sizeof(bufA), length - offset);
        if (!GuestCopyIn_nid_no_patch(bufA, a + offset, step) || !GuestCopyIn_nid_no_patch(bufB, b + offset, step))
            NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        for (std::size_t i = 0; i < step; ++i) {
            result |= static_cast<std::uint8_t>(bufA[i] ^ bufB[i]);
        }
        offset += step;
    }
    return result;
}

}

extern "C" {

bool APS5_VABI _ZN3WTF14FileSystemImpl14MappedFileData13mapFileHandleEiNS0_12FileOpenModeENS0_14MappedFileModeE(WTF::MappedFileData* self, int handle, int openMode, int mapMode) {
    if (!self) throw std::invalid_argument("WTF::FileSystemImpl::MappedFileData::mapFileHandle: null this");
    RequireReadableGuestBytes(self, sizeof(WTF::MappedFileData), __func__);
    RequireWritableGuestBytes(self, sizeof(WTF::MappedFileData), __func__);
    const WTF::MappedFileData previous = ReadGuestValue(self, __func__);
    if (handle == -1) return false;
    FileStat status{};
    if (_fstat_nid_postfix(handle, &status) != 0 || status.st_size < 0 ||
        static_cast<std::uint64_t>(status.st_size) > std::numeric_limits<unsigned>::max()) return false;
    const auto size = static_cast<unsigned>(status.st_size);
    if (!size) return true;
    const int protection = openMode == 1 ? 2 : openMode == 2 ? 3 : 1;
    void* data = mmap_nid_postfix(nullptr, size, protection, mapMode == 0 ? 1 : 2, handle, 0);
    if (data == reinterpret_cast<void*>(std::uintptr_t(-1))) return false;
    const WTF::MappedFileData updated{data, size};
    if (!TryPublishGuestValue(self, updated, previous)) {
        munmap_nid_postfix(data, size);
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    return true;
}

void APS5_VABI _ZN3WTF14FileSystemImpl14MappedFileDataC1ERKNS_6StringENS0_14MappedFileModeERb(WTF::MappedFileData* self, const NKWebKit::WtfAbi::String& path, int mapMode, bool& success) {
    if (!self) throw std::invalid_argument("WTF::FileSystemImpl::MappedFileData::MappedFileData: null this");
    RequireReadableGuestBytes(self, sizeof(WTF::MappedFileData), __func__);
    RequireWritableGuestBytes(self, sizeof(WTF::MappedFileData), __func__);
    RequireReadableGuestBytes(&success, sizeof(bool), __func__);
    RequireWritableGuestBytes(&success, sizeof(bool), __func__);
    const WTF::MappedFileData previousSelf = ReadGuestValue(self, __func__);
    const bool previousSuccess = ReadGuestValue(&success, __func__);
    const String hostPath = CopyGuestStringToHost(&path, __func__);
    int handle = _ZN3WTF14FileSystemImpl8openFileERKNS_6StringENS0_12FileOpenModeENS0_20FileAccessPermissionEb(hostPath, 0, true, false);
    MappedDataGuard guard{};
    const bool ok = _ZN3WTF14FileSystemImpl14MappedFileData13mapFileHandleEiNS0_12FileOpenModeENS0_14MappedFileModeE(&guard.mapped, handle, 0, mapMode);
    _ZN3WTF14FileSystemImpl9closeFileERi(handle);
    PublishMappedResult(self, previousSelf, guard, &success, ok, previousSuccess, __func__);
}

void APS5_VABI _ZN3WTF14FileSystemImpl14MappedFileDataC1EiNS0_12FileOpenModeENS0_14MappedFileModeERb(WTF::MappedFileData* self, int handle, int openMode, int mapMode, bool& success) {
    if (!self) throw std::invalid_argument("WTF::FileSystemImpl::MappedFileData::MappedFileData: null this");
    RequireReadableGuestBytes(self, sizeof(WTF::MappedFileData), __func__);
    RequireWritableGuestBytes(self, sizeof(WTF::MappedFileData), __func__);
    RequireReadableGuestBytes(&success, sizeof(bool), __func__);
    RequireWritableGuestBytes(&success, sizeof(bool), __func__);
    const WTF::MappedFileData previousSelf = ReadGuestValue(self, __func__);
    const bool previousSuccess = ReadGuestValue(&success, __func__);
    MappedDataGuard guard{};
    const bool ok = _ZN3WTF14FileSystemImpl14MappedFileData13mapFileHandleEiNS0_12FileOpenModeENS0_14MappedFileModeE(&guard.mapped, handle, openMode, mapMode);
    PublishMappedResult(self, previousSelf, guard, &success, ok, previousSuccess, __func__);
}

void APS5_VABI _ZN3WTF14FileSystemImpl14MappedFileDataC1EiNS0_14MappedFileModeERb(WTF::MappedFileData* self, int handle, int mapMode, bool& success) {
    _ZN3WTF14FileSystemImpl14MappedFileDataC1EiNS0_12FileOpenModeENS0_14MappedFileModeERb(self, handle, 0, mapMode, success);
}

void APS5_VABI _ZN3WTF14FileSystemImpl14MappedFileDataC2ERKNS_6StringENS0_14MappedFileModeERb(WTF::MappedFileData* self, const NKWebKit::WtfAbi::String& path, int mapMode, bool& success) {
    _ZN3WTF14FileSystemImpl14MappedFileDataC1ERKNS_6StringENS0_14MappedFileModeERb(self, path, mapMode, success);
}

void APS5_VABI _ZN3WTF14FileSystemImpl14MappedFileDataD1Ev(WTF::MappedFileData* self) {
    if (!self) throw std::invalid_argument("WTF::FileSystemImpl::MappedFileData::~MappedFileData: null this");
    const WTF::MappedFileData local = ReadGuestValue(self, __func__);
    if (local.data) munmap_nid_postfix(local.data, local.size);
}

void APS5_VABI _ZN3WTF14FileSystemImpl14MappedFileDataD2Ev(WTF::MappedFileData* self) {
    _ZN3WTF14FileSystemImpl14MappedFileDataD1Ev(self);
}

}

extern "C" {

int APS5_VABI _ZN3WTF14FileSystemImpl8openFileERKNS_6StringENS0_12FileOpenModeENS0_20FileAccessPermissionEb(const NKWebKit::WtfAbi::String& path, int mode, bool permission, bool failIfFileExists) {
    const String hostPath = CopyGuestStringToHost(&path, __func__);
    const auto representation = _ZNK3WTF6String4utf8ENS_14ConversionModeE(&hostPath, 0);
    if (!representation.buffer) return -1;
    int flags = SCE_KERNEL_O_RDONLY;
    switch (mode) {
    case 1: flags = SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_TRUNC; break;
    case 2: flags = SCE_KERNEL_O_RDWR | SCE_KERNEL_O_CREAT; break;
    }
    if (failIfFileExists) flags |= SCE_KERNEL_O_CREAT | SCE_KERNEL_O_EXCL;
    return open_nid_postfix(reinterpret_cast<const char*>(representation.buffer + 1), flags, permission ? 0666 : 0600);
}

void APS5_VABI _ZN3WTF14FileSystemImpl9closeFileERi(int& handle) {
    RequireReadableGuestBytes(&handle, sizeof(int), __func__);
    RequireWritableGuestBytes(&handle, sizeof(int), __func__);
    const int previous = ReadGuestValue(&handle, __func__);
    if (previous != -1) {
        const int closed = -1;
        if (!TryPublishGuestValue(&handle, closed, previous))
            NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        close_nid_postfix(previous);
    }
}

long long APS5_VABI _ZN3WTF14FileSystemImpl8seekFileEixNS0_14FileSeekOriginE(int handle, long long offset, int origin) {
    const int whence = origin == 1 ? 1 : origin == 2 ? 2 : 0;
    return lseek_nid_postfix(handle, offset, whence);
}

bool APS5_VABI _ZN3WTF14FileSystemImpl12truncateFileEix(int handle, long long offset) {
    return ftruncate_nid_postfix(handle, offset) == 0;
}

bool APS5_VABI _ZN3WTF14FileSystemImpl18makeAllDirectoriesERKNS_6StringE(const NKWebKit::WtfAbi::String& path) {
    const String hostPath = CopyGuestStringToHost(&path, __func__);
    const auto representation = _ZNK3WTF6String4utf8ENS_14ConversionModeE(&hostPath, 0);
    if (!representation.buffer) return false;
    const std::string full(reinterpret_cast<const char*>(representation.buffer + 1));
    if (full.empty()) return false;
    FileStat status{};
    const auto exists = [&status](const std::string& candidate) { return stat_nid_postfix(candidate.c_str(), &status) == 0; };
    if (exists(full)) return true;
    for (std::size_t index = 1; index < full.size(); ++index) {
        if (full[index] != '/') continue;
        const std::string prefix = full.substr(0, index);
        if (!exists(prefix) && mkdir_nid_postfix(prefix.c_str(), 0700) != 0) return false;
    }
    return exists(full) || mkdir_nid_postfix(full.c_str(), 0700) == 0;
}

int APS5_VABI _ZN3WTF14FileSystemImpl15openAndLockFileERKNS_6StringENS0_12FileOpenModeENS_9OptionSetINS0_12FileLockModeEEE(const NKWebKit::WtfAbi::String& path, int mode, unsigned lockMode) {
    (void)lockMode;
    return _ZN3WTF14FileSystemImpl8openFileERKNS_6StringENS0_12FileOpenModeENS0_20FileAccessPermissionEb(path, mode, true, false);
}

void APS5_VABI _ZN3WTF14FileSystemImpl18unlockAndCloseFileEi(int handle) {
    _ZN3WTF14FileSystemImpl9closeFileERi(handle);
}

}
