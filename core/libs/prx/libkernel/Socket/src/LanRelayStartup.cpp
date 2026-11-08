#include "prx/libkernel/Socket/include/LanRelay.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <cstdlib>
#include <mutex>

namespace {

std::mutex lanRelayMutex;

std::unique_ptr<LanRelay::Relay> lanRelay =
    LanRelay::StartRelay(std::getenv("ANYPS5_LAN_LISTEN"), std::getenv("ANYPS5_LAN_PEERS"), stderr);

void StopLanRelay() {
    std::lock_guard lock(lanRelayMutex);
    lanRelay.reset();
}

const bool lanRelayRegistered = (lanRelay != nullptr ? (LibcRegisterShutdown_nid_postfix(StopLanRelay), true) : true);

}

namespace LanRelay {

bool WithActiveRelay(const std::function<void(Relay&)>& action) {
    std::lock_guard lock(lanRelayMutex);
    if (!lanRelay) return false;
    action(*lanRelay);
    return true;
}

}
