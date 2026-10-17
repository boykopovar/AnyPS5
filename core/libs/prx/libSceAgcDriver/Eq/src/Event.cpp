#include "prx/libSceAgcDriver/Eq/include/Event.hpp"

#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"


namespace {

constexpr int16_t EvfiltGraphicsCore = -14;

struct Registration {
    KernelEqueue eq;
    std::uint32_t queue;
    std::uintptr_t ident;
};

std::mutex g_mutex;
std::vector<Registration> g_registrations;

int AddEvent(KernelEqueue eq, std::uint32_t queue, std::uintptr_t ident, void* udata, const char* caller) {
    if (eq == 0 || !EqueuePin_nid_postfix(eq)) {
        throw std::runtime_error(std::string(caller) + ": invalid event queue");
    }
    KernelEqueueEvent event{};
    event.event.ident = ident;
    event.event.filter = EvfiltGraphicsCore;
    event.event.flags = EV_ADD | EV_CLEAR;
    event.event.udata = udata;
    event.filter.triggerFunc = [](KernelEqueueEvent* e, void*) {
        e->event.data = e->triggered ? e->event.data + 1 : 1;
        e->triggered = true;
    };
    event.filter.resetFunc = [](KernelEqueueEvent* e) {
        e->triggered = false;
        e->event.data = 0;
    };
    const int result = EqueueAddEvent_nid_postfix(eq, event);
    if (result != EQUEUE_OK) {
        throw std::runtime_error(std::string(caller) + ": event queue rejected the event");
    }
    std::lock_guard lock(g_mutex);
    if (std::none_of(g_registrations.begin(), g_registrations.end(), [&](const Registration& r) { return r.eq == eq && r.ident == ident; })) {
        g_registrations.push_back({eq, queue, ident});
    }
    return 0;
}

int DeleteEvent(KernelEqueue eq, std::uintptr_t ident, const char* caller) {
    {
        std::lock_guard lock(g_mutex);
        std::erase_if(g_registrations, [&](const Registration& r) { return r.eq == eq && r.ident == ident; });
    }
    if (EqueueDeleteEvent_nid_postfix(eq, ident, EvfiltGraphicsCore) != EQUEUE_OK) {
        throw std::runtime_error(std::string(caller) + ": event is not registered");
    }
    return 0;
}

}

void AgcDriverDeliverEopInterrupt(std::uint32_t queue) {
    std::vector<Registration> registrations;
    {
        std::lock_guard lock(g_mutex);
        registrations = g_registrations;
    }
    for (const auto& registration : registrations) {
        if (registration.queue == queue) {
            EqueueTriggerEvent_nid_postfix(registration.eq, registration.ident, EvfiltGraphicsCore, nullptr);
        }
    }
}

extern "C" {

int APS5_VABI sceAgcDriverAddEqEvent(KernelEqueue eq, int id, void* udata) {
    return AddEvent(eq, static_cast<std::uint32_t>(id), static_cast<std::uintptr_t>(id), udata, __func__);
}

int APS5_VABI sceAgcDriverDeleteEqEvent(KernelEqueue eq, int id) {
    return DeleteEvent(eq, static_cast<std::uintptr_t>(id), __func__);
}

}

extern "C" int AgcDriverAddQueueEqEvent_nid_postfix(KernelEqueue eq, std::uint32_t queue, std::uintptr_t ident, void* udata) {
    return AddEvent(eq, queue, ident, udata, __func__);
}

extern "C" int AgcDriverDeleteQueueEqEvent_nid_postfix(KernelEqueue eq, std::uintptr_t ident) {
    return DeleteEvent(eq, ident, __func__);
}
