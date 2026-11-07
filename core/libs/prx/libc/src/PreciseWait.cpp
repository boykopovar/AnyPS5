#include "prx/libc/include/PreciseWait.hpp"

#ifdef __APPLE__
#include <cerrno>
#include <sys/event.h>
#include <unistd.h>

bool PreciseKernelTimerSleep_nid_no_patch(std::uint64_t wakeNanos) {
    struct Queue {
        int descriptor = ::kqueue();
        ~Queue() {
            if (descriptor >= 0) ::close(descriptor);
        }
    };
    static thread_local const Queue queue;
    if (queue.descriptor < 0) return false;
    for (;;) {
        const std::uint64_t nowNanos = PreciseMonotonicNanos();
        if (nowNanos >= wakeNanos) return true;
        struct kevent64_s change{};
        change.ident = 1;
        change.filter = EVFILT_TIMER;
        change.flags = EV_ADD | EV_ONESHOT;
        change.fflags = NOTE_NSECONDS | NOTE_CRITICAL | NOTE_LEEWAY;
        change.data = static_cast<std::int64_t>(wakeNanos - nowNanos);
        struct kevent64_s event{};
        const int count = ::kevent64(queue.descriptor, &change, 1, &event, 1, 0, nullptr);
        if (count > 0 && (event.flags & EV_ERROR) != 0) {
            if (event.data == EINTR) continue;
            return false;
        }
        if (count >= 0) return true;
        if (errno != EINTR) return false;
    }
}
#endif
