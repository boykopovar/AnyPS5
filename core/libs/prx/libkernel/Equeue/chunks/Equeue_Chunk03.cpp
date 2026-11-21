extern "C" {
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "Equeue.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Socket/include/SocketRuntime.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/File/include/HostStreams.hpp"
#include "prx/libkernel/File/include/RandomDevice.hpp"

#include <algorithm>
#include <bit>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>

static constexpr std::uint64_t FileRepollNs = 1000000;

struct UnreadFile {
    bool measured = false;
    std::int64_t bytes = 0;
};

int APS5_VABI sceKernelAddUserEvent(KernelEqueue eq, int id) {
    KernelEqueueEvent event{};
    event.event.ident = static_cast<uintptr_t>(id);
    event.event.filter = EVFILT_USER;
    event.event.flags = EV_ADD;
    event.filter.triggerFunc = [](KernelEqueueEvent* e, void* data) {
        e->triggered = true;
        e->event.data = reinterpret_cast<intptr_t>(data);
        e->event.udata = data;
    };
    event.filter.resetFunc = [](KernelEqueueEvent* e) {
        if ((e->event.flags & EV_CLEAR) != 0) {
            e->triggered = false;
            e->event.fflags = 0;
            e->event.data = 0;
        }
    };
    return EqueueAddEvent_nid_postfix(eq, event);
}

int APS5_VABI sceKernelAddUserEventEdge(KernelEqueue eq, int id) {
    KernelEqueueEvent event{};
    event.event.ident = static_cast<uintptr_t>(id);
    event.event.filter = EVFILT_USER;
    event.event.flags = EV_ADD | EV_CLEAR;
    event.filter.triggerFunc = [](KernelEqueueEvent* e, void* data) {
        e->triggered = true;
        e->event.data = reinterpret_cast<intptr_t>(data);
        e->event.udata = data;
    };
    event.filter.resetFunc = [](KernelEqueueEvent* e) {
        if ((e->event.flags & EV_CLEAR) != 0) {
            e->triggered = false;
            e->event.fflags = 0;
            e->event.data = 0;
        }
    };
    return EqueueAddEvent_nid_postfix(eq, event);
}

int APS5_VABI sceKernelTriggerUserEvent(KernelEqueue eq, int id, void* udata) {
    return EqueueTriggerEvent_nid_postfix(eq, static_cast<uintptr_t>(id), EVFILT_USER, udata);
}

int APS5_VABI sceKernelDeleteUserEvent(KernelEqueue eq, int id) {
    return EqueueDeleteEvent_nid_postfix(eq, static_cast<uintptr_t>(id), EVFILT_USER);
}

int APS5_VABI sceKernelAddHRTimerEvent(KernelEqueue eq, int id, const KernelTimespec* ts, void* udata) {
    KernelTimespec delay{};
    if (!GuestCopyIn_nid_no_patch(&delay, ts, sizeof(delay))) {
        return SCE_KERNEL_ERROR_EFAULT;
    }
    if (delay.tv_sec < 0 || delay.tv_nsec < 0 || delay.tv_nsec >= 1000000000LL) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
    const uint64_t delayNs =
        static_cast<uint64_t>(delay.tv_sec) * 1000000000ULL +
        static_cast<uint64_t>(delay.tv_nsec);
    const uint64_t nowNs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );
    KernelEqueueEvent event{};
    event.deadlineNs = (delayNs <= std::numeric_limits<uint64_t>::max() - nowNs)
        ? nowNs + delayNs : std::numeric_limits<uint64_t>::max();
    event.event.ident = static_cast<uintptr_t>(id);
    event.event.filter = EVFILT_HRTIMER;
    event.event.flags = EV_ADD | EV_ONESHOT;
    event.event.udata = udata;
    return EqueueAddEvent_nid_postfix(eq, event);
}

int APS5_VABI sceKernelDeleteHRTimerEvent(KernelEqueue eq, int id) {
    return EqueueDeleteEvent_nid_postfix(eq, static_cast<uintptr_t>(id), EVFILT_HRTIMER);
}

int APS5_VABI sceKernelAddTimerEvent(KernelEqueue eq, int id, KernelUseconds usec, void* udata) {
    const uint64_t intervalNs = static_cast<uint64_t>(usec) * 1000ULL;
    KernelEqueueEvent event{};
    event.deadlineNs = KernelEqueuePrivate::MonotonicNs() + intervalNs;
    event.intervalNs = intervalNs;
    event.event.ident = static_cast<uintptr_t>(id);
    event.event.filter = EVFILT_TIMER;
    event.event.flags = EV_ADD | EV_CLEAR;
    event.event.udata = udata;
    return EqueueAddEvent_nid_postfix(eq, event);
}

int APS5_VABI sceKernelDeleteTimerEvent(KernelEqueue eq, int id) {
    return EqueueDeleteEvent_nid_postfix(eq, static_cast<uintptr_t>(id), EVFILT_TIMER);
}

int APS5_VABI sceKernelAddAmprEvent(KernelEqueue eq, int id, void* udata) {
    KernelEqueueEvent event{};
    event.event.ident = static_cast<uintptr_t>(id);
    event.event.filter = EVFILT_AMPR;
    event.event.flags = EV_ADD | EV_CLEAR;
    event.event.udata = udata;
    event.filter.triggerFunc = [](KernelEqueueEvent* e, void* data) {
        KernelEvent triggered = e->event;
        triggered.data = static_cast<intptr_t>(reinterpret_cast<uintptr_t>(data));
        if (e->triggered) {
            e->pendingEvents.push_back(triggered);
        } else {
            e->event = triggered;
            e->triggered = true;
        }
    };
    event.filter.resetFunc = [](KernelEqueueEvent* e) {
        if ((e->event.flags & EV_CLEAR) != 0) {
            e->triggered = false;
            e->event.fflags = 0;
            e->event.data = 0;
        }
    };
    return EqueueAddEvent_nid_postfix(eq, event);
}

int APS5_VABI sceKernelAddAmprSystemEvent(KernelEqueue eq, int id, void* udata) {
    return sceKernelAddAmprEvent(eq, id, udata);
}

int APS5_VABI sceKernelDeleteAmprEvent(KernelEqueue eq, int id) {
    if (eq == 0) {
        return EQUEUE_OK;
    }
    EqueueDeleteEvent_nid_postfix(eq, static_cast<uintptr_t>(id), EVFILT_AMPR);

    return EQUEUE_OK;
}

int APS5_VABI sceKernelDeleteAmprSystemEvent(KernelEqueue eq, int id) {
    return sceKernelDeleteAmprEvent(eq, id);
}

static int AddDescriptorEvent(KernelEqueue eq, int fd, std::size_t size, void* udata, int16_t filter) {
    const bool isSocket = GuestSockets::IsOpen(fd);
    std::int64_t available = 0;
    bool eof = false;
    const bool isPipe = !isSocket && filter == EVFILT_READ && !File::IsHostStream(fd) && GuestSockets::PipeReadState(fd, &available, &eof);
    const bool isFile = !isSocket && !isPipe && RegularFile(fd);
    if (!isSocket && !isPipe && !isFile && DescriptorIsOpen_nid_no_patch(fd)) {
        NotImplemented_nid_no_patch(filter == EVFILT_READ ? "sceKernelAddReadEvent: descriptor that is neither a socket, the read end of a pipe nor a regular file" : "sceKernelAddWriteEvent: descriptor that is neither a socket nor a regular file");
    }
    if (size > 1 && !isPipe) {
        NotImplemented_nid_no_patch(filter == EVFILT_READ ? "sceKernelAddReadEvent: low watermark" : "sceKernelAddWriteEvent: low watermark");
    }
    if (!isSocket && !isPipe && !isFile) {
        return SCE_KERNEL_ERROR_EBADF;
    }
    if (isFile) {
        return EqueueAddEvent_nid_postfix(eq, FileEvent(fd, filter, EV_ADD, udata));
    }
    return EqueueAddEvent_nid_postfix(eq, EqueueDescriptorEvent(fd, filter, EV_ADD, udata));
}

int APS5_VABI sceKernelAddReadEvent(KernelEqueue eq, int fd, std::size_t size, void* udata) {
    if (File::IsRandomDevice(fd)) return EqueuePin_nid_postfix(eq) ? SCE_KERNEL_ERROR_ENODEV : SCE_KERNEL_ERROR_EBADF;
    return AddDescriptorEvent(eq, fd, size, udata, EVFILT_READ);
}

int APS5_VABI sceKernelDeleteReadEvent(KernelEqueue eq, int fd) {
    return EqueueDeleteEvent_nid_postfix(eq, static_cast<uintptr_t>(fd), EVFILT_READ);
}

int APS5_VABI sceKernelAddWriteEvent(KernelEqueue eq, int fd, std::size_t size, void* udata) {
    if (File::IsRandomDevice(fd)) return EqueuePin_nid_postfix(eq) ? SCE_KERNEL_ERROR_ENODEV : SCE_KERNEL_ERROR_EBADF;
    return AddDescriptorEvent(eq, fd, size, udata, EVFILT_WRITE);
}

int APS5_VABI sceKernelDeleteWriteEvent(KernelEqueue eq, int fd) {
    return EqueueDeleteEvent_nid_postfix(eq, static_cast<uintptr_t>(fd), EVFILT_WRITE);
}

}

KernelEqueueEvent EqueueDescriptorEvent(int descriptor, int16_t filter, uint16_t flags, void* udata) {
    KernelEqueueEvent event{};
    event.event.ident = static_cast<uintptr_t>(descriptor);
    event.event.filter = filter;
    event.event.flags = flags;
    event.event.udata = udata;
    event.filter.pollFunc = [](KernelEqueueEvent* e) {
        std::int64_t data = 0;
        bool eof = false;
        if (!GuestSockets::Ready(static_cast<int>(e->event.ident), e->event.filter == EVFILT_WRITE, &data, &eof)) {
            if (!e->lowWaterAlwaysMet || GuestSockets::Listening(static_cast<int>(e->event.ident))) {
                return false;
            }
            data = 0;
            eof = false;
        }
        if (!eof && data < e->lowWater && !GuestSockets::Listening(static_cast<int>(e->event.ident))) {
            return false;
        }
        e->event.data = static_cast<intptr_t>(data);
        e->event.flags = static_cast<uint16_t>(eof ? (e->event.flags | EV_EOF) : (e->event.flags & ~EV_EOF));
        return true;
    };
    event.filter.resetFunc = [](KernelEqueueEvent* e) {
        e->triggered = false;
    };
    return event;
}

struct CloseObserverRegistry {
    std::mutex mutex;
    std::vector<void (*)(int)> observers;
};

static CloseObserverRegistry& GetCloseObserverRegistry() {
    static auto* registry = new CloseObserverRegistry;
    return *registry;
}

extern "C" void RegisterDescriptorCloseObserver_nid_no_patch(void (*observer)(int descriptor)) {
    auto& registry = GetCloseObserverRegistry();
    std::lock_guard lock(registry.mutex);
    registry.observers.push_back(observer);
}

static void NotifyDescriptorCloseObservers(int descriptor) {
    auto& registry = GetCloseObserverRegistry();
    std::vector<void (*)(int)> observers;
    {
        std::lock_guard lock(registry.mutex);
        observers = registry.observers;
    }
    for (const auto observer : observers) observer(descriptor);
}

void EqueueDescriptorClosed(int descriptor) {
    KqueueDescriptorClosed(descriptor);
    NotifyDescriptorCloseObservers(descriptor);
}

void EqueueDescriptorReadShut(int descriptor) {
    KqueueDescriptorReadShut(descriptor);
}
