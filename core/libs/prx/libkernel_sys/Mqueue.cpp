#include "SceTypes.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libkernel_sys/MqueueWait.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {

constexpr int GuestEnoent = 2;
constexpr int GuestEbadf = 9;
constexpr int GuestEfault = 14;
constexpr int GuestEexist = 17;
constexpr int GuestEinval = 22;
constexpr int GuestEnfile = 23;
constexpr int GuestEmfile = 24;
constexpr int GuestEagain = 35;
constexpr int GuestEmsgsize = 40;
constexpr int GuestEtimedout = 60;
constexpr int GuestEnametoolong = 63;
constexpr int GuestClockRealtime = 0;
constexpr int GuestOAccmode = 0x0003;
constexpr int GuestOWronly = 0x0001;
constexpr int GuestORdwr = 0x0002;
constexpr int GuestONonblock = 0x0004;
constexpr int GuestOCreat = 0x0200;
constexpr int GuestOExcl = 0x0800;
constexpr int GuestOExec = 0x00040000;
constexpr std::int64_t MaxMessages = 100;
constexpr std::int64_t MaxMessageSize = 16384;
constexpr std::int64_t DefaultMessages = 10;
constexpr std::int64_t DefaultMessageSize = 1024;
constexpr unsigned PriorityMax = 64;
constexpr std::size_t NameBytes = 256;

struct Message {
    unsigned priority;
    std::vector<char> bytes;
};

struct Queue {
    std::mutex lock;
    TimedWait::Condition readable;
    TimedWait::Condition writable;
    std::int64_t maxmsg = DefaultMessages;
    std::int64_t msgsize = DefaultMessageSize;
    std::vector<Message> messages;
};

struct Open {
    std::shared_ptr<Queue> queue;
    int access = 0;
    bool nonblocking = false;
};

std::mutex g_mqueueLock;
std::map<int, std::shared_ptr<Open>> g_opens;
std::map<std::string, std::shared_ptr<Queue>> g_queues;

int Fail(int error) {
    *__error_nid_postfix() = error;
    return -1;
}

bool CopyIn(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return copied;
}

bool ReadDeadline(const GuestTimespec* abstime, GuestTimespec& deadline) {
    return abstime == nullptr || CopyIn(&deadline, abstime, sizeof(deadline));
}

std::shared_ptr<Open> Find(int descriptor) {
    std::lock_guard lock(g_mqueueLock);
    const auto found = g_opens.find(descriptor);
    return found != g_opens.end() ? found->second : std::shared_ptr<Open>{};
}

int ReserveDescriptor() {
#ifdef _WIN32
    return ::_open("NUL", _O_RDONLY | _O_BINARY);
#else
    return ::open("/dev/null", O_RDONLY | O_CLOEXEC);
#endif
}

void DescriptorClosed(int descriptor) {
    std::lock_guard lock(g_mqueueLock);
    g_opens.erase(descriptor);
}

const bool g_observerRegistered = [] {
    RegisterDescriptorCloseObserver_nid_no_patch(DescriptorClosed);
    return true;
}();

int CheckName(const char* path, std::string& name) {
    if (path == nullptr) return GuestEfault;
    const std::size_t length = strnlen(path, NameBytes);
    if (length == NameBytes) return GuestEnametoolong;
    name.assign(path, length);
    if (length < 2 || name[0] != '/' || name.find('/', 1) != std::string::npos) return GuestEinval;
    return 0;
}

std::uint64_t MonotonicDeadline(const GuestTimespec* abstime) {
    KernelTimespec realtime{};
    clock_gettime_nid_postfix(GuestClockRealtime, &realtime);
    return MqueueWait::MonotonicDeadline(abstime->seconds, abstime->nanoseconds, realtime.tv_sec, realtime.tv_nsec,
                                         TimedWait::NowNanos());
}

template <typename Ready>
int WaitFor(const Open& open, std::unique_lock<std::mutex>& lock, TimedWait::Condition& signal, const GuestTimespec* abstime, Ready ready) {
    if (ready()) return 0;
    if (open.nonblocking) return GuestEagain;
    if (abstime == nullptr) {
        signal.Wait(lock, ready);
        return 0;
    }
    if (abstime->nanoseconds < 0 || abstime->nanoseconds >= 1000000000) return GuestEinval;
    const std::uint64_t deadline = MonotonicDeadline(abstime);
    return MqueueWait::WaitUntilMonotonic(
               deadline, ready, [] { return TimedWait::NowNanos(); },
               [&](std::uint64_t waitDeadline, const auto& predicate) {
                   return signal.WaitUntil(lock, waitDeadline, predicate);
               })
               ? 0
               : GuestEtimedout;
}

}

extern "C" {

int APS5_VABI kmq_open_nid_postfix(const char* path, int flags, unsigned int mode, const GuestMqAttr* attr) {
    (void)mode;
    if ((flags & GuestOAccmode) == GuestOAccmode || (flags & GuestOExec) != 0) return Fail(GuestEinval);
    const bool create = (flags & GuestOCreat) != 0;
    GuestMqAttr localAttr{};
    if (create && attr != nullptr) {
        const int savedErrno = errno;
        const bool copied = GuestCopyIn_nid_no_patch(&localAttr, attr, sizeof(localAttr));
        errno = savedErrno;
        if (!copied) return Fail(GuestEfault);
        if (localAttr.maxmsg <= 0 || localAttr.maxmsg > MaxMessages) return Fail(GuestEinval);
        if (localAttr.msgsize <= 0 || localAttr.msgsize > MaxMessageSize) return Fail(GuestEinval);
    }
    std::string name;
    if (const int error = CheckName(path, name)) return Fail(error);
    std::lock_guard lock(g_mqueueLock);
    std::shared_ptr<Queue> queue;
    const auto found = g_queues.find(name);
    if (found == g_queues.end()) {
        if (!create) return Fail(GuestEnoent);
        queue = std::make_shared<Queue>();
        if (attr != nullptr) {
            queue->maxmsg = localAttr.maxmsg;
            queue->msgsize = localAttr.msgsize;
        }
    } else {
        if ((flags & (GuestOCreat | GuestOExcl)) == (GuestOCreat | GuestOExcl)) return Fail(GuestEexist);
        queue = found->second;
    }
    const int descriptor = ReserveDescriptor();
    if (descriptor < 0) return Fail(errno == ENFILE ? GuestEnfile : GuestEmfile);
    if (found == g_queues.end()) g_queues[name] = queue;
    auto open = std::make_shared<Open>();
    open->queue = std::move(queue);
    open->access = flags & GuestOAccmode;
    open->nonblocking = (flags & GuestONonblock) != 0;
    g_opens[descriptor] = std::move(open);
    return descriptor;
}

int APS5_VABI kmq_unlink_nid_postfix(const char* path) {
    std::string name;
    if (const int error = CheckName(path, name)) return Fail(error);
    std::lock_guard lock(g_mqueueLock);
    if (g_queues.erase(name) == 0) return Fail(GuestEnoent);
    return 0;
}

int APS5_VABI kmq_setattr_nid_postfix(int descriptor, const GuestMqAttr* attr, GuestMqAttr* oattr) {
    GuestMqAttr localAttr{};
    if (attr != nullptr) {
        const int savedErrno = errno;
        const bool copied = GuestCopyIn_nid_no_patch(&localAttr, attr, sizeof(localAttr));
        errno = savedErrno;
        if (!copied) return Fail(GuestEfault);
        if ((localAttr.flags & ~static_cast<std::int64_t>(GuestONonblock)) != 0) return Fail(GuestEinval);
    }
    const auto open = Find(descriptor);
    if (!open) return Fail(GuestEbadf);
    std::lock_guard lock(open->queue->lock);
    if (oattr != nullptr) {
        const GuestMqAttr previous{open->nonblocking ? GuestONonblock : 0, open->queue->maxmsg, open->queue->msgsize,
                                   static_cast<std::int64_t>(open->queue->messages.size()), {}};
        const int savedErrno = errno;
        const bool copied = GuestCopyOut_nid_no_patch(oattr, &previous, sizeof(previous));
        errno = savedErrno;
        if (!copied) return Fail(GuestEfault);
    }
    if (attr != nullptr) open->nonblocking = (localAttr.flags & GuestONonblock) != 0;
    return 0;
}

int APS5_VABI kmq_timedsend_nid_postfix(int descriptor, const char* message, std::size_t length, unsigned int priority, const GuestTimespec* abstime) {
    const auto open = Find(descriptor);
    if (!open || (open->access != GuestOWronly && open->access != GuestORdwr)) return Fail(GuestEbadf);
    GuestTimespec deadline{};
    if (!ReadDeadline(abstime, deadline)) return Fail(GuestEfault);
    auto& queue = *open->queue;
    if (priority >= PriorityMax) return Fail(GuestEinval);
    if (length > static_cast<std::size_t>(queue.msgsize)) return Fail(GuestEmsgsize);
    Message entry{priority, std::vector<char>(length)};
    if (!CopyIn(entry.bytes.data(), message, length)) return Fail(GuestEfault);
    std::unique_lock lock(queue.lock);
    const auto space = [&] { return static_cast<std::int64_t>(queue.messages.size()) < queue.maxmsg; };
    if (const int error = WaitFor(*open, lock, queue.writable, abstime != nullptr ? &deadline : nullptr, space)) return Fail(error);
    auto position = queue.messages.begin();
    while (position != queue.messages.end() && position->priority >= priority) ++position;
    queue.messages.insert(position, std::move(entry));
    queue.readable.NotifyOne();
    return 0;
}

std::int64_t APS5_VABI kmq_timedreceive_nid_postfix(int descriptor, char* buffer, std::size_t length, unsigned int* priority, const GuestTimespec* abstime) {
    const auto open = Find(descriptor);
    if (!open || open->access == GuestOWronly) return Fail(GuestEbadf);
    GuestTimespec deadline{};
    if (!ReadDeadline(abstime, deadline)) return Fail(GuestEfault);
    auto& queue = *open->queue;
    if (length < static_cast<std::size_t>(queue.msgsize)) return Fail(GuestEmsgsize);
    if (buffer == nullptr) return Fail(GuestEfault);
    std::unique_lock lock(queue.lock);
    const auto available = [&] { return !queue.messages.empty(); };
    if (const int error = WaitFor(*open, lock, queue.readable, abstime != nullptr ? &deadline : nullptr, available)) return Fail(error);
    Message entry = std::move(queue.messages.front());
    queue.messages.erase(queue.messages.begin());
    queue.writable.NotifyOne();
    lock.unlock();
    if (!entry.bytes.empty() && !GuestCopyOut_nid_no_patch(buffer, entry.bytes.data(), entry.bytes.size())) return Fail(GuestEfault);
    if (priority != nullptr && !GuestCopyOut_nid_no_patch(priority, &entry.priority, sizeof(entry.priority))) return Fail(GuestEfault);
    return static_cast<std::int64_t>(entry.bytes.size());
}

}
