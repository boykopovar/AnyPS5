#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_GCNPACKETS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_GCNPACKETS_HPP

#include "prx/libc/include/General.hpp"
#include <cstdint>
#include <initializer_list>
#include <span>
#include <stdexcept>
#include <vector>

namespace AgcDriver::GcnPackets {

inline constexpr std::uint32_t OpNop = 0x10;
inline constexpr std::uint32_t OpSurfaceSync = 0x43;
inline constexpr std::uint32_t OpEventWriteEop = 0x47;
inline constexpr std::uint32_t OpEventWriteEos = 0x48;
inline constexpr std::uint32_t OpReleaseMem = 0x49;
inline constexpr std::uint32_t OpDmaData = 0x50;
inline constexpr std::uint32_t OpAcquireMem = 0x58;
inline constexpr std::uint32_t DmaRegisterDestination = 1u << 27;
inline constexpr std::uint32_t DmaNoDestinationIncrement = 1u << 29;
inline constexpr std::uint32_t CpDmaReadTagsBytes = 0xc08bu * 4;
inline constexpr std::uint32_t EventControlMask = 0xf3fu;
inline constexpr std::uint32_t EosStoreData = 2;
inline constexpr std::uint32_t ReleaseMemData32 = 1u << 29;
inline constexpr std::uint32_t ReleaseMemControlMask = 0xe7000000u;
inline constexpr std::uint32_t ReleaseMemDestinationMask = 3u << 16;

constexpr std::uint32_t Header(std::uint32_t opcode, std::uint32_t bodyWords) {
    return 0xc0000000u | ((bodyWords - 1) << 16) | (opcode << 8);
}

inline void Append(std::vector<std::uint32_t>& out, std::initializer_list<std::uint32_t> words) {
    out.insert(out.end(), words);
}

inline bool Lower(std::vector<std::uint32_t>& out, std::span<const std::uint32_t> packet) {
    switch ((packet[0] >> 8) & 0xffu) {
        case OpEventWriteEop:
            if (packet.size() != 6) throw std::runtime_error("invalid EVENT_WRITE_EOP size");
            Append(out, {Header(OpReleaseMem, 7), packet[1] & EventControlMask, packet[3] & 0xe3000000u, packet[2], packet[3] & 0xffffu, packet[4], packet[5], 0});
            return true;
        case OpEventWriteEos:
            if (packet.size() != 5) throw std::runtime_error("invalid EVENT_WRITE_EOS size");
            if ((packet[3] >> 29) != EosStoreData) NotImplemented_nid_no_patch("EVENT_WRITE_EOS GDS store");
            Append(out, {Header(OpReleaseMem, 7), packet[1] & EventControlMask, ReleaseMemData32, packet[2], packet[3] & 0xffffu, packet[4], 0, 0});
            return true;
        case OpReleaseMem:
            if (packet.size() != 7) throw std::runtime_error("invalid RELEASE_MEM size");
            if ((packet[2] & ReleaseMemDestinationMask) != 0) NotImplemented_nid_no_patch("RELEASE_MEM to a destination other than memory");
            Append(out, {Header(OpReleaseMem, 7), packet[1] & EventControlMask, packet[2] & ReleaseMemControlMask, packet[3], packet[4] & 0xffffu, packet[5], packet[6], 0});
            return true;
        case OpSurfaceSync:
            if (packet.size() != 5) throw std::runtime_error("invalid SURFACE_SYNC size");
            Append(out, {Header(OpAcquireMem, 6), packet[1], packet[2], 0, packet[3], 0, packet[4]});
            return true;
        case OpDmaData:
            if (packet.size() != 7 || (packet[6] & DmaRegisterDestination) == 0 || (packet[6] & DmaNoDestinationIncrement) == 0 || packet[4] != CpDmaReadTagsBytes || packet[5] != 0) return false;
            out.push_back(Header(OpNop, static_cast<std::uint32_t>(packet.size()) - 1));
            out.insert(out.end(), packet.size() - 1, 0);
            return true;
        default:
            return false;
    }
}

}

#endif
