#include "prx/libkernel/include/Api.hpp"
#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/PreciseWait.hpp"
#include "prx/libkernel/File/include/ArchivedFile.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/File/include/SharedMemory.hpp"
#include "prx/libkernel/KernelErrors.hpp"

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {

constexpr std::int32_t AioProcessing = 2;
constexpr std::int32_t AioCompleted = 3;
constexpr std::int32_t AioAborted = 4;
constexpr std::int32_t AioReported = 0x10000;
constexpr std::int32_t AioMaxQueues = 512;
constexpr std::int32_t AioRequestNumMax = 128;
constexpr std::int32_t AioIdNumMax = 128;
constexpr std::uint32_t AioWaitAnd = 1;
constexpr std::uint32_t AioWaitOr = 2;
constexpr std::int32_t AioSchedulingWindowMax = 128;
constexpr std::int32_t AioDelayedCountMax = 128;
constexpr std::uint32_t AioSplitSizeMax = 0x1000000;
constexpr std::uint32_t AioSplitChunkSizeMax = 0x1000000;
constexpr std::int32_t AioPriorityLow = 1;
constexpr std::int32_t AioPriorityHigh = 3;
constexpr std::int32_t AioDefaultSchedulingWindow = 32;
constexpr std::int32_t AioDefaultDelayedCountLimit = 32;
constexpr std::uint32_t AioDefaultSplitSize = 0x100000;

std::mutex g_mutex;
std::array<std::int32_t, AioMaxQueues> g_states{};
std::int32_t g_nextId = 1;

bool ValidId(std::int32_t id) {
    return id > 0 && id < AioMaxQueues;
}

std::int32_t AllocateId() {
    std::lock_guard<std::mutex> lock(g_mutex);
    const std::int32_t id = g_nextId;
    g_nextId = (g_nextId + 1) % AioMaxQueues;
    if (g_nextId == 0) g_nextId = 1;
    g_states[id] = AioProcessing;
    return id;
}

void SetState(std::int32_t id, std::int32_t state) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_states[id] = state;
}

std::int32_t StateLocked(std::int32_t id) {
    return ValidId(id) && g_states[id] != 0 ? g_states[id] : SCE_KERNEL_ERROR_ESRCH;
}

std::int32_t ReportLocked(std::int32_t id) {
    const std::int32_t state = StateLocked(id);
    if (state != SCE_KERNEL_ERROR_ESRCH && state != AioProcessing) g_states[id] |= AioReported;
    return state;
}

std::int32_t CancelLocked(std::int32_t id) {
    const std::int32_t state = StateLocked(id);
    if (state == SCE_KERNEL_ERROR_ESRCH) return state;
    if (state != AioProcessing) return state & ~AioReported;
    g_states[id] = AioAborted;
    return AioAborted;
}

std::int32_t DeleteLocked(std::int32_t id) {
    if (StateLocked(id) == SCE_KERNEL_ERROR_ESRCH) return SCE_KERNEL_ERROR_ESRCH;
    g_states[id] = 0;
    return 0;
}

std::int64_t NativePread(std::int32_t fd, void* buf, std::size_t nbyte, std::int64_t offset) {
    const GuestArena::HostWrite destination(buf, nbyte);
    if (!destination.Open()) {
        errno = EFAULT;
        return -1;
    }
    if (const auto archived = File::ReadArchivedFile(fd, buf, nbyte, &offset)) {
        if (*archived >= 0) return *archived;
        errno = static_cast<int>(-*archived);
        return -1;
    }
#ifdef _WIN32
    if (offset < 0) {
        errno = EINVAL;
        return -1;
    }
    return NativePositioned_nid_no_patch(fd, buf, nbyte, offset, false);
#else
    return static_cast<std::int64_t>(::pread(fd, buf, nbyte, static_cast<off_t>(offset)));
#endif
}

std::int64_t NativePwrite(std::int32_t fd, const void* buf, std::size_t nbyte, std::int64_t offset) {
    if (offset < 0) {
        errno = EINVAL;
        return -1;
    }
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(buf, nbyte);
#ifdef _WIN32
    return NativePositioned_nid_no_patch(fd, const_cast<void*>(buf), nbyte, offset, true);
#else
    const auto result = File::PwriteSharedMemory_nid_no_patch(fd, buf, nbyte, offset);
    return result == -2 ? static_cast<std::int64_t>(::pwrite(fd, buf, nbyte, static_cast<off_t>(offset))) : result;
#endif
}

void RunRequest(KernelAioRwRequest& req, bool write) {
    const std::int64_t done = write
        ? NativePwrite(req.fd, req.buf, req.nbyte, req.offset)
        : NativePread(req.fd, req.buf, req.nbyte, req.offset);
    if (done < 0) {
        const int error = errno;
        req.result->return_value = static_cast<std::int64_t>(SCE_KERNEL_ERROR_EIO);
        if (error == EBADF) req.result->return_value = static_cast<std::int64_t>(SCE_KERNEL_ERROR_EBADF);
        if (error == EFAULT) req.result->return_value = static_cast<std::int64_t>(SCE_KERNEL_ERROR_EFAULT);
        if (error == EINVAL) req.result->return_value = static_cast<std::int64_t>(SCE_KERNEL_ERROR_EINVAL);
        req.result->state = AioCompleted;
        return;
    }
    req.result->return_value = done;
    req.result->state = AioCompleted;
}

int ValidateRequests(KernelAioRwRequest* req, std::int32_t size, std::int32_t prio, const void* id) {
    if (req == nullptr || id == nullptr) return SCE_KERNEL_ERROR_EFAULT;
    if (size <= 0 || size > AioRequestNumMax || prio < AioPriorityLow || prio > AioPriorityHigh) return SCE_KERNEL_ERROR_EINVAL;
    for (std::int32_t i = 0; i < size; ++i) {
        if (req[i].result == nullptr) return SCE_KERNEL_ERROR_EFAULT;
    }
    return 0;
}

int SubmitCommands(KernelAioRwRequest* req, std::int32_t size, std::int32_t prio, bool write, std::int32_t* id) {
    const int error = ValidateRequests(req, size, prio, id);
    if (error != 0) return error;
    const std::int32_t queue = AllocateId();
    for (std::int32_t i = 0; i < size; ++i) RunRequest(req[i], write);
    SetState(queue, AioCompleted);
    *id = queue;
    return 0;
}

int SubmitCommandsMultiple(KernelAioRwRequest* req, std::int32_t size, std::int32_t prio, bool write, std::int32_t* id) {
    const int error = ValidateRequests(req, size, prio, id);
    if (error != 0) return error;
    for (std::int32_t i = 0; i < size; ++i) {
        const std::int32_t queue = AllocateId();
        RunRequest(req[i], write);
        SetState(queue, AioCompleted);
        id[i] = queue;
    }
    return 0;
}

int ValidateIds(const std::int32_t* id, std::int32_t num, const void* out) {
    if (id == nullptr || out == nullptr) return SCE_KERNEL_ERROR_EFAULT;
    if (num <= 0 || num > AioIdNumMax) return SCE_KERNEL_ERROR_EINVAL;
    return 0;
}

}

extern "C" {

int APS5_VABI sceKernelAioDeleteRequest(int32_t id, int32_t* ret) {
    if (ret == nullptr) return SCE_KERNEL_ERROR_EFAULT;
    std::lock_guard<std::mutex> lock(g_mutex);
    *ret = DeleteLocked(id);
    return 0;
}

int APS5_VABI sceKernelAioDeleteRequests(int32_t* id, int32_t num, int32_t* ret) {
    const int error = ValidateIds(id, num, ret);
    if (error != 0) return error;
    std::lock_guard<std::mutex> lock(g_mutex);
    for (int32_t i = 0; i < num; ++i) ret[i] = DeleteLocked(id[i]);
    return 0;
}

int APS5_VABI sceKernelAioCancelRequest(int32_t id, int32_t* state) {
    if (state == nullptr) return SCE_KERNEL_ERROR_EFAULT;
    std::lock_guard<std::mutex> lock(g_mutex);
    *state = CancelLocked(id);
    return 0;
}

int APS5_VABI sceKernelAioCancelRequests(int32_t* id, int32_t num, int32_t* state) {
    const int error = ValidateIds(id, num, state);
    if (error != 0) return error;
    std::lock_guard<std::mutex> lock(g_mutex);
    for (int32_t i = 0; i < num; ++i) state[i] = CancelLocked(id[i]);
    return 0;
}

int APS5_VABI sceKernelAioInitializeImpl(void* param, int32_t size) {
    (void)param;
    (void)size;
    return 0;
}

void APS5_VABI sceKernelAioInitializeParam(void* param) {
    if (param == nullptr) throw std::invalid_argument("sceKernelAioInitializeParam: param is null");
    auto* scheduling = static_cast<KernelAioSchedulingParam*>(param);
    for (int index = 0; index < 3; ++index) {
        const bool split = index < 2;
        scheduling[index] = {AioDefaultSchedulingWindow, AioDefaultDelayedCountLimit, split ? 1u : 0u, split ? AioDefaultSplitSize : 0u, split ? AioDefaultSplitSize : 0u};
    }
}

int APS5_VABI sceKernelAioSetParam(KernelAioSchedulingParam* param, int32_t schedulingWindowSize, int32_t delayedCountLimit, uint32_t enableSplit, uint32_t splitSize, uint32_t splitChunkSize) {
    if (param == nullptr) throw std::invalid_argument("sceKernelAioSetParam: param is null");
    if (schedulingWindowSize < 0 || schedulingWindowSize > AioSchedulingWindowMax || delayedCountLimit < 0 || delayedCountLimit > AioDelayedCountMax || enableSplit > 1)
        return SCE_KERNEL_ERROR_EINVAL;
    if (enableSplit == 1 && (splitSize == 0 || splitSize > AioSplitSizeMax || splitChunkSize == 0 || splitChunkSize > AioSplitChunkSizeMax)) return SCE_KERNEL_ERROR_EINVAL;
    param->schedulingWindowSize = schedulingWindowSize;
    param->delayedCountLimit = delayedCountLimit;
    param->enableSplit = enableSplit;
    param->splitSize = splitSize;
    param->splitChunkSize = splitChunkSize;
    return 0;
}

int APS5_VABI sceKernelAioSubmitReadCommands(KernelAioRwRequest* req, int32_t size, int32_t prio, int32_t* id) {
    return SubmitCommands(req, size, prio, false, id);
}

int APS5_VABI sceKernelAioSubmitWriteCommands(KernelAioRwRequest* req, int32_t size, int32_t prio, int32_t* id) {
    return SubmitCommands(req, size, prio, true, id);
}

int APS5_VABI sceKernelAioSubmitReadCommandsMultiple(KernelAioRwRequest* req, int32_t size, int32_t prio, int32_t* id) {
    return SubmitCommandsMultiple(req, size, prio, false, id);
}

int APS5_VABI sceKernelAioSubmitWriteCommandsMultiple(KernelAioRwRequest* req, int32_t size, int32_t prio, int32_t* id) {
    return SubmitCommandsMultiple(req, size, prio, true, id);
}

int APS5_VABI sceKernelAioPollRequest(int32_t id, int32_t* state) {
    if (state == nullptr) return SCE_KERNEL_ERROR_EFAULT;
    std::lock_guard<std::mutex> lock(g_mutex);
    *state = ReportLocked(id);
    return 0;
}

int APS5_VABI sceKernelAioWaitRequests(int32_t* id, int32_t num, int32_t* state, uint32_t mode, uint32_t* usec) {
    const int error = ValidateIds(id, num, state);
    if (error != 0) return error;
    if (mode != AioWaitAnd && mode != AioWaitOr) return SCE_KERNEL_ERROR_EINVAL;
    const auto start = std::chrono::steady_clock::now();
    for (;;) {
        bool allDone = true;
        bool anyCompleted = false;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            for (int32_t i = 0; i < num; ++i) {
                const std::int32_t current = StateLocked(id[i]);
                if (current == AioProcessing) allDone = false;
                if ((current & ~AioReported) == AioCompleted) anyCompleted = true;
            }
            if (allDone || (mode == AioWaitOr && anyCompleted)) {
                for (int32_t i = 0; i < num; ++i) state[i] = ReportLocked(id[i]);
                return 0;
            }
        }
        if (usec != nullptr && *usec != 0) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
            if (elapsed > static_cast<std::int64_t>(*usec)) {
                std::lock_guard<std::mutex> lock(g_mutex);
                for (int32_t i = 0; i < num; ++i) state[i] = ReportLocked(id[i]);
                return SCE_KERNEL_ERROR_ETIMEDOUT;
            }
        }
        PreciseSleepUs(10);
    }
}

int APS5_VABI sceKernelAioWaitRequest(int32_t id, int32_t* state, uint32_t* usec) {
    if (state == nullptr) return SCE_KERNEL_ERROR_EFAULT;
    const auto start = std::chrono::steady_clock::now();
    for (;;) {
        std::int32_t current;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            current = ReportLocked(id);
        }
        if (current != AioProcessing) {
            *state = current;
            return 0;
        }
        if (usec != nullptr && *usec != 0) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
            if (elapsed > static_cast<std::int64_t>(*usec)) {
                *state = current;
                return SCE_KERNEL_ERROR_ETIMEDOUT;
            }
        }
        PreciseSleepUs(10);
    }
}

int APS5_VABI sceKernelAioPollRequests(int32_t* id, int32_t num, int32_t* state) {
    const int error = ValidateIds(id, num, state);
    if (error != 0) return error;
    std::lock_guard<std::mutex> lock(g_mutex);
    for (int32_t i = 0; i < num; ++i) state[i] = ReportLocked(id[i]);
    return 0;
}

}
