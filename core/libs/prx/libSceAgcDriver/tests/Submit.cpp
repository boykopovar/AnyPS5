#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "execution/VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/QueueState.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include "prx/libSceAgcDriver/Eq/include/Query.hpp"
#include "prx/libSceAgcDriver/Eq/include/Event.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include <array>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

extern "C" int APS5_VABI sceKernelCreateEqueue(KernelEqueue* eq, const char* name);
extern "C" int APS5_VABI sceKernelDeleteEqueue(KernelEqueue eq);

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
    event.ident = 0x29;
    check(sceAgcDriverGetEqContextId(&event) == 0x29, "graphics event context id uses wrong field");
    event.ident = std::numeric_limits<std::uintptr_t>::max();
    expectFailure([&] { sceAgcDriverGetEqContextId(&event); });
    event.ident = 1;
    event.filter = -1;
    expectFailure([&] { sceAgcDriverGetEqContextId(&event); });
    expectFailure([] { sceAgcDriverGetEqContextId(nullptr); });
    expectFailure([&] { sceAgcDriverGetEqContextId(reinterpret_cast<const KernelEvent*>(reinterpret_cast<const std::byte*>(&event) + 1)); });
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
    commands[0] = 0xc0017608;
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
    check(graphics.context.count(0x1b3) == 1 && graphics.context.at(0x1b3) == 0 && graphics.context.count(0x1b4) == 1 && graphics.context.at(0x1b4) == 0, "CLEAR_STATE left SPI_PS_INPUT_ENA/ADDR unset");
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
    words[0] = 0xc0001202;
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

void testEndOfPipeInterrupts(bool implicitEop = false) {
    KernelEqueue eq = 0;
    check(sceKernelCreateEqueue(&eq, "AGC test") == 0, "event queue creation failed");
    auto owner = EqueuePin_nid_postfix(eq);
    int graphicsTag = 0;
    int computeTag = 0;
    check(sceAgcDriverAddEqEvent(eq, 0, &graphicsTag) == 0, "graphics event registration failed");
    check(sceAgcDriverAddEqEvent(eq, 0x20, &computeTag) == 0, "compute event registration failed");
    expectFailure([] { sceAgcDriverAddEqEvent(0, 0, nullptr); });
    std::array<std::uint32_t, 8> words{0xc0064900, 0, 1u << 24u, 0, 0, 0, 0, 0};
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    check(sceAgcDriverSubmitDcb(&packet) == 0 && sceAgcDriverSubmitDcb(&packet) == 0, "interrupt submit failed");
    AgcDriverWaitIdle_nid_postfix();
    std::array<KernelEvent, 2> events{};
    check(owner->GetTriggeredEvents(events.data(), 2) == 1, "graphics end-of-pipe interrupt was not delivered to its queue only");
    check(events[0].filter == -14 && events[0].udata == &graphicsTag && events[0].data == 2 && sceAgcDriverGetEqEventType(events.data()) == 0, "graphics end-of-pipe event encoding is wrong");
    check(sceAgcDriverGetEqContextId(events.data()) == 0, "graphics end-of-pipe event reports another context");
    check(owner->GetTriggeredEvents(events.data(), 2) == 0, "delivered interrupt was not cleared");
    check(sceAgcDriverSubmitAcb(0x20, &packet) == 0, "compute interrupt submit failed");
    AgcDriverWaitIdle_nid_postfix();
    check(owner->GetTriggeredEvents(events.data(), 2) == 1 && events[0].udata == &computeTag && sceAgcDriverGetEqEventType(events.data()) == 0x20, "compute end-of-pipe interrupt missing");
    check(sceAgcDriverGetEqContextId(events.data()) == 0x20, "compute end-of-pipe event reports another context");
    words[2] = 0;
    check(sceAgcDriverSubmitDcb(&packet) == 0, "plain release submit failed");
    AgcDriverWaitIdle_nid_postfix();
    const auto plainEvents = owner->GetTriggeredEvents(events.data(), 2);
    check(plainEvents == (implicitEop ? 1 : 0), "plain release has the wrong submission completion count");
    if (implicitEop) check(events[0].ident == 0 && events[0].data == 1, "plain release completion has the wrong queue or count");
    alignas(8) static volatile std::uint64_t label = 0;
    const auto labelAddress = reinterpret_cast<std::uintptr_t>(&label);
    words = {0xc0064900, 0x528, (3u << 29u) | (3u << 24u) | (1u << 16u), static_cast<std::uint32_t>(labelAddress), static_cast<std::uint32_t>(static_cast<std::uint64_t>(labelAddress) >> 32u), 0x89abcdefu, 0x01234567u, 0};
    check(sceAgcDriverSubmitDcb(&packet) == 0, "send-data release submit failed");
    AgcDriverWaitIdle_nid_postfix();
    check(label != 0, "send-data release did not write its label");
    const auto sendDataEvents = owner->GetTriggeredEvents(events.data(), 2);
    check(sendDataEvents == (implicitEop ? 1 : 0), "send-data release has the wrong submission completion count");
    if (implicitEop) check(events[0].ident == 0 && events[0].data == 1, "send-data release completion has the wrong queue or count");
    words = {0xc0064900, 0, 1u << 24u, 0, 0, 0, 0, 0};
    check(sceAgcDriverDeleteEqEvent(eq, 0) == 0, "graphics event deletion failed");
    expectFailure([&] { sceAgcDriverDeleteEqEvent(eq, 0); });
    words[2] = 1u << 24u;
    check(sceAgcDriverSubmitDcb(&packet) == 0, "interrupt submit after deletion failed");
    AgcDriverWaitIdle_nid_postfix();
    check(owner->GetTriggeredEvents(events.data(), 2) == 0, "deleted event still received interrupts");
    check(sceAgcDriverDeleteEqEvent(eq, 0x20) == 0, "compute event deletion failed");
    owner.reset();
    check(sceKernelDeleteEqueue(eq) == 0, "event queue deletion failed");
}

std::array<std::uint32_t, 5> writeData(volatile std::uint32_t* address, std::uint32_t value) {
    const auto target = reinterpret_cast<std::uintptr_t>(address);
    return {0xc0033700, 0x00100200, static_cast<std::uint32_t>(target), static_cast<std::uint32_t>(static_cast<std::uint64_t>(target) >> 32u), value};
}

std::array<std::uint32_t, 7> waitEqual(volatile std::uint32_t* address, std::uint32_t value) {
    const auto target = reinterpret_cast<std::uintptr_t>(address);
    return {0xc0053c00, 0x13, static_cast<std::uint32_t>(target), static_cast<std::uint32_t>(static_cast<std::uint64_t>(target) >> 32u), value, 0xffffffffu, 0x19};
}

std::array<std::uint32_t, 9> waitEqual64(volatile std::uint32_t* address, std::uint64_t value, std::uint64_t mask) {
    const auto target = reinterpret_cast<std::uintptr_t>(address);
    return {0xc0079300, 0x13, static_cast<std::uint32_t>(target), static_cast<std::uint32_t>(static_cast<std::uint64_t>(target) >> 32u), static_cast<std::uint32_t>(value), static_cast<std::uint32_t>(value >> 32u), static_cast<std::uint32_t>(mask), static_cast<std::uint32_t>(mask >> 32u), 0x19};
}

void submit(std::uint32_t queue, const std::vector<std::uint32_t>& words) {
    Packet packet{const_cast<std::uint32_t*>(words.data()), static_cast<std::uint32_t>(words.size()), 0, {}};
    check((queue == 0 ? sceAgcDriverSubmitDcb(&packet) : sceAgcDriverSubmitAcb(queue, &packet)) == 0, "label submit failed");
}

std::chrono::milliseconds waitFor(volatile std::uint32_t* address, std::uint32_t value, const char* message) {
    const auto start = std::chrono::steady_clock::now();
    while (*address != value) {
        check(std::chrono::steady_clock::now() - start < std::chrono::seconds(10), message);
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
}

template<std::size_t... N>
std::vector<std::uint32_t> commands(const std::array<std::uint32_t, N>&... packets) {
    std::vector<std::uint32_t> words;
    (words.insert(words.end(), packets.begin(), packets.end()), ...);
    return words;
}

void testImplicitSubmissionCompletions() {
    KernelEqueue eq = 0;
    check(sceKernelCreateEqueue(&eq, "AGC submission completions") == 0, "event queue creation failed");
    auto owner = EqueuePin_nid_postfix(eq);
    int graphicsTag = 0, computeTag = 0;
    check(sceAgcDriverAddEqEvent(eq, 0, &graphicsTag) == 0, "graphics event registration failed");
    check(sceAgcDriverAddEqEvent(eq, 0x20, &computeTag) == 0, "compute event registration failed");
    std::array<KernelEvent, 2> events{};
    const auto expectCompletion = [&](std::uintptr_t queue, void* tag, std::intptr_t count) {
        check(owner->GetTriggeredEvents(events.data(), 2) == 1, "submission completion was missing or reached another queue");
        check(events[0].filter == -14 && events[0].ident == queue && events[0].udata == tag && events[0].data == count, "submission completion has the wrong encoding or count");
        check(owner->GetTriggeredEvents(events.data(), 2) == 0, "submission completion did not clear");
    };
    alignas(64) static volatile std::uint32_t gate = 0, started = 0, finished = 0;
    submit(0, commands(writeData(&started, 1), waitEqual(&gate, 1), writeData(&finished, 1)));
    waitFor(&started, 1, "graphics submission never reached its wait");
    check(owner->GetTriggeredEvents(events.data(), 2) == 0 && finished == 0, "a blocked submission signaled completion early");
    submit(0x20, commands(writeData(&gate, 1)));
    AgcDriverWaitIdle_nid_postfix();
    check(finished == 1, "graphics submission did not finish after its dependency");
    check(owner->GetTriggeredEvents(events.data(), 2) == 2, "independent queue completions were lost");
    for (const auto& event : events) {
        check(event.filter == -14 && event.data == 1, "queue completion was duplicated");
        check((event.ident == 0 && event.udata == &graphicsTag) || (event.ident == 0x20 && event.udata == &computeTag), "queue completion reached the wrong registration");
    }
    for (int i = 0; i < 3; ++i) submit(0, {0xc0001000, 0});
    AgcDriverWaitIdle_nid_postfix();
    expectCompletion(0, &graphicsTag, 3);
    Packet empty{};
    check(sceAgcDriverSubmitDcb(&empty) == 0, "empty submit failed");
    AgcDriverWaitIdle_nid_postfix();
    expectCompletion(0, &graphicsTag, 1);
    const std::vector<std::uint32_t> rewindThenInterrupt{0xc0005900, 0x80000000u, 0xc0064900, 0, 1u << 24u, 0, 0, 0, 0, 0};
    submit(0, rewindThenInterrupt);
    AgcDriverWaitIdle_nid_postfix();
    expectCompletion(0, &graphicsTag, 1);
    const std::vector<std::uint32_t> interruptThenRewind{0xc0064900, 0, 1u << 24u, 0, 0, 0, 0, 0, 0xc0005900, 0x80000000u, 0xc0001000, 0};
    submit(0, interruptThenRewind);
    AgcDriverWaitIdle_nid_postfix();
    expectCompletion(0, &graphicsTag, 1);
    submit(0, {0xc0001000, 0});
    AgcDriverWaitIdle_nid_postfix();
    expectCompletion(0, &graphicsTag, 1);
    AgcDriverSuspendPoint_nid_postfix();
    AgcDriverWaitIdle_nid_postfix();
    check(owner->GetTriggeredEvents(events.data(), 2) == 0, "suspend boundary raised a submission interrupt");
    check(sceAgcDriverDeleteEqEvent(eq, 0) == 0, "graphics event deletion failed");
    submit(0, {0xc0001000, 0});
    AgcDriverWaitIdle_nid_postfix();
    check(owner->GetTriggeredEvents(events.data(), 2) == 0, "a removed registration received completion");
    check(sceAgcDriverDeleteEqEvent(eq, 0x20) == 0, "compute event deletion failed");
    owner.reset();
    check(sceKernelDeleteEqueue(eq) == 0, "event queue deletion failed");
}

std::array<std::uint32_t, 8> endOfPipeLabel(volatile std::uint32_t* address, std::uint32_t value) {
    const auto target = reinterpret_cast<std::uintptr_t>(address);
    return {0xc0064900, 0x514, (1u << 29u) | (2u << 24u), static_cast<std::uint32_t>(target), static_cast<std::uint32_t>(static_cast<std::uint64_t>(target) >> 32u), value, 0, 0};
}

void testEndOfPipeLabelsWithoutWork() {
    alignas(64) static volatile std::uint32_t first = 0, second = 0, done = 0;
    KernelEqueue eq = 0;
    check(sceKernelCreateEqueue(&eq, "AGC EOP labels") == 0, "event queue creation failed");
    auto owner = EqueuePin_nid_postfix(eq);
    int tag = 0;
    check(sceAgcDriverAddEqEvent(eq, 0, &tag) == 0, "graphics event registration failed");
    submit(0x20, commands(waitEqual(&second, 2), writeData(&done, 1)));
    submit(0, commands(endOfPipeLabel(&first, 1), endOfPipeLabel(&second, 2)));
    waitFor(&done, 1, "an end-of-pipe label with no work before it never landed");
    check(first == 1 && second == 2, "end-of-pipe labels with no work before them landed wrong");
    AgcDriverWaitIdle_nid_postfix();
    std::array<KernelEvent, 2> events{};
    check(owner->GetTriggeredEvents(events.data(), 2) == 1 && events[0].udata == &tag && events[0].data == 2, "end-of-pipe labels with no work before them lost their interrupts");
    check(sceAgcDriverDeleteEqEvent(eq, 0) == 0, "graphics event deletion failed");
    owner.reset();
    check(sceKernelDeleteEqueue(eq) == 0, "event queue deletion failed");
}

void testLabelStoredSinceSubmission() {
    alignas(64) static volatile std::uint32_t gate = 0, label = 0, done = 0, late = 0;
    submit(0x20, commands(waitEqual(&gate, 1), waitEqual(&label, 1), writeData(&done, 1)));
    submit(0, commands(writeData(&label, 1)));
    waitFor(&label, 1, "producer label never landed");
    label = 0;
    gate = 1;
    check(waitFor(&done, 1, "consumer never passed its waits") < std::chrono::milliseconds(500), "a label stored after the wait's submission did not satisfy it");
    AgcDriverWaitIdle_nid_postfix();
    submit(0x20, commands(waitEqual(&label, 1), writeData(&late, 1)));
    check(waitFor(&late, 1, "consumer never passed its wait") >= std::chrono::milliseconds(900), "a label stored before the wait's submission satisfied it");
    AgcDriverWaitIdle_nid_postfix();
}

void testWideLabelStoredSinceSubmission() {
    alignas(64) static volatile std::uint32_t gate = 0, done = 0, late = 0;
    alignas(64) static volatile std::uint32_t label[2] = {0, 0x5eed};
    submit(0x20, commands(waitEqual(&gate, 1), waitEqual64(label, 1, 0xffffffffu), writeData(&done, 1)));
    submit(0, commands(writeData(label, 1)));
    waitFor(label, 1, "producer label never landed");
    label[0] = 0;
    gate = 1;
    check(waitFor(&done, 1, "consumer never passed its waits") < std::chrono::milliseconds(500), "a 32-bit label stored after a low-dword 64-bit wait's submission did not satisfy it");
    AgcDriverWaitIdle_nid_postfix();
    gate = 0;
    submit(0x20, commands(waitEqual(&gate, 1), waitEqual64(label, 1, ~0ull), writeData(&late, 1)));
    submit(0, commands(writeData(label, 1)));
    waitFor(label, 1, "producer label never landed");
    label[0] = 0;
    gate = 1;
    check(waitFor(&late, 1, "consumer never passed its wait") >= std::chrono::milliseconds(900), "a 32-bit store satisfied a 64-bit wait whose high dword never matched");
    AgcDriverWaitIdle_nid_postfix();
}

void testLabelHeldAtSubmission() {
    alignas(64) static volatile std::uint32_t gate = 0, label = 1, done = 0, reset = 0;
    submit(0x20, commands(waitEqual(&gate, 1), waitEqual(&label, 1), writeData(&done, 1)));
    label = 0;
    gate = 1;
    check(waitFor(&done, 1, "consumer never passed its waits") < std::chrono::milliseconds(500), "a label held when the wait was submitted did not satisfy it");
    AgcDriverWaitIdle_nid_postfix();
    gate = 0;
    label = 1;
    submit(0x20, commands(waitEqual(&gate, 1), writeData(&label, 0), waitEqual(&label, 1), writeData(&reset, 1)));
    gate = 1;
    check(waitFor(&reset, 1, "consumer never passed its wait") >= std::chrono::milliseconds(900), "a label its own queue stored first counted as held at the submission");
    AgcDriverWaitIdle_nid_postfix();
}

void testWaitFreeSubmissionAfterEarlierQueue0Work() {
    alignas(64) static volatile std::uint32_t first = 0, second = 0;
    constexpr std::uint32_t writes = 20000;
    std::vector<std::uint32_t> words;
    for (std::uint32_t i = 1; i <= writes; ++i) {
        const auto write = writeData(&first, i);
        words.insert(words.end(), write.begin(), write.end());
    }
    submit(0, words);
    submit(0x20, commands(writeData(&second, 1)));
    waitFor(&second, 1, "the wait-free submission never ran");
    check(first == writes, "a wait-free submission ran before queue 0's earlier submission finished");
    AgcDriverWaitIdle_nid_postfix();
}

void testWaitFreeSubmissionQueue0WaitsOn() {
    alignas(64) static volatile std::uint32_t label = 0, done = 0;
    submit(0, commands(waitEqual(&label, 1), writeData(&done, 1)));
    submit(0x20, commands(writeData(&label, 1)));
    check(waitFor(&done, 1, "queue 0 never passed the wait the held submission satisfies") < std::chrono::milliseconds(500), "queue 0's wait on a held wait-free submission's label was not released at once");
    AgcDriverWaitIdle_nid_postfix();
}

void testWaitFreeSubmissionBehindHeldOne() {
    alignas(64) static volatile std::uint32_t other = 0, label = 0, done = 0;
    submit(0, commands(waitEqual(&label, 1), writeData(&done, 1)));
    submit(0x20, commands(writeData(&other, 1)));
    submit(0x20, commands(writeData(&label, 1)));
    check(waitFor(&done, 1, "queue 0 never passed the wait a later submission of the held queue satisfies") < std::chrono::milliseconds(500), "queue 0's wait on a label stored behind a held submission was not released at once");
    check(other == 1, "the held submission did not run before the one behind it");
    AgcDriverWaitIdle_nid_postfix();
}

void testMultiAcbs() {
    alignas(64) static volatile std::uint32_t value = 0, done = 0, gate = 0;
    check(sceAgcDriverSubmitMultiAcbs(0x20, nullptr, nullptr, 0) == 0, "empty multi-ACB submit failed");
    auto write = writeData(&value, 1);
    std::array<std::uint32_t*, 1> addresses{write.data()};
    std::array<std::uint32_t, 1> sizes{static_cast<std::uint32_t>(write.size())};
    expectFailure([&] { sceAgcDriverSubmitMultiAcbs(0x1f, addresses.data(), sizes.data(), 1); });
    expectFailure([&] { sceAgcDriverSubmitMultiAcbs(0x58, addresses.data(), sizes.data(), 1); });
    expectFailure([&] { sceAgcDriverSubmitMultiAcbs(0, addresses.data(), sizes.data(), 1); });
    check(sceAgcDriverSubmitMultiAcbs(0, nullptr, nullptr, 0) == 0, "empty multi-ACB submit checked its queue");
    expectFailure([&] { sceAgcDriverSubmitMultiAcbs(0x20, nullptr, sizes.data(), 1); });
    expectFailure([&] { sceAgcDriverSubmitMultiAcbs(0x20, addresses.data(), nullptr, 1); });
    check(value == 0, "a rejected multi-ACB submit ran a command buffer");
    for (std::uint32_t queue : {0x20u, 0x57u}) {
        value = 0;
        done = 0;
        auto first = writeData(&value, 1);
        auto second = writeData(&value, 2);
        auto third = writeData(&done, 1);
        std::array<std::uint32_t*, 3> buffers{first.data(), second.data(), third.data()};
        std::array<std::uint32_t, 3> lengths{static_cast<std::uint32_t>(first.size()), static_cast<std::uint32_t>(second.size()), static_cast<std::uint32_t>(third.size())};
        check(sceAgcDriverSubmitMultiAcbs(queue, buffers.data(), lengths.data(), 3) == 0, "multi-ACB submit failed");
        waitFor(&done, 1, "the last ACB of a multi-ACB submit never ran");
        check(value == 2, "multi-ACB submit did not run its ACBs in order");
        AgcDriverWaitIdle_nid_postfix();
    }
    done = 0;
    value = 0;
    auto wait = waitEqual(&gate, 1);
    auto finish = writeData(&done, 1);
    std::array<std::uint32_t*, 2> buffers{wait.data(), finish.data()};
    std::array<std::uint32_t, 2> lengths{static_cast<std::uint32_t>(wait.size()), static_cast<std::uint32_t>(finish.size())};
    check(sceAgcDriverSubmitMultiAcbs(0x21, buffers.data(), lengths.data(), 2) == 0, "waiting multi-ACB submit failed");
    submit(0x21, commands(writeData(&value, 1)));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    check(value == 0 && done == 0, "a later submission on the same compute queue overtook a multi-ACB submit");
    gate = 1;
    waitFor(&value, 1, "a submission queued behind a multi-ACB submit never ran");
    check(done == 1, "a multi-ACB submit did not run before a later submission on its queue");
    AgcDriverWaitIdle_nid_postfix();
}

void testWaitFreeSubmissionTheCpuWaitsFor() {
    alignas(64) static volatile std::uint32_t stored = 0, flag = 0, done = 0;
    submit(0, commands(waitEqual(&flag, 1), writeData(&done, 1)));
    submit(0x20, commands(writeData(&stored, 1)));
    std::thread title([] {
        waitFor(&stored, 1, "the held submission never ran while queue 0 waited on the CPU");
        flag = 1;
    });
    const auto waited = waitFor(&done, 1, "queue 0 never passed a wait the CPU satisfies after the held submission");
    title.join();
    check(waited < std::chrono::milliseconds(500), "a held submission the CPU waits for was not released while queue 0 waited on the CPU");
    AgcDriverWaitIdle_nid_postfix();
}

void testMultiSubmissions() {
    alignas(64) static volatile std::uint32_t value = 0, done = 0;
    check(sceAgcDriverSubmitMultiDcbs(nullptr, nullptr, 0) == 0, "empty multi-DCB submit failed");
    check(sceAgcDriverAgrSubmitMultiDcbs(nullptr, nullptr, 0) == 0, "empty AGR multi-DCB submit failed");
    std::array<std::uint32_t, 3> invalid{0xc0017600, 0x20c, 0};
    std::array<std::uint32_t*, 1> invalidAddresses{invalid.data()};
    std::array<std::uint32_t, 1> invalidSizes{2};
    expectFailure([&] { sceAgcDriverSubmitMultiDcbs(nullptr, invalidSizes.data(), 1); });
    expectFailure([&] { sceAgcDriverAgrSubmitMultiDcbs(invalidAddresses.data(), nullptr, 1); });
    expectFailure([&] { sceAgcDriverSubmitMultiDcbs(invalidAddresses.data(), invalidSizes.data(), 1); });
    expectFailure([&] { sceAgcDriverAgrSubmitMultiDcbs(invalidAddresses.data(), invalidSizes.data(), 1); });
    for (bool agr : {false, true}) {
        value = 0;
        done = 0;
        auto first = writeData(&value, 1);
        auto second = writeData(&value, 2);
        auto third = writeData(&done, 1);
        std::array<std::uint32_t*, 3> addresses{first.data(), second.data(), third.data()};
        std::array<std::uint32_t, 3> sizes{static_cast<std::uint32_t>(first.size()), static_cast<std::uint32_t>(second.size()), static_cast<std::uint32_t>(third.size())};
        const int result = agr ? sceAgcDriverAgrSubmitMultiDcbs(addresses.data(), sizes.data(), 3) : sceAgcDriverSubmitMultiDcbs(addresses.data(), sizes.data(), 3);
        check(result == 0, "multi-DCB submit failed");
        waitFor(&done, 1, "the last DCB of a multi-DCB submit never ran");
        check(value == 2, "multi-DCB submit did not run its DCBs in order");
        AgcDriverWaitIdle_nid_postfix();
    }
}

void testShaderHeaderAlignment() {
    alignas(256) static const std::array<std::uint32_t, 64> code{0xbf810000};
    struct Header {
        Shader shader{};
        std::array<ShaderRegister, 7> registers{};
        ShaderSpecialRegs specials{};
    };
    alignas(8) static std::array<std::byte, sizeof(Header) + 8> storage{};
    Shader shader{};
    shader.file_header = 0x34333231;
    shader.version = 0x18;
    shader.header_size = sizeof(Header);
    shader.shader_size = sizeof(code);
    shader.code = code.data();
    const auto at = [](std::size_t offset, const Shader& fields) {
        Header header;
        header.shader = fields;
        const auto address = reinterpret_cast<std::uintptr_t>(fields.code);
        header.registers = {{{0x20c, static_cast<std::uint32_t>(address >> 8u)}, {0x20d, static_cast<std::uint32_t>(address >> 40u)}, {0x207, 1}, {0x208, 1}, {0x209, 1}, {0x212, 0}, {0x213, 0}}};
        header.specials.dispatch_modifier = 0x8000u;
        header.shader.sh_registers = reinterpret_cast<ShaderRegister*>(storage.data() + offset + offsetof(Header, registers));
        header.shader.num_sh_registers = header.registers.size();
        header.shader.specials = reinterpret_cast<ShaderSpecialRegs*>(storage.data() + offset + offsetof(Header, specials));
        std::memcpy(storage.data() + offset, &header, sizeof(header));
        return reinterpret_cast<const Shader*>(storage.data() + offset);
    };
    for (const std::size_t offset : {0, 4, 1}) AgcDriverRegisterShader_nid_postfix(at(offset, shader));
    const auto refused = [](const std::string& message, const char* reason) { check(message.find(reason) != std::string::npos, message.c_str()); };
    refused(expectFailure([] { AgcDriverRegisterShader_nid_postfix(nullptr); }), "null or misaligned address");
    refused(expectFailure([] { AgcDriverRegisterShader_nid_postfix(reinterpret_cast<const Shader*>(0x1001)); }), "not readable");
    Shader misplaced = shader;
    misplaced.code = code.data() + 1;
    refused(expectFailure([&] { AgcDriverRegisterShader_nid_postfix(at(4, misplaced)); }), "null or misaligned address");
    Shader older = shader;
    older.version = 0x17;
    refused(expectFailure([&] { AgcDriverRegisterShader_nid_postfix(at(1, older)); }), "invalid shader header");
    Shader truncated = shader;
    truncated.header_size = sizeof(Shader) - 4;
    refused(expectFailure([&] { AgcDriverRegisterShader_nid_postfix(at(4, truncated)); }), "smaller than its fixed fields");
}

void testHeaderWithoutProgramAddress() {
    alignas(256) static const std::array<std::uint32_t, 64> code{0xbf810000};
    struct Header {
        Shader shader{};
        std::array<ShaderRegister, 3> registers{};
    };
    alignas(8) static Header header{};
    header.shader.file_header = 0x34333231;
    header.shader.version = 0x18;
    header.shader.header_size = sizeof(Header);
    header.shader.shader_size = sizeof(code);
    header.shader.code = code.data();
    header.shader.type = 4;
    header.registers = {{{0x08a, 0x60000002}, {0x08b, 0x00030008}, {0x0ca, 0x03000002}}};
    header.shader.sh_registers = header.registers.data();
    header.shader.num_sh_registers = header.registers.size();
    AgcDriverRegisterShader_nid_postfix(&header.shader);
}

void testRegisteredFloatMode() {
    using AgcDriver::DriverDetail::RegisteredFloatMode;
    using AgcDriver::DriverDetail::ShaderSnapshot;
    using AgcDriver::DriverDetail::RegisteredShaderState;
    struct Stage {
        std::uint8_t type;
        std::uint32_t rsrc1;
        std::uint32_t fp16OverflowBit;
    };
    constexpr std::array<Stage, 7> stages{{{0, 0x212, 26}, {1, 0x00a, 29}, {2, 0x08a, 31}, {4, 0x08a, 31}, {6, 0x08a, 31}, {5, 0x10a, 30}, {7, 0x10a, 30}}};
    constexpr std::array<std::uint32_t, 4> otherBits{26, 29, 30, 31};
    for (const auto& stage : stages) {
        const auto mode = [&](std::uint32_t value) {
            ShaderSnapshot snapshot{0x20000, 0, stage.type, {}, {}};
            auto state = std::make_shared<RegisteredShaderState>();
            state->shader.emplace(stage.rsrc1, value);
            snapshot.registeredState = state;
            return RegisteredFloatMode(snapshot);
        };
        const auto name = "stage type " + std::to_string(stage.type);
        const auto astro = mode((0xc0u << 12u) | (1u << 21u) | 0x3fu);
        check(astro.has_value() && astro->floatMode == 0xc0u && astro->dx10Clamp && !astro->ieeeMode && !astro->fp16Overflow, (name + ": FLOAT_MODE 0xc0 with DX10_CLAMP decoded wrong").c_str());
        const auto ieee = mode(1u << 23u);
        check(ieee && ieee->ieeeMode && ieee->floatMode == 0u && !ieee->dx10Clamp, (name + ": IEEE_MODE decoded wrong").c_str());
        check(mode(1u << stage.fp16OverflowBit)->fp16Overflow, (name + ": FP16_OVFL not read from bit " + std::to_string(stage.fp16OverflowBit)).c_str());
        for (const auto bit : otherBits) {
            if (bit != stage.fp16OverflowBit) check(!mode(1u << bit)->fp16Overflow, (name + ": bit " + std::to_string(bit) + " read as FP16_OVFL").c_str());
        }
        ShaderSnapshot missing{0x20000, 0, stage.type, {}, {}};
        check(!RegisteredFloatMode(missing).has_value(), (name + ": a snapshot without registered state has a float mode").c_str());
        missing.registeredState = std::make_shared<RegisteredShaderState>();
        check(!RegisteredFloatMode(missing).has_value(), (name + ": a missing RSRC1 has a float mode").c_str());
    }
}

void testWorkerFailure() {
    std::array<std::uint32_t, 5> words{0xc0031500, 1, 1, 1, 0x41};
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    check(sceAgcDriverSubmitAcb(0x21, &packet) == 0, "dispatch was not accepted");
    std::array<std::string, 4> messages;
    std::vector<std::thread> waiters;
    for (auto& message : messages) {
        waiters.emplace_back([&message] { message = expectFailure([] { AgcDriverWaitIdle_nid_postfix(); }); });
    }
    for (auto& waiter : waiters) waiter.join();
    for (const auto& message : messages) check(message.find("required shader register") != std::string::npos, "worker failure was lost");
    check(expectFailure([&] { sceAgcDriverSubmitDcb(&packet); }) == messages[0], "subsequent DCB lost worker failure");
    check(expectFailure([&] { sceAgcDriverAgrSubmitDcb(&packet); }) == messages[0], "subsequent AGR lost worker failure");
    check(expectFailure([&] { sceAgcDriverSubmitAcb(0x20, &packet); }) == messages[0], "subsequent ACB lost worker failure");
}

}

int main(int argc, char** argv) {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (argc == 2 && std::string_view(argv[1]) == "--implicit-eop-only") {
            testImplicitSubmissionCompletions();
            testEndOfPipeInterrupts(true);
            testEndOfPipeLabelsWithoutWork();
            LibcRunShutdown_nid_postfix();
            std::puts("AGC implicit submission completion tests passed");
            return 0;
        }
        alignas(256) std::array<std::uint32_t, 64> rawCode{};
        rawCode.fill(0xbf800000);
        rawCode[0] = 0xbe8003ff;
        rawCode[1] = 0xbf810000;
        rawCode[2] = 0xbf810000;
        const auto rawAddress = reinterpret_cast<std::uintptr_t>(rawCode.data());
        const auto literal = AgcDriver::DriverDetail::ReadRawComputeShader(rawAddress);
        check(literal->code.size() == 3 && literal->header.empty(), "raw compute stopped at an instruction literal");
        check(AgcDriver::DriverDetail::ReadRawComputeShader(rawAddress) == literal, "unchanged raw compute code lost its snapshot identity");
        rawCode[0] = 0xbf820002;
        rawCode[1] = 0xbf810000;
        rawCode[2] = 0xbf800000;
        rawCode[3] = 0xbf810000;
        const auto branched = AgcDriver::DriverDetail::ReadRawComputeShader(rawAddress);
        check(branched->code.size() == 4 && literal->code[0] == 0xbe8003ff, "raw compute lost branch targets or modified an earlier snapshot");
        check(branched != literal, "changed raw compute code reused a stale snapshot");
        std::array<std::shared_ptr<const AgcDriver::DriverDetail::ShaderSnapshot>, 8> concurrent;
        std::vector<std::thread> readers;
        for (auto& snapshot : concurrent) readers.emplace_back([&snapshot, rawAddress] {
            snapshot = AgcDriver::DriverDetail::ReadRawComputeShader(rawAddress);
        });
        for (auto& reader : readers) reader.join();
        for (const auto& snapshot : concurrent) check(snapshot == branched, "concurrent raw compute readers lost snapshot reuse");
        alignas(256) std::array<std::array<std::uint32_t, 64>, 65> programs{};
        for (auto& program : programs) program[0] = 0xbf810000;
        const auto firstAddress = reinterpret_cast<std::uintptr_t>(programs.front().data());
        readers.clear();
        for (auto& snapshot : concurrent) readers.emplace_back([&snapshot, firstAddress] {
            snapshot = AgcDriver::DriverDetail::ReadRawComputeShader(firstAddress);
        });
        for (auto& reader : readers) reader.join();
        for (const auto& snapshot : concurrent) check(snapshot == concurrent.front(), "concurrent raw compute misses duplicated snapshots");
        const auto evicted = AgcDriver::DriverDetail::ReadRawComputeShader(firstAddress);
        for (std::size_t i = 1; i < programs.size(); ++i)
            AgcDriver::DriverDetail::ReadRawComputeShader(reinterpret_cast<std::uintptr_t>(programs[i].data()));
        check(AgcDriver::DriverDetail::ReadRawComputeShader(firstAddress) != evicted && evicted->code[0] == 0xbf810000,
            "raw compute cache eviction lost snapshot lifetime or exceeded its entry limit");
        check(!expectFailure([&] { AgcDriver::DriverDetail::ReadRawComputeShader(rawAddress + 4); }).empty(), "raw compute accepted a misaligned entry");
        check(!expectFailure([] { AgcDriver::DriverDetail::ReadRawComputeShader(0); }).empty(), "raw compute accepted an unmapped entry");
        alignas(256) std::array<std::uint32_t, 128> straddling{};
        straddling.fill(0xbf800000);
        straddling[63] = 0xf4000000;
        straddling[64] = 0xfa000000;
        straddling[65] = 0xbf810000;
        const auto straddled = AgcDriver::DriverDetail::ReadRawComputeShader(reinterpret_cast<std::uintptr_t>(straddling.data()));
        check(straddled->code.size() == 66 && straddled->code[64] == 0xfa000000, "raw compute stopped at a memory instruction across its first read window");
#ifdef _WIN32
        auto* mapping = static_cast<std::uint32_t*>(VirtualAlloc(nullptr, 8192, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        check(mapping != nullptr, "cannot allocate raw compute boundary test");
        DWORD protection = 0;
        check(VirtualProtect(mapping + 1024, 4096, PAGE_NOACCESS, &protection) != 0, "cannot protect raw compute boundary");
        auto* boundedCode = mapping + 1024 - 64;
        std::fill_n(boundedCode, 64, 0xbf800000u);
        const auto boundedAddress = reinterpret_cast<std::uintptr_t>(boundedCode);
        const auto unterminated = expectFailure([&] { AgcDriver::DriverDetail::ReadRawComputeShader(boundedAddress); });
        boundedCode[63] = 0xbf810000;
        const auto bounded = AgcDriver::DriverDetail::ReadRawComputeShader(boundedAddress);
        check(VirtualProtect(mapping + 1024, 4096, PAGE_READWRITE, &protection) != 0, "cannot extend raw compute mapping");
        boundedCode[63] = 0xbf800000;
        boundedCode[64] = 0xbf810000;
        const auto extended = AgcDriver::DriverDetail::ReadRawComputeShader(boundedAddress);
        check(extended != bounded && extended->code.size() == 65, "raw compute reused code before its end changed");
        check(VirtualProtect(mapping + 1024, 4096, PAGE_NOACCESS, &protection) != 0, "cannot revoke cached raw compute tail");
        check(!expectFailure([&] { AgcDriver::DriverDetail::ReadRawComputeShader(boundedAddress); }).empty(), "raw compute reused an inaccessible cached tail");
        check(VirtualProtect(mapping, 4096, PAGE_NOACCESS, &protection) != 0, "cannot revoke cached raw compute code");
        check(!expectFailure([&] { AgcDriver::DriverDetail::ReadRawComputeShader(boundedAddress); }).empty(), "raw compute reused inaccessible cached code");
        check(VirtualFree(mapping, 0, MEM_RELEASE) != 0, "cannot release raw compute boundary test");
        check(!unterminated.empty() && bounded->code.size() == 64, "raw compute crossed inaccessible memory or missed its last instruction");
#endif
        testEvents();
        testValidation();
        testClearState();
        testSubmissions();
        testEndOfPipeInterrupts();
        testLabelStoredSinceSubmission();
        testEndOfPipeLabelsWithoutWork();
        testLabelHeldAtSubmission();
        testWideLabelStoredSinceSubmission();
        testWaitFreeSubmissionAfterEarlierQueue0Work();
        testWaitFreeSubmissionQueue0WaitsOn();
        testMultiAcbs();
        testWaitFreeSubmissionBehindHeldOne();
        testWaitFreeSubmissionTheCpuWaitsFor();
        testMultiSubmissions();
        testShaderHeaderAlignment();
        testHeaderWithoutProgramAddress();
        testRegisteredFloatMode();
        testWorkerFailure();
        check(expectFailure([] { LibcRunShutdown_nid_postfix(); }).find("required shader register") != std::string::npos, "shutdown lost worker failure");
        std::puts("AGC driver submit tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try { LibcRunShutdown_nid_postfix(); }
        catch (const std::exception& shutdown) { std::fprintf(stderr, "shutdown: %s\n", shutdown.what()); }
        return 1;
    }
}
