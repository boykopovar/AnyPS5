#include "Equeue.hpp"

#include <algorithm>
#include <chrono>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <unordered_map>

static std::unordered_map<KernelEqueue, KernelEqueueRef> g_equeues;
static std::mutex g_equeueMutex;
static uint64_t g_nextEqueue = 1;

extern "C" {

KernelEqueuePrivate::KernelEqueuePrivate(KernelEqueue handle) : m_handle(handle) {}

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
    m_cond.notify_all();
}

void KernelEqueuePrivate::TriggerExpiredTimers(uint64_t nowNs) {
    for (auto& ev : m_events) {
        if (!ev.triggered && ev.deadlineNs != 0 && ev.deadlineNs <= nowNs) {
            ev.triggered = true;
        }
    }
}

bool KernelEqueuePrivate::NextTimerWaitMicros(uint64_t nowNs, uint32_t* out) const {
    uint64_t nearest = std::numeric_limits<uint64_t>::max();
    for (const auto& ev : m_events) {
        if (!ev.triggered && ev.deadlineNs != 0) {
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

int KernelEqueuePrivate::GetTriggeredEvents(KernelEvent* ev, int num) {
    std::unique_lock lock(m_mutex);
    if (m_closed) {
        return EQUEUE_ERROR_EBADF;
    }
    TriggerExpiredTimers(MonotonicNs());
    int ret = 0;
    for (auto it = m_events.begin(); it != m_events.end();) {
        auto& e = *it;
        bool erase = false;
        while (e.triggered) {
            ev[ret++] = e.event;
            if ((e.event.flags & EV_ONESHOT) != 0) {
                erase = true;
                break;
            }
            if (e.filter.resetFunc != nullptr) {
                e.filter.resetFunc(&e);
            } else if ((e.event.flags & EV_CLEAR) != 0) {
                e.triggered = false;
                e.event.fflags = 0;
                e.event.data = 0;
            }
            if (!e.pendingEvents.empty()) {
                e.event = e.pendingEvents.front();
                e.pendingEvents.pop_front();
                e.triggered = true;
            }
            if (ret >= num) {
                break;
            }
        }
        it = erase ? m_events.erase(it) : std::next(it);
        if (ret >= num) {
            break;
        }
    }
    return ret;
}

int KernelEqueuePrivate::WaitForEvents(KernelEvent* ev, int num, uint32_t micros) {
    std::unique_lock lock(m_mutex);
    if (m_closed) {
        return EQUEUE_ERROR_EBADF;
    }
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::microseconds(micros);
    for (;;) {
        TriggerExpiredTimers(MonotonicNs());
        int ret = 0;
        for (auto it = m_events.begin(); it != m_events.end() && ret < num;) {
            auto& e = *it;
            bool erase = false;
            while (e.triggered) {
                ev[ret++] = e.event;
                if ((e.event.flags & EV_ONESHOT) != 0) {
                    erase = true;
                    break;
                }
                if (e.filter.resetFunc != nullptr) {
                    e.filter.resetFunc(&e);
                } else if ((e.event.flags & EV_CLEAR) != 0) {
                    e.triggered = false;
                    e.event.fflags = 0;
                    e.event.data = 0;
                }
                if (!e.pendingEvents.empty()) {
                    e.event = e.pendingEvents.front();
                    e.pendingEvents.pop_front();
                    e.triggered = true;
                }
                if (ret >= num) {
                    break;
                }
            }
            it = erase ? m_events.erase(it) : std::next(it);
        }
        if (ret != 0) {
            return ret;
        }
        if (m_closed) {
            return EQUEUE_ERROR_EBADF;
        }
        if (micros == 0) {
            m_cond.wait(lock);
        } else {
            uint32_t timerWait = 0;
            const bool hasTimer = NextTimerWaitMicros(MonotonicNs(), &timerWait);
            const auto now = std::chrono::steady_clock::now();
            if (now >= deadline) {
                return 0;
            }
            const auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(
                deadline - now
            ).count();
            const auto waitUs = hasTimer
                ? std::min<uint64_t>(static_cast<uint64_t>(remaining), timerWait)
                : static_cast<uint64_t>(remaining);
            m_cond.wait_for(lock, std::chrono::microseconds(waitUs));
        }
    }
}

int KernelEqueuePrivate::AddEvent(const KernelEqueueEvent& event) {
    std::unique_lock lock(m_mutex);
    if (m_closed) {
        return EQUEUE_ERROR_EBADF;
    }
    auto it = std::find_if(m_events.begin(), m_events.end(),
        [ident = event.event.ident, filter = event.event.filter](const auto& e) {
            return e.event.ident == ident && e.event.filter == filter;
        }
    );
    if (it != m_events.end()) {
        it->deadlineNs = event.deadlineNs;
        it->event.udata = event.event.udata;
        for (auto& pending : it->pendingEvents) {
            pending.udata = event.event.udata;
        }
    } else {
        m_events.push_back(event);
    }
    m_cond.notify_one();
    return EQUEUE_OK;
}

int KernelEqueuePrivate::TriggerEvent(uintptr_t ident, int16_t filter, void* triggerData) {
    std::unique_lock lock(m_mutex);
    if (m_closed) {
        return EQUEUE_ERROR_EBADF;
    }
    auto it = std::find_if(m_events.begin(), m_events.end(),
        [ident, filter](const auto& e) {
            return e.event.ident == ident && e.event.filter == filter;
        }
    );
    if (it == m_events.end()) {
        return EQUEUE_ERROR_ENOENT;
    }
    if (it->filter.triggerFunc != nullptr) {
        it->filter.triggerFunc(&*it, triggerData);
    } else {
        it->triggered = true;
    }
    m_cond.notify_one();
    return EQUEUE_OK;
}

int KernelEqueuePrivate::DeleteEvent(uintptr_t ident, int16_t filter) {
    std::unique_lock lock(m_mutex);
    if (m_closed) {
        return EQUEUE_ERROR_EBADF;
    }
    auto it = std::find_if(m_events.begin(), m_events.end(),
        [ident, filter](const auto& e) {
            return e.event.ident == ident && e.event.filter == filter;
        }
    );
    if (it == m_events.end()) {
        return EQUEUE_ERROR_ENOENT;
    }
    if (it->filter.deleteEventFunc != nullptr) {
        auto owner = it->filter.owner;
        it->filter.deleteEventFunc(m_handle, &*it);
    }
    m_events.erase(it);
    return EQUEUE_OK;
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
        return EQUEUE_ERROR_EBADF;
    }
    return owner->AddEvent(event);
}

int APS5_VABI EqueueTriggerEvent_nid_postfix(KernelEqueue eq, uintptr_t ident, int16_t filter, void* triggerData) {
    auto owner = EqueuePin_nid_postfix(eq);
    if (!owner) {
        return EQUEUE_ERROR_EBADF;
    }
    return owner->TriggerEvent(ident, filter, triggerData);
}

int APS5_VABI EqueueDeleteEvent_nid_postfix(KernelEqueue eq, uintptr_t ident, int16_t filter) {
    auto owner = EqueuePin_nid_postfix(eq);
    if (!owner) {
        return EQUEUE_ERROR_EBADF;
    }
    return owner->DeleteEvent(ident, filter);
}


int APS5_VABI sceKernelCreateEqueue(KernelEqueue* eq, const char* name) {
    if (eq == nullptr || name == nullptr) {
        return EQUEUE_ERROR_EINVAL;
    }
    std::unique_lock lock(g_equeueMutex);
    if (g_nextEqueue > static_cast<uint64_t>(std::numeric_limits<KernelEqueue>::max())) {
        throw std::runtime_error("equeue handle space exhausted");
    }
    *eq = static_cast<KernelEqueue>(g_nextEqueue++);
    auto owner = std::make_shared<KernelEqueuePrivate>(*eq);
    owner->SetName(std::string(name));
    g_equeues.emplace(*eq, std::move(owner));
    return EQUEUE_OK;
}

int APS5_VABI sceKernelDeleteEqueue(KernelEqueue eq) {
    KernelEqueueRef owner;
    {
        std::unique_lock lock(g_equeueMutex);
        auto it = g_equeues.find(eq);
        if (it == g_equeues.end()) {
            return EQUEUE_ERROR_EBADF;
        }
        owner = std::move(it->second);
        g_equeues.erase(it);
    }
    owner->Close();
    return EQUEUE_OK;
}

int APS5_VABI sceKernelWaitEqueue(KernelEqueue eq, KernelEvent* ev, int num, int* out, const KernelUseconds* timo) {
    auto owner = EqueuePin_nid_postfix(eq);
    if (!owner) {
        return EQUEUE_ERROR_EBADF;
    }
    if (ev == nullptr) {
        return EQUEUE_ERROR_EFAULT;
    }
    if (num < 1 || out == nullptr) {
        return EQUEUE_ERROR_EINVAL;
    }
    if (timo == nullptr) {
        *out = owner->WaitForEvents(ev, num, 0);
    } else if (*timo == 0) {
        *out = owner->GetTriggeredEvents(ev, num);
    } else {
        *out = owner->WaitForEvents(ev, num, *timo);
    }
    if (*out == EQUEUE_ERROR_EBADF) {
        return EQUEUE_ERROR_EBADF;
    }
    if (*out == 0) {
        return EQUEUE_ERROR_ETIMEDOUT;
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
    if (ts == nullptr) {
        return EQUEUE_ERROR_EFAULT;
    }
    if (ts->tv_sec < 0 || ts->tv_nsec < 0 || ts->tv_nsec >= 1000000000LL) {
        return EQUEUE_ERROR_EINVAL;
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

int APS5_VABI sceKernelAddAmprEvent(KernelEqueue eq, int id, void* udata) {
    if (eq == 0) {
        return EQUEUE_OK;
    }
    KernelEqueueEvent event{};
    event.event.ident = static_cast<uintptr_t>(id);
    event.event.filter = EVFILT_USER;
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
    EqueueDeleteEvent_nid_postfix(eq, static_cast<uintptr_t>(id), EVFILT_USER);
    return EQUEUE_OK;
}

int APS5_VABI sceKernelDeleteAmprSystemEvent(KernelEqueue eq, int id) {
    return sceKernelDeleteAmprEvent(eq, id);
}

}
