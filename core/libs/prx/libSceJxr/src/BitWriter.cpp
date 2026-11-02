#include "prx/libSceJxr/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

constexpr std::uint32_t ShortBits = 16;
constexpr std::uint32_t LongBits = 32;
constexpr std::uintptr_t PacketLength = 4096;
constexpr std::uintptr_t WindowLength = PacketLength * 2;

std::uint32_t LowBits(std::uint32_t value, std::uint32_t width) {
    return value & ((std::uint32_t{1} << width) - 1);
}

std::uint8_t* MaskPointer(std::uintptr_t address, std::int32_t mask) {
    return reinterpret_cast<std::uint8_t*>(address & static_cast<std::uintptr_t>(static_cast<std::intptr_t>(mask)));
}

void Put(Jxr::BitIo* io, std::uint32_t bits, std::uint32_t width, const char* name) {
    std::uint8_t* const current = io->current;
    if (!current) throw std::invalid_argument(std::string(name) + ": invalid argument");
    const std::uint32_t accumulator = (io->accumulator << width) | bits;
    const std::uint32_t used = io->bitsUsed + width;
    std::uint8_t* const next = MaskPointer(reinterpret_cast<std::uintptr_t>(current) + ((used >> 3) & 2), io->mask);
    if (!next) throw std::invalid_argument(std::string(name) + ": invalid argument");
    const std::uint32_t window = (accumulator << ((LongBits - used) & (LongBits - 1))) >> ShortBits;
    const std::uint8_t halfword[2] = {static_cast<std::uint8_t>(window >> 8), static_cast<std::uint8_t>(window)};
    std::memcpy(current, halfword, sizeof(halfword));
    io->accumulator = accumulator;
    io->bitsUsed = used & (ShortBits - 1);
    io->current = next;
}

}

extern "C" {

void APS5_VABI putBit16z_nid_postfix(Jxr::BitIo* io, std::uint32_t bits, std::uint32_t width) {
    if (!io || width > ShortBits || (bits >> width) != 0) APS5_INVALID_ARG_EX;
    Put(io, bits, width, __func__);
}

void APS5_VABI putBit16_nid_postfix(Jxr::BitIo* io, std::uint32_t bits, std::uint32_t width) {
    if (!io || width > ShortBits) APS5_INVALID_ARG_EX;
    Put(io, LowBits(bits, width), width, __func__);
}

void APS5_VABI putBit32_nid_postfix(Jxr::BitIo* io, std::uint32_t bits, std::uint32_t width) {
    if (!io || width > LongBits) APS5_INVALID_ARG_EX;
    const Jxr::BitIo saved = *io;
    try {
        if (width > ShortBits) {
            Put(io, LowBits(bits >> (width - ShortBits), ShortBits), ShortBits, __func__);
            width -= ShortBits;
        }
        Put(io, LowBits(bits, width), width, __func__);
    } catch (...) {
        *io = saved;
        throw;
    }
}

void APS5_VABI fillToByte_nid_postfix(Jxr::BitIo* io) {
    if (!io) APS5_INVALID_ARG_EX;
    Put(io, 0, (ShortBits - io->bitsUsed) & 7, __func__);
}

std::uint32_t APS5_VABI getSizeRead_nid_postfix(const Jxr::BitIo* io) {
    if (!io) APS5_INVALID_ARG_EX;
    const auto start = reinterpret_cast<std::uintptr_t>(io->start);
    const auto current = reinterpret_cast<std::uintptr_t>(io->current);
    return static_cast<std::uint32_t>(start + WindowLength - current) - io->bitsUsed / 8;
}

std::uint32_t APS5_VABI getSizeWrite_nid_postfix(const Jxr::BitIo* io) {
    if (!io) APS5_INVALID_ARG_EX;
    const auto start = reinterpret_cast<std::uintptr_t>(io->start);
    const auto current = reinterpret_cast<std::uintptr_t>(io->current);
    return static_cast<std::uint32_t>(current + (start <= current ? 0 : WindowLength) - start) + io->bitsUsed / 8;
}

std::uint32_t APS5_VABI getPosRead_nid_postfix(const Jxr::BitIo* io) {
    if (!io) APS5_INVALID_ARG_EX;
    const auto start = reinterpret_cast<std::uintptr_t>(io->start);
    const auto current = reinterpret_cast<std::uintptr_t>(io->current);
    const std::size_t cached = (start + WindowLength - current) - io->bitsUsed / 8;
    return static_cast<std::uint32_t>(io->referenceOffset - cached);
}

}
