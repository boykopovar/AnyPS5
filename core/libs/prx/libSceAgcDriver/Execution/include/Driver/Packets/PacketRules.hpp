#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_PACKETS_PACKETRULES_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_PACKETS_PACKETRULES_HPP

#include <cstdint>
#include <span>

namespace AgcDriver::DriverDetail {

struct Submission;

}

namespace AgcDriver::DriverDetail::PacketRules {

bool orderedFills();
bool keepsDrawFinisher(std::uint32_t header, std::uint32_t opcode);
bool waitTakesPendingLabel(std::span<const std::uint32_t> packet);
bool keepsDrawPipeline(std::span<const std::uint32_t> packet, std::uint32_t header, std::uint32_t opcode);
bool sealsWriteOnly(std::span<const std::uint32_t> packet, std::uint32_t opcode);
bool& drawnSinceSeal();
void dumpQueue(const Submission& submission);

}

#endif
