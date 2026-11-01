#include "prx/libSceFios2/include/Api.hpp"
#include "Runtime.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include <cstring>
#include <vector>

namespace {

constexpr std::int32_t DefaultNativeMode = -1;
constexpr std::uint16_t CreateMode = 0x1ff;
constexpr std::int64_t MaxVectors = 1024;

using HandleWork = std::function<void(Fios::FileHandle&, Fios::Completion&)>;

bool CanRead(const Fios::FileHandle& file) {
    return (file.params.openFlags & SCE_FIOS_O_READ) != 0;
}

bool CanWrite(const Fios::FileHandle& file) {
    return (file.params.openFlags & SCE_FIOS_O_WRITE) != 0;
}

SceFiosOp SubmitHandle(const SceFiosOpAttr* attr, SceFiosFH handle, std::int64_t request, std::int64_t offset, void* buffer, std::int32_t invalid, HandleWork work) {
    Fios::OpInfo info;
    info.request = request;
    info.offset = offset;
    info.buffer = buffer;
    if (const auto file = Fios::FindFile(handle)) info.path = file->path;
    return Fios::Submit(attr, std::move(info), [handle, invalid, work = std::move(work)](Fios::Completion& completion) {
        if (invalid != SCE_FIOS_OK) {
            completion.error = invalid;
            return;
        }
        const auto file = Fios::FindFile(handle);
        if (!file) {
            completion.error = SCE_FIOS_ERROR_BAD_FH;
            return;
        }
        const std::lock_guard lock(file->mutex);
        if (file->descriptor < 0) {
            completion.error = SCE_FIOS_ERROR_BAD_FH;
            return;
        }
        work(*file, completion);
    });
}

std::int32_t CheckRange(const void* buffer, SceFiosSize length, SceFiosOffset offset, bool positioned) {
    if (length < 0) return SCE_FIOS_ERROR_BAD_SIZE;
    if (positioned && offset < 0) return SCE_FIOS_ERROR_BAD_OFFSET;
    if (buffer == nullptr && length != 0) return SCE_FIOS_ERROR_BAD_PTR;
    return SCE_FIOS_OK;
}

void Transfer(Fios::FileHandle& file, Fios::Completion& completion, void* buffer, SceFiosSize length, SceFiosOffset offset, bool positioned, bool write) {
    if (write ? !CanWrite(file) : !CanRead(file)) {
        completion.error = write ? SCE_FIOS_ERROR_READ_ONLY : SCE_FIOS_ERROR_WRITE_ONLY;
        return;
    }
    const std::int64_t count = write ? Fios::WriteFully(file.descriptor, buffer, static_cast<std::uint64_t>(length), offset, positioned)
                                     : Fios::ReadFully(file.descriptor, buffer, static_cast<std::uint64_t>(length), offset, positioned);
    if (count < 0) completion.error = static_cast<std::int32_t>(count);
    else completion.result = count;
}

SceFiosOp StartTransfer(const SceFiosOpAttr* attr, SceFiosFH handle, void* buffer, SceFiosSize length, SceFiosOffset offset, bool positioned, bool write) {
    const std::int32_t invalid = CheckRange(buffer, length, offset, positioned);
    return SubmitHandle(attr, handle, length, positioned ? offset : 0, buffer, invalid, [=](Fios::FileHandle& file, Fios::Completion& completion) {
        Transfer(file, completion, buffer, length, offset, positioned, write);
    });
}

SceFiosOp StartVectors(const SceFiosOpAttr* attr, SceFiosFH handle, const SceFiosBuffer* iov, int iovcnt, SceFiosOffset offset, bool positioned, bool write) {
    std::int32_t invalid = SCE_FIOS_OK;
    std::vector<SceFiosBuffer> vectors;
    if (iovcnt <= 0 || iovcnt > MaxVectors) invalid = SCE_FIOS_ERROR_BAD_IOVCNT;
    else if (iov == nullptr) invalid = SCE_FIOS_ERROR_BAD_PTR;
    else if (positioned && offset < 0) invalid = SCE_FIOS_ERROR_BAD_OFFSET;
    else {
        vectors.resize(static_cast<std::size_t>(iovcnt));
        if (!Fios::CopyIn(vectors.data(), iov, vectors.size() * sizeof(SceFiosBuffer))) invalid = SCE_FIOS_ERROR_BAD_PTR;
    }
    std::int64_t request = 0;
    for (const SceFiosBuffer& vector : vectors) {
        if (vector.pPtr == nullptr && vector.length != 0) invalid = SCE_FIOS_ERROR_BAD_PTR;
        if (vector.length > static_cast<std::uint64_t>(INT64_MAX) || request > INT64_MAX - static_cast<std::int64_t>(vector.length)) invalid = SCE_FIOS_ERROR_BAD_SIZE;
        else request += static_cast<std::int64_t>(vector.length);
    }
    return SubmitHandle(attr, handle, request, positioned ? offset : 0, nullptr, invalid, [=](Fios::FileHandle& file, Fios::Completion& completion) {
        if (write ? !CanWrite(file) : !CanRead(file)) {
            completion.error = write ? SCE_FIOS_ERROR_READ_ONLY : SCE_FIOS_ERROR_WRITE_ONLY;
            return;
        }
        std::int64_t position = offset;
        for (const SceFiosBuffer& vector : vectors) {
            const std::int64_t count = write ? Fios::WriteFully(file.descriptor, vector.pPtr, vector.length, position, positioned)
                                             : Fios::ReadFully(file.descriptor, vector.pPtr, vector.length, position, positioned);
            if (count < 0) {
                if (completion.result == 0) completion.error = static_cast<std::int32_t>(count);
                return;
            }
            completion.result += count;
            position += count;
            if (static_cast<std::uint64_t>(count) < vector.length) return;
        }
    });
}

SceFiosOp StartOpen(const SceFiosOpAttr* attr, SceFiosFH* outFH, const char* path, const SceFiosOpenParams* openParams, std::int32_t nativeMode) {
    Fios::OpInfo info;
    std::int32_t invalid = outFH == nullptr ? SCE_FIOS_ERROR_BAD_PTR : Fios::CopyPath(path, info.path);
    SceFiosOpenParams params{};
    if (openParams == nullptr) params.openFlags = SCE_FIOS_O_READ;
    else if (!Fios::CopyIn(&params, openParams, sizeof(params)) && invalid == SCE_FIOS_OK) invalid = SCE_FIOS_ERROR_BAD_PTR;
    const std::string target = info.path;
    return Fios::Submit(attr, std::move(info), [=](Fios::Completion& completion) {
        if (invalid != SCE_FIOS_OK) {
            completion.error = invalid;
            return;
        }
        const std::uint32_t flags = params.openFlags;
        std::int32_t native = (flags & (SCE_FIOS_O_READ | SCE_FIOS_O_WRITE)) == (SCE_FIOS_O_READ | SCE_FIOS_O_WRITE) ? SCE_KERNEL_O_RDWR
                              : (flags & SCE_FIOS_O_WRITE) != 0 ? SCE_KERNEL_O_WRONLY : SCE_KERNEL_O_RDONLY;
        if ((flags & SCE_FIOS_O_APPEND) != 0) native |= SCE_KERNEL_O_APPEND;
        if ((flags & SCE_FIOS_O_CREAT) != 0) native |= SCE_KERNEL_O_CREAT;
        if ((flags & SCE_FIOS_O_TRUNC) != 0) native |= SCE_KERNEL_O_TRUNC;
        const std::uint16_t mode = nativeMode == DefaultNativeMode ? CreateMode : static_cast<std::uint16_t>(nativeMode);
        const int descriptor = sceKernelOpen(target.c_str(), native, (flags & SCE_FIOS_O_CREAT) != 0 ? mode : 0);
        if (descriptor < 0) {
            completion.error = Fios::FromKernelError(descriptor);
            return;
        }
        SceFiosStat stat{};
        if (const std::int32_t error = Fios::StatDescriptor(descriptor, stat); error != SCE_FIOS_OK || (stat.statFlags & SCE_FIOS_STAT_DIRECTORY) != 0) {
            sceKernelClose(descriptor);
            completion.error = error != SCE_FIOS_OK ? error : SCE_FIOS_ERROR_NOT_A_FILE;
            return;
        }
        auto file = std::make_shared<Fios::FileHandle>();
        file->descriptor = descriptor;
        file->path = target;
        file->params = params;
        if ((file->params.openFlags & (SCE_FIOS_O_READ | SCE_FIOS_O_WRITE)) == 0) file->params.openFlags |= SCE_FIOS_O_READ;
        const SceFiosFH handle = Fios::AddFile(file);
        if (handle < 0) {
            sceKernelClose(descriptor);
            completion.error = handle;
            return;
        }
        if (!Fios::CopyOut(outFH, &handle, sizeof(handle))) {
            Fios::RemoveFile(handle);
            sceKernelClose(descriptor);
            completion.error = SCE_FIOS_ERROR_BAD_PTR;
        }
    });
}

SceFiosOp StartClose(const SceFiosOpAttr* attr, SceFiosFH handle) {
    Fios::OpInfo info;
    if (const auto file = Fios::FindFile(handle)) info.path = file->path;
    return Fios::Submit(attr, std::move(info), [handle](Fios::Completion& completion) {
        const auto file = Fios::RemoveFile(handle);
        if (!file) {
            completion.error = SCE_FIOS_ERROR_BAD_FH;
            return;
        }
        const std::lock_guard lock(file->mutex);
        completion.error = Fios::FromKernelError(sceKernelClose(file->descriptor));
        file->descriptor = -1;
    });
}

SceFiosOp StartStat(const SceFiosOpAttr* attr, SceFiosFH handle, SceFiosStat* outStatus) {
    return SubmitHandle(attr, handle, 0, 0, nullptr, SCE_FIOS_OK, [outStatus](Fios::FileHandle& file, Fios::Completion& completion) {
        SceFiosStat stat{};
        if (const std::int32_t error = Fios::StatDescriptor(file.descriptor, stat)) completion.error = error;
        else if (outStatus == nullptr || !Fios::CopyOut(outStatus, &stat, sizeof(stat))) completion.error = SCE_FIOS_ERROR_BAD_PTR;
    });
}

SceFiosOp StartTruncate(const SceFiosOpAttr* attr, SceFiosFH handle, SceFiosSize length) {
    return SubmitHandle(attr, handle, length, 0, nullptr, length < 0 ? SCE_FIOS_ERROR_BAD_SIZE : SCE_FIOS_OK, [length](Fios::FileHandle& file, Fios::Completion& completion) {
        if (!CanWrite(file)) completion.error = SCE_FIOS_ERROR_READ_ONLY;
        else completion.error = Fios::FromKernelError(sceKernelFtruncate(file.descriptor, length));
    });
}

SceFiosOp StartSync(const SceFiosOpAttr* attr, SceFiosFH handle) {
    return SubmitHandle(attr, handle, 0, 0, nullptr, SCE_FIOS_OK, [](Fios::FileHandle& file, Fios::Completion& completion) {
        completion.error = Fios::FromKernelError(sceKernelFsync(file.descriptor));
    });
}

int StatusSync(SceFiosOp op) {
    return Fios::SyncStatus(op);
}

std::int64_t CountSync(SceFiosOp op) {
    return Fios::SyncCount(op);
}

}

extern "C" {

SceFiosOp APS5_VABI sceFiosFHOpen(const SceFiosOpAttr* attr, SceFiosFH* outFH, const char* path, const SceFiosOpenParams* openParams) {
    return StartOpen(attr, outFH, path, openParams, DefaultNativeMode);
}

int APS5_VABI sceFiosFHOpenSync(const SceFiosOpAttr* attr, SceFiosFH* outFH, const char* path, const SceFiosOpenParams* openParams) {
    return StatusSync(StartOpen(attr, outFH, path, openParams, DefaultNativeMode));
}

SceFiosOp APS5_VABI sceFiosFHOpenWithMode(const SceFiosOpAttr* attr, SceFiosFH* outFH, const char* path, const SceFiosOpenParams* openParams, std::int32_t nativeMode) {
    return StartOpen(attr, outFH, path, openParams, nativeMode);
}

int APS5_VABI sceFiosFHOpenWithModeSync(const SceFiosOpAttr* attr, SceFiosFH* outFH, const char* path, const SceFiosOpenParams* openParams, std::int32_t nativeMode) {
    return StatusSync(StartOpen(attr, outFH, path, openParams, nativeMode));
}

SceFiosOp APS5_VABI sceFiosFHClose(const SceFiosOpAttr* attr, SceFiosFH fh) {
    return StartClose(attr, fh);
}

int APS5_VABI sceFiosFHCloseSync(const SceFiosOpAttr* attr, SceFiosFH fh) {
    return StatusSync(StartClose(attr, fh));
}

SceFiosOp APS5_VABI sceFiosFHStat(const SceFiosOpAttr* attr, SceFiosFH fh, SceFiosStat* outStatus) {
    return StartStat(attr, fh, outStatus);
}

int APS5_VABI sceFiosFHStatSync(const SceFiosOpAttr* attr, SceFiosFH fh, SceFiosStat* outStatus) {
    return StatusSync(StartStat(attr, fh, outStatus));
}

SceFiosOp APS5_VABI sceFiosFHTruncate(const SceFiosOpAttr* attr, SceFiosFH fh, SceFiosSize length) {
    return StartTruncate(attr, fh, length);
}

int APS5_VABI sceFiosFHTruncateSync(const SceFiosOpAttr* attr, SceFiosFH fh, SceFiosSize length) {
    return StatusSync(StartTruncate(attr, fh, length));
}

SceFiosOp APS5_VABI sceFiosFHSync(const SceFiosOpAttr* attr, SceFiosFH fh) {
    return StartSync(attr, fh);
}

int APS5_VABI sceFiosFHSyncSync(const SceFiosOpAttr* attr, SceFiosFH fh) {
    return StatusSync(StartSync(attr, fh));
}

SceFiosOp APS5_VABI sceFiosFHRead(const SceFiosOpAttr* attr, SceFiosFH fh, void* buffer, SceFiosSize length) {
    return StartTransfer(attr, fh, buffer, length, 0, false, false);
}

SceFiosSize APS5_VABI sceFiosFHReadSync(const SceFiosOpAttr* attr, SceFiosFH fh, void* buffer, SceFiosSize length) {
    return CountSync(StartTransfer(attr, fh, buffer, length, 0, false, false));
}

SceFiosOp APS5_VABI sceFiosFHWrite(const SceFiosOpAttr* attr, SceFiosFH fh, const void* buffer, SceFiosSize length) {
    return StartTransfer(attr, fh, const_cast<void*>(buffer), length, 0, false, true);
}

SceFiosSize APS5_VABI sceFiosFHWriteSync(const SceFiosOpAttr* attr, SceFiosFH fh, const void* buffer, SceFiosSize length) {
    return CountSync(StartTransfer(attr, fh, const_cast<void*>(buffer), length, 0, false, true));
}

SceFiosOp APS5_VABI sceFiosFHPread(const SceFiosOpAttr* attr, SceFiosFH fh, void* buffer, SceFiosSize length, SceFiosOffset offset) {
    return StartTransfer(attr, fh, buffer, length, offset, true, false);
}

SceFiosSize APS5_VABI sceFiosFHPreadSync(const SceFiosOpAttr* attr, SceFiosFH fh, void* buffer, SceFiosSize length, SceFiosOffset offset) {
    return CountSync(StartTransfer(attr, fh, buffer, length, offset, true, false));
}

SceFiosOp APS5_VABI sceFiosFHPwrite(const SceFiosOpAttr* attr, SceFiosFH fh, const void* buffer, SceFiosSize length, SceFiosOffset offset) {
    return StartTransfer(attr, fh, const_cast<void*>(buffer), length, offset, true, true);
}

SceFiosSize APS5_VABI sceFiosFHPwriteSync(const SceFiosOpAttr* attr, SceFiosFH fh, const void* buffer, SceFiosSize length, SceFiosOffset offset) {
    return CountSync(StartTransfer(attr, fh, const_cast<void*>(buffer), length, offset, true, true));
}

SceFiosOp APS5_VABI sceFiosFHReadv(const SceFiosOpAttr* attr, SceFiosFH fh, const SceFiosBuffer* iov, int iovcnt) {
    return StartVectors(attr, fh, iov, iovcnt, 0, false, false);
}

SceFiosSize APS5_VABI sceFiosFHReadvSync(const SceFiosOpAttr* attr, SceFiosFH fh, const SceFiosBuffer* iov, int iovcnt) {
    return CountSync(StartVectors(attr, fh, iov, iovcnt, 0, false, false));
}

SceFiosOp APS5_VABI sceFiosFHWritev(const SceFiosOpAttr* attr, SceFiosFH fh, const SceFiosBuffer* iov, int iovcnt) {
    return StartVectors(attr, fh, iov, iovcnt, 0, false, true);
}

SceFiosSize APS5_VABI sceFiosFHWritevSync(const SceFiosOpAttr* attr, SceFiosFH fh, const SceFiosBuffer* iov, int iovcnt) {
    return CountSync(StartVectors(attr, fh, iov, iovcnt, 0, false, true));
}

SceFiosOp APS5_VABI sceFiosFHPreadv(const SceFiosOpAttr* attr, SceFiosFH fh, const SceFiosBuffer* iov, int iovcnt, SceFiosOffset offset) {
    return StartVectors(attr, fh, iov, iovcnt, offset, true, false);
}

SceFiosSize APS5_VABI sceFiosFHPreadvSync(const SceFiosOpAttr* attr, SceFiosFH fh, const SceFiosBuffer* iov, int iovcnt, SceFiosOffset offset) {
    return CountSync(StartVectors(attr, fh, iov, iovcnt, offset, true, false));
}

SceFiosOp APS5_VABI sceFiosFHPwritev(const SceFiosOpAttr* attr, SceFiosFH fh, const SceFiosBuffer* iov, int iovcnt, SceFiosOffset offset) {
    return StartVectors(attr, fh, iov, iovcnt, offset, true, true);
}

SceFiosSize APS5_VABI sceFiosFHPwritevSync(const SceFiosOpAttr* attr, SceFiosFH fh, const SceFiosBuffer* iov, int iovcnt, SceFiosOffset offset) {
    return CountSync(StartVectors(attr, fh, iov, iovcnt, offset, true, true));
}

SceFiosOffset APS5_VABI sceFiosFHSeek(SceFiosFH fh, SceFiosOffset offset, std::int32_t whence) {
    if (whence < SCE_FIOS_SEEK_SET || whence > SCE_FIOS_SEEK_END) return SCE_FIOS_ERROR_BAD_OFFSET;
    const auto file = Fios::FindFile(fh);
    if (!file) return SCE_FIOS_ERROR_BAD_FH;
    const std::lock_guard lock(file->mutex);
    if (file->descriptor < 0) return SCE_FIOS_ERROR_BAD_FH;
    const std::int64_t position = sceKernelLseek(file->descriptor, offset, whence);
    return position < 0 ? Fios::FromKernelError(position) : position;
}

SceFiosOffset APS5_VABI sceFiosFHTell(SceFiosFH fh) {
    return sceFiosFHSeek(fh, 0, SCE_FIOS_SEEK_CUR);
}

SceFiosSize APS5_VABI sceFiosFHGetSize(SceFiosFH fh) {
    const auto file = Fios::FindFile(fh);
    if (!file) return SCE_FIOS_ERROR_BAD_FH;
    const std::lock_guard lock(file->mutex);
    SceFiosStat stat{};
    if (file->descriptor < 0) return SCE_FIOS_ERROR_BAD_FH;
    if (const std::int32_t error = Fios::StatDescriptor(file->descriptor, stat)) return error;
    return stat.fileSize;
}

const char* APS5_VABI sceFiosFHGetPath(SceFiosFH fh) {
    const auto file = Fios::FindFile(fh);
    return file ? file->path.c_str() : nullptr;
}

const SceFiosOpenParams* APS5_VABI sceFiosFHGetOpenParams(SceFiosFH fh) {
    const auto file = Fios::FindFile(fh);
    return file ? &file->params : nullptr;
}

}
