#include "prx/libSceAgc/Command/include/Packet.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace Agc::Command {

void Require(bool condition, const char* function, const char* reason) {
    if (!condition) {
        throw std::runtime_error(std::string(function) + ": " + reason);
    }
}

void CheckBits(std::uint64_t value, std::uint64_t mask, const char* function) {
    Require((value & ~mask) == 0, function, "reserved bits are set");
}

void CheckAddress(std::uint64_t address, std::uint32_t alignment, const char* function) {
    Require(address != 0 && (address & (alignment - 1u)) == 0, function, "null or misaligned address");
}

std::uint32_t Header(std::uint32_t opcode, std::uint32_t count, std::uint32_t flags) {
    Require(count >= 2 && count <= 0x4001u && opcode <= 0xffu && flags <= 0xffu, __func__, "invalid packet header");
    return 0xc0000000u | ((count - 2u) << 16u) | (opcode << 8u) | flags;
}

namespace {

std::uint32_t available(const CommandBuffer& buffer, const char* function) {
    const auto bottom = reinterpret_cast<std::uintptr_t>(buffer.bottom);
    const auto top = reinterpret_cast<std::uintptr_t>(buffer.top);
    const auto up = reinterpret_cast<std::uintptr_t>(buffer.cursor_up);
    const auto down = reinterpret_cast<std::uintptr_t>(buffer.cursor_down);
    Require(((bottom | top | up | down) & 3u) == 0, function, "misaligned command buffer");
    Require(bottom <= up && up <= down && down <= top, function, "invalid command buffer cursors");
    Require(bottom != 0 || top == 0, function, "null command buffer storage");
    const auto count = (down - up) / sizeof(std::uint32_t);
    Require(count >= buffer.reserved_dw, function, "reserved space exceeds command buffer capacity");
    Require(count - buffer.reserved_dw <= std::numeric_limits<std::uint32_t>::max(), function, "command buffer capacity overflow");
    return static_cast<std::uint32_t>(count - buffer.reserved_dw);
}

}

std::uint32_t* Allocate(CommandBuffer* buffer, std::uint32_t count, const char* function) {
    Require(buffer != nullptr && count != 0, function, "null command buffer or empty allocation");
    if (available(*buffer, function) < count) {
        Require(buffer->callback != nullptr, function, "command buffer exhausted");
        Require(count <= std::numeric_limits<std::uint32_t>::max() - buffer->reserved_dw, function, "command buffer allocation overflow");
        Require(buffer->callback(buffer, count + buffer->reserved_dw, buffer->user_data), function, "command buffer allocation callback failed");
        Require(available(*buffer, function) >= count, function, "command buffer allocation callback returned insufficient space");
    }
    auto* result = buffer->cursor_up;
    buffer->cursor_up += count;
    return result;
}

std::uint32_t* Emit(CommandBuffer* buffer, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload, const char* function) {
    Require(payload.size() <= 0x4000u, function, "packet payload exceeds maximum size");
    const auto count = static_cast<std::uint32_t>(payload.size()) + 1u;
    const auto header = Header(opcode, count);
    auto* packet = Allocate(buffer, count, function);
    packet[0] = header;
    std::copy(payload.begin(), payload.end(), packet + 1);
    return packet;
}

void ValidatePacket(const std::uint32_t* packet, std::uint32_t opcode, std::uint32_t count, const char* function) {
    CheckAddress(reinterpret_cast<std::uintptr_t>(packet), 4, function);
    Require(packet[0] == Header(opcode, count), function, "unexpected packet type, size or flags");
}

std::uint32_t* WriteNop(CommandBuffer* buffer, std::uint32_t count, const char* function) {
    const auto header = Header(0x10u, count);
    auto* packet = Allocate(buffer, count, function);
    packet[0] = header;
    std::fill_n(packet + 1, count - 1u, 0u);
    return packet;
}

std::uint32_t* WriteRegisterRange(CommandBuffer* buffer, std::uint32_t opcode, std::uint32_t offset, const std::uint32_t* values, std::uint32_t count, const char* function) {
    Require(count != 0 && count <= 0x3fffu && offset <= 0xffffu && count <= 0x10000u - offset, function, "invalid register range");
    CheckAddress(reinterpret_cast<std::uintptr_t>(values), 4, function);
    const std::vector<std::uint32_t> snapshot(values, values + count);
    auto* packet = Allocate(buffer, count + 2u, function);
    packet[0] = Header(opcode, count + 2u);
    packet[1] = offset;
    std::copy(snapshot.begin(), snapshot.end(), packet + 2);
    return packet;
}

std::uint32_t* WriteRegisters(CommandBuffer* buffer, std::uint32_t opcode, const volatile ShaderRegister* registers, std::uint32_t count, bool snapshotAll, const char* function) {
    Require(count != 0, function, "empty register list");
    CheckAddress(reinterpret_cast<std::uintptr_t>(registers), 4, function);
    std::vector<ShaderRegister> snapshot;
    if (snapshotAll) {
        snapshot.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i) {
            snapshot.push_back({registers[i].offset, registers[i].value});
            CheckBits(snapshot.back().offset, 0xffffu, function);
        }
    }
    const auto offsetAt = [&](std::uint32_t index) { return snapshotAll ? snapshot[index].offset : registers[index].offset; };
    const auto valueAt = [&](std::uint32_t index) { return snapshotAll ? snapshot[index].value : registers[index].value; };
    std::uint32_t* first = nullptr;
    std::uint32_t index = 0;
    while (index < count) {
        const auto offset = offsetAt(index);
        CheckBits(offset, 0xffffu, function);
        std::vector<std::uint32_t> values{valueAt(index++)};
        while (index < count && offsetAt(index) == offset + values.size() && values.size() < 0x3fffu) {
            CheckBits(offsetAt(index), 0xffffu, function);
            values.push_back(valueAt(index++));
        }
        auto* packet = WriteRegisterRange(buffer, opcode, offset, values.data(), static_cast<std::uint32_t>(values.size()), function);
        if (first == nullptr) {
            first = packet;
        }
    }
    return first;
}

std::uint32_t* WriteIndirectRegisters(CommandBuffer* buffer, std::uint32_t opcode, const volatile ShaderRegister* registers, std::uint32_t count, const char* function) {
    const auto address = reinterpret_cast<std::uintptr_t>(registers);
    CheckAddress(address, 4, function);
    CheckBits(count, 0x3fffu, function);
    return Emit(buffer, opcode, {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), 0x80000000u, count}, function);
}

void PatchIndirectAddress(std::uint32_t* packet, std::uint32_t opcode, const volatile ShaderRegister* registers, const char* function) {
    ValidatePacket(packet, opcode, 5, function);
    Require(packet[3] == 0x80000000u && (packet[4] & ~0x3fffu) == 0, function, "invalid indirect register packet");
    const auto address = reinterpret_cast<std::uintptr_t>(registers);
    CheckAddress(address, 4, function);
    packet[1] = static_cast<std::uint32_t>(address);
    packet[2] = static_cast<std::uint32_t>(address >> 32u);
}

void PatchIndirectCount(std::uint32_t* packet, std::uint32_t opcode, std::uint32_t count, const char* function) {
    ValidatePacket(packet, opcode, 5, function);
    Require(packet[3] == 0x80000000u && packet[4] <= 0x3fffu && count <= 0x3fffu - packet[4], function, "indirect register count overflow or invalid packet");
    packet[4] += count;
}

}
