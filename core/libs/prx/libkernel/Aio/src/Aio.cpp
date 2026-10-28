#include "prx/libkernel/include/Api.hpp"
#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <stdexcept>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/PreciseWait.hpp"
#include "prx/libkernel/File/include/ArchivedFile.hpp"
#include "prx/libkernel/File/include/DirectoryDescriptor.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/File/include/SharedMemory.hpp"
#include "prx/libkernel/KernelErrors.hpp"

#ifdef ANYPS5_AIO_TESTING
#include <atomic>
#include "tests/AioTest.hpp"
#endif

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {

constexpr std::int32_t AioProcessing = 2;
constexpr std::int32_t AioCompleted = 3;
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
struct RequestState {
    std::int32_t id = 0;
    std::int32_t state = 0;
};
std::array<RequestState, AioMaxQueues - 1> g_requests{};
std::int64_t g_nextId = 1;

#ifdef ANYPS5_AIO_TESTING
std::atomic<std::int32_t> g_processingWaiters{0};

class ProcessingWaiter {
public:
    ~ProcessingWaiter() {
        if (observed) g_processingWaiters.fetch_sub(1);
    }

    void Observe() {
        if (observed) return;
        g_processingWaiters.fetch_add(1);
        observed = true;
    }

private:
    bool observed = false;
};
#endif

RequestState* FindLocked(std::int32_t id) {
    if (id <= 0) return nullptr;
    for (auto& request : g_requests) {
        if (request.id == id) return &request;
    }
    return nullptr;
}

class Reservation {
public:
    explicit Reservation(std::int32_t count) : count(count) {
        std::lock_guard lock(g_mutex);
        std::array<RequestState*, AioRequestNumMax> slots{};
        std::int32_t available = 0;
        for (auto& request : g_requests) {
            if (request.id == 0) slots[available++] = &request;
            if (available == count) break;
        }
        if (available != count) throw std::runtime_error("AIO: host request registry exhausted");
        if (g_nextId + count - 1 > std::numeric_limits<std::int32_t>::max())
            throw std::runtime_error("AIO: host request identities exhausted");
        for (std::int32_t index = 0; index < count; ++index) {
            ids[index] = static_cast<std::int32_t>(g_nextId++);
            *slots[index] = {ids[index], AioProcessing};
        }
    }

    ~Reservation() {
        if (published) return;
        std::lock_guard lock(g_mutex);
        for (std::int32_t index = 0; index < count; ++index) {
            if (auto* request = FindLocked(ids[index])) *request = {};
        }
    }

    Reservation(const Reservation&) = delete;
    Reservation& operator=(const Reservation&) = delete;

    void Publish(std::int32_t* output) {
        std::lock_guard lock(g_mutex);
        for (std::int32_t index = 0; index < count; ++index) {
            output[index] = ids[index];
            FindLocked(ids[index])->state = AioCompleted;
        }
        published = true;
    }

private:
    std::array<std::int32_t, AioRequestNumMax> ids{};
    std::int32_t count;
    bool published = false;
};

std::int32_t StateLocked(std::int32_t id) {
    const auto* request = FindLocked(id);
    return request != nullptr ? request->state : SCE_KERNEL_ERROR_ESRCH;
}

std::int32_t ReportLocked(std::int32_t id) {
    const std::int32_t state = StateLocked(id);
    if (state != SCE_KERNEL_ERROR_ESRCH && state != AioProcessing) FindLocked(id)->state |= AioReported;
    return state;
}

std::int32_t CancelLocked(std::int32_t id) {
    const std::int32_t state = StateLocked(id);
    if (state == SCE_KERNEL_ERROR_ESRCH) return state;
    return state & ~AioReported;
}

std::int32_t DeleteLocked(std::int32_t id) {
    auto* request = FindLocked(id);
    if (request == nullptr) return SCE_KERNEL_ERROR_ESRCH;
    *request = {};
    return 0;
}

void RequireFinishedLocked(std::int32_t id, const char* function) {
    if (StateLocked(id) == AioProcessing) NotImplemented_nid_no_patch(function);
}

std::int64_t NativePread(std::int32_t fd, void* buf, std::size_t nbyte, std::int64_t offset) {
    if (File::IsVirtualDirectoryDescriptor(fd)) { errno = EISDIR; return -1; }
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
    if (File::IsVirtualDirectoryDescriptor(fd)) { errno = EISDIR; return -1; }
    if (offset < 0) {
        errno = EINVAL;
        return -1;
    }
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(buf, nbyte);
#ifdef _WIN32
    return NativePositioned_nid_no_patch(fd, const_cast<void*>(buf), nbyte, offset, true);
#else
    const auto result = File::PwriteSharedMemory_nid_no_patch(fd, buf, nbyte, offset);
    if (result != -2) return result;
#ifdef __APPLE__
    const iovec whole{const_cast<void*>(buf), nbyte};
    if (const auto appended = File::WritevAtEnd(fd, &whole, 1); appended != -2) return appended;
#endif
    return static_cast<std::int64_t>(::pwrite(fd, buf, nbyte, static_cast<off_t>(offset)));
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
        if (error == EISDIR) req.result->return_value = static_cast<std::int64_t>(SCE_KERNEL_ERROR_EISDIR);
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
    Reservation reservation(1);
    for (std::int32_t i = 0; i < size; ++i) RunRequest(req[i], write);
    reservation.Publish(id);
    return 0;
}

int SubmitCommandsMultiple(KernelAioRwRequest* req, std::int32_t size, std::int32_t prio, bool write, std::int32_t* id) {
    const int error = ValidateRequests(req, size, prio, id);
    if (error != 0) return error;
    Reservation reservation(size);
    for (std::int32_t i = 0; i < size; ++i) {
        RunRequest(req[i], write);
    }
    reservation.Publish(id);
    return 0;
}

int ValidateIds(const std::int32_t* id, std::int32_t num, const void* out) {
    if (id == nullptr || out == nullptr) return SCE_KERNEL_ERROR_EFAULT;
    if (num <= 0 || num > AioIdNumMax) return SCE_KERNEL_ERROR_EINVAL;
    return 0;
}

}

extern "C" {

#ifdef ANYPS5_AIO_TESTING
std::int32_t AioProcessingWaitersForTest_nid_no_patch() {
    return g_processingWaiters.load();
}

void AioAdvanceIdentityForTest_nid_no_patch(std::int64_t next) {
    std::lock_guard lock(g_mutex);
    if (next < g_nextId || next > static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()) + 1)
        throw std::invalid_argument("AIO: invalid test identity advance");
    g_nextId = next;
}
#endif

int APS5_VABI sceKernelAioDeleteRequest(int32_t id, int32_t* ret) {
    if (ret == nullptr) return SCE_KERNEL_ERROR_EFAULT;
    std::lock_guard<std::mutex> lock(g_mutex);
    RequireFinishedLocked(id, __func__);
    *ret = DeleteLocked(id);
    return 0;
}

int APS5_VABI sceKernelAioDeleteRequests(int32_t* id, int32_t num, int32_t* ret) {
    const int error = ValidateIds(id, num, ret);
    if (error != 0) return error;
    std::lock_guard<std::mutex> lock(g_mutex);
    for (int32_t i = 0; i < num; ++i) RequireFinishedLocked(id[i], __func__);
    for (int32_t i = 0; i < num; ++i) ret[i] = DeleteLocked(id[i]);
    return 0;
}

int APS5_VABI sceKernelAioCancelRequest(int32_t id, int32_t* state) {
    if (state == nullptr) return SCE_KERNEL_ERROR_EFAULT;
    std::lock_guard<std::mutex> lock(g_mutex);
    RequireFinishedLocked(id, __func__);
    *state = CancelLocked(id);
    return 0;
}

int APS5_VABI sceKernelAioCancelRequests(int32_t* id, int32_t num, int32_t* state) {
    const int error = ValidateIds(id, num, state);
    if (error != 0) return error;
    std::lock_guard<std::mutex> lock(g_mutex);
    for (int32_t i = 0; i < num; ++i) RequireFinishedLocked(id[i], __func__);
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
#ifdef ANYPS5_AIO_TESTING
    ProcessingWaiter waiter;
#endif
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
#ifdef ANYPS5_AIO_TESTING
        waiter.Observe();
#endif
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
#ifdef ANYPS5_AIO_TESTING
    ProcessingWaiter waiter;
#endif
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
#ifdef ANYPS5_AIO_TESTING
        waiter.Observe();
#endif
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
