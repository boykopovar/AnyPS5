#ifndef CORE_LIBS_PRX_LIBKERNEL_EQUEUE_EQUEUE_HPP
#define CORE_LIBS_PRX_LIBKERNEL_EQUEUE_EQUEUE_HPP

#include <cstdint>
#include <deque>
#include <functional>
#include <list>
#include <memory>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"

static constexpr int16_t EVFILT_READ = -1;
static constexpr int16_t EVFILT_WRITE = -2;
static constexpr int16_t EVFILT_VNODE = -4;
static constexpr int16_t EVFILT_TIMER = -7;
static constexpr int16_t EVFILT_USER = -11;
static constexpr int16_t EVFILT_VIDEO_OUT = -13;
static constexpr int16_t EVFILT_HRTIMER = -15;
static constexpr int16_t EVFILT_AMPR = -25;

static constexpr uint16_t EV_ADD = 0x0001;
static constexpr uint16_t EV_DELETE = 0x0002;
static constexpr uint16_t EV_ENABLE = 0x0004;
static constexpr uint16_t EV_DISABLE = 0x0008;
static constexpr uint16_t EV_ONESHOT = 0x0010;
static constexpr uint16_t EV_CLEAR = 0x0020;
static constexpr uint16_t EV_RECEIPT = 0x0040;
static constexpr uint16_t EV_DISPATCH = 0x0080;
static constexpr uint16_t EV_FORCEONESHOT = 0x0100;
static constexpr uint16_t EV_SYSFLAGS = 0xF000;
static constexpr uint16_t EV_ERROR = 0x4000;
static constexpr uint16_t EV_EOF = 0x8000;

static constexpr int EQUEUE_OK = 0;

struct KernelEqueueEvent;
class KernelEqueuePrivate;

using EqueueTriggerFunc = void (*)(KernelEqueueEvent* event, void* triggerData);
using EqueueResetFunc = void (*)(KernelEqueueEvent* event);
using EqueueDeleteFunc = void (*)(KernelEqueue eq, KernelEqueueEvent* event);
using EqueuePollFunc = bool (*)(KernelEqueueEvent* event);

struct KernelFilter {
    void* data = nullptr;
    std::shared_ptr<void> owner;
    EqueueTriggerFunc triggerFunc = nullptr;
    EqueueResetFunc resetFunc = nullptr;
    EqueueDeleteFunc deleteEventFunc = nullptr;
    EqueuePollFunc pollFunc = nullptr;
    int waitDescriptor = -1;
    bool repoll = false;
    std::shared_ptr<KernelEqueuePrivate> watched;
};

struct KernelEqueueEvent {
    bool triggered = false;
    bool enabled = true;
    uint64_t deadlineNs = 0;
    uint64_t intervalNs = 0;
    std::int64_t lowWater = 0;
    KernelEvent event;
    KernelFilter filter;
    std::deque<KernelEvent> pendingEvents;
};

class KernelEqueuePrivate {
public:
    explicit KernelEqueuePrivate(KernelEqueue handle, bool reportOncePerWait = false);
    ~KernelEqueuePrivate();

    KernelEqueuePrivate(const KernelEqueuePrivate&) = delete;
    KernelEqueuePrivate& operator=(const KernelEqueuePrivate&) = delete;

    const std::string& GetName() const { return m_name; }
    void SetName(std::string name) { m_name = std::move(name); }

    int AddEvent(const KernelEqueueEvent& event);
    int TriggerEvent(uintptr_t ident, int16_t filter, void* triggerData);
    int DeleteEvent(uintptr_t ident, int16_t filter);
    bool HasEvent(uintptr_t ident, int16_t filter);
    int EditEvent(uintptr_t ident, int16_t filter, const std::function<void(KernelEqueueEvent&)>& edit);
    int GetTriggeredEvents(KernelEvent* ev, int num);
    bool HasTriggeredEvents();
    std::size_t CountTriggeredEvents();
    std::vector<std::shared_ptr<KernelEqueuePrivate>> WatchedQueues();
    int WaitForEvents(KernelEvent* ev, int num, uint32_t micros);
    int WaitForEventsUntil(KernelEvent* ev, int num, std::uint64_t deadlineNanos);
    void RemoveDescriptorEvents(uintptr_t ident);
    void WakeReadWaiters(uintptr_t ident);
    void Close();
    static uint64_t MonotonicNs();

private:
    struct WaitSet;
    void TriggerExpiredTimers(uint64_t nowNs);
    bool NextTimerWaitMicros(uint64_t nowNs, uint32_t* out) const;
    int CollectTriggered(KernelEvent* ev, int num);
    int CollectOncePerWait(KernelEvent* ev, int num);
    bool PollEvents();
    void MeasureFiles(std::unique_lock<std::mutex>& lock);
    void WakePollers();
    void WaitForDescriptors(std::unique_lock<std::mutex>& lock, std::uint64_t deadlineNanos);
    void GatherInterests(WaitSet& wait, std::uintptr_t waker, bool nested);
    static void GatherWatched(WaitSet& wait, const std::shared_ptr<KernelEqueuePrivate>& queue, std::uintptr_t waker);
    static void ReleaseWatched(WaitSet& wait, std::uintptr_t waker);

    std::list<KernelEqueueEvent> m_events;
    std::vector<std::uintptr_t> m_pollers;
    std::vector<int> m_measuring;
    std::mutex m_mutex;
    std::condition_variable m_measured;
    TimedWait::Condition m_cond;
    std::string m_name;
    KernelEqueue m_handle;
    bool m_reportOncePerWait;
    bool m_closed = false;
};

using KernelEqueueRef = std::shared_ptr<KernelEqueuePrivate>;
void EqueueDescriptorClosed(int descriptor);
void KqueueDescriptorClosed(int descriptor);
void EqueueDescriptorReadShut(int descriptor);
void KqueueDescriptorReadShut(int descriptor);
bool IsKqueueDescriptor(int descriptor);
std::optional<bool> KqueueReadable(int descriptor);
KernelEqueueEvent EqueueDescriptorEvent(int descriptor, int16_t filter, uint16_t flags, void* udata);
void RequireVnodeNotes(uint32_t fflags);
void SetVnodeNotes(KernelEqueueEvent& event, uint32_t fflags);
int AddVnodeEvent(const KernelEqueueRef& queue, int descriptor, uint32_t fflags, uint16_t flags, void* udata);
extern "C" {

KernelEqueueRef EqueuePin_nid_postfix(KernelEqueue eq);
int APS5_VABI EqueueAddEvent_nid_postfix(KernelEqueue eq, const KernelEqueueEvent& event);
int APS5_VABI EqueueTriggerEvent_nid_postfix(KernelEqueue eq, uintptr_t ident, int16_t filter, void* triggerData);
int APS5_VABI EqueueDeleteEvent_nid_postfix(KernelEqueue eq, uintptr_t ident, int16_t filter);

}

#endif
