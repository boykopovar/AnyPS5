#pragma once

#include "prx/libSceFios2/include/Types.hpp"
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace Fios {

struct Completion {
    std::int32_t error = SCE_FIOS_OK;
    std::int64_t result = 0;
};

using Work = std::function<void(Completion&)>;

struct OpInfo {
    std::string path;
    std::int64_t offset = 0;
    std::int64_t request = 0;
    void* buffer = nullptr;
};

struct FileHandle {
    std::mutex mutex;
    int descriptor = -1;
    std::string path;
    SceFiosOpenParams params{};
};

struct DirectoryHandle {
    std::mutex mutex;
    std::string path;
    std::vector<std::string> names;
    std::size_t next = 0;
};

std::int32_t FromKernelError(std::int64_t kernelResult);
std::int32_t CopyPath(const char* source, std::string& destination);
bool CopyOut(void* destination, const void* source, std::size_t bytes);
bool CopyIn(void* destination, const void* source, std::size_t bytes);
std::int32_t SyncStatus(SceFiosOp op);
std::int64_t SyncCount(SceFiosOp op);
std::uint64_t TimespecToDate(std::int64_t seconds, std::int64_t nanoseconds);

SceFiosOp Submit(const SceFiosOpAttr* attr, OpInfo info, Work work);
SceFiosOp SubmitFailed(const SceFiosOpAttr* attr, OpInfo info, std::int32_t error);
std::int32_t Wait(SceFiosOp op);
bool SyncWait(SceFiosOp op, Completion& completion);
bool OpDone(SceFiosOp op);
bool DeleteOp(SceFiosOp op);
bool CancelOp(SceFiosOp op);
bool OpIsCancelled(SceFiosOp op);
bool QueryOp(SceFiosOp op, std::int64_t& request, std::int64_t& actual, std::int64_t& offset);
bool OpError(SceFiosOp op, std::int32_t& error);
const SceFiosOpAttr* OpAttr(SceFiosOp op);
const char* OpPath(SceFiosOp op);
void* OpBuffer(SceFiosOp op);
std::uint32_t ListOps(SceFiosOp* out, std::uint32_t capacity);
void CancelAll();

SceFiosFH AddFile(std::shared_ptr<FileHandle> file);
std::shared_ptr<FileHandle> FindFile(SceFiosFH handle);
std::shared_ptr<FileHandle> RemoveFile(SceFiosFH handle);
std::uint32_t ListFiles(SceFiosFH* out, std::uint32_t capacity);
void CloseAllFiles();

SceFiosDH AddDirectory(std::shared_ptr<DirectoryHandle> directory);
std::shared_ptr<DirectoryHandle> FindDirectory(SceFiosDH handle);
std::shared_ptr<DirectoryHandle> RemoveDirectory(SceFiosDH handle);
std::uint32_t ListDirectories(SceFiosDH* out, std::uint32_t capacity);

bool IsValidHandle(SceFiosHandle handle);

bool Initialize(const void* params);
bool UpdateParameters(const void* params);
void Terminate();
bool IsInitialized(void* out);
void Suspend();
void Resume();
std::uint32_t SuspendCount();
bool IsIdle();

bool SetGlobalAttr(const SceFiosOpAttr* attr);
bool GlobalAttr(SceFiosOpAttr& attr);
bool SetThreadAttr(const SceFiosOpAttr* attr);
bool ThreadAttr(SceFiosOpAttr& attr);

std::int64_t ReadFully(int descriptor, void* buffer, std::uint64_t length, std::int64_t offset, bool positioned);
std::int64_t WriteFully(int descriptor, const void* buffer, std::uint64_t length, std::int64_t offset, bool positioned);
std::int32_t StatPath(const std::string& path, SceFiosStat& stat);
std::int32_t StatDescriptor(int descriptor, SceFiosStat& stat);

}
