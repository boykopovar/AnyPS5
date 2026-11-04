#include "prx/libSceJxr/include/Api.hpp"
#include "prx/libc/include/Api.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

constexpr std::int64_t Success = 0;
constexpr std::int64_t OutOfMemory = -101;
constexpr std::int64_t BufferOverflow = -103;
constexpr std::size_t PacketLength = 4096;
constexpr std::size_t Buffer = 0;
constexpr std::size_t Capacity = 1;
constexpr std::size_t Position = 2;
constexpr std::size_t Packets = 3;

std::array<std::uintptr_t, 4>& State(Jxr::Stream* stream, const char* function) {
    if (!stream) throw std::invalid_argument(std::string(function) + " requires a stream");
    return stream->state;
}

std::uint8_t* PacketAt(std::uintptr_t buffer, const char* function) {
    if (buffer <= sizeof(void*))
        throw std::invalid_argument(std::string(function) + ": the stream is past its last packet");
    return reinterpret_cast<std::uint8_t*>(buffer);
}

std::uint8_t* Next(const std::uint8_t* link) {
    std::uint8_t* next;
    std::memcpy(&next, link, sizeof(next));
    return next;
}

}

extern "C" {

std::int64_t APS5_VABI WriteWS_List_nid_postfix(Jxr::Stream* stream, const void* data, std::size_t size) {
    auto& state = State(stream, __func__);
    if (state[Position] + size < state[Position]) return BufferOverflow;
    if (state[Capacity] < state[Position] + size) return BufferOverflow;
    if (state[Position] > PacketLength) throw std::invalid_argument("WriteWS_List: position beyond the packet");
    if (size && !data) throw std::invalid_argument("WriteWS_List: null source");
    const auto* source = static_cast<const std::uint8_t*>(data);
    while (size) {
        const std::size_t chunk = std::min(PacketLength - state[Position], size);
        if (chunk) std::memcpy(PacketAt(state[Buffer], __func__) + state[Position], source, chunk);
        state[Position] += chunk;
        source += chunk;
        size -= chunk;
        if (state[Position] != PacketLength) continue;
        auto* link = static_cast<std::uint8_t*>(calloc_nid_postfix(1, PacketLength + sizeof(void*)));
        if (!link) return OutOfMemory;
        std::memcpy(PacketAt(state[Buffer], __func__) - sizeof(void*), &link, sizeof(link));
        state[Buffer] = reinterpret_cast<std::uintptr_t>(link + sizeof(void*));
        state[Capacity] += PacketLength;
        state[Position] = 0;
        ++state[Packets];
    }
    return Success;
}

std::int64_t APS5_VABI ReadWS_List_nid_postfix(Jxr::Stream* stream, void* data, std::size_t size) {
    auto& state = State(stream, __func__);
    if (state[Position] + size < state[Position]) return BufferOverflow;
    if (state[Capacity] < state[Position] + PacketLength * state[Packets] + size)
        size = state[Capacity] - state[Position] - PacketLength * state[Packets];
    if (state[Position] > PacketLength) throw std::invalid_argument("ReadWS_List: position beyond the packet");
    if (size && !data) throw std::invalid_argument("ReadWS_List: null destination");
    auto* target = static_cast<std::uint8_t*>(data);
    while (size) {
        const std::size_t chunk = std::min(PacketLength - state[Position], size);
        if (chunk) std::memcpy(target, PacketAt(state[Buffer], __func__) + state[Position], chunk);
        state[Position] += chunk;
        target += chunk;
        size -= chunk;
        if (state[Position] != PacketLength) continue;
        state[Buffer] = reinterpret_cast<std::uintptr_t>(Next(PacketAt(state[Buffer], __func__) - sizeof(void*))) + sizeof(void*);
        state[Position] = 0;
        ++state[Packets];
    }
    return Success;
}

std::int64_t APS5_VABI SetPosWS_List_nid_postfix(Jxr::Stream* stream, std::size_t position) {
    auto& state = State(stream, __func__);
    auto* link = reinterpret_cast<std::uint8_t*>(stream + 1);
    state[Position] = 0;
    state[Packets] = 0;
    while (position >= PacketLength && link) {
        link = Next(link);
        position -= PacketLength;
        ++state[Packets];
    }
    if (!link) return Success;
    state[Position] = position;
    state[Buffer] = reinterpret_cast<std::uintptr_t>(link + sizeof(void*));
    return Success;
}

std::int64_t APS5_VABI GetPosWS_List_nid_postfix(Jxr::Stream* stream, std::size_t* position) {
    auto& state = State(stream, __func__);
    if (!position) throw std::invalid_argument("GetPosWS_List requires an output");
    *position = state[Position] + PacketLength * state[Packets];
    return Success;
}

}
