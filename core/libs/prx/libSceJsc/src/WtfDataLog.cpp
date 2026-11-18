/*
 * Derived from Source/WTF/wtf/DataLog.cpp, FilePrintStream.cpp, LockedPrintStream.cpp and PrintStream.cpp
 * at https://github.com/neonmodder123/PS5-WebKit-11.x/tree/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf
 *
 * DataLog.cpp:
 * Copyright (C) 2012-2022 Apple Inc. All rights reserved.
 *
 * FilePrintStream.cpp:
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * LockedPrintStream.cpp:
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 * PrintStream.cpp:
 * Copyright (C) 2012-2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceJsc/include/WtfDataLogExports.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"

#include <atomic>
#include <mutex>
#include <new>
#include <stdexcept>

namespace {

using Jsc::WtfAbi::FilePrintStream;
using Jsc::WtfAbi::PrintStream;

struct LockedPrintStream {
    const void* const* vtable;
    std::recursive_mutex lock;
    PrintStream* target;
};

using Vprintf = void (APS5_VABI*)(PrintStream*, const char*, VaList*);
using Flush = void (APS5_VABI*)(PrintStream*);

template <typename TFunction>
TFunction Slot(PrintStream* stream, Jsc::WtfAbi::PrintStreamSlot slot) {
    if (!stream || !stream->vtable) throw std::invalid_argument("WTF::PrintStream: null stream or vtable");
    const void* entry = stream->vtable[slot];
    if (!entry) throw std::invalid_argument("WTF::PrintStream: null virtual function");
    return reinterpret_cast<TFunction>(const_cast<void*>(entry));
}

FilePrintStream* RequireFile(FilePrintStream* stream) {
    if (!stream) throw std::invalid_argument("WTF::FilePrintStream: null stream");
    return stream;
}

LockedPrintStream* RequireLocked(PrintStream* stream) {
    if (!stream) throw std::invalid_argument("WTF::LockedPrintStream: null stream");
    auto* locked = reinterpret_cast<LockedPrintStream*>(stream);
    if (!locked->target) throw std::logic_error("WTF::LockedPrintStream: null target");
    return locked;
}

void APS5_VABI FilePrintStreamCompleteDestructor_nid_no_patch(PrintStream* stream) {
    WtfFilePrintStreamDestroy_nid_no_patch(reinterpret_cast<FilePrintStream*>(stream));
}

void APS5_VABI FilePrintStreamDeletingDestructor_nid_no_patch(PrintStream* stream) {
    WtfFilePrintStreamDelete_nid_no_patch(reinterpret_cast<FilePrintStream*>(stream));
}

void APS5_VABI FilePrintStreamVprintf_nid_no_patch(PrintStream* stream, const char* format, VaList* args) {
    vfprintf_nid_postfix(RequireFile(reinterpret_cast<FilePrintStream*>(stream))->file, format, args);
}

void APS5_VABI FilePrintStreamFlush_nid_no_patch(PrintStream* stream) {
    fflush_nid_postfix(RequireFile(reinterpret_cast<FilePrintStream*>(stream))->file);
}

PrintStream* APS5_VABI PrintStreamBegin_nid_no_patch(PrintStream* stream) {
    return stream;
}

void APS5_VABI PrintStreamEnd_nid_no_patch(PrintStream*) {
}

[[noreturn]] void ThrowStaticLockedStreamDestroyed() {
    throw std::logic_error("WTF::LockedPrintStream: the data log stream lives in static storage and cannot be destroyed");
}

void APS5_VABI LockedPrintStreamCompleteDestructor_nid_no_patch(PrintStream*) {
    ThrowStaticLockedStreamDestroyed();
}

void APS5_VABI LockedPrintStreamDeletingDestructor_nid_no_patch(PrintStream*) {
    ThrowStaticLockedStreamDestroyed();
}

void APS5_VABI LockedPrintStreamVprintf_nid_no_patch(PrintStream* stream, const char* format, VaList* args) {
    auto* locked = RequireLocked(stream);
    std::lock_guard guard(locked->lock);
    Slot<Vprintf>(locked->target, Jsc::WtfAbi::PrintStreamVprintf)(locked->target, format, args);
}

void APS5_VABI LockedPrintStreamFlush_nid_no_patch(PrintStream* stream) {
    auto* locked = RequireLocked(stream);
    std::lock_guard guard(locked->lock);
    Slot<Flush>(locked->target, Jsc::WtfAbi::PrintStreamFlush)(locked->target);
}

PrintStream* APS5_VABI LockedPrintStreamBegin_nid_no_patch(PrintStream* stream) {
    auto* locked = RequireLocked(stream);
    locked->lock.lock();
    return locked->target;
}

void APS5_VABI LockedPrintStreamEnd_nid_no_patch(PrintStream* stream) {
    RequireLocked(stream)->lock.unlock();
}

const void* const FilePrintStreamVtable[] = {
    nullptr,
    nullptr,
    reinterpret_cast<const void*>(&FilePrintStreamCompleteDestructor_nid_no_patch),
    reinterpret_cast<const void*>(&FilePrintStreamDeletingDestructor_nid_no_patch),
    reinterpret_cast<const void*>(&FilePrintStreamVprintf_nid_no_patch),
    reinterpret_cast<const void*>(&FilePrintStreamFlush_nid_no_patch),
    reinterpret_cast<const void*>(&PrintStreamBegin_nid_no_patch),
    reinterpret_cast<const void*>(&PrintStreamEnd_nid_no_patch),
};

const void* const LockedPrintStreamVtable[] = {
    nullptr,
    nullptr,
    reinterpret_cast<const void*>(&LockedPrintStreamCompleteDestructor_nid_no_patch),
    reinterpret_cast<const void*>(&LockedPrintStreamDeletingDestructor_nid_no_patch),
    reinterpret_cast<const void*>(&LockedPrintStreamVprintf_nid_no_patch),
    reinterpret_cast<const void*>(&LockedPrintStreamFlush_nid_no_patch),
    reinterpret_cast<const void*>(&LockedPrintStreamBegin_nid_no_patch),
    reinterpret_cast<const void*>(&LockedPrintStreamEnd_nid_no_patch),
};

std::atomic<PrintStream*> dataFile{nullptr};
alignas(FilePrintStream) unsigned char fileData[sizeof(FilePrintStream)];
alignas(LockedPrintStream) unsigned char lockedFileData[sizeof(LockedPrintStream)];
std::once_flag initializeOnce;

PrintStream* LockedFileData() {
    return reinterpret_cast<PrintStream*>(lockedFileData);
}

void SetDefaultDataFile() {
    auto* file = new (fileData) FilePrintStream{FilePrintStreamVtable + 2, &_Stdout_nid_postfix, Jsc::WtfAbi::FilePrintStreamBorrow};
    if (PrintStream* previous = dataFile.load(std::memory_order_acquire)) Slot<Flush>(previous, Jsc::WtfAbi::PrintStreamFlush)(previous);
    auto* locked = new (lockedFileData) LockedPrintStream{};
    locked->vtable = LockedPrintStreamVtable + 2;
    locked->target = reinterpret_cast<PrintStream*>(file);
    dataFile.store(LockedFileData(), std::memory_order_release);
}

void InitializeLogFileOnce() {
    if (dataFile.load(std::memory_order_acquire)) return;
    SetDefaultDataFile();
}

} // namespace

extern "C" {

Jsc::WtfAbi::PrintStream* WtfDataFile_nid_no_patch() {
    std::call_once(initializeOnce, InitializeLogFileOnce);
    PrintStream* file = dataFile.load(std::memory_order_acquire);
    if (!file) throw std::logic_error("WTF::dataFile: setDataFile installed a null stream");
    return file;
}

void WtfFilePrintStreamDestroy_nid_no_patch(Jsc::WtfAbi::FilePrintStream* stream) {
    RequireFile(stream);
    if (stream->adoptionMode == Jsc::WtfAbi::FilePrintStreamBorrow) return;
    fclose_nid_postfix(stream->file);
}

void WtfFilePrintStreamDelete_nid_no_patch(Jsc::WtfAbi::FilePrintStream* stream) {
    WtfFilePrintStreamDestroy_nid_no_patch(stream);
    _ZdlPv_nid_postfix(stream);
}

void APS5_VABI _ZN3WTF11setDataFileEOSt10unique_ptrINS_11PrintStreamESt14default_deleteIS1_EE(Jsc::WtfAbi::PrintStreamPtr* file) {
    if (!file) throw std::invalid_argument("WTF::setDataFile: null unique_ptr reference");
    PrintStream* current = dataFile.load(std::memory_order_acquire);
    if (current && current != LockedFileData()) throw std::logic_error("WTF::setDataFile: a data file stream was already installed");
    dataFile.store(file->pointer, std::memory_order_release);
    file->pointer = nullptr;
}

Jsc::WtfAbi::FilePrintStreamPtr* APS5_VABI _ZN3WTF15FilePrintStream4openEPKcS2_(Jsc::WtfAbi::FilePrintStreamPtr* result, const char* filename, const char* mode) {
    if (!result) throw std::invalid_argument("WTF::FilePrintStream::open: null result");
    FileStream* file = fopen_nid_postfix(filename, mode);
    if (!file) {
        result->pointer = nullptr;
        return result;
    }
    void* memory = nullptr;
    try {
        memory = _Znwm_nid_postfix(sizeof(FilePrintStream));
    } catch (...) {
        fclose_nid_postfix(file);
        throw;
    }
    result->pointer = new (memory) FilePrintStream{FilePrintStreamVtable + 2, file, Jsc::WtfAbi::FilePrintStreamAdopt};
    return result;
}

}
