#include "prx/libSceAgc/DcbDraw/include/DrawIndexed.hpp"
#include "prx/libSceAgc/DcbDraw/include/DrawNonIndexed.hpp"
#include "prx/libSceAgc/DcbDraw/include/IndexBuffer.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Execution/include/QueueState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <limits>
#include <string>
#include <vector>

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template<typename TAction>
static void Reject(TAction action) {
    bool rejected = false;
    try {
        action();
    } catch (const std::exception&) {
        rejected = true;
    }
    Require(rejected);
}

static void TestInvalidPrimitiveTypes() {
    AgcDriver::QueueState queue{};
    queue.userConfig[0x24b] = 1;
    queue.context[0x103] = 0xffffffffu;

    const std::uint32_t invalidRestartTypes[] = {
        0u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u, 17u, 18u, 19u, 20u, 22u, 255u, 0xffffffffu
    };
    for (const auto primitiveType : invalidRestartTypes) {
        queue.userConfig[0x242] = primitiveType;
        const auto rejection = AgcDriver::Graphics::DrawRejection(queue, true);
        Require(!rejection.empty());
        Require(rejection.find("point, line and triangle") != std::string::npos);
    }

    queue.userConfig[0x242] = 4;
    queue.context[0x103] = 0;
    const auto badIndexRejection = AgcDriver::Graphics::DrawRejection(queue, true);
    Require(!badIndexRejection.empty());
    Require(badIndexRejection.find("all ones") != std::string::npos);

    const std::array<std::uint32_t, 3> badAutoDrawFlag0{0xc0012d00u, 3u, 0u};
    const std::array<std::uint32_t, 3> badAutoDrawFlag1{0xc0012d00u, 3u, 1u};
    const std::array<std::uint32_t, 3> badAutoDrawFlag4{0xc0012d00u, 3u, 4u};
    const std::array<std::uint32_t, 3> badAutoDrawFlagFf{0xc0012d00u, 3u, 0xffu};
    Reject([&] { AgcDriver::Pm4::Validate(badAutoDrawFlag0, 0); });
    Reject([&] { AgcDriver::Pm4::Validate(badAutoDrawFlag1, 0); });
    Reject([&] { AgcDriver::Pm4::Validate(badAutoDrawFlag4, 0); });
    Reject([&] { AgcDriver::Pm4::Validate(badAutoDrawFlagFf, 0); });
}

static void TestZeroCountVertexDraws() {
    AgcDriver::QueueState queue{};
    const std::array<std::uint32_t, 3> zeroAutoDraw{0xc0012d00u, 0u, 2u};
    AgcDriver::Pm4::Validate(zeroAutoDraw, 0);
    Reject([&] { AgcDriver::Pm4::ResolveDraw(zeroAutoDraw, queue); });

    queue.userConfig[0x24a] = 0;
    const auto zeroParams = AgcDriver::Pm4::ResolveDraw(zeroAutoDraw, queue);
    Require(zeroParams.indexCount == 0);
    Require(!zeroParams.indexed);
    Require(zeroParams.firstVertex == 0);

    queue.userConfig[0x24a] = std::numeric_limits<std::uint32_t>::max();
    const auto zeroWithMaxOffset = AgcDriver::Pm4::ResolveDraw(zeroAutoDraw, queue);
    Require(zeroWithMaxOffset.indexCount == 0);

    const std::array<std::uint32_t, 3> overflowAutoDraw{0xc0012d00u, 2u, 2u};
    Reject([&] { AgcDriver::Pm4::ResolveDraw(overflowAutoDraw, queue); });

    alignas(4) const std::array<std::uint32_t, 4> indices{0, 1, 2, 3};
    const auto baseAddr = reinterpret_cast<std::uintptr_t>(indices.data());

    const std::array<std::uint32_t, 6> zeroIndexedDraw{0xc0042700u, 0u, 0u, 0u, 0u, 0u};
    Reject([&] { AgcDriver::Pm4::ResolveDraw(zeroIndexedDraw, queue); });

    const std::array<std::uint32_t, 6> countExceedsMax{0xc0042700u, 2u, 0u, 0u, 3u, 0u};
    Reject([&] { AgcDriver::Pm4::Validate(countExceedsMax, 0); });

    const std::array<std::uint32_t, 6> badIndexedFlags{0xc0042700u, 3u, 0u, 0u, 3u, 0x40u};
    Reject([&] { AgcDriver::Pm4::Validate(badIndexedFlags, 0); });

    const std::array<std::uint32_t, 6> highAddressBits{0xc0042700u, 3u, 0u, 0x10000u, 3u, 0u};
    Reject([&] { AgcDriver::Pm4::Validate(highAddressBits, 0); });

    const std::array<std::uint32_t, 5> offsetCountExceedsMax{0xc0033500u, 2u, 0u, 3u, 0u};
    Reject([&] { AgcDriver::Pm4::Validate(offsetCountExceedsMax, 0); });

    const std::array<std::uint32_t, 5> badOffsetFlags{0xc0033500u, 3u, 0u, 3u, 0x10u};
    Reject([&] { AgcDriver::Pm4::Validate(badOffsetFlags, 0); });

    queue.indexBase = 0;
    const std::array<std::uint32_t, 5> zeroOffsetDraw{0xc0033500u, 0u, 0u, 0u, 0u};
    Reject([&] { AgcDriver::Pm4::ResolveDraw(zeroOffsetDraw, queue); });

    queue.indexBase = baseAddr;
    queue.indexType = 1;
    const auto validZeroOffset = AgcDriver::Pm4::ResolveDraw(zeroOffsetDraw, queue);
    Require(validZeroOffset.indexCount == 0);
    Require(validZeroOffset.indexed);
}

static void TestComputeDispatchBounds() {
    const std::array<std::uint32_t, 3> truncatedDirect{0xc0011500u, 1u, 1u};
    Reject([&] { AgcDriver::Pm4::Validate(truncatedDirect, 1); });

    const std::array<std::uint32_t, 6> oversizedDirect{0xc0041500u, 1u, 1u, 1u, 1u, 1u};
    Reject([&] { AgcDriver::Pm4::Validate(oversizedDirect, 1); });

    const std::array<std::uint32_t, 5> badDirectModifiersBits{0xc0031500u, 1u, 1u, 1u, 0x23u};
    Reject([&] { AgcDriver::Pm4::Validate(badDirectModifiersBits, 1); });

    const std::array<std::uint32_t, 5> badDirectModifiersNoBit0{0xc0031500u, 1u, 1u, 1u, 0u};
    Reject([&] { AgcDriver::Pm4::Validate(badDirectModifiersNoBit0, 1); });

    const std::array<std::uint32_t, 5> directHeaderBadFlags{0xc0031504u, 1u, 1u, 1u, 1u};
    Reject([&] { AgcDriver::Pm4::Validate(directHeaderBadFlags, 1); });

    const std::array<std::uint32_t, 2> truncatedIndirect{0xc0001600u, 1u};
    Reject([&] { AgcDriver::Pm4::Validate(truncatedIndirect, 1); });

    const std::array<std::uint32_t, 5> oversizedIndirect{0xc0031600u, 1u, 1u, 1u, 1u};
    Reject([&] { AgcDriver::Pm4::Validate(oversizedIndirect, 1); });

    const std::array<std::uint32_t, 3> badIndirectModifiers{0xc0011600u, 0u, 0u};
    Reject([&] { AgcDriver::Pm4::Validate(badIndirectModifiers, 1); });

    const std::array<std::uint32_t, 3> autoDrawInCompute{0xc0012d00u, 3u, 2u};
    Reject([&] { AgcDriver::Pm4::Validate(autoDrawInCompute, 1); });

    const std::array<std::uint32_t, 6> indexedDrawInCompute{0xc0042700u, 3u, 0u, 0u, 3u, 0u};
    Reject([&] { AgcDriver::Pm4::Validate(indexedDrawInCompute, 1); });

    const std::array<std::uint32_t, 5> offsetDrawInCompute{0xc0033500u, 3u, 0u, 3u, 0u};
    Reject([&] { AgcDriver::Pm4::Validate(offsetDrawInCompute, 1); });

    const std::array<std::uint32_t, 5> indirectDrawInCompute{0xc0032400u, 0u, 0x8cu, 0x8cu, 2u};
    Reject([&] { AgcDriver::Pm4::Validate(indirectDrawInCompute, 1); });
}

static void TestGuestCommandBufferBounds() {
    std::array<std::uint32_t, 32> words{};
    CommandBuffer exhausted{words.data(), words.data(), words.data(), words.data(), nullptr, nullptr, 0};

    Reject([&] { sceAgcDcbDrawIndexAuto(&exhausted, 0, 0); });
    Reject([&] { sceAgcDcbDrawIndexAuto(&exhausted, 3, 0); });
    Reject([&] { sceAgcDcbDrawIndex(&exhausted, 0, nullptr, 0); });
    Reject([&] { sceAgcDcbDrawIndex(&exhausted, 3, words.data(), 0); });
    Reject([&] { sceAgcDcbDrawIndexOffset(&exhausted, 0, 0, 0); });
    Reject([&] { sceAgcDcbDrawIndexOffset(&exhausted, 0, 3, 0); });

    Reject([&] { sceAgcDcbDrawIndexAuto(nullptr, 0, 0); });
    Reject([&] { sceAgcDcbDrawIndexAuto(nullptr, 3, 0); });
    Reject([&] { sceAgcDcbDrawIndex(nullptr, 0, nullptr, 0); });
    Reject([&] { sceAgcDcbDrawIndex(nullptr, 3, words.data(), 0); });
    Reject([&] { sceAgcDcbDrawIndexOffset(nullptr, 0, 0, 0); });
    Reject([&] { sceAgcDcbDrawIndexOffset(nullptr, 0, 3, 0); });

    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
    auto* saved = buffer.cursor_up;

    Reject([&] { sceAgcDcbDrawIndexAuto(&buffer, 3, 1ull << 40); });
    Require(buffer.cursor_up == saved);

    Reject([&] { sceAgcDcbDrawIndex(&buffer, 3, words.data(), 1ull << 40); });
    Require(buffer.cursor_up == saved);

    Reject([&] { sceAgcDcbDrawIndexOffset(&buffer, 0, 3, 1ull << 40); });
    Require(buffer.cursor_up == saved);

    auto* autoPacket = sceAgcDcbDrawIndexAuto(&buffer, 0, 0);
    Require(autoPacket == words.data());
    Require(words[0] == 0xc0012d00u);
    Require(words[1] == 0u);
    Require(words[2] == 2u);
    Require(buffer.cursor_up == saved + 3);
}

int main() {
    TestInvalidPrimitiveTypes();
    TestZeroCountVertexDraws();
    TestComputeDispatchBounds();
    TestGuestCommandBufferBounds();
    return 0;
}
