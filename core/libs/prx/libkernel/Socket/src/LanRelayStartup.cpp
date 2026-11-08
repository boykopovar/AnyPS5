#include "prx/libkernel/Socket/include/LanRelay.hpp"
#include <cstdlib>

namespace {

const std::unique_ptr<LanRelay::Relay> lanRelay =
    LanRelay::StartRelay(std::getenv("ANYPS5_LAN_LISTEN"), std::getenv("ANYPS5_LAN_PEERS"), stderr);

}
