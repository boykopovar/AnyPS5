#include "prx/libSceFios2/include/Api.hpp"
#include "Runtime.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"

namespace {

constexpr std::uint16_t DefaultFileMode = 0666;
constexpr std::uint16_t DefaultDirectoryMode = 0777;

using PathWork = std::function<void(const std::string&, Fios::Completion&)>;

SceFiosOp SubmitPath(const SceFiosOpAttr* attr, const char* path, std::int64_t request, std::int64_t offset, void* buffer, PathWork work, Fios::ExecutionPolicy policy = Fios::Async) {
    Fios::OpInfo info;
    info.request = request;
    info.offset = offset;
    info.buffer = buffer;
    if (const std::int32_t error = Fios::CopyPath(path, info.path)) return Fios::SubmitFailed(attr, std::move(info), error, policy);
    const std::string target = info.path;
    return Fios::Submit(attr, std::move(info), [target, work = std::move(work)](Fios::Completion& completion) { work(target, completion); }, policy);
}

int StatusSync(SceFiosOp op) {
    return Fios::SyncStatus(op);
}

std::int64_t CountSync(SceFiosOp op) {
    return Fios::SyncCount(op);
}

bool IsDirectory(const std::string& path, std::int32_t& error) {
    SceFiosStat stat{};
    error = Fios::StatPath(path, stat);
    return error == SCE_FIOS_OK && (stat.statFlags & SCE_FIOS_STAT_DIRECTORY) != 0;
}

void RemoveFile(const std::string& path, Fios::Completion& completion) {
    std::int32_t error = SCE_FIOS_OK;
    if (IsDirectory(path, error)) completion.error = SCE_FIOS_ERROR_NOT_A_FILE;
    else if (error != SCE_FIOS_OK) completion.error = error;
    else completion.error = Fios::FromKernelError(sceKernelUnlink(path.c_str()));
}

void RemoveDirectoryAt(const std::string& path, Fios::Completion& completion) {
    std::int32_t error = SCE_FIOS_OK;
    if (!IsDirectory(path, error)) completion.error = error != SCE_FIOS_OK ? error : SCE_FIOS_ERROR_NOT_A_DIRECTORY;
    else completion.error = Fios::FromKernelError(sceKernelRmdir(path.c_str()));
}

SceFiosOp StartDelete(const SceFiosOpAttr* attr, const char* path, Fios::ExecutionPolicy policy = Fios::Async) {
    return SubmitPath(attr, path, 0, 0, nullptr, [](const std::string& target, Fios::Completion& completion) {
        std::int32_t error = SCE_FIOS_OK;
        if (IsDirectory(target, error)) RemoveDirectoryAt(target, completion);
        else if (error != SCE_FIOS_OK) completion.error = error;
        else RemoveFile(target, completion);
    }, policy);
}

SceFiosOp StartFileDelete(const SceFiosOpAttr* attr, const char* path, Fios::ExecutionPolicy policy = Fios::Async) {
    return SubmitPath(attr, path, 0, 0, nullptr, RemoveFile, policy);
}

SceFiosOp StartDirectoryCreate(const SceFiosOpAttr* attr, const char* path, std::uint16_t mode = DefaultDirectoryMode, Fios::ExecutionPolicy policy = Fios::Async) {
    return SubmitPath(attr, path, 0, 0, nullptr, [mode](const std::string& target, Fios::Completion& completion) {
        completion.error = Fios::FromKernelError(sceKernelMkdir(target.c_str(), mode));
    }, policy);
}

SceFiosOp StartDirectoryDelete(const SceFiosOpAttr* attr, const char* path, Fios::ExecutionPolicy policy = Fios::Async) {
    return SubmitPath(attr, path, 0, 0, nullptr, RemoveDirectoryAt, policy);
}

SceFiosOp StartRename(const SceFiosOpAttr* attr, const char* oldPath, const char* newPath, Fios::ExecutionPolicy policy = Fios::Async) {
    std::string destination;
    if (const std::int32_t error = Fios::CopyPath(newPath, destination)) {
        return SubmitPath(attr, oldPath, 0, 0, nullptr, [error](const std::string&, Fios::Completion& completion) { completion.error = error; }, policy);
    }
    return SubmitPath(attr, oldPath, 0, 0, nullptr, [destination](const std::string& source, Fios::Completion& completion) {
        completion.error = Fios::FromKernelError(sceKernelRename(source.c_str(), destination.c_str()));
    }, policy);
}

SceFiosOp StartFileRead(const SceFiosOpAttr* attr, const char* path, void* buffer, SceFiosSize length, SceFiosOffset offset, Fios::ExecutionPolicy policy = Fios::Async) {
    std::int32_t invalid = SCE_FIOS_OK;
    if (length < 0) invalid = SCE_FIOS_ERROR_BAD_SIZE;
    else if (offset < 0) invalid = SCE_FIOS_ERROR_BAD_OFFSET;
    else if (buffer == nullptr && length != 0) invalid = SCE_FIOS_ERROR_BAD_PTR;
    return SubmitPath(attr, path, length, offset, buffer, [=](const std::string& target, Fios::Completion& completion) {
        if (invalid != SCE_FIOS_OK) {
            completion.error = invalid;
            return;
        }
        const int descriptor = sceKernelOpen(target.c_str(), SCE_KERNEL_O_RDONLY, 0);
        if (descriptor < 0) {
            completion.error = Fios::FromKernelError(descriptor);
            return;
        }
        const std::int64_t count = Fios::ReadFully(descriptor, buffer, static_cast<std::uint64_t>(length), offset, true);
        sceKernelClose(descriptor);
        if (count < 0) completion.error = static_cast<std::int32_t>(count);
        else completion.result = count;
    }, policy);
}

SceFiosOp StartFileWrite(const SceFiosOpAttr* attr, const char* path, const void* buffer, SceFiosSize length, SceFiosOffset offset, Fios::ExecutionPolicy policy = Fios::Async) {
    std::int32_t invalid = SCE_FIOS_OK;
    if (length < 0) invalid = SCE_FIOS_ERROR_BAD_SIZE;
    else if (offset < 0) invalid = SCE_FIOS_ERROR_BAD_OFFSET;
    else if (buffer == nullptr && length != 0) invalid = SCE_FIOS_ERROR_BAD_PTR;
    return SubmitPath(attr, path, length, offset, const_cast<void*>(buffer), [=](const std::string& target, Fios::Completion& completion) {
        if (invalid != SCE_FIOS_OK) {
            completion.error = invalid;
            return;
        }
        const int descriptor = sceKernelOpen(target.c_str(), SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT, DefaultFileMode);
        if (descriptor < 0) {
            completion.error = Fios::FromKernelError(descriptor);
            return;
        }
        const std::int64_t count = Fios::WriteFully(descriptor, buffer, static_cast<std::uint64_t>(length), offset, true);
        sceKernelClose(descriptor);
        if (count < 0) completion.error = static_cast<std::int32_t>(count);
        else completion.result = count;
    }, policy);
}

SceFiosOp StartFileTruncate(const SceFiosOpAttr* attr, const char* path, SceFiosSize length, Fios::ExecutionPolicy policy = Fios::Async) {
    return SubmitPath(attr, path, length, 0, nullptr, [length](const std::string& target, Fios::Completion& completion) {
        if (length < 0) {
            completion.error = SCE_FIOS_ERROR_BAD_SIZE;
            return;
        }
        const int descriptor = sceKernelOpen(target.c_str(), SCE_KERNEL_O_WRONLY, 0);
        if (descriptor < 0) {
            completion.error = Fios::FromKernelError(descriptor);
            return;
        }
        completion.error = Fios::FromKernelError(sceKernelFtruncate(descriptor, length));
        sceKernelClose(descriptor);
    }, policy);
}

}

extern "C" {

SceFiosOp APS5_VABI sceFiosDelete(const SceFiosOpAttr* attr, const char* path) {
    return StartDelete(attr, path, Fios::Async);
}

int APS5_VABI sceFiosDeleteSync(const SceFiosOpAttr* attr, const char* path) {
    return StatusSync(StartDelete(attr, path, Fios::Inline));
}

SceFiosOp APS5_VABI sceFiosFileDelete(const SceFiosOpAttr* attr, const char* path) {
    return StartFileDelete(attr, path, Fios::Async);
}

int APS5_VABI sceFiosFileDeleteSync(const SceFiosOpAttr* attr, const char* path) {
    return StatusSync(StartFileDelete(attr, path, Fios::Inline));
}

SceFiosOp APS5_VABI sceFiosDirectoryCreate(const SceFiosOpAttr* attr, const char* path) {
    return StartDirectoryCreate(attr, path, DefaultDirectoryMode, Fios::Async);
}

int APS5_VABI sceFiosDirectoryCreateSync(const SceFiosOpAttr* attr, const char* path) {
    return StatusSync(StartDirectoryCreate(attr, path, DefaultDirectoryMode, Fios::Inline));
}

SceFiosOp APS5_VABI sceFiosDirectoryCreateWithMode(const SceFiosOpAttr* attr, const char* path, std::uint32_t mode) {
    return StartDirectoryCreate(attr, path, static_cast<std::uint16_t>(mode), Fios::Async);
}

int APS5_VABI sceFiosDirectoryCreateWithModeSync(const SceFiosOpAttr* attr, const char* path, std::uint32_t mode) {
    return StatusSync(StartDirectoryCreate(attr, path, static_cast<std::uint16_t>(mode), Fios::Inline));
}

SceFiosOp APS5_VABI sceFiosDirectoryDelete(const SceFiosOpAttr* attr, const char* path) {
    return StartDirectoryDelete(attr, path, Fios::Async);
}

int APS5_VABI sceFiosDirectoryDeleteSync(const SceFiosOpAttr* attr, const char* path) {
    return StatusSync(StartDirectoryDelete(attr, path, Fios::Inline));
}

SceFiosOp APS5_VABI sceFiosRename(const SceFiosOpAttr* attr, const char* oldPath, const char* newPath) {
    return StartRename(attr, oldPath, newPath, Fios::Async);
}

int APS5_VABI sceFiosRenameSync(const SceFiosOpAttr* attr, const char* oldPath, const char* newPath) {
    return StatusSync(StartRename(attr, oldPath, newPath, Fios::Inline));
}

SceFiosOp APS5_VABI sceFiosFileRead(const SceFiosOpAttr* attr, const char* path, void* buffer, SceFiosSize length, SceFiosOffset offset) {
    return StartFileRead(attr, path, buffer, length, offset, Fios::Async);
}

SceFiosSize APS5_VABI sceFiosFileReadSync(const SceFiosOpAttr* attr, const char* path, void* buffer, SceFiosSize length, SceFiosOffset offset) {
    return CountSync(StartFileRead(attr, path, buffer, length, offset, Fios::Inline));
}

SceFiosOp APS5_VABI sceFiosFileWrite(const SceFiosOpAttr* attr, const char* path, const void* buffer, SceFiosSize length, SceFiosOffset offset) {
    return StartFileWrite(attr, path, buffer, length, offset, Fios::Async);
}

SceFiosSize APS5_VABI sceFiosFileWriteSync(const SceFiosOpAttr* attr, const char* path, const void* buffer, SceFiosSize length, SceFiosOffset offset) {
    return CountSync(StartFileWrite(attr, path, buffer, length, offset, Fios::Inline));
}

SceFiosOp APS5_VABI sceFiosFileTruncate(const SceFiosOpAttr* attr, const char* path, SceFiosSize length) {
    return StartFileTruncate(attr, path, length, Fios::Async);
}

int APS5_VABI sceFiosFileTruncateSync(const SceFiosOpAttr* attr, const char* path, SceFiosSize length) {
    return StatusSync(StartFileTruncate(attr, path, length, Fios::Inline));
}

}
