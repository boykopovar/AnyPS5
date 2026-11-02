#include "prx/libSceJxr/libSceJxr.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>

extern "C" {
std::uint32_t JxrPeekBit16_nid_no_patch(void* io, std::uint32_t bits);
std::uint32_t JxrFlushBit16_nid_no_patch(void* io, std::uint32_t bits);
std::uint32_t JxrGetBit16_nid_no_patch(void* io, std::uint32_t bits);
std::uint32_t JxrGetBool16_nid_no_patch(void* io);
std::int32_t JxrGetBit16s_nid_no_patch(void* io, std::uint32_t bits);
std::uint32_t JxrGetBit32_nid_no_patch(void* io, std::uint32_t bits);
std::uint32_t JxrFlushToByte_nid_no_patch(void* io);
void JxrPutBit16z_nid_no_patch(void* io, std::uint32_t value, std::uint32_t bits);
void JxrPutBit16_nid_no_patch(void* io, std::uint32_t value, std::uint32_t bits);
void JxrPutBit32_nid_no_patch(void* io, std::uint32_t value, std::uint32_t bits);
void JxrFillToByte_nid_no_patch(void* io);
std::uint32_t JxrGetSizeRead_nid_no_patch(void* io);
std::uint32_t JxrGetSizeWrite_nid_no_patch(void* io);
std::uint32_t JxrGetPosRead_nid_no_patch(void* io);
std::uint8_t JxrDquantBits_nid_no_patch(std::uint8_t qp);
}

namespace {

void Require(bool condition, const char* function, const char* message) {
    if (!condition) throw std::invalid_argument(std::string(function) + ": " + message);
}

void* State(Jxr::BitIo* io, const char* function) {
    Require(io != nullptr, function, "null bit state");
    return io;
}

void* Stream(Jxr::BitIo* io, const char* function) {
    State(io, function);
    Require(io->current != nullptr, function, "null current pointer");
    return io;
}

void* Read(Jxr::BitIo* io, std::uint32_t bits, std::uint32_t limit, const char* function) {
    Stream(io, function);
    Require(bits <= limit, function, "bit count over the jxrlib limit");
    Require((io->mask & 1) == 0, function, "odd read wrap mask");
    return io;
}

void* Write(Jxr::BitIo* io, std::uint32_t bits, std::uint32_t limit, const char* function) {
    Stream(io, function);
    Require(bits <= limit, function, "bit count over the jxrlib limit");
    return io;
}

} // namespace

extern "C" {

std::uint32_t APS5_VABI peekBit16_nid_postfix(Jxr::BitIo* io, std::uint32_t bits) {
    return JxrPeekBit16_nid_no_patch(Read(io, bits, 16, __func__), bits);
}

std::uint32_t APS5_VABI flushBit16_nid_postfix(Jxr::BitIo* io, std::uint32_t bits) {
    return JxrFlushBit16_nid_no_patch(Read(io, bits, 16, __func__), bits);
}

std::uint32_t APS5_VABI getBit16_nid_postfix(Jxr::BitIo* io, std::uint32_t bits) {
    return JxrGetBit16_nid_no_patch(Read(io, bits, 16, __func__), bits);
}

std::uint32_t APS5_VABI getBool16_nid_postfix(Jxr::BitIo* io) {
    return JxrGetBool16_nid_no_patch(Read(io, 1, 16, __func__));
}

std::int32_t APS5_VABI getBit16s_nid_postfix(Jxr::BitIo* io, std::uint32_t bits) {
    return JxrGetBit16s_nid_no_patch(Read(io, bits, 15, __func__), bits);
}

std::uint32_t APS5_VABI getBit32_nid_postfix(Jxr::BitIo* io, std::uint32_t bits) {
    return JxrGetBit32_nid_no_patch(Read(io, bits, 32, __func__), bits);
}

std::uint32_t APS5_VABI flushToByte_nid_postfix(Jxr::BitIo* io) {
    return JxrFlushToByte_nid_no_patch(Read(io, 0, 16, __func__));
}

void APS5_VABI putBit16z_nid_postfix(Jxr::BitIo* io, std::uint32_t value, std::uint32_t bits) {
    Require(bits >= 32 || (value >> bits) == 0, __func__, "value wider than the bit count");
    JxrPutBit16z_nid_no_patch(Write(io, bits, 16, __func__), value, bits);
}

void APS5_VABI putBit16_nid_postfix(Jxr::BitIo* io, std::uint32_t value, std::uint32_t bits) {
    JxrPutBit16_nid_no_patch(Write(io, bits, 16, __func__), value, bits);
}

void APS5_VABI putBit32_nid_postfix(Jxr::BitIo* io, std::uint32_t value, std::uint32_t bits) {
    JxrPutBit32_nid_no_patch(Write(io, bits, 32, __func__), value, bits);
}

void APS5_VABI fillToByte_nid_postfix(Jxr::BitIo* io) {
    JxrFillToByte_nid_no_patch(Write(io, 0, 16, __func__));
}

std::uint32_t APS5_VABI getBit16_S_nid_postfix(void* codec, Jxr::BitIo* io, std::uint32_t bits) {
    const auto value = getBit16_nid_postfix(io, bits);
    readIS_nid_postfix(codec, io);
    return value;
}

std::uint32_t APS5_VABI getSizeRead_nid_postfix(Jxr::BitIo* io) {
    return JxrGetSizeRead_nid_no_patch(State(io, __func__));
}

std::uint32_t APS5_VABI getSizeWrite_nid_postfix(Jxr::BitIo* io) {
    return JxrGetSizeWrite_nid_no_patch(State(io, __func__));
}

std::uint32_t APS5_VABI getPosRead_nid_postfix(Jxr::BitIo* io) {
    return JxrGetPosRead_nid_no_patch(State(io, __func__));
}

std::uint8_t APS5_VABI dquantBits_nid_postfix(std::uint8_t qp) {
    return JxrDquantBits_nid_no_patch(qp);
}

} // extern "C"
