#include "prx/libSceIpmi/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace {

constexpr std::size_t ObjectSize = 0x180;

struct Store {
    std::size_t offset;
    std::size_t size;
    std::uint64_t value;
};

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libSceIpmi config default: %s\n", message);
    std::abort();
}

template <typename Construct>
void ExpectWithFill(Construct construct, std::initializer_list<Store> stores, unsigned char fill, const char* name) {
    alignas(8) unsigned char object[ObjectSize];
    alignas(8) unsigned char expected[ObjectSize];
    std::memset(object, fill, sizeof(object));
    std::memset(expected, fill, sizeof(expected));
    for (const Store& store : stores) {
        for (std::size_t index = 0; index < store.size; ++index) {
            expected[store.offset + index] = static_cast<unsigned char>(store.value >> (8 * (index % 8)));
        }
    }
    construct(object);
    for (std::size_t index = 0; index < ObjectSize; ++index) {
        if (object[index] != expected[index]) {
            std::fprintf(stderr, "%s: byte 0x%zx is 0x%02x, expected 0x%02x\n", name, index, object[index], expected[index]);
            std::abort();
        }
    }
}

template <typename Construct>
void Expect(Construct construct, std::initializer_list<Store> stores, const char* name) {
    for (const unsigned char fill : {static_cast<unsigned char>(0x00), static_cast<unsigned char>(0x5a), static_cast<unsigned char>(0xff)}) {
        ExpectWithFill(construct, stores, fill, name);
    }
}

}

int main() {
    for (auto construct : {_ZN4IPMI6Client6Config26SyncMethodInvocationConfigC1Ev, _ZN4IPMI6Client6Config26SyncMethodInvocationConfigC2Ev,
                           _ZN4IPMI6Client6Config27AsyncMethodInvocationConfigC1Ev, _ZN4IPMI6Client6Config27AsyncMethodInvocationConfigC2Ev}) {
        Expect(construct, {{0x00, 4, 1}, {0x08, 8, 0xf00}, {0x10, 8, 0xf00}}, "method invocation config");
    }
    for (auto construct : {_ZN4IPMI6Client6Config14MsgQueueConfigC1Ev, _ZN4IPMI6Client6Config14MsgQueueConfigC2Ev}) {
        Expect(construct, {{0x00, 4, 0}, {0x08, 0x100, 0}}, "message queue config");
    }
    for (auto construct : {_ZN4IPMI6Server6ConfigC1Ev, _ZN4IPMI6Server6ConfigC2Ev}) {
        Expect(construct, {{0x00, 8, 0xf00}, {0x08, 8, 0xf00}, {0x10, 8, 0}, {0x18, 2, 0}, {0x32, 2, 0}}, "server config");
    }
    for (auto construct : {_ZN4IPMI6Client6ConfigC1Ev, _ZN4IPMI6Client6ConfigC2Ev}) {
        Expect(construct,
               {{0x00, 1, 0}, {0x19, 1, 0}, {0x1c, 8, 0x100000000}, {0x28, 8, 0xf00}, {0x30, 8, 0xf00}, {0x38, 4, 1}, {0x40, 8, 0xf00},
                {0x48, 8, 0xf00}, {0x50, 4, 0}, {0x58, 0x100, 0}},
               "client config");
    }

    Require(_ZN4IPMI6Client6Config14MsgQueueConfig20estimateMsgQueueSizeEmj(0, 0) == 0, "no messages need no queue");
    Require(_ZN4IPMI6Client6Config14MsgQueueConfig20estimateMsgQueueSizeEmj(0xf00, 0) == 0, "zero messages ignore message size");
    Require(_ZN4IPMI6Client6Config14MsgQueueConfig20estimateMsgQueueSizeEmj(0, 1) == 0x10, "a message carries a 0x10-byte header");
    Require(_ZN4IPMI6Client6Config14MsgQueueConfig20estimateMsgQueueSizeEmj(0xf00, 4) == 0x3c40, "four 0xf00-byte messages");
    Require(_ZN4IPMI6Client6Config14MsgQueueConfig20estimateMsgQueueSizeEmj(0x100000000, 2) == 0x200000020, "the size is not truncated to 32 bits");
    Require(_ZN4IPMI6Client6Config14MsgQueueConfig20estimateMsgQueueSizeEmj(0xfffffffffffffff8, 1) == 8, "the header addition wraps");
    Require(_ZN4IPMI6Client6Config14MsgQueueConfig20estimateMsgQueueSizeEmj(1, 0xffffffff) == 0x10ffffffef, "the count is unsigned");

    std::puts("libSceIpmi config default checks passed");
    return 0;
}
