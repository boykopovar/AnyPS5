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
extern "C" std::uint32_t* APS5_VABI sceAgcCbMemSemaphore(CommandBuffer* buf, const volatile void* address, std::uint32_t operation, std::uint32_t modifier, std::uint32_t value);
extern "C" std::uint32_t* APS5_VABI sceAgcAcbMemSemaphore(CommandBuffer* buf, const volatile void* address, std::uint8_t waitForMailbox, std::uint8_t signalType, std::uint8_t operation);
extern "C" std::uint32_t* APS5_VABI sceAgcDcbMemSemaphore(CommandBuffer* buf, const volatile void* address, std::uint8_t waitForMailbox, std::uint8_t signalType, std::uint8_t operation);

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
    check(sceAgcGetSemaphoreLabel(0, &labels[1]) == static_cast<int>(0x8a6c0049u), "unregistered label error mismatch");
    check(sceAgcGetSemaphoreLabel(0, nullptr) == static_cast<int>(0x8a6c0049u), "unregistered null label error mismatch");
    check(labels[0] == storage.get() && labels[1] == storage.get() && labels[2] == storage.get(), "a rejected semaphore label changed the output");
}

void testRegistrationRejections() {
    const auto before = snapshot();
    expectFailure([] { sceAgcSetAmmSemaphoreMemory(nullptr, memoryBytes); });
    expectFailure([] { sceAgcSetAmmSemaphoreMemory(nullptr, 0); });
    expectFailure([] { sceAgcSetAmmSemaphoreMemory(memory(), 0); });
    check(sceAgcSetAmmSemaphoreMemory(memory() + 1, memoryBytes) == static_cast<int>(0x8a6c0002u), "unaligned semaphore memory error mismatch");
    check(sceAgcSetAmmSemaphoreMemory(memory() + labelBytes, blockBytes) == static_cast<int>(0x8a6c0002u), "unaligned semaphore memory error mismatch");
    check(sceAgcSetAmmSemaphoreMemory(memory(), blockBytes - 1u) == static_cast<int>(0x8a6c0002u), "unaligned semaphore memory error mismatch");
    check(sceAgcSetAmmSemaphoreMemory(memory(), blockBytes + labelBytes) == static_cast<int>(0x8a6c0002u), "unaligned semaphore memory error mismatch");
    expectFailure([] {
        const auto top = std::numeric_limits<std::uintptr_t>::max() & ~static_cast<std::uintptr_t>(blockBytes - 1u);
        sceAgcSetAmmSemaphoreMemory(reinterpret_cast<void*>(top), memoryBytes);
    });
    checkUnchanged(before, "a rejected registration changed memory");
    testUnregistered();
}

void testRegistration() {
    std::fill(memory(), memory() + memoryBytes, std::uint8_t{0x77});
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
        check(sceAgcGetSemaphoreLabel(index, &label) == static_cast<int>(0x8a6c000bu), "out-of-range label error mismatch");
        check(label == storage.get(), "a rejected semaphore label changed the output");
    }
    expectFailure([] { sceAgcGetSemaphoreLabel(0, nullptr); });
}

void testRepeatedRegistration() {
    std::fill(memory(), memory() + memoryBytes, std::uint8_t{0x3c});
    const auto before = snapshot();
    check(sceAgcSetAmmSemaphoreMemory(memory(), memoryBytes) == static_cast<int>(0x8a6c0048u), "repeated registration error mismatch");
    check(sceAgcSetAmmSemaphoreMemory(storage.get(), blockBytes) == static_cast<int>(0x8a6c0048u), "repeated registration error mismatch");
    check(sceAgcSetAmmSemaphoreMemory(nullptr, 0) == static_cast<int>(0x8a6c0048u), "repeated registration error mismatch");
    checkUnchanged(before, "a repeated registration changed memory");
    void* label = nullptr;
    check(sceAgcGetSemaphoreLabel(labelCount - 1u, &label) == 0 && label == memory() + memoryBytes - labelBytes, "a repeated registration replaced the semaphore memory");
}

void testMemSemaphore() {
    void* label = nullptr;
    check(sceAgcGetSemaphoreLabel(0, &label) == 0 && label != nullptr, "failed to get label for memory semaphore test");
    const auto guestAddress = reinterpret_cast<std::uintptr_t>(label);

    {
        std::array<std::uint32_t, 16> words{};
        words.fill(0xdeadbeefu);
        CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
        const auto* packet = sceAgcCbMemSemaphore(&buffer, reinterpret_cast<const volatile void*>(0x1111111122222222ull), 0x33333333u, 0x44444444u, 0);
        check(packet == words.data(), "packet address mismatch");
        check(packet[0] == 0xc0023900u, "MEM_SEMAPHORE header opcode mismatch");
        check(packet[1] == 0x22222220u, "MEM_SEMAPHORE address low mismatch");
        check(packet[2] == 0x11111111u, "MEM_SEMAPHORE address high mismatch");
        check(packet[3] == 0x80100000u, "MEM_SEMAPHORE control word mismatch");
        check(buffer.cursor_up == words.data() + 4, "cursor did not advance by 4 words");
        check(words[4] == 0xdeadbeefu, "words past packet modified");
    }

    {
        std::array<std::uint32_t, 16> words{};
        words.fill(0xdeadbeefu);
        CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
        const auto* packet = sceAgcCbMemSemaphore(&buffer, reinterpret_cast<const volatile void*>(1ull), 2, 3, 4);
        check(packet == words.data(), "packet address mismatch");
        check(packet[0] == 0xc0023900u, "MEM_SEMAPHORE header opcode mismatch");
        check(packet[1] == 0u, "MEM_SEMAPHORE address low mismatch");
        check(packet[2] == 0u, "MEM_SEMAPHORE address high mismatch");
        check(packet[3] == 0x80100000u, "MEM_SEMAPHORE control word mismatch");
        check(buffer.cursor_up == words.data() + 4, "cursor did not advance by 4 words");
        check(words[4] == 0xdeadbeefu, "words past packet modified");
    }

    {
        std::array<std::uint32_t, 16> words{};
        words.fill(0xdeadbeefu);
        CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
        const auto* packet = sceAgcCbMemSemaphore(&buffer, label, 0x12345678u, 0x9abcdef0u, 0x55aa55aau);
        check(packet == words.data(), "packet address mismatch");
        check(packet[0] == 0xc0023900u, "MEM_SEMAPHORE header opcode mismatch");
        check(packet[1] == static_cast<std::uint32_t>(guestAddress & ~static_cast<std::uintptr_t>(0xfu)), "MEM_SEMAPHORE address low mismatch");
        check(packet[2] == static_cast<std::uint32_t>(guestAddress >> 32u), "MEM_SEMAPHORE address high mismatch");
        check(packet[3] == 0x80100000u, "MEM_SEMAPHORE control word mismatch");
        check(buffer.cursor_up == words.data() + 4, "cursor did not advance by 4 words");
        check(words[4] == 0xdeadbeefu, "words past packet modified");
    }

    expectFailure([&] { sceAgcCbMemSemaphore(nullptr, label, 0, 0, 0); });
}

using QueueMemSemaphore = std::uint32_t* APS5_VABI (*)(CommandBuffer*, const volatile void*, std::uint8_t, std::uint8_t, std::uint8_t);

void testQueueMemSemaphore(QueueMemSemaphore write) {
    const auto* semaphore = reinterpret_cast<const volatile void*>(0x102f221d98ull);

    {
        std::array<std::uint32_t, 16> words{};
        words.fill(0xdeadbeefu);
        CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
        const auto* packet = write(&buffer, semaphore, 0, 1, 6);
        check(packet == words.data(), "packet address mismatch");
        check(packet[0] == 0xc0023900u, "MEM_SEMAPHORE header opcode mismatch");
        check(packet[1] == 0x2f221d98u, "MEM_SEMAPHORE address low mismatch");
        check(packet[2] == 0x10u, "MEM_SEMAPHORE address high mismatch");
        check(packet[3] == 0xc0100000u, "MEM_SEMAPHORE signal control word mismatch");
        check(buffer.cursor_up == words.data() + 4, "cursor did not advance by 4 words");
        check(words[4] == 0xdeadbeefu, "words past packet modified");
    }

    {
        std::array<std::uint32_t, 16> words{};
        words.fill(0xdeadbeefu);
        CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
        const auto* packet = write(&buffer, semaphore, 1, 0, 7);
        check(packet[3] == 0xe0010000u, "MEM_SEMAPHORE wait control word mismatch");
    }

    std::array<std::uint32_t, 16> words{};
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
    expectFailure([&] { write(&buffer, reinterpret_cast<const volatile void*>(1ull), 0, 1, 6); });
    expectFailure([&] { write(&buffer, semaphore, 0, 1, 5); });
    expectFailure([&] { write(&buffer, semaphore, 2, 0, 7); });
}

}

int main() {
    try {
        storage.reset(static_cast<std::uint8_t*>(::operator new(storageBytes, std::align_val_t{blockBytes})));
        fillStorage();
        testUnregistered();
        testRegistrationRejections();
        testRegistration();
        testLabels();
        testMemSemaphore();
        testQueueMemSemaphore(sceAgcAcbMemSemaphore);
        testQueueMemSemaphore(sceAgcDcbMemSemaphore);
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
