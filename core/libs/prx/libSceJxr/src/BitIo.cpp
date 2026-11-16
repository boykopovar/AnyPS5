#include "prx/libSceJxr/libSceJxr.hpp"
#include "prx/libc/include/General.hpp"
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
    Require(io->bitsUsed < 16, function, "write position outside the 16-bit word");
    Require(reinterpret_cast<std::uintptr_t>(io->current) % alignof(std::uint16_t) == 0, function,
            "unaligned write position");
    return io;
}

std::uint32_t ZeroBitRead(Jxr::BitIo* io) {
    JxrFlushBit16_nid_no_patch(io, 0);
    return 0;
}

std::uint32_t LoadBigEndian(const std::uint8_t* bytes) {
    return (std::uint32_t{bytes[0]} << 24) | (std::uint32_t{bytes[1]} << 16) |
        (std::uint32_t{bytes[2]} << 8) | bytes[3];
}

void PutMasked(Jxr::BitIo* io, std::uint32_t value, std::uint32_t bits) {
    if (bits == 0 && io->bitsUsed == 0) return;
    JxrPutBit16z_nid_no_patch(io, value & (0xffffu >> (16 - bits)), bits);
}

} // namespace

extern "C" {

std::int64_t APS5_VABI attachISRead_nid_postfix(Jxr::BitIo* io, Jxr::Stream* stream, void* codec) {
    static_cast<void>(codec);
    Require(io != nullptr, __func__, "null bit state");
    Require(stream != nullptr, __func__, "null stream");
    Require(stream->getPosition != nullptr, __func__, "null get-position callback");
    stream->getPosition(stream, &io->referenceOffset);
    const auto ioAddress = reinterpret_cast<std::uintptr_t>(io);
    Require(ioAddress >= 8192, __func__, "bit state address underflows the packet buffer");
    auto* const start = reinterpret_cast<std::uint8_t*>(ioAddress - 8192);
    io->start = start;
    io->current = start;
    Require(stream->setPosition != nullptr, __func__, "null set-position callback");
    stream->setPosition(stream, io->referenceOffset);
    Require(stream->read != nullptr, __func__, "null read callback");
    stream->read(stream, io->start, 8192);
    io->referenceOffset += 8192;
    io->accumulator = LoadBigEndian(io->start);
    io->bitsUsed = 0;
    io->mask = ~static_cast<std::int32_t>(8192) & ~std::int32_t{1};
    io->stream = stream;
    return 0;
}

std::int64_t APS5_VABI attachISWrite_nid_postfix(Jxr::BitIo* io, Jxr::Stream* stream) {
    State(io, __func__);
    Require(stream && stream->getPosition, __func__, "missing stream position callback");
    stream->getPosition(stream, &io->referenceOffset);
    const auto address = reinterpret_cast<std::uintptr_t>(io);
    if (address < 8192) NotImplemented_nid_no_patch(__func__);
    io->start = reinterpret_cast<std::uint8_t*>(address - 8192);
    io->current = io->start;
    io->accumulator = 0;
    io->bitsUsed = 0;
    io->mask = ~8192;
    io->stream = stream;
    return 0;
}

std::uint32_t APS5_VABI peekBit16_nid_postfix(Jxr::BitIo* io, std::uint32_t bits) {
    Read(io, bits, 16, __func__);
    return bits == 0 ? 0 : JxrPeekBit16_nid_no_patch(io, bits);
}

std::uint32_t APS5_VABI flushBit16_nid_postfix(Jxr::BitIo* io, std::uint32_t bits) {
    return JxrFlushBit16_nid_no_patch(Read(io, bits, 16, __func__), bits);
}

std::uint32_t APS5_VABI getBit16_nid_postfix(Jxr::BitIo* io, std::uint32_t bits) {
    Read(io, bits, 16, __func__);
    return bits == 0 ? ZeroBitRead(io) : JxrGetBit16_nid_no_patch(io, bits);
}

std::uint32_t APS5_VABI getBool16_nid_postfix(Jxr::BitIo* io) {
    return JxrGetBool16_nid_no_patch(Read(io, 1, 16, __func__));
}

std::int32_t APS5_VABI getBit16s_nid_postfix(Jxr::BitIo* io, std::uint32_t bits) {
    return JxrGetBit16s_nid_no_patch(Read(io, bits, 15, __func__), bits);
}

std::uint32_t APS5_VABI getBit32_nid_postfix(Jxr::BitIo* io, std::uint32_t bits) {
    Read(io, bits, 32, __func__);
    return bits == 0 ? ZeroBitRead(io) : JxrGetBit32_nid_no_patch(io, bits);
}

std::uint32_t APS5_VABI flushToByte_nid_postfix(Jxr::BitIo* io) {
    return JxrFlushToByte_nid_no_patch(Read(io, 0, 16, __func__));
}

void APS5_VABI putBit16z_nid_postfix(Jxr::BitIo* io, std::uint32_t value, std::uint32_t bits) {
    Require(bits >= 32 || (value >> bits) == 0, __func__, "value wider than the bit count");
    Write(io, bits, 16, __func__);
    if (bits == 0 && io->bitsUsed == 0) return;
    JxrPutBit16z_nid_no_patch(io, value, bits);
}

void APS5_VABI putBit16_nid_postfix(Jxr::BitIo* io, std::uint32_t value, std::uint32_t bits) {
    Write(io, bits, 16, __func__);
    PutMasked(io, value, bits);
}

void APS5_VABI putBit32_nid_postfix(Jxr::BitIo* io, std::uint32_t value, std::uint32_t bits) {
    Write(io, bits, 32, __func__);
    if (bits > 16) {
        PutMasked(io, value >> (bits - 16), 16);
        bits -= 16;
    }
    PutMasked(io, value, bits);
}

void APS5_VABI fillToByte_nid_postfix(Jxr::BitIo* io) {
    Write(io, 0, 16, __func__);
    if (io->bitsUsed == 0) return;
    JxrFillToByte_nid_no_patch(io);
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
