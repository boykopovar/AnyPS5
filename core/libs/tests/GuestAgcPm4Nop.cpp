#include "prx/libSceAgc/Cb/include/Sync.hpp"
#include "prx/libSceAgc/Patch/include/Nop.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libSceAgcDriver/include/AgcDriver.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <vector>

static void Require(bool condition) {
    if (!condition) std::abort();
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

int main() {
    std::array<std::uint32_t, 32> words{};
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};

    Require(sceAgcCbNopGetSize(0) == 0);
    Require(sceAgcCbNopGetSize(1) == 4);
    Require(sceAgcCbNopGetSize(2) == 8);
    Require(sceAgcCbNopGetSize(3) == 12);
    Require(sceAgcCbNopGetSize(5) == 20);
    Require(sceAgcCbNopGetSize(0x4001u) == 0x10004u);

    auto* single = sceAgcCbNop_nid_postfix(&buffer, 1);
    Require(single == words.data());
    Require(single[0] == 0xffff1000u);
    Require(AgcDriver::Pm4::FillerPacket(single[0]));
    Require(AgcDriver::Pm4::PacketWords(single[0]) == 1);

    AgcDriver::QueueState queue;
    AgcDriver::Pm4::Validate({single, 1}, 0);
    AgcDriver::Pm4::Validate({single, 1}, 0x20);
    AgcDriver::Pm4::Execute({single, 1}, queue);

    auto* pair = sceAgcCbNop_nid_postfix(&buffer, 2);
    Require(pair == words.data() + 1);
    Require(pair[0] == 0xc0001000u);
    Require(pair[1] == 0);
    Require(!AgcDriver::Pm4::FillerPacket(pair[0]));
    Require(AgcDriver::Pm4::PacketWords(pair[0]) == 2);

    AgcDriver::Pm4::Validate({pair, 2}, 0);
    AgcDriver::Pm4::Validate({pair, 2}, 0x20);
    AgcDriver::Pm4::Execute({pair, 2}, queue);

    auto* triple = sceAgcSetNop(&buffer, 3);
    Require(triple == words.data() + 3);
    Require(triple[0] == 0xc0011000u);
    Require(triple[1] == 0 && triple[2] == 0);
    Require(AgcDriver::Pm4::PacketWords(triple[0]) == 3);

    AgcDriver::Pm4::Validate({triple, 3}, 0);
    AgcDriver::Pm4::Validate({triple, 3}, 0x20);
    AgcDriver::Pm4::Execute({triple, 3}, queue);

    auto* quad = sceAgcSetNop(&buffer, 4);
    Require(quad == words.data() + 6);
    Require(quad[0] == 0xc0021000u);
    Require(AgcDriver::Pm4::PacketWords(quad[0]) == 4);

    AgcDriver::Pm4::Validate({quad, 4}, 0);
    AgcDriver::Pm4::Execute({quad, 4}, queue);

    Require(AgcDriver::Pm4::PacketWords(0x80000000u) == 1);
    Require(AgcDriver::Pm4::FillerPacket(0x80000000u));
    Require(AgcDriver::Pm4::PacketWords(0xc0001000u) == 2);
    Require(AgcDriver::Pm4::PacketWords(0xc0011000u) == 3);
    Require(AgcDriver::Pm4::PacketWords(0xc0021000u) == 4);
    Require(AgcDriver::Pm4::PacketWords(0xc0031000u) == 5);
    Require(AgcDriver::Pm4::PacketWords(0xc3ff1000u) == 0x401u);
    Require(AgcDriver::Pm4::PacketWords(0xdfff1000u) == 0x4001u);
    Require(AgcDriver::Pm4::PacketWords(0xffff1001u) == 0x4001u);
    Require(AgcDriver::Pm4::PacketWords(0xffff1002u) == 0x4001u);

    Reject([&] { AgcDriver::Pm4::Validate({}, 0); });

    const std::array<std::uint32_t, 1> singleWordType3{0xc0001000u};
    Reject([&] { AgcDriver::Pm4::Validate(singleWordType3, 0); });

    const std::array<std::uint32_t, 2> fillerTooLong{0xffff1000u, 0u};
    Reject([&] { AgcDriver::Pm4::Validate(fillerTooLong, 0); });

    const std::array<std::uint32_t, 3> fillerThreeWords{0xffff1000u, 0u, 0u};
    Reject([&] { AgcDriver::Pm4::Validate(fillerThreeWords, 0); });

    const std::array<std::uint32_t, 2> type2FillerTooLong{0x80000000u, 0u};
    Reject([&] { AgcDriver::Pm4::Validate(type2FillerTooLong, 0); });

    const std::array<std::uint32_t, 3> count0TooLong{0xc0001000u, 0u, 0u};
    Reject([&] { AgcDriver::Pm4::Validate(count0TooLong, 0); });

    const std::array<std::uint32_t, 4> count0FourWords{0xc0001000u, 0u, 0u, 0u};
    Reject([&] { AgcDriver::Pm4::Validate(count0FourWords, 0); });

    const std::array<std::uint32_t, 2> count1TooShort{0xc0011000u, 0u};
    Reject([&] { AgcDriver::Pm4::Validate(count1TooShort, 0); });

    const std::array<std::uint32_t, 4> count1TooLong{0xc0011000u, 0u, 0u, 0u};
    Reject([&] { AgcDriver::Pm4::Validate(count1TooLong, 0); });

    const std::array<std::uint32_t, 3> count2TooShort{0xc0021000u, 0u, 0u};
    Reject([&] { AgcDriver::Pm4::Validate(count2TooShort, 0); });

    const std::array<std::uint32_t, 5> count2TooLong{0xc0021000u, 0u, 0u, 0u, 0u};
    Reject([&] { AgcDriver::Pm4::Validate(count2TooLong, 0); });

    const std::array<std::uint32_t, 2> maxCountTooShort{0xdfff1000u, 0u};
    Reject([&] { AgcDriver::Pm4::Validate(maxCountTooShort, 0); });

    const std::array<std::uint32_t, 2> type0Nop{0x00001000u, 0u};
    Reject([&] { AgcDriver::Pm4::Validate(type0Nop, 0); });

    const std::array<std::uint32_t, 2> type1Nop{0x40001000u, 0u};
    Reject([&] { AgcDriver::Pm4::Validate(type1Nop, 0); });

    const std::array<std::uint32_t, 2> nopBit0Flag{0xc0001001u, 0u};
    Reject([&] { AgcDriver::Pm4::Validate(nopBit0Flag, 0); });

    const std::array<std::uint32_t, 2> nopBit1Flag{0xc0001002u, 0u};
    Reject([&] { AgcDriver::Pm4::Validate(nopBit1Flag, 0); });

    const std::array<std::uint32_t, 2> nopBothFlags{0xc0001003u, 0u};
    Reject([&] { AgcDriver::Pm4::Validate(nopBothFlags, 0); });

    const std::array<std::uint32_t, 2> validDispatchReset{0xc0001024u, 0u};
    AgcDriver::Pm4::Validate(validDispatchReset, 0x20);
    AgcDriver::Pm4::Execute(validDispatchReset, queue);

    const std::array<std::uint32_t, 3> dispatchResetCount1{0xc0011024u, 0u, 0u};
    Reject([&] { AgcDriver::Pm4::Validate(dispatchResetCount1, 0x20); });

    const std::array<std::uint32_t, 4> dispatchResetCount2{0xc0021024u, 0u, 0u, 0u};
    Reject([&] { AgcDriver::Pm4::Validate(dispatchResetCount2, 0x20); });

    const std::array<std::uint32_t, 4> validWaitFlipDone{0xc0021018u, 0u, 0u, 0u};
    AgcDriver::Pm4::Validate(validWaitFlipDone, 0);

    const std::array<std::uint32_t, 2> waitFlipDoneCount0{0xc0001018u, 0u};
    Reject([&] { AgcDriver::Pm4::Validate(waitFlipDoneCount0, 0); });

    const std::array<std::uint32_t, 3> waitFlipDoneCount1{0xc0011018u, 0u, 0u};
    Reject([&] { AgcDriver::Pm4::Validate(waitFlipDoneCount1, 0); });

    const std::array<std::uint32_t, 5> waitFlipDoneCount3{0xc0031018u, 0u, 0u, 0u, 0u};
    Reject([&] { AgcDriver::Pm4::Validate(waitFlipDoneCount3, 0); });

    const std::array<std::uint32_t, 6> validFlip{0xc004105cu, 0u, 0u, 0u, 0u, 0u};
    AgcDriver::Pm4::Validate(validFlip, 0);

    const std::array<std::uint32_t, 2> flipCount0{0xc000105cu, 0u};
    Reject([&] { AgcDriver::Pm4::Validate(flipCount0, 0); });

    const std::array<std::uint32_t, 4> flipCount2{0xc002105cu, 0u, 0u, 0u};
    Reject([&] { AgcDriver::Pm4::Validate(flipCount2, 0); });

    const std::array<std::uint32_t, 7> flipCount5{0xc005105cu, 0u, 0u, 0u, 0u, 0u, 0u};
    Reject([&] { AgcDriver::Pm4::Validate(flipCount5, 0); });

    const std::array<std::uint32_t, 3> validContextState3{0xc0011068u, 0u, 0u};
    AgcDriver::Pm4::Validate(validContextState3, 0);

    const std::array<std::uint32_t, 5> validContextState5{0xc0031068u, 0u, 0u, 0u, 0u};
    AgcDriver::Pm4::Validate(validContextState5, 0);

    const std::array<std::uint32_t, 2> contextStateCount0{0xc0001068u, 0u};
    Reject([&] { AgcDriver::Pm4::Validate(contextStateCount0, 0); });

    const std::array<std::uint32_t, 4> contextStateCount2{0xc0021068u, 0u, 0u, 0u};
    Reject([&] { AgcDriver::Pm4::Validate(contextStateCount2, 0); });

    const std::array<std::uint32_t, 6> contextStateCount4{0xc0041068u, 0u, 0u, 0u, 0u, 0u};
    Reject([&] { AgcDriver::Pm4::Validate(contextStateCount4, 0); });

    auto* savedCursor = buffer.cursor_up;
    Reject([&] { sceAgcCbNop_nid_postfix(&buffer, 0); });
    Require(buffer.cursor_up == savedCursor);
    Reject([&] { sceAgcSetNop(&buffer, 0); });
    Require(buffer.cursor_up == savedCursor);
    Reject([&] { sceAgcCbNop_nid_postfix(&buffer, 0x4002u); });
    Require(buffer.cursor_up == savedCursor);
    Reject([&] { sceAgcSetNop(&buffer, 0x4002u); });
    Require(buffer.cursor_up == savedCursor);
    Reject([&] { sceAgcCbNop_nid_postfix(&buffer, 0xffffffffu); });
    Require(buffer.cursor_up == savedCursor);
    Reject([&] { sceAgcSetNop(&buffer, 0xffffffffu); });
    Require(buffer.cursor_up == savedCursor);
    Reject([&] { sceAgcCbNop_nid_postfix(nullptr, 1); });
    Reject([&] { sceAgcSetNop(nullptr, 1); });

    buffer.cursor_down = buffer.cursor_up + 2;
    Reject([&] { sceAgcCbNop_nid_postfix(&buffer, 3); });
    Require(buffer.cursor_up == savedCursor);
    Reject([&] { sceAgcSetNop(&buffer, 3); });
    Require(buffer.cursor_up == savedCursor);
    buffer.cursor_down = words.data() + words.size();

    Require(sceAgcDriverNopDataGetPacketSize(0) == 2);
    Require(sceAgcDriverNopDataGetPacketSize(1) == 3);
    Require(sceAgcDriverNopDataGetPacketSize(4) == 3);
    Require(sceAgcDriverNopDataGetPacketSize(5) == 4);
    Require(sceAgcDriverNopDataGetPacketSize(0xffffffffu) == 0x40000002u);

    std::array<std::uint32_t, 8> dataStorage{};
    const std::uint32_t payload = 0x12345678u;
    Require(sceAgcDriverNopDataWritePacket(dataStorage.data(), 0x1000u, nullptr, 0) == 2);
    Require(dataStorage[0] == 0xc0001000u);
    Require(dataStorage[1] == 0x1000u);
    AgcDriver::Pm4::Validate({dataStorage.data(), 2}, 0);

    Require(sceAgcDriverNopDataWritePacket(dataStorage.data(), 0x2000u, &payload, sizeof(payload)) == 3);
    Require(dataStorage[0] == 0xc0011000u);
    Require(dataStorage[1] == 0x2000u);
    Require(dataStorage[2] == 0x12345678u);
    AgcDriver::Pm4::Validate({dataStorage.data(), 3}, 0);

    return 0;
}
