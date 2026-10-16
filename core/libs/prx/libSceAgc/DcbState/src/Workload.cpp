#include "prx/libSceAgc/DcbState/include/Workload.hpp"

#include "prx/libSceAgc/Command/include/Packet.hpp"
#include "prx/libSceAgcDriver/Resource/include/Registration.hpp"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr std::uint32_t OpcodeNop = 0x10;
constexpr std::uint32_t ActivePacketWords = 18;
constexpr std::uint32_t CompletePacketWords = 12;
constexpr std::uint32_t InactivePacketWords = 9;
constexpr std::uint32_t MaxWorkloadId = 63;
constexpr std::uint32_t MaxActiveCount = 63;

uint32_t* NewPacket(CommandBuffer* buf, std::uint32_t words, const char* function) {
 auto* packet = Agc::Command::Allocate(buf, words, function);
 std::memset(packet, 0, words * sizeof(std::uint32_t));
 packet[0] = Agc::Command::Header(OpcodeNop, words, 0);
 return packet;
}

}

namespace Agc::Workload {

std::uint32_t* WriteComplete(CommandBuffer* buf, std::uint32_t streamId, std::uint32_t workloadId, const char* function) {
 if (buf == nullptr || workloadId > MaxWorkloadId || !AgcDriverWorkloadStreamRegistered_nid_postfix(streamId)) return nullptr;
 const std::uint64_t clearMask = ~(std::uint64_t{1} << workloadId);
 auto* packet = NewPacket(buf, CompletePacketWords, function);
 packet[1] = streamId;
 packet[2] = workloadId;
 packet[3] = static_cast<std::uint32_t>(clearMask);
 packet[4] = static_cast<std::uint32_t>(clearMask >> 32);
 return packet;
}

std::uint32_t* WriteActive(CommandBuffer* buf, std::uint32_t streamId, const std::uint32_t* workloadIds, std::uint32_t workloadCount, const char* function) {
 if (buf == nullptr || workloadIds == nullptr || workloadCount == 0 || workloadCount > MaxActiveCount || !AgcDriverWorkloadStreamRegistered_nid_postfix(streamId)) return nullptr;
 std::uint64_t mask = 0;
 for (std::uint32_t index = 0; index < workloadCount; ++index) {
  const auto id = workloadIds[index];
  if (id > MaxWorkloadId) return nullptr;
  const std::uint64_t bit = std::uint64_t{1} << id;
  if ((mask & bit) != 0) return nullptr;
  mask |= bit;
 }
 auto* packet = NewPacket(buf, ActivePacketWords, function);
 packet[1] = streamId;
 packet[2] = static_cast<std::uint32_t>(mask);
 packet[3] = static_cast<std::uint32_t>(mask >> 32);
 return packet;
}

std::uint32_t* WriteStreamInactive(CommandBuffer* buf, std::uint32_t streamId, const char* function) {
 if (buf == nullptr || !AgcDriverWorkloadStreamRegistered_nid_postfix(streamId)) return nullptr;
 auto* packet = NewPacket(buf, InactivePacketWords, function);
 packet[1] = streamId;
 return packet;
}

}

extern "C" {

std::uint32_t* APS5_VABI sceAgcDcbSetWorkloadComplete(CommandBuffer* buf, std::uint32_t streamId, std::uint32_t workloadId) {
 return Agc::Workload::WriteComplete(buf, streamId, workloadId, __func__);
}

std::uint32_t* APS5_VABI sceAgcDcbSetWorkloadsActive(CommandBuffer* buf, std::uint32_t streamId, const std::uint32_t* workloadIds, std::uint32_t workloadCount) {
 return Agc::Workload::WriteActive(buf, streamId, workloadIds, workloadCount, __func__);
}

std::uint32_t* APS5_VABI sceAgcDcbSetWorkloadStreamInactive(CommandBuffer* buf, std::uint32_t streamId) {
 return Agc::Workload::WriteStreamInactive(buf, streamId, __func__);
}

}
