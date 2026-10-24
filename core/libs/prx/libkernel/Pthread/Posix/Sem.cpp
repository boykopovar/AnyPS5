#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "Common.hpp"
#include "../include/Cancel.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"
#include <cerrno>
#include <chrono>
#include <cstdarg>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>

// The guest sem_t is at least 8 bytes; its first word holds a pointer to the host semaphore.
struct PosixSemaphore {
    std::mutex lock;
    TimedWait::Condition available;
    unsigned int count;
};

static constexpr int GUEST_ENOENT = 2;
static constexpr int GUEST_EEXIST = 17;
static constexpr int GUEST_EAGAIN = 35;
static constexpr int GUEST_ENAMETOOLONG = 63;
static constexpr int GUEST_EOVERFLOW = 84;
static constexpr unsigned int GUEST_SEM_VALUE_MAX = 0x7fffffffu;
static constexpr int GUEST_REALTIME_CLOCK = 0;
static constexpr int GUEST_O_CREAT = 0x200;
static constexpr int GUEST_O_EXCL = 0x800;
static constexpr std::size_t GUEST_PATH_MAX = 1024;
static constexpr std::size_t GUEST_NAME_MAX = 255;
static constexpr std::string_view NAMED_SEMAPHORE_PREFIX = "/tmp/SEMD";
static constexpr std::string_view NAMED_SEMAPHORE_FILE_PREFIX = "SEMD";

static int Fail(int error) {
    errno = error;
    return -1;
}

static PosixSemaphore* Get(void* sem) {
    if (!sem) return nullptr;
    PosixSemaphore* semaphore = nullptr;
    std::memcpy(&semaphore, sem, sizeof(semaphore));
    return semaphore;
}

struct NamedSemaphoreMapping {
    PosixSemaphore* semaphore;
    std::uint64_t reserved;
};

struct NamedSemaphore {
    std::unique_ptr<PosixSemaphore> semaphore;
    std::unique_ptr<NamedSemaphoreMapping> mapping;
    int openCount = 0;
};

struct NamedSemaphores {
    std::mutex lock;
    std::map<std::string, std::shared_ptr<NamedSemaphore>> linked;
    std::map<const void*, std::shared_ptr<NamedSemaphore>> open;
};

static NamedSemaphores& Named() {
    static NamedSemaphores named;
    return named;
}

static bool IsNamed(const void* sem) {
    auto& named = Named();
    std::lock_guard lock(named.lock);
    return named.open.count(sem) != 0;
}

static int NamedPathError(const char* function, const char* name, int notAbsolute) {
    if (!name) throw std::invalid_argument(std::string(function) + ": null name");
    if (name[0] != '/') return notAbsolute;
    if (NAMED_SEMAPHORE_PREFIX.size() + std::strlen(name + 1) >= GUEST_PATH_MAX) return GUEST_ENAMETOOLONG;
    return 0;
}

static int NamedFileError(const char* function, const char* name) {
    const std::string_view file(name + 1);
    if (file.find('/') != std::string_view::npos)
        throw std::runtime_error(std::string(function) + ": the name " + name + " would resolve below /tmp, which is not modelled");
    if (NAMED_SEMAPHORE_FILE_PREFIX.size() + file.size() > GUEST_NAME_MAX) return GUEST_ENAMETOOLONG;
    return 0;
}

static int WaitFor(PosixSemaphore* semaphore, const KernelUseconds* usec) {
    ThreadCancel::Check();
    std::unique_lock lock(semaphore->lock);
    const auto ready = [&] { return semaphore->count > 0; };
    if (usec) {
        if (!ThreadCancel::WaitUntil(semaphore->available, lock, TimedWait::DeadlineNanos(*usec), ready)) return Fail(PosixThread::GUEST_ETIMEDOUT);
    } else {
        ThreadCancel::WaitUntil(semaphore->available, lock, std::nullopt, ready);
    }
    --semaphore->count;
    return 0;
}

extern "C" {

int APS5_VABI sem_init_nid_postfix(void* sem, int pshared, unsigned int value) {
    (void)pshared;
    if (!sem || value > GUEST_SEM_VALUE_MAX) return Fail(PosixThread::GUEST_EINVAL);
    auto* semaphore = new PosixSemaphore();
    semaphore->count = value;
    std::memcpy(sem, &semaphore, sizeof(semaphore));
    return 0;
}

int APS5_VABI sem_destroy_nid_postfix(void* sem) {
    auto* semaphore = Get(sem);
    if (!semaphore || IsNamed(sem)) return Fail(PosixThread::GUEST_EINVAL);
    delete semaphore;
    std::memset(sem, 0, sizeof(semaphore));
    return 0;
}

int APS5_VABI sem_wait_nid_postfix(void* sem) {
    auto* semaphore = Get(sem);
    if (!semaphore) return Fail(PosixThread::GUEST_EINVAL);
    return WaitFor(semaphore, nullptr);
}

int APS5_VABI sem_trywait_nid_postfix(void* sem) {
    auto* semaphore = Get(sem);
    if (!semaphore) return Fail(PosixThread::GUEST_EINVAL);
    std::lock_guard lock(semaphore->lock);
    if (semaphore->count == 0) return Fail(GUEST_EAGAIN);
    --semaphore->count;
    return 0;
}

int APS5_VABI sem_timedwait_nid_postfix(void* sem, const KernelTimespec* abstime) {
    auto* semaphore = Get(sem);
    if (!semaphore) return Fail(PosixThread::GUEST_EINVAL);
    ThreadCancel::Check();
    {
        std::lock_guard lock(semaphore->lock);
        if (semaphore->count > 0) {
            --semaphore->count;
            return 0;
        }
    }
    if (!abstime) return WaitFor(semaphore, nullptr);
    KernelUseconds usec = 0;
    if (!PosixThread::RelativeMicroseconds(GUEST_REALTIME_CLOCK, abstime, &usec)) return Fail(PosixThread::GUEST_EINVAL);
    return WaitFor(semaphore, &usec);
}

int APS5_VABI sem_reltimedwait_np_nid_postfix(void* sem, uint32_t usec) {
    auto* semaphore = Get(sem);
    if (!semaphore) return Fail(PosixThread::GUEST_EINVAL);
    const KernelUseconds timeout = usec;
    return WaitFor(semaphore, &timeout);
}

int APS5_VABI sem_post_nid_postfix(void* sem) {
    auto* semaphore = Get(sem);
    if (!semaphore) return Fail(PosixThread::GUEST_EINVAL);
    {
        std::lock_guard lock(semaphore->lock);
        if (semaphore->count == GUEST_SEM_VALUE_MAX) return Fail(GUEST_EOVERFLOW);
        ++semaphore->count;
    }
    semaphore->available.NotifyOne();
    return 0;
}

int APS5_VABI sem_getvalue_nid_postfix(void* sem, int* value) {
    auto* semaphore = Get(sem);
    if (!semaphore || !value) return Fail(PosixThread::GUEST_EINVAL);
    std::lock_guard lock(semaphore->lock);
    *value = static_cast<int>(semaphore->count);
    return 0;
}

void* APS5_VABI sem_open_nid_postfix(const char* name, int flags, ...) {
    if (const int error = NamedPathError("sem_open", name, PosixThread::GUEST_EINVAL)) {
        Fail(error);
        return nullptr;
    }
    if ((flags & ~(GUEST_O_CREAT | GUEST_O_EXCL)) != 0) {
        Fail(PosixThread::GUEST_EINVAL);
        return nullptr;
    }
    unsigned int value = 0;
    if ((flags & GUEST_O_CREAT) != 0) {
#ifdef _WIN32
        __builtin_sysv_va_list arguments;
        __builtin_sysv_va_start(arguments, flags);
        static_cast<void>(__builtin_va_arg(arguments, int));
        value = static_cast<unsigned int>(__builtin_va_arg(arguments, int));
        __builtin_sysv_va_end(arguments);
#else
        std::va_list arguments;
        va_start(arguments, flags);
        static_cast<void>(va_arg(arguments, int));
        value = static_cast<unsigned int>(va_arg(arguments, int));
        va_end(arguments);
#endif
    }
    if (const int error = NamedFileError("sem_open", name)) {
        Fail(error);
        return nullptr;
    }
    auto& named = Named();
    std::lock_guard lock(named.lock);
    const std::string file(name + 1);
    std::shared_ptr<NamedSemaphore> object;
    if (const auto found = named.linked.find(file); found != named.linked.end()) {
        if ((flags & (GUEST_O_CREAT | GUEST_O_EXCL)) == (GUEST_O_CREAT | GUEST_O_EXCL)) {
            Fail(GUEST_EEXIST);
            return nullptr;
        }
        object = found->second;
    } else {
        if ((flags & GUEST_O_CREAT) == 0) {
            Fail(GUEST_ENOENT);
            return nullptr;
        }
        if (value > GUEST_SEM_VALUE_MAX)
            throw std::runtime_error("sem_open: initial value " + std::to_string(value) + " is above SEM_VALUE_MAX, which FreeBSD stores unchecked");
        object = std::make_shared<NamedSemaphore>();
        object->semaphore = std::make_unique<PosixSemaphore>();
        object->semaphore->count = value;
        named.linked.emplace(file, object);
    }
    if (!object->mapping) {
        object->mapping = std::make_unique<NamedSemaphoreMapping>(NamedSemaphoreMapping{object->semaphore.get(), 0});
        named.open.emplace(object->mapping.get(), object);
    }
    ++object->openCount;
    return object->mapping.get();
}

int APS5_VABI sem_close_nid_postfix(void* sem) {
    if (!sem) throw std::invalid_argument("sem_close: null semaphore");
    auto& named = Named();
    std::lock_guard lock(named.lock);
    const auto found = named.open.find(sem);
    if (found == named.open.end()) return Fail(PosixThread::GUEST_EINVAL);
    const auto object = found->second;
    if (--object->openCount == 0) {
        named.open.erase(found);
        object->mapping.reset();
    }
    return 0;
}

int APS5_VABI sem_unlink_nid_postfix(const char* name) {
    if (const int error = NamedPathError("sem_unlink", name, GUEST_ENOENT)) return Fail(error);
    if (const int error = NamedFileError("sem_unlink", name)) return Fail(error);
    auto& named = Named();
    std::lock_guard lock(named.lock);
    if (named.linked.erase(std::string(name + 1)) == 0) return Fail(GUEST_ENOENT);
    return 0;
}

}
