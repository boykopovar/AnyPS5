// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/WTF/wtf/StackBounds.cpp
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

namespace NKWebKit {
void FillRandomBytes(void* buffer, std::size_t bytes);
}  // namespace NKWebKit

namespace {
bool g_configPermanentlyFrozen = false;
}  // namespace

extern "C" {
int APS5_VABI _ZN3WTF22numberOfProcessorCoresEv() {
    unsigned int count = std::thread::hardware_concurrency();
    if (count == 0) throw std::runtime_error("numberOfProcessorCores: the host reports no processor count");
    return static_cast<int>(count);
}

std::uint64_t APS5_VABI _ZN3WTF7ramSizeEv() {
#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        return status.ullTotalPhys;
    }
#else
    const long pages = sysconf(_SC_PHYS_PAGES);
    const long pageBytes = sysconf(_SC_PAGE_SIZE);
    if (pages > 0 && pageBytes > 0) return static_cast<std::uint64_t>(pages) * static_cast<std::uint64_t>(pageBytes);
#endif
    throw std::runtime_error("ramSize: the host reports no physical memory size");
}

void APS5_VABI _ZN3WTF29cryptographicallyRandomValuesEPvm(void* buffer, std::size_t length) {
    NKWebKit::FillRandomBytes(buffer, length);
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
    return 4096;
}

void APS5_VABI _ZN3WTF6Config17permanentlyFreezeEv() {
    g_configPermanentlyFrozen = true;
}

int APS5_VABI _ZN3WTF18constantTimeMemcmpEPKvS1_m(const void* voidA, const void* voidB, std::size_t length) {
    const auto* a = static_cast<const std::uint8_t*>(voidA);
    const auto* b = static_cast<const std::uint8_t*>(voidB);
    std::uint8_t result = 0;
    for (std::size_t i = 0; i < length; ++i) {
        result |= static_cast<std::uint8_t>(a[i] ^ b[i]);
    }
    return result;
}

}

extern "C" {

bool APS5_VABI _ZN3WTF14FileSystemImpl14MappedFileData13mapFileHandleEiNS0_12FileOpenModeENS0_14MappedFileModeE(WTF::MappedFileData* self, int handle, int openMode, int mapMode) {
    if (handle == -1) return false;
    FileStat status{};
    if (_fstat_nid_postfix(handle, &status) != 0 || status.st_size < 0 ||
        static_cast<std::uint64_t>(status.st_size) > std::numeric_limits<unsigned>::max()) return false;
    const auto size = static_cast<unsigned>(status.st_size);
    if (!size) return true;
    const int protection = openMode == 1 ? 2 : openMode == 2 ? 3 : 1;
    void* data = mmap_nid_postfix(nullptr, size, protection, mapMode == 0 ? 1 : 2, handle, 0);
    if (data == reinterpret_cast<void*>(std::uintptr_t(-1))) return false;
    self->data = data;
    self->size = size;
    return true;
}

void APS5_VABI _ZN3WTF14FileSystemImpl14MappedFileDataC1ERKNS_6StringENS0_14MappedFileModeERb(WTF::MappedFileData* self, const NKWebKit::WtfAbi::String& path, int mapMode, bool& success) {
    *self = {};
    int handle = _ZN3WTF14FileSystemImpl8openFileERKNS_6StringENS0_12FileOpenModeENS0_20FileAccessPermissionEb(path, 0, true, false);
    success = _ZN3WTF14FileSystemImpl14MappedFileData13mapFileHandleEiNS0_12FileOpenModeENS0_14MappedFileModeE(self, handle, 0, mapMode);
    _ZN3WTF14FileSystemImpl9closeFileERi(handle);
}

void APS5_VABI _ZN3WTF14FileSystemImpl14MappedFileDataC1EiNS0_12FileOpenModeENS0_14MappedFileModeERb(WTF::MappedFileData* self, int handle, int openMode, int mapMode, bool& success) {
    *self = {};
    success = _ZN3WTF14FileSystemImpl14MappedFileData13mapFileHandleEiNS0_12FileOpenModeENS0_14MappedFileModeE(self, handle, openMode, mapMode);
}

void APS5_VABI _ZN3WTF14FileSystemImpl14MappedFileDataC1EiNS0_14MappedFileModeERb(WTF::MappedFileData* self, int handle, int mapMode, bool& success) {
    _ZN3WTF14FileSystemImpl14MappedFileDataC1EiNS0_12FileOpenModeENS0_14MappedFileModeERb(self, handle, 0, mapMode, success);
}

void APS5_VABI _ZN3WTF14FileSystemImpl14MappedFileDataC2ERKNS_6StringENS0_14MappedFileModeERb(WTF::MappedFileData* self, const NKWebKit::WtfAbi::String& path, int mapMode, bool& success) {
    _ZN3WTF14FileSystemImpl14MappedFileDataC1ERKNS_6StringENS0_14MappedFileModeERb(self, path, mapMode, success);
}

void APS5_VABI _ZN3WTF14FileSystemImpl14MappedFileDataD1Ev(WTF::MappedFileData* self) {
    if (self->data) munmap_nid_postfix(self->data, self->size);
}

void APS5_VABI _ZN3WTF14FileSystemImpl14MappedFileDataD2Ev(WTF::MappedFileData* self) {
    _ZN3WTF14FileSystemImpl14MappedFileDataD1Ev(self);
}

}

extern "C" {

int APS5_VABI _ZN3WTF14FileSystemImpl8openFileERKNS_6StringENS0_12FileOpenModeENS0_20FileAccessPermissionEb(const NKWebKit::WtfAbi::String& path, int mode, bool permission, bool failIfFileExists) {
    const auto representation = _ZNK3WTF6String4utf8ENS_14ConversionModeE(&path, 0);
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
    if (handle != -1) {
        close_nid_postfix(handle);
        handle = -1;
    }
}

long long APS5_VABI _ZN3WTF14FileSystemImpl8seekFileEixNS0_14FileSeekOriginE(int handle, long long offset, int origin) {
    const int whence = origin == 1 ? 1 : origin == 2 ? 2 : 0;
    return lseek_nid_postfix(handle, offset, whence);
}

bool APS5_VABI _ZN3WTF14FileSystemImpl12truncateFileEix(int handle, long long offset) {
    return ftruncate_nid_postfix(handle, offset) == 0;
}

int APS5_VABI _ZN3WTF14FileSystemImpl15openAndLockFileERKNS_6StringENS0_12FileOpenModeENS_9OptionSetINS0_12FileLockModeEEE(const NKWebKit::WtfAbi::String& path, int mode, unsigned lockMode) {
    return _ZN3WTF14FileSystemImpl8openFileERKNS_6StringENS0_12FileOpenModeENS0_20FileAccessPermissionEb(path, mode, true, false);
}

void APS5_VABI _ZN3WTF14FileSystemImpl18unlockAndCloseFileEi(int handle) {
    _ZN3WTF14FileSystemImpl9closeFileERi(handle);
}

}
