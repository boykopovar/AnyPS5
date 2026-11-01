#include "prx/libSceAgc/DcbState/include/Registers.hpp"
#include "prx/libSceAgc/Cb/include/Registers.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "SceTypes.hpp"
#include "SceShaders.hpp"
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

    auto* direct = sceAgcDcbSetUcRegisterDirect(&buffer, {0x242u, 4u});
    Require(direct == words.data());
    Require(direct[0] == 0xc0017900u);
    Require(direct[1] == 0x242u);
    Require(direct[2] == 4u);
    Require(sceAgcDcbSetUcRegisterDirectGetSize() == 12);

    AgcDriver::QueueState queue;
    AgcDriver::Pm4::Validate({direct, 3}, 0);
    AgcDriver::Pm4::Execute({direct, 3}, queue);
    Require(queue.userConfig.at(0x242u) == 4u);

    auto* indexTypeDirect = sceAgcDcbSetUcRegisterDirect(&buffer, {0x243u, 2u});
    Require(indexTypeDirect == words.data() + 3);
    Require(indexTypeDirect[0] == 0xc0017900u);
    Require(indexTypeDirect[1] == 0x243u);
    Require(indexTypeDirect[2] == 2u);

    AgcDriver::Pm4::Validate({indexTypeDirect, 3}, 0);
    AgcDriver::Pm4::Execute({indexTypeDirect, 3}, queue);
    Require(queue.userConfig.at(0x243u) == 2u);
    Require(queue.indexType == 2u);

    const std::array<std::uint32_t, 2> rangeValues{10u, 20u};
    auto* rangePacket = sceAgcCbSetUcRegisterRangeDirect(&buffer, 0x100u, rangeValues.data(), 2u);
    Require(rangePacket == words.data() + 6);
    Require(rangePacket[0] == 0xc0027900u);
    Require(rangePacket[1] == 0x100u);
    Require(rangePacket[2] == 10u && rangePacket[3] == 20u);
    Require(sceAgcCbSetUcRegisterRangeDirectGetSize(2u) == 16);

    AgcDriver::Pm4::Validate({rangePacket, 4}, 0);
    AgcDriver::Pm4::Execute({rangePacket, 4}, queue);
    Require(queue.userConfig.at(0x100u) == 10u && queue.userConfig.at(0x101u) == 20u);

    const std::array<ShaderRegister, 2> regList{{{0x110u, 30u}, {0x111u, 40u}}};
    auto* listPacket = sceAgcCbSetUcRegistersDirect(&buffer, regList.data(), 2u);
    Require(listPacket == rangePacket + 4);
    Require(listPacket[0] == 0xc0027900u);
    Require(listPacket[1] == 0x110u);
    Require(listPacket[2] == 30u && listPacket[3] == 40u);
    Require(sceAgcCbSetUcRegistersDirectGetSize(2u) == 24);

    AgcDriver::Pm4::Validate({listPacket, 4}, 0);
    AgcDriver::Pm4::Execute({listPacket, 4}, queue);
    Require(queue.userConfig.at(0x110u) == 30u && queue.userConfig.at(0x111u) == 40u);

    const std::array<std::uint32_t, 3> invalidOffset{0xc0017900u, 0x10000u, 1u};
    Reject([&] { AgcDriver::Pm4::Validate(invalidOffset, 0); });
    Reject([&] { AgcDriver::Pm4::Execute(invalidOffset, queue); });

    const std::array<std::uint32_t, 3> invalidLargeOffset{0xc0017900u, 0x100000u, 1u};
    Reject([&] { AgcDriver::Pm4::Validate(invalidLargeOffset, 0); });

    const std::array<std::uint32_t, 3> highBitOffset{0xc0017900u, 0x80000000u, 1u};
    Reject([&] { AgcDriver::Pm4::Validate(highBitOffset, 0); });

    const std::array<std::uint32_t, 3> sentinelPacket{0xc0017900u, 0xffffffffu, 1u};
    Reject([&] { AgcDriver::Pm4::Validate(sentinelPacket, 0); });
    Reject([&] { AgcDriver::Pm4::Execute(sentinelPacket, queue); });

    const std::array<std::uint32_t, 4> overflowRange{0xc0027900u, 0xffffu, 1u, 2u};
    Reject([&] { AgcDriver::Pm4::Validate(overflowRange, 0); });
    Reject([&] { AgcDriver::Pm4::Execute(overflowRange, queue); });

    const std::array<std::uint32_t, 2> shortPacket{0xc0007900u, 0x242u};
    Reject([&] { AgcDriver::Pm4::Validate(shortPacket, 0); });

    const std::array<std::uint32_t, 3> validPacket{0xc0017900u, 0x242u, 1u};
    Reject([&] { AgcDriver::Pm4::Validate(validPacket, 0x20); });

    auto* savedCursor = buffer.cursor_up;
    Reject([&] { sceAgcDcbSetUcRegisterDirect(&buffer, {0x10000u, 1u}); });
    Require(buffer.cursor_up == savedCursor);
    Reject([&] { sceAgcDcbSetUcRegisterDirect(&buffer, {0xffffffffu, 1u}); });
    Require(buffer.cursor_up == savedCursor);
    Reject([&] { sceAgcDcbSetUcRegisterDirect(nullptr, {0x242u, 1u}); });

    buffer.cursor_down = buffer.cursor_up + 2;
    Reject([&] { sceAgcDcbSetUcRegisterDirect(&buffer, {0x242u, 1u}); });
    buffer.cursor_down = words.data() + words.size();
    Require(buffer.cursor_up == savedCursor);

    const std::uint32_t val = 5u;
    Reject([&] { sceAgcCbSetUcRegisterRangeDirect(&buffer, 0x10000u, &val, 1u); });
    Require(buffer.cursor_up == savedCursor);
    Reject([&] { sceAgcCbSetUcRegisterRangeDirect(&buffer, 0xffffu, &val, 2u); });
    Require(buffer.cursor_up == savedCursor);
    Reject([&] { sceAgcCbSetUcRegisterRangeDirect(&buffer, 0x100u, &val, 0u); });
    Require(buffer.cursor_up == savedCursor);
    Reject([&] { sceAgcCbSetUcRegisterRangeDirect(&buffer, 0x100u, &val, 0x4000u); });
    Require(buffer.cursor_up == savedCursor);
    Reject([&] { sceAgcCbSetUcRegisterRangeDirect(nullptr, 0x100u, &val, 1u); });

    const ShaderRegister invalidReg{0x10000u, 1u};
    Reject([&] { sceAgcCbSetUcRegistersDirect(&buffer, &invalidReg, 1u); });
    Require(buffer.cursor_up == savedCursor);
    const ShaderRegister sentinelReg{0xffffffffu, 1u};
    Reject([&] { sceAgcCbSetUcRegistersDirect(&buffer, &sentinelReg, 1u); });
    Require(buffer.cursor_up == savedCursor);
    const ShaderRegister validReg{0x100u, 1u};
    Reject([&] { sceAgcCbSetUcRegistersDirect(nullptr, &validReg, 1u); });

    return 0;
}
