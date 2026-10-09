#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libkernel/KernelErrors.hpp"

#ifdef _WIN32
#include <io.h>
#include <limits>
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {

constexpr std::int32_t AioProcessing = 2;
constexpr std::int32_t AioCompleted = 3;
constexpr std::int32_t AioAborted = 4;
constexpr std::int32_t AioMaxQueues = 512;
constexpr std::int32_t AioRequestNumMax = 128;
constexpr std::int32_t AioIdNumMax = 128;
constexpr std::uint32_t AioWaitAnd = 1;
constexpr std::uint32_t AioWaitOr = 2;

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

#ifdef _WIN32
std::int64_t NativePositioned(std::int32_t fd, void* buf, std::size_t nbyte, std::int64_t offset, bool write, const char* name) {
    if (nbyte > static_cast<std::size_t>(std::numeric_limits<DWORD>::max())) {
        throw std::runtime_error(std::string(name) + ": nbytes exceeds platform limit");
    }
    if (offset < 0) {
        errno = EINVAL;
        return -1;
    }
    const auto handle = reinterpret_cast<HANDLE>(::_get_osfhandle(fd));
    if (handle == INVALID_HANDLE_VALUE) {
        errno = EBADF;
        return -1;
    }
    OVERLAPPED overlapped{};
    overlapped.Offset = static_cast<DWORD>(offset);
    overlapped.OffsetHigh = static_cast<DWORD>(static_cast<std::uint64_t>(offset) >> 32u);
    DWORD done = 0;
    const BOOL ok = write ? ::WriteFile(handle, buf, static_cast<DWORD>(nbyte), &done, &overlapped)
                          : ::ReadFile(handle, buf, static_cast<DWORD>(nbyte), &done, &overlapped);
    if (!ok) {
        if (!write && ::GetLastError() == ERROR_HANDLE_EOF) return 0;
        errno = EIO;
        return -1;
    }
    return done;
}
#endif

std::int64_t NativePread(std::int32_t fd, void* buf, std::size_t nbyte, std::int64_t offset) {
    const GuestArena::HostWrite destination(buf, nbyte);
    if (!destination.Open()) {
        errno = EFAULT;
        return -1;
    }
#ifdef _WIN32
    return NativePositioned(fd, buf, nbyte, offset, false, "sceKernelAioSubmitReadCommands");
#else
    return static_cast<std::int64_t>(::pread(fd, buf, nbyte, static_cast<off_t>(offset)));
#endif
}

std::int64_t NativePwrite(std::int32_t fd, const void* buf, std::size_t nbyte, std::int64_t offset) {
#ifdef _WIN32
    return NativePositioned(fd, const_cast<void*>(buf), nbyte, offset, true, "sceKernelAioSubmitWriteCommands");
#else
    return static_cast<std::int64_t>(::pwrite(fd, buf, nbyte, static_cast<off_t>(offset)));
#endif
}

bool RunRequest(KernelAioRwRequest& req, bool write) {
    const std::int64_t done = write
        ? NativePwrite(req.fd, req.buf, req.nbyte, req.offset)
        : NativePread(req.fd, req.buf, req.nbyte, req.offset);
    if (done < 0) {
        const int error = errno;
        req.result->return_value = static_cast<std::int64_t>(SCE_KERNEL_ERROR_EIO);
        if (error == EBADF) req.result->return_value = static_cast<std::int64_t>(SCE_KERNEL_ERROR_EBADF);
        if (error == EFAULT) req.result->return_value = static_cast<std::int64_t>(SCE_KERNEL_ERROR_EFAULT);
        req.result->state = AioAborted;
        return false;
    }
    req.result->return_value = done;
    req.result->state = AioCompleted;
    return true;
}

int ValidateRequests(KernelAioRwRequest* req, std::int32_t size, const void* id) {
    if (req == nullptr || id == nullptr) return SCE_KERNEL_ERROR_EFAULT;
    if (size <= 0) return SCE_KERNEL_ERROR_EINVAL;
    for (std::int32_t i = 0; i < size; ++i) {
        if (req[i].result == nullptr) return SCE_KERNEL_ERROR_EFAULT;
    }
    return 0;
}

int SubmitCommands(KernelAioRwRequest* req, std::int32_t size, bool write, std::int32_t* id) {
    const int error = ValidateRequests(req, size, id);
    if (error != 0) return error;
    const std::int32_t queue = AllocateId();
    bool aborted = false;
    for (std::int32_t i = 0; i < size; ++i) {
        if (!RunRequest(req[i], write)) aborted = true;
    }
    SetState(queue, aborted ? AioAborted : AioCompleted);
    *id = queue;
    return 0;
}

int SubmitCommandsMultiple(KernelAioRwRequest* req, std::int32_t size, bool write, std::int32_t* id, const char* name) {
    const int error = ValidateRequests(req, size, id);
    if (error != 0) return error;
    if (size > AioRequestNumMax) throw std::runtime_error(std::string(name) + ": more than 128 requests per batch is not modelled");
    for (std::int32_t i = 0; i < size; ++i) {
        const std::int32_t queue = AllocateId();
        SetState(queue, RunRequest(req[i], write) ? AioCompleted : AioAborted);
        id[i] = queue;
    }
    return 0;
}

int ValidateIds(const std::int32_t* id, std::int32_t num, const void* out, const char* name) {
    if (id == nullptr || out == nullptr) return SCE_KERNEL_ERROR_EFAULT;
    if (num < 0) return SCE_KERNEL_ERROR_EINVAL;
    if (num > AioIdNumMax) throw std::runtime_error(std::string(name) + ": more than 128 ids per batch is not modelled");
    for (std::int32_t i = 0; i < num; ++i) {
        if (!ValidId(id[i])) return SCE_KERNEL_ERROR_EINVAL;
    }
    return 0;
}

}

extern "C" {

int APS5_VABI sceKernelAioDeleteRequest(int32_t id, int32_t* ret) {
    if (ret == nullptr) return SCE_KERNEL_ERROR_EFAULT;
    if (!ValidId(id)) return SCE_KERNEL_ERROR_EINVAL;
    SetState(id, AioAborted);
    *ret = 0;
    return 0;
}

int APS5_VABI sceKernelAioDeleteRequests(int32_t* id, int32_t num, int32_t* ret) {
    const int error = ValidateIds(id, num, ret, "sceKernelAioDeleteRequests");
    if (error != 0) return error;
    for (int32_t i = 0; i < num; ++i) {
        SetState(id[i], AioAborted);
        ret[i] = 0;
    }
    return 0;
}

int APS5_VABI sceKernelAioCancelRequest(int32_t id, int32_t* state) {
    if (state == nullptr) return SCE_KERNEL_ERROR_EFAULT;
    if (id == 0) {
        *state = AioProcessing;
        return 0;
    }
    if (!ValidId(id)) return SCE_KERNEL_ERROR_EINVAL;
    SetState(id, AioAborted);
    *state = AioAborted;
    return 0;
}

int APS5_VABI sceKernelAioCancelRequests(int32_t* id, int32_t num, int32_t* state) {
    if (id == nullptr || state == nullptr) return SCE_KERNEL_ERROR_EFAULT;
    if (num < 0) return SCE_KERNEL_ERROR_EINVAL;
    if (num > AioIdNumMax) throw std::runtime_error("sceKernelAioCancelRequests: more than 128 ids per batch is not modelled");
    for (int32_t i = 0; i < num; ++i) {
        if (id[i] != 0 && !ValidId(id[i])) return SCE_KERNEL_ERROR_EINVAL;
    }
    for (int32_t i = 0; i < num; ++i) {
        if (id[i] == 0) {
            state[i] = AioProcessing;
            continue;
        }
        SetState(id[i], AioAborted);
        state[i] = AioAborted;
    }
    return 0;
}

int APS5_VABI sceKernelAioInitializeImpl(void* param, int32_t size) {
    (void)param;
    (void)size;
    return 0;
}

void APS5_VABI sceKernelAioInitializeParam(void* param) {
    if (param == nullptr) throw std::invalid_argument("sceKernelAioInitializeParam: param is null");
}

int APS5_VABI sceKernelAioSubmitReadCommands(KernelAioRwRequest* req, int32_t size, int32_t prio, int32_t* id) {
    (void)prio;
    return SubmitCommands(req, size, false, id);
}

int APS5_VABI sceKernelAioSubmitWriteCommands(KernelAioRwRequest* req, int32_t size, int32_t prio, int32_t* id) {
    (void)prio;
    return SubmitCommands(req, size, true, id);
}

int APS5_VABI sceKernelAioSubmitReadCommandsMultiple(KernelAioRwRequest* req, int32_t size, int32_t prio, int32_t* id) {
    (void)prio;
    return SubmitCommandsMultiple(req, size, false, id, "sceKernelAioSubmitReadCommandsMultiple");
}

int APS5_VABI sceKernelAioSubmitWriteCommandsMultiple(KernelAioRwRequest* req, int32_t size, int32_t prio, int32_t* id) {
    (void)prio;
    return SubmitCommandsMultiple(req, size, true, id, "sceKernelAioSubmitWriteCommandsMultiple");
}

int APS5_VABI sceKernelAioPollRequest(int32_t id, int32_t* state) {
    if (state == nullptr) return SCE_KERNEL_ERROR_EFAULT;
    if (!ValidId(id)) return SCE_KERNEL_ERROR_EINVAL;
    std::lock_guard<std::mutex> lock(g_mutex);
    *state = g_states[id];
    return 0;
}

int APS5_VABI sceKernelAioWaitRequests(int32_t* id, int32_t num, int32_t* state, uint32_t mode, uint32_t* usec) {
    const int error = ValidateIds(id, num, state, "sceKernelAioWaitRequests");
    if (error != 0) return error;
    if (mode != AioWaitAnd && mode != AioWaitOr) throw std::runtime_error("sceKernelAioWaitRequests: unsupported wait mode");
    const auto start = std::chrono::steady_clock::now();
    for (;;) {
        bool allDone = true;
        bool anyCompleted = false;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            for (int32_t i = 0; i < num; ++i) {
                state[i] = g_states[id[i]];
                if (state[i] == AioProcessing) allDone = false;
                if (state[i] == AioCompleted) anyCompleted = true;
            }
        }
        if (allDone || (mode == AioWaitOr && anyCompleted)) return 0;
        if (usec != nullptr && *usec != 0) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
            if (elapsed > static_cast<std::int64_t>(*usec)) return SCE_KERNEL_ERROR_ETIMEDOUT;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
}

int APS5_VABI sceKernelAioWaitRequest(int32_t id, int32_t* state, uint32_t* usec) {
    if (state == nullptr) return SCE_KERNEL_ERROR_EFAULT;
    if (!ValidId(id)) return SCE_KERNEL_ERROR_EINVAL;
    const auto start = std::chrono::steady_clock::now();
    for (;;) {
        std::int32_t current;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            current = g_states[id];
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
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
}

int APS5_VABI sceKernelAioPollRequests(int32_t* id, int32_t num, int32_t* state) {
    const int error = ValidateIds(id, num, state, "sceKernelAioPollRequests");
    if (error != 0) return error;
    std::lock_guard<std::mutex> lock(g_mutex);
    for (int32_t i = 0; i < num; ++i) {
        state[i] = g_states[id[i]];
    }
    return 0;
}

}
