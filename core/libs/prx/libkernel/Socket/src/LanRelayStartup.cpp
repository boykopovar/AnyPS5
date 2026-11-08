#include "prx/libkernel/Socket/include/LanRelay.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <cstdlib>

namespace {

std::unique_ptr<LanRelay::Relay> lanRelay =
    LanRelay::StartRelay(std::getenv("ANYPS5_LAN_LISTEN"), std::getenv("ANYPS5_LAN_PEERS"), stderr);

void StopLanRelay() {
    lanRelay.reset();
}

const bool lanRelayRegistered = (lanRelay != nullptr ? (LibcRegisterShutdown_nid_postfix(StopLanRelay), true) : true);

}
