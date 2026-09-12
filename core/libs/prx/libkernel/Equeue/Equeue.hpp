#ifndef CORE_LIBS_PRX_LIBKERNEL_EQUEUE_EQUEUE_HPP
#define CORE_LIBS_PRX_LIBKERNEL_EQUEUE_EQUEUE_HPP

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

static constexpr int16_t EVFILT_USER = -11;
static constexpr int16_t EVFILT_VIDEO_OUT = -13;
static constexpr int16_t EVFILT_HRTIMER = -15;

static constexpr uint16_t EV_ADD = 0x0001;
static constexpr uint16_t EV_ONESHOT = 0x0010;
static constexpr uint16_t EV_CLEAR = 0x0020;
static constexpr uint16_t EV_ERROR = 0x4000;

static constexpr int EQUEUE_OK = 0;
static constexpr int EQUEUE_ERROR_EBADF = -2147418090;
static constexpr int EQUEUE_ERROR_EFAULT = -2147418103;
static constexpr int EQUEUE_ERROR_EINVAL = -2147418107;
static constexpr int EQUEUE_ERROR_ENOENT = -2147418095;
static constexpr int EQUEUE_ERROR_ETIMEDOUT = -2147418077;

struct KernelEqueueEvent;

using EqueueTriggerFunc = void (*)(KernelEqueueEvent* event, void* triggerData);
using EqueueResetFunc = void (*)(KernelEqueueEvent* event);
using EqueueDeleteFunc = void (*)(KernelEqueue eq, KernelEqueueEvent* event);

struct KernelFilter {
    void* data = nullptr;
    std::shared_ptr<void> owner;
    EqueueTriggerFunc triggerFunc = nullptr;
    EqueueResetFunc resetFunc = nullptr;
    EqueueDeleteFunc deleteEventFunc = nullptr;
};

struct KernelEqueueEvent {
    bool triggered = false;
    uint64_t deadlineNs = 0;
    KernelEvent event;
    KernelFilter filter;
    std::deque<KernelEvent> pendingEvents;
};

class KernelEqueuePrivate {
public:
    explicit KernelEqueuePrivate(KernelEqueue handle);
    ~KernelEqueuePrivate();

    KernelEqueuePrivate(const KernelEqueuePrivate&) = delete;
    KernelEqueuePrivate& operator=(const KernelEqueuePrivate&) = delete;

    const std::string& GetName() const { return m_name; }
    void SetName(std::string name) { m_name = std::move(name); }

    int AddEvent(const KernelEqueueEvent& event);
    int TriggerEvent(uintptr_t ident, int16_t filter, void* triggerData);
    int DeleteEvent(uintptr_t ident, int16_t filter);
    int GetTriggeredEvents(KernelEvent* ev, int num);
    int WaitForEvents(KernelEvent* ev, int num, uint32_t micros);
    void Close();

private:
    static uint64_t MonotonicNs();
    void TriggerExpiredTimers(uint64_t nowNs);
    bool NextTimerWaitMicros(uint64_t nowNs, uint32_t* out) const;

    std::list<KernelEqueueEvent> m_events;
    std::mutex m_mutex;
    std::condition_variable_any m_cond;
    std::string m_name;
    KernelEqueue m_handle;
    bool m_closed = false;
};

using KernelEqueueRef = std::shared_ptr<KernelEqueuePrivate>;

KernelEqueueRef EqueuePin(KernelEqueue eq);
int APS5_VABI EqueueAddEvent(KernelEqueue eq, const KernelEqueueEvent& event);
int APS5_VABI EqueueTriggerEvent(KernelEqueue eq, uintptr_t ident, int16_t filter, void* triggerData);
int APS5_VABI EqueueDeleteEvent(KernelEqueue eq, uintptr_t ident, int16_t filter);

#endif
