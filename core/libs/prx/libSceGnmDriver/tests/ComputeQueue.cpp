#include "prx/libSceGnmDriver/libSceGnmDriver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "SceTypes.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

template<typename TAction>
std::string ExpectFailure(TAction action) {
    try { action(); }
    catch (const std::runtime_error& error) { return error.what(); }
    throw std::runtime_error("expected an exception");
}

constexpr std::uint32_t OpNop = 0x10;
constexpr std::uint32_t RingWords = 16;
constexpr std::size_t BlockBytes = 65536;

constexpr std::uint32_t Type3(std::uint32_t opcode, std::uint32_t bodyWords) {
    return 0xc0000000u | ((bodyWords - 1u) << 16u) | (opcode << 8u);
}

struct Memory {
    alignas(256) std::uint32_t ringA[RingWords];
    alignas(256) std::uint32_t ringB[RingWords];
    alignas(256) std::uint32_t called[8];
    alignas(256) std::uint32_t readA;
    alignas(256) std::uint32_t readB;
    alignas(256) volatile std::uint32_t gate;
    alignas(256) volatile std::uint32_t direct;
    alignas(256) volatile std::uint32_t indirect;
    alignas(256) volatile std::uint32_t wrapped;
    alignas(256) volatile std::uint32_t waiting;
    alignas(256) volatile std::uint32_t independent;
};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "cannot allocate the guest block");
        std::memset(block, 0, BlockBytes);
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        std::free(block);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    Memory& Data() { return *reinterpret_cast<Memory*>(block); }

private:
    std::uint8_t* block = nullptr;
};

std::uint32_t Low(const volatile void* pointer) {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

std::uint32_t High(const volatile void* pointer) {
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer)) >> 32u);
}

void WriteData(std::vector<std::uint32_t>& words, volatile std::uint32_t* target, std::uint32_t value) {
    words.insert(words.end(), {0xc0033700u, 0x00100200u, Low(target), High(target), value});
}

void WaitEqual(std::vector<std::uint32_t>& words, volatile std::uint32_t* target, std::uint32_t value) {
    words.insert(words.end(), {0xc0053c00u, 0x13u, Low(target), High(target), value, 0xffffffffu, 0x19u});
}

std::uint32_t Put(std::uint32_t* ring, std::uint32_t offset, const std::vector<std::uint32_t>& words) {
    for (const auto word : words) {
        ring[offset] = word;
        offset = (offset + 1) % RingWords;
    }
    return offset;
}

bool WaitFor(const volatile std::uint32_t& label, std::uint32_t value) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (label != value) {
        if (std::chrono::steady_clock::now() > deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}

void TestComputeQueues() {
    GuestBlock block;
    auto& memory = block.Data();
    const int first = sceGnmMapComputeQueue(0, 0, memory.ringA, RingWords, &memory.readA);
    Require(first > 0, "sceGnmMapComputeQueue did not return a queue id");

    std::vector<std::uint32_t> words{Type3(OpNop, 2), 0x68750003u, 0u};
    WriteData(words, &memory.direct, 1);
    auto offset = Put(memory.ringA, 0, words);
    Require(sceGnmDingDong(first, offset) == 0, "sceGnmDingDong failed");
    AgcDriverWaitIdle_nid_postfix();
    Require(memory.direct == 1, "a WRITE_DATA behind a marker NOP in the ring did not run");
    Require(memory.readA == offset, "the read offset was not moved to the ring offset sceGnmDingDong was given");

    words.clear();
    WriteData(words, &memory.indirect, 2);
    std::memcpy(memory.called, words.data(), words.size() * sizeof(std::uint32_t));
    offset = Put(memory.ringA, offset, {Type3(0x3f, 3), Low(memory.called), High(memory.called) & 0xffffu, static_cast<std::uint32_t>(words.size())});
    Require(sceGnmDingDong(first, offset) == 0, "sceGnmDingDong with an INDIRECT_BUFFER failed");
    AgcDriverWaitIdle_nid_postfix();
    Require(memory.indirect == 2, "the command buffer an INDIRECT_BUFFER in the ring calls did not run");

    words.clear();
    WriteData(words, &memory.wrapped, 3);
    offset = Put(memory.ringA, offset, words);
    Require(offset < 5, "the third packet does not wrap past the end of the ring");
    Require(sceGnmDingDong(first, offset) == 0, "sceGnmDingDong across the end of the ring failed");
    AgcDriverWaitIdle_nid_postfix();
    Require(memory.wrapped == 3, "a packet that wraps past the end of the ring did not run");
    Require(memory.readA == offset, "the read offset did not wrap");

    const int second = sceGnmMapComputeQueue(0, 1, memory.ringB, RingWords, &memory.readB);
    Require(second > 0 && second != first, "a second ring did not get its own queue id");
    words.clear();
    WaitEqual(words, &memory.gate, 1);
    WriteData(words, &memory.waiting, 4);
    const auto waitingOffset = Put(memory.ringB, 0, words);
    Require(sceGnmDingDong(second, waitingOffset) == 0, "sceGnmDingDong on the second ring failed");
    words.clear();
    WriteData(words, &memory.independent, 5);
    offset = Put(memory.ringA, offset, words);
    Require(sceGnmDingDong(first, offset) == 0, "sceGnmDingDong on the first ring while the second waits failed");
    const bool ran = WaitFor(memory.independent, 5);
    const bool waited = memory.waiting == 0;
    memory.gate = 1;
    AgcDriverWaitIdle_nid_postfix();
    Require(ran, "work on one ring waited for a WAIT_REG_MEM on another ring");
    Require(waited, "the WAIT_REG_MEM on the second ring did not wait");
    Require(memory.waiting == 4 && memory.readB == waitingOffset, "the second ring's work did not run after its wait");

    words.clear();
    WriteData(words, &memory.wrapped, 6);
    const auto packetEnd = Put(memory.ringA, offset, words);
    Require(ExpectFailure([&] { sceGnmDingDong(first, (offset + 3) % RingWords); }).find("inside a packet") != std::string::npos,
        "an offset inside a packet did not throw");
    Require(memory.readA == offset && packetEnd != offset, "an offset inside a packet moved the read offset");
    const auto rewindEnd = Put(memory.ringA, packetEnd, {Type3(0x59, 1), 0u});
    Require(ExpectFailure([&] { sceGnmDingDong(first, rewindEnd); }).find("REWIND") != std::string::npos, "a REWIND packet in the ring did not throw");
    AgcDriverWaitIdle_nid_postfix();
    Require(memory.readA == offset && memory.wrapped == 3, "a ring with a REWIND packet was submitted in part");

    Require(ExpectFailure([&] { sceGnmMapComputeQueue(0, 0, memory.ringB, RingWords, &memory.readB); }).find("already mapped") != std::string::npos,
        "mapping a pipe and queue twice did not throw");
    Require(ExpectFailure([&] { sceGnmMapComputeQueue(1, 0, memory.ringB, 12, &memory.readB); }).find("power of two") != std::string::npos,
        "a ring size that is not a power of two did not throw");
    Require(ExpectFailure([&] { sceGnmMapComputeQueue(1, 0, nullptr, RingWords, &memory.readB); }).find("missing") != std::string::npos,
        "a null ring did not throw");
    Require(ExpectFailure([&] { sceGnmMapComputeQueue(7, 0, memory.ringB, RingWords, &memory.readB); }).find("pipe above 6") != std::string::npos,
        "pipe 7 did not throw");
    Require(ExpectFailure([&] { sceGnmMapComputeQueue(1, 8, memory.ringB, RingWords, &memory.readB); }).find("queue above 7") != std::string::npos,
        "queue 8 did not throw");
    Require(ExpectFailure([&] { sceGnmMapComputeQueue(1, 0, memory.ringB + 1, RingWords, &memory.readB); }).find("256-byte") != std::string::npos,
        "a ring that is not 256-byte aligned did not throw");
    Require(ExpectFailure([&] { sceGnmMapComputeQueue(1, 0, memory.ringB, RingWords, reinterpret_cast<std::uint8_t*>(&memory.readB) + 2); }).find("4-byte") != std::string::npos,
        "a read offset that is not 4-byte aligned did not throw");
    const int last = sceGnmMapComputeQueue(6, 7, memory.ringB, RingWords, &memory.readB);
    Require(last > 0 && last != first && last != second, "pipe 6, queue 7 did not get its own queue id");
    Require(sceGnmUnmapComputeQueue(last) == 0, "sceGnmUnmapComputeQueue of pipe 6, queue 7 failed");
    Require(ExpectFailure([&] { sceGnmDingDong(first, RingWords); }).find("past the end") != std::string::npos, "an offset past the end of the ring did not throw");
    Require(ExpectFailure([&] { sceGnmDingDong(0x7fffffffu, 0); }).find("not mapped") != std::string::npos, "an unknown queue id did not throw");
    Require(sceGnmUnmapComputeQueue(second) == 0, "sceGnmUnmapComputeQueue failed");
    Require(ExpectFailure([&] { sceGnmDingDong(second, 0); }).find("not mapped") != std::string::npos, "sceGnmDingDong after sceGnmUnmapComputeQueue did not throw");
    Require(ExpectFailure([&] { sceGnmUnmapComputeQueue(second); }).find("not mapped") != std::string::npos, "unmapping a queue twice did not throw");
    Require(sceGnmUnmapComputeQueue(first) == 0, "sceGnmUnmapComputeQueue of the first ring failed");
    const int again = sceGnmMapComputeQueue(0, 0, memory.ringA, RingWords, &memory.readA);
    Require(again > 0, "a pipe and queue could not be mapped again after sceGnmUnmapComputeQueue");
    Require(sceGnmUnmapComputeQueue(again) == 0, "sceGnmUnmapComputeQueue of the remapped ring failed");
}

}

int main() {
    try {
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        device.reset();
        TestComputeQueues();
        std::puts("GnmDriver compute queue tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
