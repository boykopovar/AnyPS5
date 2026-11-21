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

static UnreadFile UnreadFileBytes(int file) {
    FileStat status{};
    if (sceKernelFstat(file, &status) != 0) {
        return {};
    }
    const std::int64_t offset = sceKernelLseek(file, 0, 1);
    if (offset < 0) {
        return {};
    }
    return {true, status.st_size - offset};
}

static bool RegularFile(int descriptor) {
    if (File::IsHostStream(descriptor)) {
        return false;
    }
    FileStat status{};
    return sceKernelFstat(descriptor, &status) == 0 && (status.st_mode & 0170000) == 0100000;
}

static KernelEqueueEvent FileEvent(int descriptor, int16_t filter, uint16_t flags, void* udata) {
    KernelEqueueEvent event{};
    event.event.ident = static_cast<uintptr_t>(descriptor);
    event.event.filter = filter;
    event.event.flags = flags;
    event.event.udata = udata;
    event.filter.repoll = true;
    if (filter == EVFILT_WRITE) {
        event.filter.pollFunc = [](KernelEqueueEvent* e) {
            e->event.data = 0;
            return true;
        };
    } else {
        event.filter.owner = std::make_shared<UnreadFile>();
        event.filter.pollFunc = [](KernelEqueueEvent* e) {
            const auto* unread = static_cast<const UnreadFile*>(e->filter.owner.get());
            if (!unread->measured || unread->bytes == 0) {
                return false;
            }
            e->event.data = static_cast<intptr_t>(unread->bytes);
            return true;
        };
    }
    event.filter.resetFunc = [](KernelEqueueEvent* e) {
        e->triggered = false;
    };
    return event;
}

static std::mutex g_equeueMutex;
static std::uint32_t g_nextEqueueId = 1;

struct KernelEqueuePrivate::WaitSet {
    std::vector<GuestSockets::Interest> interests;
    std::vector<std::shared_ptr<void>> held;
    std::vector<std::shared_ptr<KernelEqueuePrivate>> queues;
    uint64_t nearestTimerNs = std::numeric_limits<uint64_t>::max();
    bool repoll = false;
};


KernelEqueuePrivate::KernelEqueuePrivate(KernelEqueue handle, bool reportOncePerWait) : m_handle(handle), m_reportOncePerWait(reportOncePerWait) {}

KernelEqueuePrivate::~KernelEqueuePrivate() {
    Close();
}

uint64_t KernelEqueuePrivate::MonotonicNs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );
}

void KernelEqueuePrivate::Close() {
    std::unique_lock lock(m_mutex);
    if (m_closed) {
        return;
    }
    m_closed = true;
    for (auto& ev : m_events) {
        if (ev.filter.deleteEventFunc != nullptr) {
            auto owner = ev.filter.owner;
            ev.filter.deleteEventFunc(m_handle, &ev);
        }
    }
    m_events.clear();
    m_cond.NotifyAll();
    WakePollers();
}

void KernelEqueuePrivate::TriggerExpiredTimers(uint64_t nowNs) {
    for (auto& ev : m_events) {
        if (ev.enabled && ev.deadlineNs != 0 && ev.deadlineNs <= nowNs) {
            if (ev.event.filter == EVFILT_TIMER) {
                const uint64_t count = ev.intervalNs == 0
                    ? (ev.triggered ? 0 : 1)
                    : 1 + (nowNs - ev.deadlineNs) / ev.intervalNs;
                ev.event.data += static_cast<intptr_t>(count);
                ev.deadlineNs += count * ev.intervalNs;
            }
            if (!ev.triggered) ++m_activations;
            ev.triggered = true;
        }
    }
}

bool KernelEqueuePrivate::NextTimerWaitMicros(uint64_t nowNs, uint32_t* out) const {
    uint64_t nearest = std::numeric_limits<uint64_t>::max();
    for (const auto& ev : m_events) {
        if (ev.enabled && !ev.triggered && ev.deadlineNs != 0) {
            nearest = std::min(nearest, ev.deadlineNs);
        }
    }
    if (nearest == std::numeric_limits<uint64_t>::max()) {
        return false;
    }
    const uint64_t remainingNs = nearest > nowNs ? nearest - nowNs : 0;
    const uint64_t roundedUs = std::max<uint64_t>(1, (remainingNs + 999u) / 1000u);
    *out = static_cast<uint32_t>(std::min<uint64_t>(roundedUs, std::numeric_limits<uint32_t>::max()));
    return true;
}

bool KernelEqueuePrivate::PollEvents() {
    bool polled = false;
    for (auto& ev : m_events) {
        if (ev.enabled && ev.filter.pollFunc != nullptr) {
            const bool was = ev.triggered;
            ev.triggered = ev.filter.pollFunc(&ev);
            if (!was && ev.triggered) ++m_activations;
            polled = true;
        }
    }
    return polled;
}

void KernelEqueuePrivate::MeasureFiles(std::unique_lock<std::mutex>& lock) {
    std::vector<std::pair<int, std::shared_ptr<void>>> files;
    for (const auto& e : m_events) {
        if (e.enabled && e.filter.repoll && e.event.filter == EVFILT_READ) {
            files.emplace_back(static_cast<int>(e.event.ident), e.filter.owner);
        }
    }
    if (files.empty()) {
        return;
    }
    std::vector<UnreadFile> measured;
    measured.reserve(files.size());
    m_measuring.reserve(m_measuring.size() + files.size());
    struct Measuring {
        KernelEqueuePrivate& queue;
        std::unique_lock<std::mutex>& lock;
        const std::vector<std::pair<int, std::shared_ptr<void>>>& files;
        ~Measuring() {
            if (!lock.owns_lock()) {
                lock.lock();
            }
            for (const auto& [file, owner] : files) {
                queue.m_measuring.erase(std::find(queue.m_measuring.begin(), queue.m_measuring.end(), file));
            }
            queue.m_measured.notify_all();
        }
    };
    for (const auto& [file, owner] : files) {
        m_measuring.push_back(file);
    }
    const Measuring measuring{*this, lock, files};
    lock.unlock();
    for (const auto& [file, owner] : files) {
        measured.push_back(UnreadFileBytes(file));
    }
    lock.lock();
    for (std::size_t i = 0; i < files.size(); ++i) {
        *static_cast<UnreadFile*>(files[i].second.get()) = measured[i];
    }
}

void KernelEqueuePrivate::WakePollers() {
    for (const auto waker : m_pollers) {
        GuestSockets::Wake(waker);
    }
}

void KernelEqueuePrivate::GatherInterests(WaitSet& wait, std::uintptr_t waker, bool nested) {
    for (const auto& e : m_events) {
        if (!e.enabled) {
            continue;
        }
        if (nested && !e.triggered && e.deadlineNs != 0) {
            wait.nearestTimerNs = std::min(wait.nearestTimerNs, e.deadlineNs);
        }
        if (e.filter.pollFunc == nullptr) {
            continue;
        }
        if (e.filter.watched) {
            GatherWatched(wait, e.filter.watched, waker);
        } else if (e.filter.repoll) {
            wait.repoll = true;
        } else if (e.filter.waitDescriptor >= 0) {
            wait.interests.push_back({e.filter.waitDescriptor, false});
            wait.held.push_back(e.filter.owner);
        } else {
            wait.interests.push_back({static_cast<int>(e.event.ident), e.event.filter == EVFILT_WRITE});
        }
    }
}

void KernelEqueuePrivate::GatherWatched(WaitSet& wait, const std::shared_ptr<KernelEqueuePrivate>& queue, std::uintptr_t waker) {
    std::lock_guard lock(queue->m_mutex);
    if (queue->m_closed) {
        return;
    }
    queue->m_pollers.push_back(waker);
    wait.queues.push_back(queue);
    queue->GatherInterests(wait, waker, true);
}

void KernelEqueuePrivate::ReleaseWatched(WaitSet& wait, std::uintptr_t waker) {
    for (const auto& queue : wait.queues) {
        std::lock_guard lock(queue->m_mutex);
        const auto found = std::find(queue->m_pollers.begin(), queue->m_pollers.end(), waker);
        if (found != queue->m_pollers.end()) {
            queue->m_pollers.erase(found);
        }
    }
    wait.queues.clear();
}

void KernelEqueuePrivate::WaitForDescriptors(std::unique_lock<std::mutex>& lock, std::uint64_t deadlineNanos) {
    const auto waker = GuestSockets::CurrentWaker();
    WaitSet wait;
    GatherInterests(wait, waker, false);
    if (wait.repoll) {
        const std::uint64_t soon = TimedWait::NowNanos() + FileRepollNs;
        if (deadlineNanos == 0 || deadlineNanos > soon) {
            deadlineNanos = soon;
        }
    }
    m_pollers.push_back(waker);
    if (!wait.queues.empty()) {
        PollEvents();
        if (std::any_of(m_events.begin(), m_events.end(), [](const KernelEqueueEvent& event) { return event.enabled && event.triggered; })) {
            m_pollers.erase(std::find(m_pollers.begin(), m_pollers.end(), waker));
            ReleaseWatched(wait, waker);
            return;
        }
        if (wait.nearestTimerNs != std::numeric_limits<uint64_t>::max()) {
            const uint64_t nowMonotonic = MonotonicNs();
            const uint64_t remainingNs = wait.nearestTimerNs > nowMonotonic ? wait.nearestTimerNs - nowMonotonic : 0;
            const uint64_t timerDeadline = TimedWait::NowNanos() + std::clamp<uint64_t>(remainingNs, 1000, static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) * 1000ULL);
            deadlineNanos = deadlineNanos == 0 ? timerDeadline : std::min(deadlineNanos, timerDeadline);
        }
    }
    lock.unlock();
    bool waited = false;
    try {
        waited = GuestSockets::WaitAny(wait.interests, deadlineNanos);
    } catch (...) {
        ReleaseWatched(wait, waker);
        throw;
    }
    ReleaseWatched(wait, waker);
    lock.lock();
    m_pollers.erase(std::find(m_pollers.begin(), m_pollers.end(), waker));
    if (!waited) {
        m_cond.WaitUntil(lock, deadlineNanos);
    }
}

int KernelEqueuePrivate::CollectTriggered(KernelEvent* ev, int num) {
    if (m_reportOncePerWait) {
        return CollectOncePerWait(ev, num);
    }
    int ret = 0;
    std::list<KernelEqueueEvent> requeued;
    for (auto it = m_events.begin(); it != m_events.end() && ret < num;) {
        auto& e = *it;
        if (!e.triggered || !e.enabled) {
            ++it;
            continue;
        }
        bool erase = false;
        while (e.triggered && ret < num) {
            ev[ret++] = e.event;
            if ((e.event.flags & EV_ONESHOT) != 0) {
                if (e.filter.deleteEventFunc != nullptr) {
                    auto owner = e.filter.owner;
                    e.filter.deleteEventFunc(m_handle, &e);
                }
                erase = true;
                break;
            }
            if ((e.event.flags & EV_DISPATCH) != 0) {
                e.enabled = false;
                e.triggered = false;
                e.event.fflags = 0;
                e.event.data = 0;
                break;
            }
            if (e.filter.resetFunc != nullptr) {
                e.filter.resetFunc(&e);
            } else if ((e.event.flags & EV_CLEAR) != 0) {
                e.triggered = false;
                e.event.fflags = 0;
                e.event.data = 0;
            }
            if (e.pendingEvents.empty()) {
                break;
            }
            e.event = e.pendingEvents.front();
            e.pendingEvents.pop_front();
            e.triggered = true;
            ++m_activations;
        }
        const auto next = std::next(it);
        if (erase) {
            m_events.erase(it);
        } else {
            requeued.splice(requeued.end(), m_events, it);
        }
        it = next;
    }
    m_events.splice(m_events.end(), requeued);
    return ret;
}

int KernelEqueuePrivate::CollectOncePerWait(KernelEvent* ev, int num) {
    int ret = 0;
    std::list<KernelEqueueEvent> requeued;
    for (auto it = m_events.begin(); it != m_events.end() && ret < num;) {
        auto& e = *it;
        if (!e.triggered || !e.enabled) {
            ++it;
            continue;
        }
        ev[ret++] = e.event;
        if ((e.event.flags & EV_ONESHOT) != 0) {
            if (e.filter.deleteEventFunc != nullptr) {
                auto owner = e.filter.owner;
                e.filter.deleteEventFunc(m_handle, &e);
            }
            it = m_events.erase(it);
            continue;
        }
        if ((e.event.flags & EV_DISPATCH) != 0) {
            e.enabled = false;
            e.triggered = false;
            e.event.fflags = 0;
            e.event.data = 0;
        } else if (e.filter.resetFunc != nullptr) {
            e.filter.resetFunc(&e);
        } else if ((e.event.flags & EV_CLEAR) != 0) {
            e.triggered = false;
            e.event.fflags = 0;
            e.event.data = 0;
        }
        const auto next = std::next(it);
        requeued.splice(requeued.end(), m_events, it);
        it = next;
    }
    m_events.splice(m_events.end(), requeued);
    return ret;
}

int KernelEqueuePrivate::GetTriggeredEvents(KernelEvent* ev, int num) {
    std::unique_lock lock(m_mutex);
    if (m_closed) {
        return SCE_KERNEL_ERROR_EBADF;
    }
    MeasureFiles(lock);
    if (m_closed) {
        return SCE_KERNEL_ERROR_EBADF;
    }
    TriggerExpiredTimers(MonotonicNs());
    PollEvents();
    return CollectTriggered(ev, num);
}

bool KernelEqueuePrivate::HasTriggeredEvents() {
    return CountTriggeredEvents() != 0;
}

}
