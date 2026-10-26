#include "prx/libc/include/Api.hpp"
#include "prx/libSceJxr/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "Stream.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {
constexpr std::int64_t WmpErrSuccess = 0;
}

extern "C" {

APS5_DUMMY_FUN
std::int64_t APS5_VABI PKFree_nid_postfix(void** memory) {
    if (memory != nullptr) {
        free_nid_postfix(*memory);

        *memory = nullptr;
    }
    return WmpErrSuccess;
}

int APS5_VABI PixelFormatLookup_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::int64_t APS5_VABI readIS_nid_postfix(void*, Jxr::BitIo* io) {
    if (!io) NotImplemented_nid_no_patch(__func__);
    constexpr std::uintptr_t packetSize = 4096;
    const auto start = reinterpret_cast<std::uintptr_t>(io->start);
    if (((start ^ reinterpret_cast<std::uintptr_t>(io->current)) & packetSize) == 0) return WmpErrSuccess;
    auto* stream = io->stream;
    if (!stream || !stream->setPosition) NotImplemented_nid_no_patch(__func__);
    stream->setPosition(stream, io->referenceOffset);
    if (!io->start || !stream->read) NotImplemented_nid_no_patch(__func__);
    {
        GuestArena::HostWrite access(io->start, packetSize);
        if (!access.Open()) NotImplemented_nid_no_patch(__func__);
        stream->read(stream, io->start, packetSize);
    }
    io->referenceOffset += packetSize;
    std::memcpy(&io->shadow, io->start, sizeof(io->shadow));
    const auto mask = static_cast<std::uintptr_t>(static_cast<std::intptr_t>(io->mask));
    io->start = reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(io->start) + packetSize) & mask);
    return WmpErrSuccess;
}

std::int64_t APS5_VABI writeIS_nid_postfix(void*, Jxr::BitIo* io) {
    if (!io) NotImplemented_nid_no_patch(__func__);
    constexpr std::uintptr_t packetSize = 4096;
    const auto start = reinterpret_cast<std::uintptr_t>(io->start);
    if (((start ^ reinterpret_cast<std::uintptr_t>(io->current)) & packetSize) == 0) return WmpErrSuccess;
    if (!io->start || !io->stream || !io->stream->write) NotImplemented_nid_no_patch(__func__);
    const auto result = io->stream->write(io->stream, io->start, packetSize);
    if (result < 0) return result;
    const auto mask = static_cast<std::uintptr_t>(static_cast<std::intptr_t>(io->mask));
    io->start = reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(io->start) + packetSize) & mask);
    return result;
}

}
