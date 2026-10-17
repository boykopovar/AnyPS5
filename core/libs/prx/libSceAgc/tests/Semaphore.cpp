#include "prx/libSceAgc/Command/include/Packet.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/Shutdown.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <vector>

extern "C" int APS5_VABI sceAgcSetAmmSemaphoreMemory(void* memory, std::uint64_t sizeInBytes);
extern "C" int APS5_VABI sceAgcGetSemaphoreLabel(std::uint32_t index, void** label);
extern "C" std::uint32_t* APS5_VABI sceAgcCbMemSemaphore(CommandBuffer* buf, std::uint8_t action, std::uint8_t clientCode, const volatile void* address);

namespace {

constexpr std::uint64_t blockBytes = 0x4000u;
constexpr std::uint64_t labelBytes = 32u;
constexpr std::uint64_t counterBytes = 8u;
constexpr std::uint64_t memoryBytes = 2u * blockBytes;
constexpr std::uint64_t storageBytes = 4u * blockBytes;
constexpr std::uint32_t labelCount = static_cast<std::uint32_t>(memoryBytes / labelBytes);
constexpr std::uint8_t guardByte = 0xa5u;
constexpr std::uint8_t counterByte = 0x5au;

struct AlignedRelease {
    void operator()(std::uint8_t* memory) const { ::operator delete(memory, std::align_val_t{blockBytes}); }
};

std::unique_ptr<std::uint8_t, AlignedRelease> storage;

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename TAction>
void expectFailure(TAction action) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        check(error.what()[0] != '\0', "empty exception message");
        return;
    }
    throw std::runtime_error("expected an exception");
}

std::uint8_t* memory() {
    return storage.get() + blockBytes;
}

std::vector<std::uint8_t> snapshot() {
    return {storage.get(), storage.get() + storageBytes};
}

void checkUnchanged(const std::vector<std::uint8_t>& before, const char* message) {
    check(std::equal(before.begin(), before.end(), storage.get()), message);
}

void fillStorage() {
    std::fill(storage.get(), storage.get() + storageBytes, guardByte);
    for (std::uint64_t label = 0; label < memoryBytes; label += labelBytes) {
        std::fill(memory() + label, memory() + label + counterBytes, counterByte);
        std::fill(memory() + label + counterBytes, memory() + label + labelBytes, std::uint8_t{0});
    }
}

void testUnregistered() {
    std::array<void*, 3> labels{storage.get(), storage.get(), storage.get()};
    expectFailure([&] { sceAgcGetSemaphoreLabel(0, &labels[1]); });
    expectFailure([] { sceAgcGetSemaphoreLabel(0, nullptr); });
    check(labels[0] == storage.get() && labels[1] == storage.get() && labels[2] == storage.get(), "a rejected semaphore label changed the output");
}

void testRegistrationRejections() {
    const auto before = snapshot();
    expectFailure([] { sceAgcSetAmmSemaphoreMemory(nullptr, memoryBytes); });
    expectFailure([] { sceAgcSetAmmSemaphoreMemory(nullptr, 0); });
    expectFailure([] { sceAgcSetAmmSemaphoreMemory(memory(), 0); });
    expectFailure([] { sceAgcSetAmmSemaphoreMemory(memory() + 1, memoryBytes); });
    expectFailure([] { sceAgcSetAmmSemaphoreMemory(memory() + labelBytes, blockBytes); });
    expectFailure([] { sceAgcSetAmmSemaphoreMemory(memory(), blockBytes - 1u); });
    expectFailure([] { sceAgcSetAmmSemaphoreMemory(memory(), blockBytes + labelBytes); });
    expectFailure([] {
        const auto top = std::numeric_limits<std::uintptr_t>::max() & ~static_cast<std::uintptr_t>(blockBytes - 1u);
        sceAgcSetAmmSemaphoreMemory(reinterpret_cast<void*>(top), memoryBytes);
    });
    checkUnchanged(before, "a rejected registration changed memory");
    testUnregistered();
}

void testUnsettledClearing() {
    for (const std::uint64_t offset : {5u * labelBytes + counterBytes, memoryBytes - 1u}) {
        memory()[offset] = 0x77u;
        const auto before = snapshot();
        expectFailure([] { sceAgcSetAmmSemaphoreMemory(memory(), memoryBytes); });
        checkUnchanged(before, "a registration with unsettled clearing changed memory");
        testUnregistered();
        memory()[offset] = 0;
    }
}

void testRegistration() {
    const auto before = snapshot();
    check(sceAgcSetAmmSemaphoreMemory(memory(), memoryBytes) == 0, "registering the AMM semaphore memory failed");
    check(std::all_of(memory(), memory() + memoryBytes, [](std::uint8_t byte) { return byte == 0; }), "registration did not clear the semaphore counters");
    check(std::equal(before.begin(), before.begin() + blockBytes, storage.get()), "registration wrote before the semaphore memory");
    check(std::equal(before.begin() + blockBytes + memoryBytes, before.end(), memory() + memoryBytes), "registration wrote past the semaphore memory");
}

void testLabels() {
    for (const std::uint32_t index : {0u, 1u, 7u, labelCount / 2u, labelCount - 1u}) {
        std::array<void*, 3> labels{storage.get(), nullptr, storage.get()};
        check(sceAgcGetSemaphoreLabel(index, &labels[1]) == 0, "a semaphore label lookup failed");
        check(labels[1] == memory() + index * labelBytes, "incorrect semaphore label address");
        check(labels[0] == storage.get() && labels[2] == storage.get(), "a semaphore label lookup wrote past its output");
    }
}

void testLabelRejections() {
    for (const std::uint32_t index : {labelCount, labelCount + 1u, std::numeric_limits<std::uint32_t>::max()}) {
        void* label = storage.get();
        expectFailure([&] { sceAgcGetSemaphoreLabel(index, &label); });
        check(label == storage.get(), "a rejected semaphore label changed the output");
    }
    expectFailure([] { sceAgcGetSemaphoreLabel(0, nullptr); });
}

void testRepeatedRegistration() {
    std::fill(memory(), memory() + memoryBytes, std::uint8_t{0x3c});
    const auto before = snapshot();
    expectFailure([] { sceAgcSetAmmSemaphoreMemory(memory(), memoryBytes); });
    expectFailure([] { sceAgcSetAmmSemaphoreMemory(storage.get(), blockBytes); });
    expectFailure([] { sceAgcSetAmmSemaphoreMemory(nullptr, 0); });
    checkUnchanged(before, "a repeated registration changed memory");
    void* label = nullptr;
    check(sceAgcGetSemaphoreLabel(labelCount - 1u, &label) == 0 && label == memory() + memoryBytes - labelBytes, "a repeated registration replaced the semaphore memory");
}

void testMemSemaphore() {
    void* label = nullptr;
    check(sceAgcGetSemaphoreLabel(0, &label) == 0 && label != nullptr, "failed to get label for memory semaphore test");
    const auto guestAddress = reinterpret_cast<std::uintptr_t>(label);

    for (std::uint8_t action = 0; action <= 1; ++action) {
        for (std::uint8_t clientCode = 0; clientCode <= 3; ++clientCode) {
            std::array<std::uint32_t, 16> words{};
            words.fill(0xdeadbeefu);
            CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};

            auto* packet = sceAgcCbMemSemaphore(&buffer, action, clientCode, label);
            check(packet == words.data(), "packet address mismatch");
            check(packet[0] == 0xc0023900u, "MEM_SEMAPHORE header opcode mismatch");
            check(packet[1] == static_cast<std::uint32_t>(guestAddress), "MEM_SEMAPHORE address low mismatch");
            check(packet[2] == static_cast<std::uint32_t>(guestAddress >> 32u), "MEM_SEMAPHORE address high mismatch");

            const std::uint32_t expectedOp = action != 0 ? 7u : 6u;
            const std::uint32_t expectedSignal = clientCode & 1u;
            const std::uint32_t expectedMailbox = (clientCode >> 1u) & 1u;
            const std::uint32_t expectedControl = (expectedOp << 29u) | (expectedSignal << 20u) | (expectedMailbox << 16u);
            check(packet[3] == expectedControl, "MEM_SEMAPHORE control word mismatch");
            check(buffer.cursor_up == words.data() + 4, "cursor did not advance by 4 words");
            check(words[4] == 0xdeadbeefu, "words past packet modified");
        }
    }

    std::array<std::uint32_t, 16> words{};
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
    expectFailure([&] { sceAgcCbMemSemaphore(nullptr, 0, 0, label); });
    expectFailure([&] { sceAgcCbMemSemaphore(&buffer, 0, 0, nullptr); });
    auto* unalignedLabel = reinterpret_cast<void*>(guestAddress + 1);
    expectFailure([&] { sceAgcCbMemSemaphore(&buffer, 0, 0, unalignedLabel); });
}

}

int main() {
    try {
        storage.reset(static_cast<std::uint8_t*>(::operator new(storageBytes, std::align_val_t{blockBytes})));
        fillStorage();
        testUnregistered();
        testRegistrationRejections();
        testUnsettledClearing();
        testRegistration();
        testLabels();
        testMemSemaphore();
        testLabelRejections();
        testRepeatedRegistration();
        storage.reset();
        LibcRunShutdown_nid_postfix();
        std::puts("AGC semaphore tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try { LibcRunShutdown_nid_postfix(); }
        catch (const std::exception& shutdown) { std::fprintf(stderr, "shutdown: %s\n", shutdown.what()); }
        return 1;
    }
}
