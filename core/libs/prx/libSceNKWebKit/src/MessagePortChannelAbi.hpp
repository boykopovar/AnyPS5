#pragma once

#include <cstddef>
#include <cstdint>

namespace NKWebKit::WebCoreAbi {

struct MessagePortIdentifier {
    std::uint64_t processIdentifier;
    std::uint64_t objectIdentifier;
};

struct OptionalProcessIdentifier {
    std::uint64_t value;
    std::uint8_t engaged;
};

struct MessageVector {
    void* buffer;
    std::uint32_t capacity;
    std::uint32_t size;
};

struct MessagePortChannel {
    std::uint32_t refCount;
    MessagePortIdentifier ports[2];
    std::uint8_t isClosed[2];
    OptionalProcessIdentifier processes[2];
    void* entangledToProcessProtectors[2];
    MessageVector pendingMessages[2];
    void* pendingMessagePortTransfers[2];
    void* pendingMessageProtectors[2];
    std::uint64_t messageBatchesInFlight;
    void* registry;
};

static_assert(sizeof(MessagePortIdentifier) == 0x10);
static_assert(sizeof(OptionalProcessIdentifier) == 0x10);
static_assert(sizeof(MessageVector) == 0x10);
static_assert(offsetof(MessagePortChannel, ports) == 0x8);
static_assert(offsetof(MessagePortChannel, isClosed) == 0x28);
static_assert(offsetof(MessagePortChannel, processes) == 0x30);
static_assert(offsetof(MessagePortChannel, entangledToProcessProtectors) == 0x50);
static_assert(offsetof(MessagePortChannel, pendingMessages) == 0x60);
static_assert(offsetof(MessageVector, size) == 0xc);
static_assert(offsetof(MessagePortChannel, pendingMessagePortTransfers) == 0x80);
static_assert(offsetof(MessagePortChannel, pendingMessageProtectors) == 0x90);
static_assert(offsetof(MessagePortChannel, messageBatchesInFlight) == 0xa0);
static_assert(sizeof(MessagePortChannel) == 0xb0);

}
