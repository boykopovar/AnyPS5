#include "prx/libSceAgc/Command/Memory.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

namespace Agc::Command {

std::uint32_t* WriteDma(CommandBuffer* buffer, bool compute, std::uint8_t engine, std::uint8_t dst, std::uint8_t dstCachePolicy, std::uint64_t dstAddress, std::uint8_t src, std::uint8_t srcCachePolicy, std::uint64_t srcAddress, std::uint32_t numBytes, std::uint8_t waitForPrevious, std::uint8_t writeConfirm, std::uint8_t blockEngine, const char* function) {
    CheckBits(engine, 1, function);
    CheckBits(dst, 0xfu, function);
    CheckBits(dstCachePolicy, 3, function);
    CheckBits(srcCachePolicy, 3, function);
    CheckBits(waitForPrevious, 1, function);
    CheckBits(writeConfirm, 1, function);
    CheckBits(blockEngine, 1, function);
    CheckBits(numBytes, 0x3ffffffu, function);
    if (src == 0x14u || src == 0x24u || (!compute && src == 0x64u) || (compute && src == 0x25u)) {
        const auto slot = src == 0x14u ? 0u : src == 0x24u ? 1u : 2u;
        srcAddress = (engine == 1 ? 0x30148u : 0x30174u) + slot * 8u;
    } else {
        CheckBits(src, 0xfu, function);
    }
    const auto control = engine | (static_cast<std::uint32_t>(srcCachePolicy) << 13u) | ((dst & 3u) << 20u) | (static_cast<std::uint32_t>(dstCachePolicy) << 25u) | ((src & 3u) << 29u) | (static_cast<std::uint32_t>(blockEngine) << 31u);
    const auto command = numBytes | ((src & 4u) << 24u) | ((dst & 4u) << 25u) | ((src & 8u) << 25u) | ((dst & 8u) << 26u) | (static_cast<std::uint32_t>(waitForPrevious) << 30u) | (static_cast<std::uint32_t>(writeConfirm) << 31u);
    return Emit(buffer, 0x50u, {control, static_cast<std::uint32_t>(srcAddress), static_cast<std::uint32_t>(srcAddress >> 32u), static_cast<std::uint32_t>(dstAddress), static_cast<std::uint32_t>(dstAddress >> 32u), command}, function);
}

std::uint32_t* WriteData(CommandBuffer* buffer, bool compute, std::uint8_t dst, std::uint8_t cachePolicy, std::uint64_t address, const void* data, std::uint32_t count, std::uint8_t increment, std::uint8_t writeConfirm, const char* function) {
    Require(data != nullptr && count != 0 && count <= 0x3ffdu, function, "invalid write data payload");
    CheckBits(dst, compute ? 0xfu : 0x1fu, function);
    CheckBits(cachePolicy, 3, function);
    CheckBits(increment, 1, function);
    CheckBits(writeConfirm, 1, function);
    Require(compute || (address & 3u) == 0, function, "misaligned write address");
    Require(dst != 0 || writeConfirm == 0, function, "register writes do not support write confirmation");
    std::vector<std::uint32_t> snapshot(count);
    std::memcpy(snapshot.data(), data, count * sizeof(std::uint32_t));
    const auto destination = compute ? static_cast<std::uint32_t>(dst) << 8u : ((dst & 1u) << 30u) | ((dst & 0x1eu) << 7u);
    auto* packet = Allocate(buffer, count + 4u, function);
    packet[0] = Header(0x37u, count + 4u);
    packet[1] = destination | (static_cast<std::uint32_t>(increment) << 16u) | (static_cast<std::uint32_t>(writeConfirm) << 20u) | (static_cast<std::uint32_t>(cachePolicy) << 25u);
    packet[2] = static_cast<std::uint32_t>(address);
    packet[3] = static_cast<std::uint32_t>(address >> 32u);
    std::copy(snapshot.begin(), snapshot.end(), packet + 4);
    return packet;
}

std::uint32_t* WriteWait(CommandBuffer* buffer, std::uint8_t size, std::uint8_t compareFunction, std::uint8_t operation, std::uint8_t cachePolicy, const volatile void* address, std::uint64_t reference, std::uint64_t mask, std::uint32_t pollCycles, const char* function) {
    CheckBits(size, 1, function);
    Require(compareFunction <= 6, function, "invalid wait comparison");
    CheckBits(operation, size == 0 ? 0xfu : 7u, function);
    CheckBits(cachePolicy, 3, function);
    Require((pollCycles >> 4u) <= 0xffffu, function, "wait poll interval overflow");
    if (size == 0) {
        CheckBits(reference, 0xffffffffu, function);
        CheckBits(mask, 0xffffffffu, function);
    }
    const auto guestAddress = reinterpret_cast<std::uintptr_t>(address);
    CheckAddress(guestAddress, size == 0 ? 4 : 8, function);
    CheckBits(guestAddress, 0xffffffffffffull, function);
    const auto waitSize = size == 0 ? 7u : 9u;
    auto* packet = Allocate(buffer, waitSize + 7u, function);
    packet[0] = Header(0x79u, 4, 1);
    packet[1] = 0x342u;
    packet[2] = (size == 0 ? 0xc8010000u : 0xc8020000u) | static_cast<std::uint32_t>(guestAddress >> 32u);
    packet[3] = static_cast<std::uint32_t>(guestAddress);
    auto* wait = packet + 4;
    wait[0] = Header(size == 0 ? 0x3cu : 0x93u, waitSize);
    const auto opBits = size == 0 ? ((operation & 3u) << 8u) | ((operation & 0xcu) << 4u) : ((operation & 1u) << 8u) | ((operation & 6u) << 5u);
    wait[1] = 0x10u | compareFunction | opBits | (static_cast<std::uint32_t>(cachePolicy) << 25u);
    wait[2] = static_cast<std::uint32_t>(guestAddress);
    wait[3] = static_cast<std::uint32_t>(guestAddress >> 32u);
    wait[4] = static_cast<std::uint32_t>(reference);
    if (size == 0) {
        wait[5] = static_cast<std::uint32_t>(mask);
    } else {
        wait[5] = static_cast<std::uint32_t>(reference >> 32u);
        wait[6] = static_cast<std::uint32_t>(mask);
        wait[7] = static_cast<std::uint32_t>(mask >> 32u);
    }
    wait[waitSize - 1u] = pollCycles >> 4u;
    wait[waitSize] = Header(0x79u, 3, 1);
    wait[waitSize + 1u] = 0x342u;
    wait[waitSize + 2u] = 0xc8000000u;
    return packet;
}

std::uint32_t* ValidateWait(std::uint32_t* packet, const char* function) {
    CheckAddress(reinterpret_cast<std::uintptr_t>(packet), 4, function);
    Require(packet[0] == Header(0x79u, 4, 1) && packet[1] == 0x342u, function, "invalid wait wrapper");
    const auto tag = packet[2] & 0xffff0000u;
    Require(tag == 0xc8010000u || tag == 0xc8020000u, function, "invalid wait tag");
    const auto size = tag == 0xc8010000u ? 7u : 9u;
    auto* wait = packet + 4;
    ValidatePacket(wait, size == 7 ? 0x3cu : 0x93u, size, function);
    Require(wait[size] == Header(0x79u, 3, 1) && wait[size + 1u] == 0x342u && wait[size + 2u] == 0xc8000000u, function, "invalid wait end tag");
    return wait;
}

}
