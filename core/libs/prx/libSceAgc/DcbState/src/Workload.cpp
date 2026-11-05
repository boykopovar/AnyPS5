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
constexpr std::uint32_t MaxWorkloadId = 63;
constexpr std::uint32_t MaxActiveCount = 63;

uint32_t* NewPacket(CommandBuffer* buf, std::uint32_t words, const char* function) {
 auto* packet = Agc::Command::Allocate(buf, words, function);
 std::memset(packet, 0, words * sizeof(std::uint32_t));
 packet[0] = Agc::Command::Header(OpcodeNop, words, 0);
 return packet;
}

}

extern "C" {

uint32_t* APS5_VABI sceAgcDcbSetWorkloadComplete(CommandBuffer* buf, uint32_t stream_id, uint32_t workload_id) {
 if (buf == nullptr || workload_id > MaxWorkloadId || !AgcDriverWorkloadStreamRegistered_nid_postfix(stream_id)) return nullptr;
 const std::uint64_t clearMask = ~(std::uint64_t{1} << workload_id);
 auto* packet = NewPacket(buf, CompletePacketWords, __func__);
 packet[1] = stream_id;
 packet[2] = workload_id;
 packet[3] = static_cast<std::uint32_t>(clearMask);
 packet[4] = static_cast<std::uint32_t>(clearMask >> 32);
 return packet;
}

uint32_t* APS5_VABI sceAgcDcbSetWorkloadsActive(CommandBuffer* buf, uint32_t stream_id, const uint32_t* workload_ids, uint32_t workload_count) {
 if (buf == nullptr || workload_ids == nullptr || workload_count == 0 || workload_count > MaxActiveCount || !AgcDriverWorkloadStreamRegistered_nid_postfix(stream_id)) return nullptr;
 std::uint64_t mask = 0;
 for (std::uint32_t index = 0; index < workload_count; ++index) {
  const auto id = workload_ids[index];
  if (id > MaxWorkloadId) return nullptr;
  const std::uint64_t bit = std::uint64_t{1} << id;
  if ((mask & bit) != 0) return nullptr;
  mask |= bit;
 }
 auto* packet = NewPacket(buf, ActivePacketWords, __func__);
 packet[1] = stream_id;
 packet[2] = static_cast<std::uint32_t>(mask);
 packet[3] = static_cast<std::uint32_t>(mask >> 32);
 return packet;
}

uint32_t* APS5_VABI sceAgcDcbSetWorkloadStreamInactive(CommandBuffer* buf, uint32_t streamId) {
 if (buf == nullptr || !AgcDriverWorkloadStreamRegistered_nid_postfix(streamId)) return nullptr;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

}
