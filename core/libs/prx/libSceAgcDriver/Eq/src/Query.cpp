#include "prx/libSceAgcDriver/Eq/include/Query.hpp"

#include <cstdint>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestPtr.hpp"

namespace {

KernelEvent ReadKernelEvent(const KernelEvent* ev, const char* caller) {
    if (ev == nullptr || reinterpret_cast<std::uintptr_t>(ev) % alignof(KernelEvent) != 0) {
        throw std::runtime_error(std::string(caller) + ": null or misaligned event");
    }
    KernelEvent event{};
    if (!GuestPtr<const KernelEvent>(ev).CopyIn(event)) {
        throw std::runtime_error(std::string(caller) + ": unreadable event");
    }
    return event;
}

}

extern "C" {

uint32_t APS5_VABI sceAgcDriverGetEqContextId(const KernelEvent* ev) {
    const auto event = ReadKernelEvent(ev, __func__);
    if (event.filter != -14) {
        throw std::runtime_error(std::string(__func__) + ": not a graphics event");
    }
    return static_cast<uint32_t>(static_cast<std::uint64_t>(event.data) >> 16u);
}

int APS5_VABI sceAgcDriverGetEqEventType(const KernelEvent* ev) {
    const auto event = ReadKernelEvent(ev, __func__);
    if (event.filter == -14) return static_cast<int>(static_cast<std::uint32_t>(static_cast<std::uint64_t>(event.data)));
    if (event.data < std::numeric_limits<int>::min() || event.data > std::numeric_limits<int>::max()) {
        throw std::runtime_error(std::string(__func__) + ": event type overflow");
    }
    return static_cast<int>(event.data);
}

}
