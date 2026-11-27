#pragma once

#include "SceTypes.hpp"

#include <cstddef>
#include <type_traits>
#include <utility>

namespace NKWebKit::WebCoreAbi {

struct FileStreamClient;

struct FileStreamClientVtable {
    void (APS5_VABI* didOpen)(FileStreamClient* self, bool success);
    void (APS5_VABI* didGetSize)(FileStreamClient* self, long long size);
    void (APS5_VABI* didRead)(FileStreamClient* self, int bytesRead);
    void (APS5_VABI* didWrite)(FileStreamClient* self, int bytesWritten);
    void (APS5_VABI* didTruncate)(FileStreamClient* self, bool success);
    void (APS5_VABI* completeDestructor)(FileStreamClient* self);
    void (APS5_VABI* deletingDestructor)(FileStreamClient* self);
};

struct FileStreamClient {
    const FileStreamClientVtable* vtable;
};

struct FileStream {
    int handle;
    long long bytesProcessed;
    long long totalBytesToRead;
};

struct FileStreamClientCallable;

struct FileStreamClientCallableVtable {
    void (APS5_VABI* completeDestructor)(FileStreamClientCallable* self);
    void (APS5_VABI* deletingDestructor)(FileStreamClientCallable* self);
    void (APS5_VABI* call)(FileStreamClientCallable* self, FileStreamClient* client);
};

struct FileStreamClientCallable {
    const FileStreamClientCallableVtable* vtable;
};

struct FileStreamClientFunction {
    FileStreamClientCallable* callable = nullptr;

    FileStreamClientFunction() = default;
    explicit FileStreamClientFunction(FileStreamClientCallable* adopted) : callable(adopted) {}
    FileStreamClientFunction(const FileStreamClientFunction&) = delete;
    FileStreamClientFunction(FileStreamClientFunction&& other) noexcept : callable(std::exchange(other.callable, nullptr)) {}
    FileStreamClientFunction& operator=(const FileStreamClientFunction&) = delete;
    FileStreamClientFunction& operator=(FileStreamClientFunction&&) = delete;
    ~FileStreamClientFunction() {
        if (callable) callable->vtable->deletingDestructor(callable);
    }
};

struct FileStreamOperationCallable;

struct FileStreamOperationCallableVtable {
    void (APS5_VABI* completeDestructor)(FileStreamOperationCallable* self);
    void (APS5_VABI* deletingDestructor)(FileStreamOperationCallable* self);
    FileStreamClientFunction (APS5_VABI* call)(FileStreamOperationCallable* self, FileStream* stream);
};

struct FileStreamOperationCallable {
    const FileStreamOperationCallableVtable* vtable;
};

struct FileStreamOperationFunction {
    FileStreamOperationCallable* callable;
};

struct AsyncFileStreamInternals;

struct AsyncFileStream {
    AsyncFileStreamInternals* internals;
};

static_assert(sizeof(FileStreamClientVtable) == 7 * sizeof(void*));
static_assert(offsetof(FileStreamClientVtable, didOpen) == 0);
static_assert(offsetof(FileStreamClientVtable, didGetSize) == 8);
static_assert(offsetof(FileStreamClientVtable, didRead) == 16);
static_assert(offsetof(FileStreamClientVtable, didWrite) == 24);
static_assert(offsetof(FileStreamClientVtable, didTruncate) == 32);
static_assert(offsetof(FileStreamClientVtable, completeDestructor) == 40);
static_assert(offsetof(FileStreamClientVtable, deletingDestructor) == 48);
static_assert(sizeof(FileStream) == 24);
static_assert(offsetof(FileStream, handle) == 0);
static_assert(offsetof(FileStream, bytesProcessed) == 8);
static_assert(offsetof(FileStream, totalBytesToRead) == 16);
static_assert(sizeof(FileStreamClientCallableVtable) == 24);
static_assert(offsetof(FileStreamClientCallableVtable, call) == 16);
static_assert(sizeof(FileStreamOperationCallableVtable) == 24);
static_assert(offsetof(FileStreamOperationCallableVtable, call) == 16);
static_assert(sizeof(FileStreamClientFunction) == 8);
static_assert(!std::is_trivially_copyable_v<FileStreamClientFunction>);
static_assert(sizeof(FileStreamOperationFunction) == 8);
static_assert(sizeof(AsyncFileStream) == 8);

}  // namespace NKWebKit::WebCoreAbi
