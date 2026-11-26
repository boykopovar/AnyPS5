#include "prx/libSceAgcDriver/Eq/include/Event.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"

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
    std::uint32_t queueCount;
    std::uintptr_t ident;
    bool identData;
};

AgcDriver::Mutex g_mutex;
std::vector<Registration> g_registrations;

int AddEvent(KernelEqueue eq, std::uint32_t queue, std::uint32_t queueCount, std::uintptr_t ident, void* udata, bool identData, const char* caller) {
    if (eq == 0 || !EqueuePin_nid_postfix(eq)) {
        throw std::runtime_error(std::string(caller) + ": invalid event queue");
    }
    KernelEqueueEvent event{};
    event.event.ident = ident;
    event.event.filter = EvfiltGraphicsCore;
    event.event.flags = EV_ADD | EV_CLEAR;
    event.event.data = identData ? static_cast<intptr_t>(ident) : static_cast<intptr_t>(queue);
    event.event.udata = udata;
    event.filter.triggerFunc = [](KernelEqueueEvent* e, void* data) {
        if (data == nullptr) throw std::runtime_error("graphics event triggered without its interrupt data");
        e->event.data = static_cast<intptr_t>(*static_cast<const std::uint64_t*>(data));
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
    const auto found = std::find_if(g_registrations.begin(), g_registrations.end(), [&](const Registration& r) { return r.eq == eq && r.ident == ident; });
    if (found == g_registrations.end()) g_registrations.push_back({eq, queue, queueCount, ident, identData});
    else *found = {eq, queue, queueCount, ident, identData};
    return 0;
}

int DeleteEvent(KernelEqueue eq, std::uintptr_t ident, const char* caller) {
    if (eq == 0 || !EqueuePin_nid_postfix(eq)) {
        throw std::runtime_error(std::string(caller) + ": invalid event queue");
    }
    if (EqueueDeleteEvent_nid_postfix(eq, ident, EvfiltGraphicsCore) != EQUEUE_OK) {
        throw std::runtime_error(std::string(caller) + ": event is not registered");
    }
    std::lock_guard lock(g_mutex);
    std::erase_if(g_registrations, [&](const Registration& r) { return r.eq == eq && r.ident == ident; });
    return 0;
}

}

void AgcDriverDeliverEopInterrupt(std::uint32_t queue, std::uint32_t context) {
    std::vector<Registration> registrations;
    {
        std::lock_guard lock(g_mutex);
        registrations = g_registrations;
    }
    for (const auto& registration : registrations) {
        if (queue < registration.queue || queue - registration.queue >= registration.queueCount) continue;
        std::uint64_t data = registration.identData ? static_cast<std::uint64_t>(registration.ident)
            : static_cast<std::uint64_t>(static_cast<std::uint8_t>(registration.queue)) | (static_cast<std::uint64_t>(context) << 16u);
        EqueueTriggerEvent_nid_postfix(registration.eq, registration.ident, EvfiltGraphicsCore, &data);
    }
}

extern "C" {

int APS5_VABI sceAgcDriverAddEqEvent(KernelEqueue eq, int id, void* udata) {
    if (id < 0) {
        throw std::runtime_error(std::string(__func__) + ": negative event id");
    }
    return AddEvent(eq, static_cast<std::uint32_t>(id), 1, static_cast<std::uintptr_t>(id), udata, false, __func__);
}

int APS5_VABI sceAgcDriverDeleteEqEvent(KernelEqueue eq, int id) {
    if (id < 0) {
        throw std::runtime_error(std::string(__func__) + ": negative event id");
    }
    return DeleteEvent(eq, static_cast<std::uintptr_t>(id), __func__);
}

}

extern "C" int AgcDriverAddQueueEqEvent_nid_postfix(KernelEqueue eq, std::uint32_t queue, std::uintptr_t ident, void* udata) {
    return AddEvent(eq, queue, 1, ident, udata, false, __func__);
}

extern "C" int AgcDriverAddQueueRangeEqEvent_nid_postfix(KernelEqueue eq, std::uint32_t firstQueue, std::uint32_t queueCount, std::uintptr_t ident, void* udata) {
    if (queueCount == 0) throw std::runtime_error(std::string(__func__) + ": no queue");
    return AddEvent(eq, firstQueue, queueCount, ident, udata, true, __func__);
}

extern "C" int AgcDriverDeleteQueueEqEvent_nid_postfix(KernelEqueue eq, std::uintptr_t ident) {
    return DeleteEvent(eq, ident, __func__);
}
