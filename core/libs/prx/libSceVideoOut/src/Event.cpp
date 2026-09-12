#include <mutex>

#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "prx/libSceVideoOut/include/Event.hpp"
#include "prx/libSceVideoOut/include/VideoOutDriver.hpp"

static int RegisterVideoOutEvent(int handle, KernelEqueue eq, int16_t eventKind, void* udata) {
    auto* cfg = VideoOutDriver::Get().GetConfig(handle);
    if (cfg == nullptr) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    if (eq == 0) {
        return VIDEO_OUT_ERROR_INVALID_EVENT_QUEUE;
    }
    if (!EqueuePin_nid_postfix(eq)) {
        return VIDEO_OUT_ERROR_INVALID_EVENT_QUEUE;
    }
    KernelEqueueEvent event{};
    event.event.ident = static_cast<uintptr_t>(eventKind);
    event.event.filter = EVFILT_VIDEO_OUT;
    event.event.flags = EV_ADD;
    event.event.udata = udata;
    if (eventKind == VIDEO_OUT_EVENT_SET_MODE) {
        std::unique_lock lock(cfg->mutex);
        event.triggered = true;
        event.event.fflags = 1;
        event.event.data = static_cast<intptr_t>(cfg->outputMode);
    }
    event.filter.triggerFunc = [](KernelEqueueEvent* e, void* data) {
        const uint64_t old = static_cast<uint64_t>(e->event.data);
        uint64_t counter = (old >> 12u) & 0xfu;
        if (counter != 0xfu) {
            counter++;
        }
        const uint64_t tsc = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
        ) & 0xfffu;
        const uint64_t payload = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(data));
        const uint64_t newData = tsc | (counter << 12u) | ((payload & 0x0000ffffffffffffULL) << 16u);
        KernelEvent triggered = e->event;
        triggered.fflags = triggered.fflags < 0xfu ? triggered.fflags + 1u : triggered.fflags;
        triggered.data = static_cast<intptr_t>(newData);
        if (e->triggered) {
            e->pendingEvents.push_back(triggered);
        } else {
            e->event = triggered;
            e->triggered = true;
        }
    };
    event.filter.resetFunc = [](KernelEqueueEvent* e) {
        e->triggered = false;
        e->event.fflags = 0;
        e->event.data = 0;
    };
    const int result = EqueueAddEvent_nid_postfix(eq, event);
    return result == EQUEUE_ERROR_EBADF ? VIDEO_OUT_ERROR_INVALID_EVENT_QUEUE : result;
}

static int DeleteVideoOutEvent(int handle, KernelEqueue eq, int16_t eventKind) {
    if (!VideoOutDriver::Get().IsOpen(handle)) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    if (eq == 0 || !EqueuePin_nid_postfix(eq)) {
        return VIDEO_OUT_ERROR_INVALID_EVENT_QUEUE;
    }
    const int result = EqueueDeleteEvent_nid_postfix(eq, static_cast<uintptr_t>(eventKind), EVFILT_VIDEO_OUT);
    if (result == EQUEUE_ERROR_EBADF) {
        return VIDEO_OUT_ERROR_INVALID_EVENT_QUEUE;
    }
    return (result == EQUEUE_ERROR_ENOENT) ? 0 : result;
}

extern "C" {

int APS5_VABI sceVideoOutAddFlipEvent(KernelEqueue eq, int handle, void* udata) {
    return RegisterVideoOutEvent(handle, eq, VIDEO_OUT_EVENT_FLIP, udata);
}

int APS5_VABI sceVideoOutAddVblankEvent(KernelEqueue eq, int handle, void* udata) {
    return RegisterVideoOutEvent(handle, eq, VIDEO_OUT_EVENT_VBLANK, udata);
}

int APS5_VABI sceVideoOutAddPreVblankStartEvent(KernelEqueue eq, int handle, void* udata) {
    return RegisterVideoOutEvent(handle, eq, VIDEO_OUT_EVENT_PRE_VBLANK_START, udata);
}

int APS5_VABI sceVideoOutAddOutputModeEvent(KernelEqueue eq, int handle, void* udata) {
    return RegisterVideoOutEvent(handle, eq, VIDEO_OUT_EVENT_SET_MODE, udata);
}

int APS5_VABI sceVideoOutDeleteFlipEvent(KernelEqueue eq, int handle) {
    return DeleteVideoOutEvent(handle, eq, VIDEO_OUT_EVENT_FLIP);
}

int APS5_VABI sceVideoOutDeleteVblankEvent(KernelEqueue eq, int handle) {
    return DeleteVideoOutEvent(handle, eq, VIDEO_OUT_EVENT_VBLANK);
}

int APS5_VABI sceVideoOutDeletePreVblankStartEvent(KernelEqueue eq, int handle) {
    return DeleteVideoOutEvent(handle, eq, VIDEO_OUT_EVENT_PRE_VBLANK_START);
}

int APS5_VABI sceVideoOutGetEventId(const KernelEvent* ev) {
    if (ev == nullptr) {
        return VIDEO_OUT_ERROR_INVALID_ADDRESS;
    }
    if (ev->filter != EVFILT_VIDEO_OUT) {
        return VIDEO_OUT_ERROR_INVALID_EVENT;
    }
    const int ident = static_cast<int>(ev->ident);
    if (ident != VIDEO_OUT_EVENT_FLIP && ident != VIDEO_OUT_EVENT_VBLANK && ident != VIDEO_OUT_EVENT_PRE_VBLANK_START && ident != VIDEO_OUT_EVENT_SET_MODE) {
        return VIDEO_OUT_ERROR_INVALID_EVENT;
    }
    return ident;
}

int APS5_VABI sceVideoOutGetEventData(const KernelEvent* ev, int64_t* data) {
    if (ev == nullptr || data == nullptr) {
        return VIDEO_OUT_ERROR_INVALID_ADDRESS;
    }
    if (ev->filter != EVFILT_VIDEO_OUT) {
        return VIDEO_OUT_ERROR_INVALID_EVENT;
    }
    uint64_t eventData = static_cast<uint64_t>(ev->data) >> 16u;
    if (ev->ident == static_cast<uintptr_t>(VIDEO_OUT_EVENT_FLIP) && (static_cast<uint64_t>(ev->data) & 0x8000000000000000ULL) != 0) {
        eventData |= 0xffff000000000000ULL;
    }
    *data = static_cast<int64_t>(eventData);
    return 0;
}

int APS5_VABI sceVideoOutGetEventCount(const KernelEvent* ev) {
    if (ev == nullptr) {
        return VIDEO_OUT_ERROR_INVALID_ADDRESS;
    }
    if (ev->filter != EVFILT_VIDEO_OUT) {
        return VIDEO_OUT_ERROR_INVALID_EVENT;
    }
    return static_cast<int>((static_cast<uint64_t>(ev->data) >> 12u) & 0xfu);
}

}
