#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Packets/StatePacketCount.hpp"
#include "prx/libSceAgcDriver/Execution/include/QueueState.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <initializer_list>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

namespace AgcDriver::DriverDetail {

struct DriverTestAccess {
    static QueueState Queue(std::uint32_t queue) {
        auto& driver = Driver::Get();
        std::lock_guard lock(driver.mutex);
        return driver.queues.at(queue);
    }
};

}

namespace {

using AgcDriver::DriverDetail::DriverTestAccess;
using Words = std::vector<std::uint32_t>;

void check(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error("state packet fast path: " + what);
}

bool killSwitch() {
    for (const char* name : {"APS5_NO_STATE_FAST_PATH", "APS5_DRAIN_ALL", "APS5_PACKET_EPOCH", "APS5_TRACE_GPU", "APS5_DUMP_QUEUE"}) {
        if (std::getenv(name) != nullptr) return true;
    }
    return false;
}

std::uint64_t fastPackets() {
    return AgcDriver::DriverDetail::StatePackets();
}

class Stream {
public:
    void Raw(std::uint32_t opcode, std::initializer_list<std::uint32_t> payload, bool predicated, bool onFastPath) {
        words.push_back(0xc0000000u | ((static_cast<std::uint32_t>(payload.size()) - 1u) << 16u) | (opcode << 8u) | (predicated ? 1u : 0u));
        words.insert(words.end(), payload);
        if (onFastPath) ++expected;
    }

    void State(std::uint32_t opcode, std::uint32_t offset, std::initializer_list<std::uint32_t> values) {
        Words payload{offset};
        payload.insert(payload.end(), values);
        words.push_back(0xc0000000u | (static_cast<std::uint32_t>(values.size()) << 16u) | (opcode << 8u));
        words.insert(words.end(), payload.begin(), payload.end());
        ++expected;
    }

    void Full(std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
        Raw(opcode, payload, false, false);
    }

    void Run(std::uint32_t queue) {
        const auto before = fastPackets();
        ::Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
        check((queue == 0 ? sceAgcDriverSubmitDcb(&packet) : sceAgcDriverSubmitAcb(queue, &packet)) == 0, "submit failed");
        AgcDriverWaitIdle_nid_postfix();
        const auto used = fastPackets() - before;
        const auto want = killSwitch() ? 0u : expected;
        check(used == want, "the fast path took " + std::to_string(used) + " packets, expected " + std::to_string(want));
    }

private:
    Words words;
    std::uint64_t expected = 0;
};

void testStatePackets() {
    Stream stream;
    stream.State(0x69, 0x10, {0x11110010u});
    stream.State(0x69, 0x11, {0x22220011u, 0x33330012u, 0x44440013u});
    stream.State(0x69, 0x8e, {0xffffffffu});
    stream.State(0x69, 0x1c4, {0x5555ccc4u});
    stream.State(0x76, 0x20c, {0x66660c20u});
    stream.State(0x76, 0x210, {0x1u, 0x2u, 0x3u});
    stream.State(0x79, 0x242, {0x77777242u});
    stream.State(0x79, 0x245, {0x88888245u, 0x99999246u});
    stream.Raw(0x13, {12345u}, false, true);
    stream.Raw(0x26, {0x2000u, 0x1u}, false, true);
    stream.Raw(0x2f, {7u}, false, true);
    stream.Raw(0x2a, {3u}, false, true);
    stream.State(0x79, 0x243, {6u});
    stream.Run(0);
    const auto queue = DriverTestAccess::Queue(0);
    check(queue.context.at(0x10) == 0x11110010u && queue.context.at(0x11) == 0x22220011u && queue.context.at(0x12) == 0x33330012u && queue.context.at(0x13) == 0x44440013u, "context registers differ");
    check(queue.context.at(0x8e) == 0xffffffffu && queue.context.at(0x1c4) == 0x5555ccc4u, "context registers differ");
    check(queue.shader.at(0x20c) == 0x66660c20u && queue.shader.at(0x210) == 1u && queue.shader.at(0x211) == 2u && queue.shader.at(0x212) == 3u, "shader registers differ");
    check(queue.userConfig.at(0x242) == 0x77777242u && queue.userConfig.at(0x245) == 0x88888245u && queue.userConfig.at(0x246) == 0x99999246u, "user config registers differ");
    check(queue.indexBufferSize.has_value() && *queue.indexBufferSize == 12345u, "index buffer size differs");
    check(queue.indexBase == ((std::uint64_t{1} << 32u) | 0x2000u), "index base differs");
    check(queue.instanceCount == 7u, "instance count differs");
    check(queue.indexType == 2u, "the index type written by register 0x243 after INDEX_TYPE differs");
}

void testSkippedOpcodes() {
    alignas(64) static std::array<std::uint32_t, 4> context{0xa0a0a0a0u, 0xb0b0b0b0u, 0, 0};
    alignas(64) static std::array<std::uint32_t, 4> shader{0xc0c0c0c0u, 0xd0d0d0d0u, 0, 0};
    alignas(64) static std::array<std::uint32_t, 4> userConfig{0xe0e0e0e0u, 0xf0f0f0f0u, 0, 0};
    const auto low = [](const void* address) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(address)); };
    const auto high = [](const void* address) { return static_cast<std::uint32_t>(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(address)) >> 32u); };
    Stream stream;
    stream.State(0x69, 0x10, {0x11110010u});
    stream.Full(0x9f, {low(context.data()), high(context.data()), 0x20u, 2u});
    stream.Full(0x63, {low(shader.data()), high(shader.data()), 0x30u, 2u});
    stream.Full(0x64, {low(userConfig.data()), high(userConfig.data()), 0x250u, 2u});
    stream.Full(0x11, {1u, 0x1000u, 0u});
    stream.Full(0x7a, {0x10000242u, 0x77u});
    stream.Full(0x81, {8u, 0x1234u});
    stream.Run(0);
    const auto queue = DriverTestAccess::Queue(0);
    check(queue.context.at(0x10) == 0x11110010u, "the state packet before the others was lost");
    check(queue.context.at(0x20) == 0xa0a0a0a0u && queue.context.at(0x21) == 0xb0b0b0b0u, "SET_CONTEXT_REG_INDIRECT did not write its list");
    check(queue.shader.at(0x30) == 0xc0c0c0c0u && queue.shader.at(0x31) == 0xd0d0d0d0u, "SET_SH_REG_INDIRECT did not write its list");
    check(queue.userConfig.at(0x250) == 0xe0e0e0e0u && queue.userConfig.at(0x251) == 0xf0f0f0f0u, "SET_UCONFIG_REG_INDIRECT did not write its list");
    check(queue.drawIndirectBase == 0x1000u, "the indirect draw base differs");
    check(queue.userConfig.at(0x242) == 0x77u, "register 0x242 through opcode 0x7a differs");
    check(queue.ConstantRam()[2] == 0x1234u, "constant RAM differs");

    Stream clear;
    clear.State(0x69, 0x10, {0x12345678u});
    clear.Full(0x12, {0u});
    clear.Run(0);
    check(DriverTestAccess::Queue(0).context == AgcDriver::ClearStateContextRegisters(), "CLEAR_STATE did not reset the context after a state packet");
}

void testPredicated() {
    alignas(64) static volatile std::uint32_t predicate = 0;
    const auto address = reinterpret_cast<std::uintptr_t>(&predicate);
    const auto low = static_cast<std::uint32_t>(address);
    const auto high = static_cast<std::uint32_t>(static_cast<std::uint64_t>(address) >> 32u);
    {
        Stream stream;
        stream.Full(0x20, {(4u << 16u) | 0x100u, low, high});
        stream.Raw(0x69, {0x30u, 0xdead0030u}, true, false);
        stream.Raw(0x76, {0x230u, 0xdead0230u}, true, false);
        stream.Raw(0x13, {999u}, true, false);
        stream.State(0x69, 0x31, {0x31313131u});
        stream.Run(0);
        const auto queue = DriverTestAccess::Queue(0);
        check(!queue.context.contains(0x30) && !queue.shader.contains(0x230), "a predicated register write ran with a false predicate");
        check(queue.indexBufferSize != 999u, "a predicated INDEX_BUFFER_SIZE ran with a false predicate");
        check(queue.context.at(0x31) == 0x31313131u, "the unpredicated write after the predicated ones was lost");
    }
    predicate = 1;
    {
        Stream stream;
        stream.Raw(0x69, {0x30u, 0xbeef0030u}, true, false);
        stream.Raw(0x13, {999u}, true, false);
        stream.Run(0);
        const auto queue = DriverTestAccess::Queue(0);
        check(queue.context.at(0x30) == 0xbeef0030u && queue.indexBufferSize.has_value() && *queue.indexBufferSize == 999u, "a predicated packet did not run with a true predicate");
    }
    {
        Stream stream;
        stream.Full(0x20, {0u, 0u, 0u});
        stream.Raw(0x69, {0x32u, 0xcafe0032u}, true, true);
        stream.Run(0);
        check(DriverTestAccess::Queue(0).context.at(0x32) == 0xcafe0032u, "a packet flagged predicated did not run with predication off");
    }
}

void testRing() {
    Stream stream;
    stream.State(0x76, 0x20c, {0x1c1c1c1cu});
    stream.State(0x76, 0x210, {0x1u, 0x2u});
    stream.Run(0x20);
    const auto queue = DriverTestAccess::Queue(0x20);
    check(queue.shader.at(0x20c) == 0x1c1c1c1cu && queue.shader.at(0x210) == 1u && queue.shader.at(0x211) == 2u, "ring shader registers differ");
}

}

int main() {
    try {
        testStatePackets();
        testSkippedOpcodes();
        testPredicated();
        testRing();
        LibcRunShutdown_nid_postfix();
        std::printf("state packet fast path tests passed (%s)\n", killSwitch() ? "kill switch on" : "fast path on");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
