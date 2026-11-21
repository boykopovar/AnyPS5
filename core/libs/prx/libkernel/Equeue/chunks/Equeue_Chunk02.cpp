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

std::size_t KernelEqueuePrivate::CountTriggeredEvents() {
    std::unique_lock lock(m_mutex);
    if (m_closed) {
        return 0;
    }
    MeasureFiles(lock);
    if (m_closed) {
        return 0;
    }
    TriggerExpiredTimers(MonotonicNs());
    PollEvents();
    return static_cast<std::size_t>(std::count_if(m_events.begin(), m_events.end(), [](const KernelEqueueEvent& event) { return event.enabled && event.triggered; }));
}

KernelEqueuePrivate::TriggeredSnapshot KernelEqueuePrivate::SnapshotTriggered() {
    std::unique_lock lock(m_mutex);
    if (m_closed) {
        return {0, m_activations};
    }
    TriggerExpiredTimers(MonotonicNs());
    PollEvents();
    const auto count = static_cast<std::size_t>(std::count_if(m_events.begin(), m_events.end(), [](const KernelEqueueEvent& event) { return event.enabled && event.triggered; }));
    return {count, m_activations};
}

std::vector<std::shared_ptr<KernelEqueuePrivate>> KernelEqueuePrivate::WatchedQueues() {
    std::unique_lock lock(m_mutex);
    std::vector<std::shared_ptr<KernelEqueuePrivate>> queues;
    for (const auto& event : m_events) {
        if (event.filter.watched) {
            queues.push_back(event.filter.watched);
        }
    }
    return queues;
}

int KernelEqueuePrivate::WaitForEvents(KernelEvent* ev, int num, uint32_t micros) {
    return WaitForEventsUntil(ev, num, micros == 0 ? 0 : TimedWait::DeadlineNanos(micros));
}

int KernelEqueuePrivate::WaitForEventsUntil(KernelEvent* ev, int num, std::uint64_t deadline) {
    std::unique_lock lock(m_mutex);
    if (m_closed) {
        return SCE_KERNEL_ERROR_EBADF;
    }
    for (;;) {
        MeasureFiles(lock);
        if (m_closed) {
            return SCE_KERNEL_ERROR_EBADF;
        }
        TriggerExpiredTimers(MonotonicNs());
        const bool polling = PollEvents();
        const int ret = CollectTriggered(ev, num);
        if (ret != 0) {
            return ret;
        }
        if (m_closed) {
            return SCE_KERNEL_ERROR_EBADF;
        }
        if (deadline == 0) {
            uint32_t timerWait = 0;
            const bool hasTimer = NextTimerWaitMicros(MonotonicNs(), &timerWait);
            const std::uint64_t wake = hasTimer ? TimedWait::NowNanos() + static_cast<std::uint64_t>(timerWait) * 1000ULL : 0;
            if (polling) {
                WaitForDescriptors(lock, wake);
            } else if (hasTimer) {
                m_cond.WaitUntil(lock, wake);
            } else {
                m_cond.Wait(lock);
            }
        } else {
            uint32_t timerWait = 0;
            const bool hasTimer = NextTimerWaitMicros(MonotonicNs(), &timerWait);
            const std::uint64_t now = TimedWait::NowNanos();
            if (now >= deadline) {
                return 0;
            }
            const std::uint64_t timerDeadline = now + static_cast<std::uint64_t>(timerWait) * 1000ULL;
            const std::uint64_t wake = hasTimer ? std::min(deadline, timerDeadline) : deadline;
            if (polling) {
                WaitForDescriptors(lock, wake);
            } else {
                m_cond.WaitUntil(lock, wake);
            }
        }
    }
}

void KernelEqueuePrivate::NoteDroppedPendingEvents(std::size_t dropped, const KernelEqueueEvent& ev) {
    if (dropped == 0) {
        return;
    }
    const auto before = m_droppedPendingEvents;
    m_droppedPendingEvents += dropped;
    static const bool trace = std::getenv("APS5_TRACE_EQUEUE") != nullptr;
    if (!trace) {
        return;
    }
    if (before != 0 && std::bit_width(m_droppedPendingEvents) == std::bit_width(before)) {
        return;
    }
    std::fprintf(stderr,
        "[equeue] %s: dropped %zu oldest pending event(s) for ident=0x%llx filter=%d past cap=%zu (total dropped %llu)\n",
        m_name.c_str(), dropped, static_cast<unsigned long long>(ev.event.ident), static_cast<int>(ev.event.filter),
        kMaxPendingEvents, static_cast<unsigned long long>(m_droppedPendingEvents));
}

int KernelEqueuePrivate::AddEvent(const KernelEqueueEvent& event) {
    std::unique_lock lock(m_mutex);
    if (m_closed) {
        return SCE_KERNEL_ERROR_EBADF;
    }
    auto it = std::find_if(m_events.begin(), m_events.end(),
        [ident = event.event.ident, filter = event.event.filter](const auto& e) {
            return e.event.ident == ident && e.event.filter == filter;
        }
    );
    if (it != m_events.end()) {
        if (event.event.filter == EVFILT_TIMER) {
            TriggerExpiredTimers(MonotonicNs());
        }
        it->deadlineNs = event.deadlineNs;
        it->intervalNs = event.intervalNs;
        it->event.udata = event.event.udata;
        for (auto& pending : it->pendingEvents) {
            pending.udata = event.event.udata;
        }
        if (it->pendingEvents.size() > kMaxPendingEvents) {
            NoteDroppedPendingEvents(it->pendingEvents.size() - kMaxPendingEvents, *it);
            it->pendingEvents.erase(it->pendingEvents.begin(), it->pendingEvents.begin() + static_cast<std::deque<KernelEvent>::difference_type>(it->pendingEvents.size() - kMaxPendingEvents));
        }
    } else if (event.pendingEvents.size() > kMaxPendingEvents) {
        NoteDroppedPendingEvents(event.pendingEvents.size() - kMaxPendingEvents, event);
        KernelEqueueEvent truncated;
        truncated.triggered = event.triggered;
        truncated.enabled = event.enabled;
        truncated.deadlineNs = event.deadlineNs;
        truncated.intervalNs = event.intervalNs;
        truncated.lowWater = event.lowWater;
        truncated.event = event.event;
        truncated.filter = event.filter;
        truncated.pendingEvents.assign(event.pendingEvents.end() - static_cast<std::deque<KernelEvent>::difference_type>(kMaxPendingEvents), event.pendingEvents.end());
        m_events.push_back(std::move(truncated));
        if (event.triggered) ++m_activations;
    } else {
        m_events.push_back(event);
        if (event.triggered) ++m_activations;
    }
    m_cond.NotifyAll();
    WakePollers();
    return EQUEUE_OK;
}

int KernelEqueuePrivate::TriggerEvent(uintptr_t ident, int16_t filter, void* triggerData) {
    std::unique_lock lock(m_mutex);
    if (m_closed) {
        return SCE_KERNEL_ERROR_EBADF;
    }
    auto it = std::find_if(m_events.begin(), m_events.end(),
        [ident, filter](const auto& e) {
            return e.event.ident == ident && e.event.filter == filter;
        }
    );
    if (it == m_events.end()) {
        return SCE_KERNEL_ERROR_ENOENT;
    }
    if (it->enabled) {
        const bool was = it->triggered;
        if (it->filter.triggerFunc != nullptr) {
            it->filter.triggerFunc(&*it, triggerData);
        } else {
            it->triggered = true;
        }
        if (it->pendingEvents.size() > KernelEqueuePrivate::kMaxPendingEvents) {
            NoteDroppedPendingEvents(it->pendingEvents.size() - KernelEqueuePrivate::kMaxPendingEvents, *it);
            it->pendingEvents.erase(it->pendingEvents.begin(), it->pendingEvents.begin() + static_cast<std::deque<KernelEvent>::difference_type>(it->pendingEvents.size() - KernelEqueuePrivate::kMaxPendingEvents));
        }
        if (!was && it->triggered) {
            ++m_activations;
            if (filter == EVFILT_USER) m_events.splice(m_events.end(), m_events, it);
        }
    }
    m_cond.NotifyAll();
    WakePollers();
    return EQUEUE_OK;
}

bool KernelEqueuePrivate::HasEvent(uintptr_t ident, int16_t filter) {
    std::unique_lock lock(m_mutex);
    return std::any_of(m_events.begin(), m_events.end(), [ident, filter](const auto& e) {
        return e.event.ident == ident && e.event.filter == filter;
    });
}

int KernelEqueuePrivate::EditEvent(uintptr_t ident, int16_t filter, const std::function<void(KernelEqueueEvent&)>& edit) {
    std::unique_lock lock(m_mutex);
    if (m_closed) {
        return SCE_KERNEL_ERROR_EBADF;
    }
    if (filter == EVFILT_TIMER) {
        TriggerExpiredTimers(MonotonicNs());
    }
    auto it = std::find_if(m_events.begin(), m_events.end(),
        [ident, filter](const auto& e) {
            return e.event.ident == ident && e.event.filter == filter;
        }
    );
    if (it == m_events.end()) {
        return SCE_KERNEL_ERROR_ENOENT;
    }
    struct Activation {
        KernelEqueuePrivate& queue;
        KernelEqueueEvent& event;
        bool active;
        ~Activation() {
            if (!active && event.enabled && event.triggered) ++queue.m_activations;
        }
    } activation{*this, *it, it->enabled && it->triggered};
    edit(*it);
    m_cond.NotifyAll();
    WakePollers();
    return EQUEUE_OK;
}

int KernelEqueuePrivate::DeleteEvent(uintptr_t ident, int16_t filter) {
    std::unique_lock lock(m_mutex);
    if (m_closed) {
        return SCE_KERNEL_ERROR_EBADF;
    }
    auto it = std::find_if(m_events.begin(), m_events.end(),
        [ident, filter](const auto& e) {
            return e.event.ident == ident && e.event.filter == filter;
        }
    );
    if (it == m_events.end()) {
        return SCE_KERNEL_ERROR_ENOENT;
    }
    if (it->filter.deleteEventFunc != nullptr) {
        auto owner = it->filter.owner;
        it->filter.deleteEventFunc(m_handle, &*it);
    }
    m_events.erase(it);
    WakePollers();
    return EQUEUE_OK;
}

void KernelEqueuePrivate::RemoveDescriptorEvents(uintptr_t ident) {
    std::unique_lock lock(m_mutex);
    bool removed = false;
    for (auto it = m_events.begin(); it != m_events.end();) {
        if (it->event.ident != ident || (it->event.filter != EVFILT_READ && it->event.filter != EVFILT_WRITE && it->event.filter != EVFILT_VNODE)) {
            ++it;
            continue;
        }
        if (it->filter.deleteEventFunc != nullptr) {
            auto owner = it->filter.owner;
            it->filter.deleteEventFunc(m_handle, &*it);
        }
        it = m_events.erase(it);
        removed = true;
    }
    if (removed) {
        WakePollers();
    }
    m_measured.wait(lock, [&] { return std::find(m_measuring.begin(), m_measuring.end(), static_cast<int>(ident)) == m_measuring.end(); });
}

void KernelEqueuePrivate::WakeReadWaiters(uintptr_t ident) {
    std::unique_lock lock(m_mutex);
    if (std::any_of(m_events.begin(), m_events.end(), [ident](const KernelEqueueEvent& event) { return event.event.ident == ident && event.event.filter == EVFILT_READ; })) {
        WakePollers();
    }
}

KernelEqueueRef EqueuePin_nid_postfix(KernelEqueue eq) {
    const auto descriptor = static_cast<std::uint32_t>(eq);
    if (descriptor == 0 || descriptor > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
        return {};
    }
    return FindKqueueDescriptor(static_cast<int>(descriptor));
}

int APS5_VABI EqueueAddEvent_nid_postfix(KernelEqueue eq, const KernelEqueueEvent& event) {
    auto owner = EqueuePin_nid_postfix(eq);
    if (!owner) {
        return SCE_KERNEL_ERROR_EBADF;
    }
    return owner->AddEvent(event);
}

int APS5_VABI EqueueTriggerEvent_nid_postfix(KernelEqueue eq, uintptr_t ident, int16_t filter, void* triggerData) {
    auto owner = EqueuePin_nid_postfix(eq);
    if (!owner) {
        return SCE_KERNEL_ERROR_EBADF;
    }
    return owner->TriggerEvent(ident, filter, triggerData);
}

int APS5_VABI EqueueDeleteEvent_nid_postfix(KernelEqueue eq, uintptr_t ident, int16_t filter) {
    auto owner = EqueuePin_nid_postfix(eq);
    if (!owner) {
        return SCE_KERNEL_ERROR_EBADF;
    }
    return owner->DeleteEvent(ident, filter);
}

int APS5_VABI sceKernelCreateEqueue(KernelEqueue* eq, const char* name) {
    if (eq == nullptr || name == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
    std::string nameCopy;
    if (const int error = CopyGuestString_nid_no_patch(name, 32, nameCopy)) return SceKernelError(error);
    std::unique_lock lock(g_equeueMutex);
    if (g_nextEqueueId > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())) {
        throw std::runtime_error("equeue handle space exhausted");
    }
    const int descriptor = ReserveKqueueDescriptor();
    if (descriptor < 0) {
        if (errno == EMFILE) return SCE_KERNEL_ERROR_EMFILE;
        if (errno == ENFILE) return SCE_KERNEL_ERROR_ENFILE;
        throw std::runtime_error("sceKernelCreateEqueue: reserving a descriptor failed, errno=" + std::to_string(errno));
    }
    const auto handle = static_cast<KernelEqueue>((static_cast<std::uint64_t>(g_nextEqueueId++) << 32) | static_cast<std::uint32_t>(descriptor));
    auto owner = std::make_shared<KernelEqueuePrivate>(handle);
    owner->SetName(std::move(nameCopy));
    RegisterKqueueDescriptor(descriptor, std::move(owner));
    lock.unlock();
    if (!GuestCopyOut_nid_no_patch(eq, &handle, sizeof(handle))) {
        sceKernelDeleteEqueue(handle);
        return SCE_KERNEL_ERROR_EFAULT;
    }
    return EQUEUE_OK;
}

int APS5_VABI sceKernelDeleteEqueue(KernelEqueue eq) {
    const auto owner = EqueuePin_nid_postfix(eq);
    if (!owner || owner->Handle() != eq) {
        return SCE_KERNEL_ERROR_EBADF;
    }
    return sceKernelClose(static_cast<int>(static_cast<std::uint32_t>(eq)));
}

int APS5_VABI sceKernelWaitEqueue(KernelEqueue eq, KernelEvent* ev, int num, int* out, const KernelUseconds* timo) {
    const auto fail = [out](int error) {
        if (out != nullptr) {
            const int zero = 0;
            const int savedErrno = errno;
            GuestCopyOut_nid_no_patch(out, &zero, sizeof(zero));
            errno = savedErrno;
        }
        return error;
    };
    if (num < 1) {
        return fail(SCE_KERNEL_ERROR_EINVAL);
    }
    auto owner = EqueuePin_nid_postfix(eq);
    if (!owner) {
        return fail(SCE_KERNEL_ERROR_EBADF);
    }
    if (ev == nullptr) {
        return fail(SCE_KERNEL_ERROR_EFAULT);
    }
    if (out == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
    KernelUseconds localTimo{};
    if (timo != nullptr) {
        const int savedErrno = errno;
        const bool copied = GuestCopyIn_nid_no_patch(&localTimo, timo, sizeof(localTimo));
        errno = savedErrno;
        if (!copied) {
            return fail(SCE_KERNEL_ERROR_EFAULT);
        }
    }
    const auto waitStart = std::chrono::steady_clock::now();
    int result = 0;
    constexpr int STACK_EVENTS = 64;
    constexpr int MAX_WAIT_EVENTS = 1024;
    KernelEvent stackEvents[STACK_EVENTS];
    std::unique_ptr<KernelEvent[]> heapEvents;
    const int capacity = std::min(num, MAX_WAIT_EVENTS);
    KernelEvent* target = stackEvents;
    if (capacity > STACK_EVENTS) {
        heapEvents = std::make_unique<KernelEvent[]>(static_cast<std::size_t>(capacity));
        target = heapEvents.get();
    }
    if (timo == nullptr) {
        result = owner->WaitForEvents(target, capacity, 0);
    } else if (localTimo == 0) {
        result = owner->GetTriggeredEvents(target, capacity);
    } else {
        result = owner->WaitForEvents(target, capacity, localTimo);
    }
    if (timo == nullptr || localTimo != 0) {
        KernelTraceWait_nid_postfix("equeue", __builtin_return_address(0), static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - waitStart).count()), result == 0);
    }
    if (result == SCE_KERNEL_ERROR_EBADF) {
        return fail(SCE_KERNEL_ERROR_EBADF);
    }
    if (result > 0) {
        const int savedErrno = errno;
        const bool copied = GuestCopyOut_nid_no_patch(ev, target, static_cast<std::size_t>(result) * sizeof(KernelEvent));
        errno = savedErrno;
        if (!copied) {
            return fail(SCE_KERNEL_ERROR_EFAULT);
        }
    }
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(out, &result, sizeof(result));
    errno = savedErrno;
    if (!copied) {
        return SCE_KERNEL_ERROR_EFAULT;
    }
    if (result == 0) {
        return SCE_KERNEL_ERROR_ETIMEDOUT;
    }
    return EQUEUE_OK;
}

}
