// Derived from https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/fileapi/AsyncFileStream.cpp
/*
 * Copyright (C) 2010 Google Inc.  All rights reserved.
 * Copyright (C) 2012, 2014 Apple Inc. All rights reserved.
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

// Derived from https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/FileStream.cpp
/*
 * Copyright (C) 2010 Google Inc.  All rights reserved.
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

// Derived from https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WTF/wtf/MathExtras.h
/*
 * Copyright (C) 2006-2018 Apple Inc. All rights reserved.
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

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceNKWebKit/include/AsyncFileStreamExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/include/WtfValueExports.hpp"
#include "prx/libSceNKWebKit/src/WtfFunctionAbi.hpp"

#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using NKWebKit::WebCoreAbi::AsyncFileStream;
using NKWebKit::WebCoreAbi::FileStream;
using NKWebKit::WebCoreAbi::FileStreamClient;
using NKWebKit::WebCoreAbi::FileStreamClientFunction;
using NKWebKit::WebCoreAbi::FileStreamClientVtable;
using NKWebKit::WebCoreAbi::FileStreamOperationCallable;
using NKWebKit::WebCoreAbi::FileStreamOperationCallableVtable;
using NKWebKit::WebCoreAbi::FileStreamOperationFunction;
using NKWebKit::WebCoreAbi::OptionalWallTime;
using NKWebKit::WtfAbi::CString;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::VoidCallableWrapper;
using NKWebKit::WtfAbi::VoidCallableWrapperVtable;
using NKWebKit::WtfAbi::VoidFunction;

struct RawOptionalWallTime {
    bool hasValue;
    double value;
};

static_assert(std::is_trivially_copyable_v<AsyncFileStream>);
static_assert(std::is_trivially_copyable_v<FileStreamClient>);
static_assert(std::is_trivially_copyable_v<FileStreamClientVtable>);
static_assert(std::is_trivially_copyable_v<FileStreamOperationFunction>);
static_assert(std::is_trivially_copyable_v<FileStreamOperationCallable>);
static_assert(std::is_trivially_copyable_v<FileStreamOperationCallableVtable>);
static_assert(std::is_trivially_copyable_v<StringImpl>);
static_assert(std::is_trivially_copyable_v<RawOptionalWallTime>);
static_assert(sizeof(RawOptionalWallTime) == sizeof(OptionalWallTime));
static_assert(offsetof(RawOptionalWallTime, hasValue) == offsetof(OptionalWallTime, hasValue));
static_assert(offsetof(RawOptionalWallTime, value) == offsetof(OptionalWallTime, value));

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

constexpr int kInvalidHandle = -1;
constexpr int kGuestEintr = 4;
constexpr int kFileOpenModeRead = 0;
constexpr int kFileSeekOriginBeginning = 0;

struct Job {
    virtual ~Job() = default;
    virtual void Run() = 0;
};

template<typename TFunction>
struct FunctionJob final : Job {
    TFunction function;

    explicit FunctionJob(TFunction&& value) : function(std::move(value)) {}

    void Run() override {
        function();
    }
};

template<typename TFunction>
std::unique_ptr<Job> MakeJob(TFunction&& function) {
    return std::make_unique<FunctionJob<std::decay_t<TFunction>>>(std::forward<TFunction>(function));
}

struct FileThreadQueue {
    std::mutex mutex;
    std::condition_variable condition;
    std::deque<std::unique_ptr<Job>> jobs;
};

FileThreadQueue& Queue() {
    static FileThreadQueue* const queue = new FileThreadQueue;
    return *queue;
}

void* APS5_VABI RunFileThread(void*) {
    FileThreadQueue& queue = Queue();
    for (;;) {
        std::unique_ptr<Job> job;
        {
            std::unique_lock lock(queue.mutex);
            queue.condition.wait(lock, [&queue] { return !queue.jobs.empty(); });
            job = std::move(queue.jobs.front());
            queue.jobs.pop_front();
        }
        job->Run();
    }
}

void Enqueue(std::unique_ptr<Job> job) {
    static std::once_flag createFileThreadOnce;
    std::call_once(createFileThreadOnce, [] {
        Pthread thread = nullptr;
        if (const int error = pthread_create_nid_postfix(&thread, nullptr, &RunFileThread, nullptr))
            throw std::runtime_error("WebCore::AsyncFileStream: pthread_create failed with " + std::to_string(error));
    });
    FileThreadQueue& queue = Queue();
    {
        std::lock_guard lock(queue.mutex);
        queue.jobs.push_back(std::move(job));
    }
    queue.condition.notify_one();
}

template<typename TFunction>
void CallOnFileThread(TFunction&& function) {
    Enqueue(MakeJob(std::forward<TFunction>(function)));
}

struct MainThreadJob {
    VoidCallableWrapper base;
    std::unique_ptr<Job> job;
};

void APS5_VABI DestroyMainThreadJob(VoidCallableWrapper* self) {
    reinterpret_cast<MainThreadJob*>(self)->~MainThreadJob();
}

void APS5_VABI DeleteMainThreadJob(VoidCallableWrapper* self) {
    delete reinterpret_cast<MainThreadJob*>(self);
}

void APS5_VABI CallMainThreadJob(VoidCallableWrapper* self) {
    reinterpret_cast<MainThreadJob*>(self)->job->Run();
}

const VoidCallableWrapperVtable mainThreadJobVtable = {DestroyMainThreadJob, DeleteMainThreadJob, CallMainThreadJob};

template<typename TFunction>
void CallOnMainThread(TFunction&& function) {
    auto* job = new MainThreadJob{{&mainThreadJobVtable}, MakeJob(std::forward<TFunction>(function))};
    VoidFunction wrapper{&job->base};
    _ZN3WTF16callOnMainThreadEONS_8FunctionIFvvEEE(&wrapper);
}

void Close(FileStream& stream) {
    if (stream.handle != kInvalidHandle) _ZN3WTF14FileSystemImpl9closeFileERi(stream.handle);
}

bool OpenForRead(FileStream& stream, const String& path, long long offset, long long length) {
    if (stream.handle != kInvalidHandle) return true;
    stream.handle = _ZN3WTF14FileSystemImpl8openFileERKNS_6StringENS0_12FileOpenModeENS0_20FileAccessPermissionEb(path, kFileOpenModeRead, true, false);
    if (stream.handle == kInvalidHandle) return false;
    if (offset > 0 && _ZN3WTF14FileSystemImpl8seekFileEixNS0_14FileSeekOriginE(stream.handle, offset, kFileSeekOriginBeginning) < 0) {
        Close(stream);
        return false;
    }
    stream.totalBytesToRead = length;
    stream.bytesProcessed = 0;
    return true;
}

int ReadFromFile(int handle, char* data, int length) {
    do {
        const std::int64_t bytesRead = read_nid_postfix(handle, data, static_cast<std::size_t>(length));
        if (bytesRead >= 0) return static_cast<int>(bytesRead);
    } while (*__error_nid_postfix() == kGuestEintr);
    return -1;
}

int Read(FileStream& stream, char* buffer, int bufferSize) {
    if (stream.handle == kInvalidHandle) return -1;
    const long long remaining = stream.totalBytesToRead - stream.bytesProcessed;
    const int bytesToRead = remaining < bufferSize ? static_cast<int>(remaining) : bufferSize;
    if (bytesToRead <= 0) return 0;
    std::vector<char> temp(static_cast<std::size_t>(bytesToRead));
    const int bytesRead = ReadFromFile(stream.handle, temp.data(), bytesToRead);
    if (bytesRead < 0) return -1;
    if (bytesRead > 0 && !GuestCopyOut_nid_no_patch(buffer, temp.data(), static_cast<std::size_t>(bytesRead)))
        return -1;
    stream.bytesProcessed += bytesRead;
    return bytesRead;
}

std::optional<FileStat> Stat(const String& path) {
    const CString representation = _ZNK3WTF6String4utf8ENS_14ConversionModeE(&path, 0);
    const char* data = representation.buffer ? reinterpret_cast<const char*>(representation.buffer + 1) : nullptr;
    if (!data) return std::nullopt;
    FileStat status{};
    if (stat_nid_postfix(data, &status)) return std::nullopt;
    return status;
}

std::int64_t SecondsAsTime(double seconds) {
    constexpr std::int64_t minimum = std::numeric_limits<std::int64_t>::min();
    constexpr std::int64_t maximum = std::numeric_limits<std::int64_t>::max();
    if (seconds >= static_cast<double>(maximum)) return maximum;
    if (seconds <= static_cast<double>(minimum)) return minimum;
    return static_cast<std::int64_t>(seconds);
}

long long GetSize(const String& path, const std::optional<double>& expectedModificationTime) {
    const std::optional<FileStat> modification = Stat(path);
    if (!modification) return -1;
    if (expectedModificationTime) {
        const double modificationTime = static_cast<double>(static_cast<std::int64_t>(modification->st_mtim.tv_sec));
        if (SecondsAsTime(*expectedModificationTime) != SecondsAsTime(modificationTime)) return -1;
    }
    const std::optional<FileStat> size = Stat(path);
    if (!size) return -1;
    return static_cast<long long>(size->st_size);
}

}  // namespace

namespace NKWebKit::WebCoreAbi {

struct AsyncFileStreamInternals {
    FileStream stream{kInvalidHandle, 0, 0};
    FileStreamClient* client;
    std::atomic<bool> destroyed{false};

    explicit AsyncFileStreamInternals(FileStreamClient* value) : client(value) {}

    ~AsyncFileStreamInternals() {
        Close(stream);
    }
};

}  // namespace NKWebKit::WebCoreAbi

namespace {

using NKWebKit::WebCoreAbi::AsyncFileStreamInternals;

struct InternalsDeleter {
    void operator()(AsyncFileStreamInternals* internals) const {
        internals->~AsyncFileStreamInternals();
        _ZN3WTF8fastFreeEPv(internals);
    }
};

using InternalsPtr = std::unique_ptr<AsyncFileStreamInternals, InternalsDeleter>;

struct GuestMainThreadWork {
    FileStreamClientFunction function;

    void operator()(FileStreamClient& client) {
        if (!function.callable) throw std::logic_error("WebCore::AsyncFileStream::perform: the operation returned a null WTF::Function");
        function.callable->vtable->call(function.callable, &client);
    }
};

struct GuestOperation {
    FileStreamOperationCallable* callable;

    explicit GuestOperation(FileStreamOperationCallable* adopted) : callable(adopted) {}
    GuestOperation(const GuestOperation&) = delete;
    GuestOperation(GuestOperation&& other) noexcept : callable(std::exchange(other.callable, nullptr)) {}
    GuestOperation& operator=(const GuestOperation&) = delete;
    GuestOperation& operator=(GuestOperation&&) = delete;
    ~GuestOperation() {
        if (callable) callable->vtable->deletingDestructor(callable);
    }

    GuestMainThreadWork operator()(FileStream& stream) {
        return GuestMainThreadWork{callable->vtable->call(callable, &stream)};
    }
};

AsyncFileStreamInternals& InternalsOf(AsyncFileStream* self, const char* function) {
    if (!self) throw std::invalid_argument("null WebCore::AsyncFileStream");
    const AsyncFileStream snapshot = ReadGuestValue(self, function);
    if (!snapshot.internals) throw std::invalid_argument("destroyed WebCore::AsyncFileStream");
    return *snapshot.internals;
}

template<typename TOperation>
void Perform(AsyncFileStreamInternals& internals, TOperation&& operation) {
    CallOnFileThread([&internals, operation = std::forward<TOperation>(operation)]() mutable {
        if (internals.destroyed) return;
        CallOnMainThread([&internals, mainThreadWork = operation(internals.stream)]() mutable {
            if (internals.destroyed) return;
            mainThreadWork(*internals.client);
        });
    });
}

void Construct(AsyncFileStream* self, FileStreamClient* client, const char* function) {
    if (!self) throw std::invalid_argument("null WebCore::AsyncFileStream");
    if (!client) throw std::invalid_argument("null WebCore::FileStreamClient");
    RequireReadableGuestBytes(self, sizeof(AsyncFileStream), function);
    RequireWritableGuestBytes(self, sizeof(AsyncFileStream), function);
    const AsyncFileStream previous = ReadGuestValue(self, function);
    const FileStreamClient clientHeader = ReadGuestValue(client, function);
    if (!clientHeader.vtable) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    static_cast<void>(ReadGuestValue(clientHeader.vtable, function));
    InternalsPtr internals(new (_ZN3WTF10fastMallocEm(sizeof(AsyncFileStreamInternals))) AsyncFileStreamInternals(client));
    const AsyncFileStream constructed{internals.get()};
    if (!TryPublishGuestValue(self, constructed, previous))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    internals.release();
}

void Destroy(AsyncFileStream* self, const char* function) {
    if (!self) throw std::invalid_argument("null WebCore::AsyncFileStream");
    RequireReadableGuestBytes(self, sizeof(AsyncFileStream), function);
    RequireWritableGuestBytes(self, sizeof(AsyncFileStream), function);
    const AsyncFileStream snapshot = ReadGuestValue(self, function);
    if (!snapshot.internals) throw std::invalid_argument("destroyed WebCore::AsyncFileStream");
    const AsyncFileStream cleared{nullptr};
    if (!TryPublishGuestValue(self, cleared, snapshot))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    InternalsPtr internals(snapshot.internals);
    internals->destroyed = true;
    CallOnFileThread([internals = std::move(internals)]() mutable {
        CallOnMainThread([internals = std::move(internals)] {});
    });
}

}  // namespace

extern "C" {

void APS5_VABI _ZN7WebCore15AsyncFileStreamC1ERNS_16FileStreamClientE(AsyncFileStream* self, FileStreamClient* client) {
    Construct(self, client, __func__);
}

void APS5_VABI _ZN7WebCore15AsyncFileStreamC2ERNS_16FileStreamClientE(AsyncFileStream* self, FileStreamClient* client) {
    Construct(self, client, __func__);
}

void APS5_VABI _ZN7WebCore15AsyncFileStreamD1Ev(AsyncFileStream* self) {
    Destroy(self, __func__);
}

void APS5_VABI _ZN7WebCore15AsyncFileStreamD2Ev(AsyncFileStream* self) {
    Destroy(self, __func__);
}

void APS5_VABI _ZN7WebCore15AsyncFileStream11openForReadERKN3WTF6StringExx(AsyncFileStream* self, const String* path, long long offset, long long length) {
    AsyncFileStreamInternals& internals = InternalsOf(self, __func__);
    if (!path) throw std::invalid_argument("null WTF::String for WebCore::AsyncFileStream::openForRead");
    Perform(internals, [path = CopyGuestStringToHost(path, __func__), offset, length](FileStream& stream) {
        const bool success = OpenForRead(stream, path, offset, length);
        return [success](FileStreamClient& client) { client.vtable->didOpen(&client, success); };
    });
}

void APS5_VABI _ZN7WebCore15AsyncFileStream5closeEv(AsyncFileStream* self) {
    AsyncFileStreamInternals& internals = InternalsOf(self, __func__);
    CallOnFileThread([&internals] { Close(internals.stream); });
}

void APS5_VABI _ZN7WebCore15AsyncFileStream7performEON3WTF8FunctionIFNS2_IFvRNS_16FileStreamClientEEEERNS_10FileStreamEEEE(
    AsyncFileStream* self, FileStreamOperationFunction* operation) {
    AsyncFileStreamInternals& internals = InternalsOf(self, __func__);
    if (!operation) throw std::invalid_argument("null WTF::Function reference for WebCore::AsyncFileStream::perform");
    RequireReadableGuestBytes(operation, sizeof(FileStreamOperationFunction), __func__);
    RequireWritableGuestBytes(operation, sizeof(FileStreamOperationFunction), __func__);
    const FileStreamOperationFunction opSnapshot = ReadGuestValue(operation, __func__);
    if (!opSnapshot.callable) throw std::invalid_argument("null WTF::Function for WebCore::AsyncFileStream::perform");
    const FileStreamOperationCallable callableHeader = ReadGuestValue(opSnapshot.callable, __func__);
    if (!callableHeader.vtable) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    const FileStreamOperationCallableVtable callableVtable = ReadGuestValue(callableHeader.vtable, __func__);
    if (!callableVtable.call || !callableVtable.deletingDestructor)
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    const FileStreamOperationFunction cleared{nullptr};
    if (!TryPublishGuestValue(operation, cleared, opSnapshot))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    Perform(internals, GuestOperation(opSnapshot.callable));
}

void APS5_VABI _ZN7WebCore15AsyncFileStream4readEPci(AsyncFileStream* self, char* buffer, int length) {
    AsyncFileStreamInternals& internals = InternalsOf(self, __func__);
    if (length > 0) RequireWritableGuestBytes(buffer, static_cast<std::size_t>(length), __func__);
    Perform(internals, [buffer, length](FileStream& stream) {
        const int bytesRead = Read(stream, buffer, length);
        return [bytesRead](FileStreamClient& client) { client.vtable->didRead(&client, bytesRead); };
    });
}

void APS5_VABI _ZN7WebCore15AsyncFileStream7getSizeERKN3WTF6StringENS1_8OptionalINS1_8WallTimeEEE(
    AsyncFileStream* self, const String* path, const OptionalWallTime* expectedModificationTime) {
    AsyncFileStreamInternals& internals = InternalsOf(self, __func__);
    if (!path) throw std::invalid_argument("null WTF::String for WebCore::AsyncFileStream::getSize");
    if (!expectedModificationTime) throw std::invalid_argument("null WTF::Optional<WTF::WallTime> for WebCore::AsyncFileStream::getSize");
    const RawOptionalWallTime rawExpected = ReadGuestValue(reinterpret_cast<const RawOptionalWallTime*>(expectedModificationTime), __func__);
    std::optional<double> expected;
    if (rawExpected.hasValue) {
        if (std::isnan(rawExpected.value))
            throw std::invalid_argument("WebCore::AsyncFileStream::getSize: the expected modification time is not a number");
        expected = rawExpected.value;
    }
    Perform(internals, [path = CopyGuestStringToHost(path, __func__), expected](FileStream&) {
        const long long size = GetSize(path, expected);
        return [size](FileStreamClient& client) { client.vtable->didGetSize(&client, size); };
    });
}

}
