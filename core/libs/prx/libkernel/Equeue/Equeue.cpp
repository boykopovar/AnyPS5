#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "prx/libkernel/Equeue/EqueueIdentityTrace.hpp"
#include "Equeue.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Socket/include/SocketRuntime.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/File/include/HostStreams.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>

// Opt-in probe (APS5_TRACE_EQUEUE_IDENTITY): identity and wait/producer correlation for equeues.
// Default off. Any value of the variable enables it, like the other APS5 probes; the launcher
// disables probes by removing the variable, not by setting it to 0.
static bool EqueueIdentityEnabled() {
    static const bool enabled = std::getenv("APS5_TRACE_EQUEUE_IDENTITY") != nullptr;
    return enabled;
}

// Process-relative nanoseconds on the same source the submit probe prints (sceKernelGetProcessTime
// returns microseconds since the same trace origin), so lines from different probes can be aligned.
static std::uint64_t EqueueIdentityProcNs() {
    return sceKernelGetProcessTime() * 1000ULL;
}

// OS thread id of the waiting thread, same domain the waits and submit probes print.
static unsigned long EqueueIdentityThreadId() {
#if defined(_WIN32)
    return static_cast<unsigned long>(::GetCurrentThreadId());
#else
    return static_cast<unsigned long>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
#endif
}

// Call site of the guest-facing producer call, captured at the exported entry point of the current
// thread, so the probe never reports the internal wrapper as the producer.
static thread_local const void* t_equeueProducerSite = nullptr;

// Probe registry: one slot per live equeue, keyed by handle, bounded. The object is intentionally
// never destroyed, so teardown of equeues during process exit cannot touch a destroyed registry.
// Nothing here stores a pointer to KernelEqueuePrivate: deletion of an equeue only clears the slot,
// so there is no dereference after delete.
struct EqueueIdentityRegistry {
    std::mutex mutex;
    EqueueIdentityTraceDetail::State slots[32];
    EqueueIdentityTraceDetail::CoordinatorState coordinator;
    std::uint64_t nextId = 1;
    std::uint64_t slotsOverflow = 0;
};

static EqueueIdentityRegistry& IdentityRegistry() {
    static EqueueIdentityRegistry* registry = new EqueueIdentityRegistry();
    return *registry;
}

static EqueueIdentityTraceDetail::State* IdentityFindLocked(EqueueIdentityRegistry& registry, std::uint64_t handle) {
    for (auto& slot : registry.slots) {
        if (slot.id != 0 && slot.handle == handle) return &slot;
    }
    return nullptr;
}

static void MaybeReportEqueueIdentity(std::uint64_t handle, std::uint64_t nowNs, std::uint64_t nowProcNs);

static void IdentityAttach(std::uint64_t handle, const char* name) {
    if (!EqueueIdentityEnabled()) return;
    auto& registry = IdentityRegistry();
    std::lock_guard lock(registry.mutex);
    for (auto& slot : registry.slots) {
        if (slot.id == 0) {
            EqueueIdentityTraceDetail::Attach(slot, registry.nextId++, handle, name, EqueueIdentityProcNs());
            return;
        }
    }
    ++registry.slotsOverflow;
}

static void IdentityDetach(std::uint64_t handle) {
    if (!EqueueIdentityEnabled()) return;
    bool report = false;
    {
        auto& registry = IdentityRegistry();
        std::lock_guard lock(registry.mutex);
        auto* slot = IdentityFindLocked(registry, handle);
        if (slot == nullptr) return;
        // A delete is not a window: the final line goes through the same admission as every other
        // report, so the bounded output contract (one line per equeue per window, MaxLines overall)
        // still holds. Activity that is not admitted here is dropped, not printed outside budget.
        // While a wait is still open nothing is classified yet, so the final report is deferred to
        // the release path in IdentityWaitExit, where the late exit is already counted.
        report = slot->openWaitCount == 0 && EqueueIdentityTraceDetail::HasActivity(*slot) &&
                 slot->lastPrintedWindow != registry.coordinator.windowIndex;
        if (slot->openWaitCount != 0) {
            // A wait can still be blocked on the object the caller just deleted. Keep a tombstone
            // so its late exit is classified (err) and attributed, instead of disappearing; the
            // slot is released when the last open wait closes (see WaitExit).
            EqueueIdentityTraceDetail::Retire(*slot);
        }
    }
    if (report) MaybeReportEqueueIdentity(handle, KernelEqueuePrivate::MonotonicNs(), EqueueIdentityProcNs());
    auto& registry = IdentityRegistry();
    std::lock_guard lock(registry.mutex);
    auto* slot = IdentityFindLocked(registry, handle);
    // Reclaim the slot when nothing is in flight. Without this a deleted equeue kept its slot
    // forever and the 32-entry registry was exhausted after 32 deletes, so later equeues were
    // never tracked (the site report was missing for exactly this reason).
    if (slot != nullptr && slot->openWaitCount == 0) *slot = EqueueIdentityTraceDetail::State{};
}

static int IdentityWaitEnter(std::uint64_t handle, std::uint64_t nowNs, std::uint64_t procNs, unsigned long tid,
                              const void* site) {
    if (!EqueueIdentityEnabled()) return -1;
    auto& registry = IdentityRegistry();
    std::lock_guard lock(registry.mutex);
    if (auto* slot = IdentityFindLocked(registry, handle)) return EqueueIdentityTraceDetail::WaitEnter(*slot, nowNs, procNs, tid, site);
    return -1;
}

static void IdentityWaitExit(std::uint64_t handle, int token, std::uint64_t nowNs, std::uint64_t procNs, int delivered) {
    if (!EqueueIdentityEnabled()) return;
    bool release = false;
    bool report = false;
    {
        auto& registry = IdentityRegistry();
        std::lock_guard lock(registry.mutex);
        auto* slot = IdentityFindLocked(registry, handle);
        if (slot == nullptr) return;
        EqueueIdentityTraceDetail::WaitExit(*slot, token, nowNs, procNs, delivered);
        release = slot->retired && slot->openWaitCount == 0;
        report = release && EqueueIdentityTraceDetail::HasActivity(*slot) &&
                 slot->lastPrintedWindow != registry.coordinator.windowIndex;
    }
    if (!release) return;
    // Last chance to report a deleted equeue's late exit inside the normal output budget, then give
    // the slot back so the 32-entry registry is not exhausted by tombstones.
    if (report) MaybeReportEqueueIdentity(handle, nowNs, procNs);
    auto& registry = IdentityRegistry();
    std::lock_guard lock(registry.mutex);
    if (auto* slot = IdentityFindLocked(registry, handle)) EqueueIdentityTraceDetail::ReleaseIfUnused(*slot);
}

// Called from the producer paths while the equeue mutex is held: the probe mutex is the only
// additional lock and no path takes the equeue mutex while holding it.
static void IdentityProducer(std::uint64_t handle, std::int16_t filter, std::uintptr_t ident, const void* udata,
                             const void* site) {
    if (!EqueueIdentityEnabled()) return;
    auto& registry = IdentityRegistry();
    std::lock_guard lock(registry.mutex);
    if (auto* slot = IdentityFindLocked(registry, handle)) {
        EqueueIdentityTraceDetail::Producer(*slot, filter, ident, udata, site, EqueueIdentityProcNs());
    }
}

// Snapshot under the probe mutex, print outside it: no I/O is done while a lock is held.
static void MaybeReportEqueueIdentity(std::uint64_t handle, std::uint64_t nowNs, std::uint64_t nowProcNs) {
    if (!EqueueIdentityEnabled()) return;
    EqueueIdentityTraceDetail::Snapshot snapshot{};
    std::uint64_t admittedWindow = 0;
    bool admitted = false;
    {
        auto& registry = IdentityRegistry();
        std::lock_guard lock(registry.mutex);
        auto* slot = IdentityFindLocked(registry, handle);
        if (slot == nullptr || !EqueueIdentityTraceDetail::HasActivity(*slot)) return;
        const auto decision = EqueueIdentityTraceDetail::Admit(registry.coordinator, nowNs, slot->lastPrintedWindow);
        if (!decision.admit) return;
        admittedWindow = decision.windowIndex;
        admitted = true;
        EqueueIdentityTraceDetail::TakeSnapshot(*slot, snapshot, nowProcNs);
        snapshot.slotsOverflow = registry.slotsOverflow;
    }
    if (!admitted) return;
    char line[8192];
    if (EqueueIdentityTraceDetail::FormatLine(snapshot, nowProcNs, line, sizeof line)) {
        {
            auto& registry = IdentityRegistry();
            std::lock_guard lock(registry.mutex);
            auto* slot = IdentityFindLocked(registry, handle);
            if (slot != nullptr) {
                EqueueIdentityTraceDetail::ResetWindow(*slot, nowProcNs);
                slot->lastPrintedWindow = admittedWindow;
            }
        }
        std::fputs(line, stderr);
        std::fflush(stderr);
    }
}

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

static std::unordered_map<KernelEqueue, KernelEqueueRef> g_equeues;
static std::mutex g_equeueMutex;
static uint64_t g_nextEqueue = 1;

struct KernelEqueuePrivate::WaitSet {
    std::vector<GuestSockets::Interest> interests;
    std::vector<std::shared_ptr<void>> held;
    std::vector<std::shared_ptr<KernelEqueuePrivate>> queues;
    uint64_t nearestTimerNs = std::numeric_limits<uint64_t>::max();
    bool repoll = false;
};

extern "C" {

KernelEqueuePrivate::KernelEqueuePrivate(KernelEqueue handle, bool reportOncePerWait)
    : m_handle(handle), m_reportOncePerWait(reportOncePerWait) {}

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
            it->pendingEvents.erase(it->pendingEvents.begin(), it->pendingEvents.begin() + static_cast<std::deque<KernelEvent>::difference_type>(it->pendingEvents.size() - kMaxPendingEvents));
        }
    } else if (event.pendingEvents.size() > kMaxPendingEvents) {
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
    if (event.triggered && EqueueIdentityEnabled()) {
        const void* const site = t_equeueProducerSite != nullptr ? t_equeueProducerSite : __builtin_return_address(0);
        IdentityProducer(m_handle, event.event.filter, event.event.ident, event.event.udata, site);
    }
    m_cond.NotifyOne();
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
            it->pendingEvents.erase(it->pendingEvents.begin(), it->pendingEvents.begin() + static_cast<std::deque<KernelEvent>::difference_type>(it->pendingEvents.size() - KernelEqueuePrivate::kMaxPendingEvents));
        }
        if (!was && it->triggered) ++m_activations;
        if (EqueueIdentityEnabled()) {
            const void* const site = t_equeueProducerSite != nullptr ? t_equeueProducerSite : __builtin_return_address(0);
            IdentityProducer(m_handle, filter, ident, it->event.udata, site);
        }
    }
    m_cond.NotifyOne();
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
    m_cond.NotifyOne();
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
    if (eq == 0) {
        return {};
    }
    std::unique_lock lock(g_equeueMutex);
    auto it = g_equeues.find(eq);
    return it != g_equeues.end() ? it->second : KernelEqueueRef{};
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
    std::unique_lock lock(g_equeueMutex);
    if (g_nextEqueue > static_cast<uint64_t>(std::numeric_limits<KernelEqueue>::max())) {
        throw std::runtime_error("equeue handle space exhausted");
    }
    *eq = static_cast<KernelEqueue>(g_nextEqueue++);
    auto owner = std::make_shared<KernelEqueuePrivate>(*eq);
    owner->SetName(std::string(name));
    IdentityAttach(*eq, name);
    g_equeues.emplace(*eq, std::move(owner));
    return EQUEUE_OK;
}

int APS5_VABI sceKernelDeleteEqueue(KernelEqueue eq) {
    KernelEqueueRef owner;
    {
        std::unique_lock lock(g_equeueMutex);
        auto it = g_equeues.find(eq);
        if (it == g_equeues.end()) {
            return SCE_KERNEL_ERROR_EBADF;
        }
        owner = std::move(it->second);
        g_equeues.erase(it);
    }
    IdentityDetach(eq);
    owner->Close();
    return EQUEUE_OK;
}

int APS5_VABI sceKernelWaitEqueue(KernelEqueue eq, KernelEvent* ev, int num, int* out, const KernelUseconds* timo) {
    auto owner = EqueuePin_nid_postfix(eq);
    if (!owner) {
        return SCE_KERNEL_ERROR_EBADF;
    }
    if (ev == nullptr) {
        return SCE_KERNEL_ERROR_EFAULT;
    }
    if (num < 1 || out == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
    const auto waitStart = std::chrono::steady_clock::now();
    const bool identity = EqueueIdentityEnabled();
    const std::uint64_t identityStartNs = identity ? KernelEqueuePrivate::MonotonicNs() : 0;
    const int identityWaitToken = identity
        ? IdentityWaitEnter(eq, identityStartNs, EqueueIdentityProcNs(), EqueueIdentityThreadId(),
                            __builtin_return_address(0))
        : -1;
    // The probe accounting identity waits = prod + noprod + empty + err requires every entered wait
    // to close exactly once. The wait can unwind (for example a socket path throwing on a descriptor
    // that is not a socket), so the catch-all closes it as an error before rethrowing.
    try {
        if (timo == nullptr) {
            *out = owner->WaitForEvents(ev, num, 0);
        } else if (*timo == 0) {
            *out = owner->GetTriggeredEvents(ev, num);
        } else {
            *out = owner->WaitForEvents(ev, num, *timo);
        }
    } catch (...) {
        if (identity) {
            const std::uint64_t endNs = KernelEqueuePrivate::MonotonicNs();
            IdentityWaitExit(eq, identityWaitToken, endNs, EqueueIdentityProcNs(), -1);
            MaybeReportEqueueIdentity(eq, endNs, EqueueIdentityProcNs());
        }
        throw;
    }
    if (identity) {
        const std::uint64_t endNs = KernelEqueuePrivate::MonotonicNs();
        IdentityWaitExit(eq, identityWaitToken, endNs, EqueueIdentityProcNs(), *out);
        MaybeReportEqueueIdentity(eq, endNs, EqueueIdentityProcNs());
    }
    if (timo == nullptr || *timo != 0) {
        KernelTraceWait_nid_postfix("equeue", __builtin_return_address(0), static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - waitStart).count()), *out == 0);
    }
    if (*out == SCE_KERNEL_ERROR_EBADF) {
        return SCE_KERNEL_ERROR_EBADF;
    }
    if (*out == 0) {
        return SCE_KERNEL_ERROR_ETIMEDOUT;
    }
    return EQUEUE_OK;
}

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
    const void* const site = __builtin_return_address(0);
    struct ProducerSiteScope { const void* previous; ~ProducerSiteScope() { t_equeueProducerSite = previous; } } scope{t_equeueProducerSite};
    if (EqueueIdentityEnabled()) t_equeueProducerSite = site;
    const int result = EqueueAddEvent_nid_postfix(eq, event);
    return result;
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
    const void* const site = __builtin_return_address(0);
    struct ProducerSiteScope { const void* previous; ~ProducerSiteScope() { t_equeueProducerSite = previous; } } scope{t_equeueProducerSite};
    if (EqueueIdentityEnabled()) t_equeueProducerSite = site;
    const int result = EqueueAddEvent_nid_postfix(eq, event);
    return result;
}

int APS5_VABI sceKernelTriggerUserEvent(KernelEqueue eq, int id, void* udata) {
    const void* const site = __builtin_return_address(0);
    struct ProducerSiteScope { const void* previous; ~ProducerSiteScope() { t_equeueProducerSite = previous; } } scope{t_equeueProducerSite};
    if (EqueueIdentityEnabled()) t_equeueProducerSite = site;
    return EqueueTriggerEvent_nid_postfix(eq, static_cast<uintptr_t>(id), EVFILT_USER, udata);
}

int APS5_VABI sceKernelDeleteUserEvent(KernelEqueue eq, int id) {
    return EqueueDeleteEvent_nid_postfix(eq, static_cast<uintptr_t>(id), EVFILT_USER);
}

int APS5_VABI sceKernelAddHRTimerEvent(KernelEqueue eq, int id, const KernelTimespec* ts, void* udata) {
    if (ts == nullptr) {
        return SCE_KERNEL_ERROR_EFAULT;
    }
    if (ts->tv_sec < 0 || ts->tv_nsec < 0 || ts->tv_nsec >= 1000000000LL) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
    const uint64_t delayNs =
        static_cast<uint64_t>(ts->tv_sec) * 1000000000ULL +
        static_cast<uint64_t>(ts->tv_nsec);
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
    if (eq == 0) {
        return EQUEUE_OK;
    }
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
    EqueueAddEvent_nid_postfix(eq, event);

    return EQUEUE_OK;
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
    return AddDescriptorEvent(eq, fd, size, udata, EVFILT_READ);
}

int APS5_VABI sceKernelDeleteReadEvent(KernelEqueue eq, int fd) {
    return EqueueDeleteEvent_nid_postfix(eq, static_cast<uintptr_t>(fd), EVFILT_READ);
}

int APS5_VABI sceKernelAddWriteEvent(KernelEqueue eq, int fd, std::size_t size, void* udata) {
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
            return false;
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
    std::vector<KernelEqueueRef> owners;
    {
        std::unique_lock lock(g_equeueMutex);
        for (const auto& [handle, owner] : g_equeues) {
            owners.push_back(owner);
        }
    }
    for (const auto& owner : owners) {
        owner->RemoveDescriptorEvents(static_cast<uintptr_t>(descriptor));
    }
}

void EqueueDescriptorReadShut(int descriptor) {
    KqueueDescriptorReadShut(descriptor);
    std::vector<KernelEqueueRef> owners;
    {
        std::unique_lock lock(g_equeueMutex);
        for (const auto& [handle, owner] : g_equeues) {
            owners.push_back(owner);
        }
    }
    for (const auto& owner : owners) {
        owner->WakeReadWaiters(static_cast<uintptr_t>(descriptor));
    }
}
