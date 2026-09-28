#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/QueueState.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include "prx/libSceAgcDriver/Eq/include/Query.hpp"
#include <array>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

static_assert(sizeof(Packet) == 16);
static_assert(offsetof(Packet, addr) == 0);
static_assert(offsetof(Packet, dw_num) == 8);
static_assert(offsetof(Packet, flags) == 12);

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<typename TAction>
std::string expectFailure(TAction action) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        return error.what();
    }
    throw std::runtime_error("expected exception");
}

void testEvents() {
    KernelEvent event{};
    event.filter = -14;
    event.ident = 0x40;
    event.data = 123;
    check(sceAgcDriverGetEqEventType(&event) == 0x40, "graphics event uses wrong field");
    event.filter = -1;
    event.data = -17;
    check(sceAgcDriverGetEqEventType(&event) == -17, "non-graphics event uses wrong field");
    event.data = std::numeric_limits<std::intptr_t>::max();
    expectFailure([&] { sceAgcDriverGetEqEventType(&event); });
    event.filter = -14;
    event.ident = std::numeric_limits<std::uintptr_t>::max();
    expectFailure([&] { sceAgcDriverGetEqEventType(&event); });
    expectFailure([] { sceAgcDriverGetEqEventType(nullptr); });
    expectFailure([&] { sceAgcDriverGetEqEventType(reinterpret_cast<const KernelEvent*>(reinterpret_cast<const std::byte*>(&event) + 1)); });
}

void testValidation() {
    std::array<std::uint32_t, 3> commands{0xc0017600, 0x20c, 0};
    Packet packet{commands.data(), 3, 0, {}};
    expectFailure([] { sceAgcDriverSubmitDcb(nullptr); });
    expectFailure([] { sceAgcDriverAgrSubmitDcb(nullptr); });
    expectFailure([] { sceAgcDriverSubmitAcb(0x20, nullptr); });
    expectFailure([&] { sceAgcDriverSubmitAcb(0, &packet); });
    expectFailure([&] { sceAgcDriverSubmitAcb(0x58, &packet); });
    packet.dw_num = 2;
    expectFailure([&] { sceAgcDriverSubmitDcb(&packet); });
    packet.dw_num = 3;
    packet.flags = 1;
    expectFailure([&] { sceAgcDriverSubmitDcb(&packet); });
    packet.flags = 0;
    commands[0] = 0xc001ff00;
    expectFailure([&] { sceAgcDriverSubmitDcb(&packet); });
    commands[0] = 0xc001105c;
    expectFailure([&] { sceAgcDriverSubmitDcb(&packet); });
    commands[0] = 0xc0017601;
    expectFailure([&] { sceAgcDriverSubmitDcb(&packet); });
    commands[0] = 0xc0017600;
    commands[1] = 0x10000;
    expectFailure([&] { sceAgcDriverSubmitDcb(&packet); });
    packet.addr = reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uintptr_t>(commands.data()) + 1);
    expectFailure([&] { sceAgcDriverSubmitDcb(&packet); });
    packet.addr = reinterpret_cast<std::uint32_t*>(std::numeric_limits<std::uintptr_t>::max() - 3);
    expectFailure([&] { sceAgcDriverSubmitDcb(&packet); });
    packet.addr = reinterpret_cast<std::uint32_t*>(0x1000);
    expectFailure([&] { sceAgcDriverSubmitDcb(&packet); });
    AgcDriverWaitIdle_nid_postfix();
}

void testClearState() {
    AgcDriver::QueueState graphics{{{0x20c, 1}}, {{0x10, 17}, {0x11, 23}}, {{0x242, 5}}};
    const auto shader = graphics.shader;
    const auto userConfig = graphics.userConfig;
    graphics.ClearContext();
    check(graphics.context == AgcDriver::InitialContextRegisters(), "CLEAR_STATE retained context registers");
    check(graphics.shader == shader && graphics.userConfig == userConfig, "CLEAR_STATE reset unrelated registers");
    graphics.context.emplace(0x10, 31);
    graphics.ClearContext();
    check(graphics.context == AgcDriver::InitialContextRegisters(), "repeated CLEAR_STATE retained context registers");

    std::array<std::uint32_t, 3> words{0xc0001200, 0, 0};
    Packet packet{words.data(), 2, 0, {}};
    expectFailure([&] { sceAgcDriverSubmitAcb(0x20, &packet); });
    words[1] = 0x10;
    expectFailure([&] { sceAgcDriverSubmitDcb(&packet); });
    words[1] = 0;
    words[0] = 0xc0011200;
    packet.dw_num = 3;
    expectFailure([&] { sceAgcDriverSubmitDcb(&packet); });
    words[0] = 0xc0001201;
    packet.dw_num = 2;
    expectFailure([&] { sceAgcDriverSubmitDcb(&packet); });
    words[0] = 0xc0001200;
    packet.dw_num = 1;
    expectFailure([&] { sceAgcDriverSubmitDcb(&packet); });
    packet.dw_num = 2;
    for (std::uint32_t state = 0; state <= 0xf; ++state) {
        words[1] = state;
        check(sceAgcDriverSubmitDcb(&packet) == 0, "CLEAR_STATE submit failed");
    }
    AgcDriverWaitIdle_nid_postfix();
}

void testSubmissions() {
    std::vector<std::thread> producers;
    std::array<std::exception_ptr, 4> errors{};
    for (std::uint32_t i = 0; i < errors.size(); ++i) {
        producers.emplace_back([&, i] {
            try {
                for (std::uint32_t j = 0; j < 100; ++j) {
                    std::array<std::uint32_t, 5> words{0xc0017600, 0x240, j, 0xc0001000, 0};
                    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
                    if (i == 0) check(sceAgcDriverSubmitDcb(&packet) == 0, "DCB submit failed");
                    else if (i == 1) check(sceAgcDriverAgrSubmitDcb(&packet) == 0, "AGR submit failed");
                    else check(sceAgcDriverSubmitAcb(i == 2 ? 0x20 : 0x57, &packet) == 0, "ACB submit failed");
                    words.fill(0xffffffffu);
                }
            } catch (...) {
                errors[i] = std::current_exception();
            }
        });
    }
    for (auto& producer : producers) producer.join();
    for (auto& error : errors) if (error) std::rethrow_exception(error);
    AgcDriverWaitIdle_nid_postfix();
    Packet empty{};
    check(sceAgcDriverSubmitDcb(&empty) == 0, "empty submit failed");
    AgcDriverWaitIdle_nid_postfix();
}

void testWorkerFailure() {
    std::array<std::uint32_t, 5> words{0xc0031500, 1, 1, 1, 0x41};
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    check(sceAgcDriverSubmitAcb(0x21, &packet) == 0, "dispatch was not accepted");
    AgcDriverWaitIdle_nid_postfix();
    std::array<std::uint32_t, 5> unmapped{0xc0033700, 0x100, 0x100, 0, 0x41};
    Packet write{unmapped.data(), static_cast<std::uint32_t>(unmapped.size()), 0, {}};
    check(sceAgcDriverSubmitDcb(&write) == 0, "write was not accepted");
    std::array<std::string, 4> messages;
    std::vector<std::thread> waiters;
    for (auto& message : messages) {
        waiters.emplace_back([&message] { message = expectFailure([] { AgcDriverWaitIdle_nid_postfix(); }); });
    }
    for (auto& waiter : waiters) waiter.join();
    for (const auto& message : messages) check(message.find("guest memory") != std::string::npos, "worker failure was lost");
    check(expectFailure([&] { sceAgcDriverSubmitDcb(&write); }) == messages[0], "subsequent DCB lost worker failure");
    check(expectFailure([&] { sceAgcDriverAgrSubmitDcb(&write); }) == messages[0], "subsequent AGR lost worker failure");
    check(expectFailure([&] { sceAgcDriverSubmitAcb(0x20, &write); }) == messages[0], "subsequent ACB lost worker failure");
}

}

int main() {
    try {
        testEvents();
        testValidation();
        testClearState();
        testSubmissions();
        testWorkerFailure();
        check(expectFailure([] { LibcRunShutdown_nid_postfix(); }).find("guest memory") != std::string::npos, "shutdown lost worker failure");
        std::puts("AGC driver submit tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try { LibcRunShutdown_nid_postfix(); }
        catch (const std::exception& shutdown) { std::fprintf(stderr, "shutdown: %s\n", shutdown.what()); }
        return 1;
    }
}
