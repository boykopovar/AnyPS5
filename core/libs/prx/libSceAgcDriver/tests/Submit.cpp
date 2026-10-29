#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "execution/VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/QueueState.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawCache.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/SynchronizationStatistics.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libc/include/GuestHeap.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include "prx/libSceAgcDriver/Eq/include/Query.hpp"
#include "prx/libSceAgcDriver/Eq/include/Event.hpp"
#include "prx/libSceAgcDriver/Execution/include/VideoOutput.hpp"
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
    check(sceAgcDriverSubmitAcb(0x58, &packet) == 0, "queue 0x58 submit failed");
    AgcDriverWaitIdle_nid_postfix();
    expectFailure([&] { sceAgcDriverSubmitAcb(0x59, &packet); });
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

void testThreadDimensionGroups() {
    using AgcDriver::DriverDetail::ThreadDimensionGroups;
    check(ThreadDimensionGroups(0, 64) == 0, "no threads need no groups");
    check(ThreadDimensionGroups(64, 64) == 1 && ThreadDimensionGroups(65, 64) == 2, "a partial group still counts");
    check(ThreadDimensionGroups(0xffffffffu, 1) == 0xffffffffu, "one thread per group keeps the thread count");
    check(ThreadDimensionGroups(0xffffffffu, 64) == 0x4000000u, "a thread count near 2^32 wrapped before the division");
}

void testReadOnlyResummarize() {
    std::vector<std::uint32_t> commands;
    for (const auto [offset, value] : {std::pair{0x000u, 0x10u}, {0x200u, 0x007007f3u}, {0x010u, 0xa0000183u}, {0x011u, 0x20000180u}, {0x8eu, 0xffffffffu}, {0x8fu, 0u}, {0x1c4u, 0u}, {0x203u, 0u}}) {
        commands.insert(commands.end(), {0xc0016900u, offset, value});
    }
    commands.insert(commands.end(), {0xc0012d00u, 3u, 2u});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    check(sceAgcDriverSubmitDcb(&packet) == 0, "read-only resummarize submission failed");
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

void testEndOfPipeInterrupts() {
    KernelEqueue eq = 0;
    check(sceKernelCreateEqueue(&eq, "AGC test") == 0, "event queue creation failed");
    auto owner = EqueuePin_nid_postfix(eq);
    int graphicsTag = 0;
    int computeTag = 0;
    check(sceAgcDriverAddEqEvent(eq, 0, &graphicsTag) == 0, "graphics event registration failed");
    check(sceAgcDriverAddEqEvent(eq, 0x20, &computeTag) == 0, "compute event registration failed");
    expectFailure([] { sceAgcDriverAddEqEvent(0, 0, nullptr); });
    struct Output final : AgcDriver::IVideoOutput, AgcDriver::IFlipRequest, std::enable_shared_from_this<Output> {
        std::atomic<int> ready{0};
        std::shared_ptr<AgcDriver::IFlipRequest> Reserve(const AgcDriver::FlipInfo&) override { return shared_from_this(); }
        void GpuReady(const std::shared_ptr<AgcDriver::FrameTiming>&) override { ready.fetch_add(1); }
        void Fail(std::exception_ptr error) noexcept override { if (!error) std::terminate(); }
    };
    const auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(7, output);
    std::array<std::uint32_t, 6> flip{AgcDriver::FlipPacketHeader, 7, 0xfffffffeu, 1, 0, 0};
    Packet flipPacket{flip.data(), static_cast<std::uint32_t>(flip.size()), 0, {}};
    std::array<KernelEvent, 2> events{};
    check(sceAgcDriverSubmitDcb(&flipPacket) == 0, "flip-only queue submit failed");
    AgcDriverWaitIdle_nid_postfix();
    check(output->ready.load() == 1, "flip-only queue flip was not handed to its output");
    check(AgcDriver::Graphics::HostImportFramesEnded() == 1, "the first flip did not end a host import frame");
    check(owner->GetTriggeredEvents(events.data(), 2) == 1 && events[0].udata == &graphicsTag && events[0].data == 1 && sceAgcDriverGetEqEventType(events.data()) == 0, "flip on a queue that never requested a release interrupt did not raise its event");
    check(owner->GetTriggeredEvents(events.data(), 2) == 0, "flip-only queue event was not cleared");
    std::array<std::uint32_t, 8> words{0xc0064900, 0, 1u << 24u, 0, 0, 0, 0, 0};
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    check(sceAgcDriverSubmitDcb(&packet) == 0 && sceAgcDriverSubmitDcb(&packet) == 0, "interrupt submit failed");
    AgcDriverWaitIdle_nid_postfix();
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
    check(owner->GetTriggeredEvents(events.data(), 2) == 0, "release without INT_SEL raised an interrupt");
    check(sceAgcDriverSubmitDcb(&flipPacket) == 0, "flip after a release interrupt submit failed");
    AgcDriverWaitIdle_nid_postfix();
    check(output->ready.load() == 2, "flip after a release interrupt was not handed to its output");
    check(AgcDriver::Graphics::HostImportFramesEnded() == 2, "a flip packet executed on the queue did not end a host import frame");
    check(owner->GetTriggeredEvents(events.data(), 2) == 0, "flip on a queue that already raised its own release interrupt raised a second event");
    words[2] = 1u << 24u;
    check(sceAgcDriverSubmitDcb(&packet) == 0, "release before sustained flips submit failed");
    AgcDriverWaitIdle_nid_postfix();
    check(owner->GetTriggeredEvents(events.data(), 2) == 1, "a release interrupt did not deliver on a queue whose flip event is suppressed");
    std::uint32_t sustainedFlips = 0;
    std::uint32_t flipsUntilEvent = 0;
    while (sustainedFlips < 8 && flipsUntilEvent == 0) {
        check(sceAgcDriverSubmitDcb(&flipPacket) == 0, "sustained flip submit failed");
        AgcDriverWaitIdle_nid_postfix();
        ++sustainedFlips;
        check(output->ready.load() == static_cast<int>(2 + sustainedFlips), "sustained flip was not handed to its output");
        if (owner->GetTriggeredEvents(events.data(), 2) != 0) flipsUntilEvent = sustainedFlips;
    }
    check(flipsUntilEvent != 0, "a flip never raised an event again on a queue whose own release interrupts stopped");
    check(flipsUntilEvent > 2, "a flip raised an event within two flips of the queue's own release interrupt");
    check(sceAgcDriverSubmitDcb(&flipPacket) == 0, "flip after the latch cleared submit failed");
    AgcDriverWaitIdle_nid_postfix();
    check(output->ready.load() == static_cast<int>(3 + sustainedFlips), "flip after the latch cleared was not handed to its output");
    check(owner->GetTriggeredEvents(events.data(), 2) == 1, "a flip did not raise an event after the latch cleared");
    words[2] = 1u << 24u;
    check(sceAgcDriverSubmitDcb(&packet) == 0, "release after a cleared latch submit failed");
    AgcDriverWaitIdle_nid_postfix();
    check(owner->GetTriggeredEvents(events.data(), 2) == 1, "a release interrupt did not deliver after the latch cleared");
    for (int index = 0; index < 2; ++index) {
        check(sceAgcDriverSubmitDcb(&flipPacket) == 0, "flip paired with a release interrupt submit failed");
        AgcDriverWaitIdle_nid_postfix();
        check(output->ready.load() == static_cast<int>(4 + sustainedFlips + index), "flip paired with a release interrupt was not handed to its output");
        check(owner->GetTriggeredEvents(events.data(), 2) == 0, "a flip raised an event within two flips of the queue's own release interrupt");
    }
    const auto framesBefore = AgcDriver::Graphics::HostImportFramesEnded();
    const auto readyBefore = output->ready.load();
    std::thread([] { AgcDriverSubmitFlip_nid_postfix(AgcDriver::FlipInfo{7, -1, 1, 0}); }).join();
    AgcDriverWaitIdle_nid_postfix();
    static_cast<void>(owner->GetTriggeredEvents(events.data(), 2));
    check(output->ready.load() == readyBefore + 1, "flip from a thread that never submitted queue 0 was not handed to its output");
    check(AgcDriver::Graphics::HostImportFramesEnded() == framesBefore + 1, "a CPU flip from a thread that never submitted queue 0 did not end a host import frame");
    AgcDriverUnregisterVideoOutput_nid_postfix(7, output);
    alignas(8) static volatile std::uint64_t label = 0;
    const auto labelAddress = reinterpret_cast<std::uintptr_t>(&label);
    words = {0xc0064900, 0x528, (3u << 29u) | (3u << 24u) | (1u << 16u), static_cast<std::uint32_t>(labelAddress), static_cast<std::uint32_t>(static_cast<std::uint64_t>(labelAddress) >> 32u), 0x89abcdefu, 0x01234567u, 0};
    check(sceAgcDriverSubmitDcb(&packet) == 0, "send-data release submit failed");
    AgcDriverWaitIdle_nid_postfix();
    check(label != 0, "send-data release did not write its label");
    check(owner->GetTriggeredEvents(events.data(), 2) == 0, "release with INT_SEL send data after write confirm raised an interrupt");
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
    alignas(64) static volatile std::uint32_t gate = 0, label = 0, done = 0, late = 0, waiting = 0;
    submit(0x20, commands(waitEqual(&gate, 1), waitEqual(&label, 1), writeData(&done, 1)));
    submit(0, commands(writeData(&label, 1)));
    waitFor(&label, 1, "producer label never landed");
    label = 0;
    gate = 1;
    check(waitFor(&done, 1, "consumer never passed its waits") < std::chrono::milliseconds(500), "a label stored after the wait's submission did not satisfy it");
    AgcDriverWaitIdle_nid_postfix();
    submit(0x20, commands(writeData(&waiting, 1), waitEqual(&label, 1), writeData(&late, 1)));
    waitFor(&waiting, 1, "consumer never reached its label wait");
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    check(late == 0, "a label stored before the wait's submission satisfied it or the wait expired");
    submit(0, commands(writeData(&label, 1)));
    waitFor(&late, 1, "consumer did not resume after a new producer label");
    AgcDriverWaitIdle_nid_postfix();
}

void testWideLabelStoredSinceSubmission() {
    alignas(64) static volatile std::uint32_t gate = 0, done = 0, late = 0, waiting = 0;
    alignas(64) static volatile std::uint32_t label[2] = {0, 0x5eed};
    submit(0x20, commands(waitEqual(&gate, 1), waitEqual64(label, 1, 0xffffffffu), writeData(&done, 1)));
    submit(0, commands(writeData(label, 1)));
    waitFor(label, 1, "producer label never landed");
    label[0] = 0;
    gate = 1;
    check(waitFor(&done, 1, "consumer never passed its waits") < std::chrono::milliseconds(500), "a 32-bit label stored after a low-dword 64-bit wait's submission did not satisfy it");
    AgcDriverWaitIdle_nid_postfix();
    gate = 0;
    submit(0x20, commands(waitEqual(&gate, 1), writeData(&waiting, 1), waitEqual64(label, 1, ~0ull), writeData(&late, 1)));
    submit(0, commands(writeData(label, 1)));
    waitFor(label, 1, "producer label never landed");
    label[0] = 0;
    gate = 1;
    waitFor(&waiting, 1, "consumer never reached its wide label wait");
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    check(late == 0, "a mismatched high dword satisfied the 64-bit wait or the wait expired");
    submit(0, commands(writeData(label + 1, 0), writeData(label, 1)));
    waitFor(&late, 1, "consumer did not resume after a matching 64-bit label");
    AgcDriverWaitIdle_nid_postfix();
}

void testLabelHeldAtSubmission() {
    alignas(64) static volatile std::uint32_t gate = 0, label = 1, done = 0, reset = 0, waiting = 0;
    submit(0x20, commands(waitEqual(&gate, 1), waitEqual(&label, 1), writeData(&done, 1)));
    label = 0;
    gate = 1;
    check(waitFor(&done, 1, "consumer never passed its waits") < std::chrono::milliseconds(500), "a label held when the wait was submitted did not satisfy it");
    AgcDriverWaitIdle_nid_postfix();
    gate = 0;
    label = 1;
    submit(0x20, commands(waitEqual(&gate, 1), writeData(&label, 0), writeData(&waiting, 1), waitEqual(&label, 1), writeData(&reset, 1)));
    gate = 1;
    waitFor(&waiting, 1, "consumer never reached its reset label wait");
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    check(reset == 0, "a label reset by its own queue satisfied the wait or the wait expired");
    submit(0x28, commands(writeData(&label, 1)));
    waitFor(&reset, 1, "consumer did not resume after another queue restored the label");
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

void testQueue0SubmissionAfterEarlierWaitFreeWork() {
    alignas(64) static volatile std::uint32_t first = 0, second = 0;
    constexpr std::uint32_t writes = 20000;
    std::vector<std::uint32_t> words;
    for (std::uint32_t i = 1; i <= writes; ++i) {
        const auto write = writeData(&first, i);
        words.insert(words.end(), write.begin(), write.end());
    }
    submit(0x20, words);
    submit(0, commands(writeData(&second, 1)));
    waitFor(&second, 1, "queue 0's submission never ran");
    check(first == writes, "queue 0 ran a submission before another queue's earlier wait-free submission finished");
    AgcDriverWaitIdle_nid_postfix();
}

void testQueue0SubmissionBehindWaitingQueue() {
    alignas(64) static volatile std::uint32_t label = 0, other = 0, done = 0;
    submit(0x20, commands(waitEqual(&label, 1)));
    submit(0x20, commands(writeData(&other, 1)));
    submit(0, commands(writeData(&label, 1), writeData(&done, 1)));
    check(waitFor(&done, 1, "queue 0 never ran while another queue waited on it") < std::chrono::milliseconds(500), "queue 0 waited for a wait-free submission queued behind another queue's wait on queue 0");
    waitFor(&other, 1, "the wait-free submission behind the satisfied wait never ran");
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
    std::array<std::uint32_t*, 1> nullAcbs{nullptr};
    std::array<std::uint32_t, 1> nonZeroAcbSizes{4};
    expectFailure([&] { sceAgcDriverSubmitMultiAcbs(0x20, nullAcbs.data(), nonZeroAcbSizes.data(), 1); });
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
    std::array<std::uint32_t*, 1> nullAddresses{nullptr};
    std::array<std::uint32_t, 1> nonZeroSizes{4};
    expectFailure([&] { sceAgcDriverSubmitMultiDcbs(nullAddresses.data(), nonZeroSizes.data(), 1); });
    expectFailure([&] { sceAgcDriverAgrSubmitMultiDcbs(nullAddresses.data(), nonZeroSizes.data(), 1); });
    expectFailure([&] { sceAgcDriverSubmitMultiDcbs(invalidAddresses.data(), invalidSizes.data(), std::numeric_limits<std::uint32_t>::max()); });
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

std::array<std::uint32_t, 7> memoryCopy(const void* destination, const void* source, std::uint32_t bytes) {
    const auto from = reinterpret_cast<std::uintptr_t>(source), to = reinterpret_cast<std::uintptr_t>(destination);
    return {0xc0055000, 0x60000000, static_cast<std::uint32_t>(from), static_cast<std::uint32_t>(static_cast<std::uint64_t>(from) >> 32u), static_cast<std::uint32_t>(to), static_cast<std::uint32_t>(static_cast<std::uint64_t>(to) >> 32u), bytes};
}

std::array<std::uint32_t, 7> dataFill(const void* destination, std::uint32_t value, std::uint32_t bytes) {
    const auto to = reinterpret_cast<std::uintptr_t>(destination);
    return {0xc0055000, 0x40300000, value, 0, static_cast<std::uint32_t>(to), static_cast<std::uint32_t>(static_cast<std::uint64_t>(to) >> 32u), bytes};
}

void testLargeDataFill() {
    alignas(64) static volatile std::uint32_t filled = 0;
    constexpr std::size_t words = 65536;
    constexpr auto bytes = static_cast<std::uint32_t>(words * 4);
    constexpr std::uint32_t value = 0x5a5a5a5au;
    auto* produced = static_cast<std::uint32_t*>(GuestHeap::GuestHeapAlignForGpu_nid_postfix(65536, bytes));
    auto* destination = static_cast<std::uint32_t*>(GuestHeap::GuestHeapAlignForGpu_nid_postfix(65536, bytes));
    for (std::size_t i = 0; i < words; ++i) {
        produced[i] = 0x96000000u | static_cast<std::uint32_t>(i);
        destination[i] = 0;
    }
    const auto drainsBefore = AgcDriver::DriverDetail::storesDrained.load();
    const auto fillsBefore = AgcDriver::DriverDetail::dmaFills.load();
    submit(0, commands(memoryCopy(destination, produced, bytes), dataFill(destination, value, bytes), writeData(&filled, 1)));
    waitFor(&filled, 1, "the label after a DMA_DATA fill never landed");
    AgcDriverWaitIdle_nid_postfix();
    check(std::all_of(destination, destination + words, [](std::uint32_t word) { return word == value; }), "a DMA_DATA fill over a GPU copy's destination lost the fill or ran before the copy");
    check(AgcDriver::DriverDetail::storesDrained.load() == drainsBefore, "a DMA_DATA fill larger than 64 KiB drained the device");
    check(AgcDriver::DriverDetail::dmaFills.load() == fillsBefore + 1, "a DMA_DATA fill larger than 64 KiB was not stored as a buffer fill");
    GuestHeap::GuestHeapFree_nid_postfix(destination);
    GuestHeap::GuestHeapFree_nid_postfix(produced);
}

void testCopyIntoHostMemory() {
    constexpr std::size_t words = 32768;
    constexpr auto bytes = static_cast<std::uint32_t>(words * 4);
    auto* produced = static_cast<std::uint32_t*>(GuestHeap::GuestHeapAlignForGpu_nid_postfix(65536, bytes));
    auto* source = static_cast<std::uint32_t*>(GuestHeap::GuestHeapAlignForGpu_nid_postfix(65536, bytes));
    std::vector<std::uint32_t> destination(words);
    for (const bool label : {false, true}) {
        for (std::size_t i = 0; i < words; ++i) {
            produced[i] = 0xa5000000u | static_cast<std::uint32_t>(i);
            source[i] = 0x5b000000u | static_cast<std::uint32_t>(i);
        }
        std::fill(destination.begin(), destination.end(), 0u);
        const auto copiesBefore = AgcDriver::DriverDetail::copiesToHostMemory.load();
        if (label) submit(0, commands(memoryCopy(source, produced, bytes), writeData(destination.data(), 0x1abe1u), memoryCopy(destination.data(), source, bytes)));
        else submit(0, commands(memoryCopy(source, produced, bytes), memoryCopy(destination.data(), source, bytes)));
        AgcDriverWaitIdle_nid_postfix();
        check(std::equal(destination.begin(), destination.end(), produced), label ? "a copy into host memory behind a label into its destination lost bytes" : "a copy into host memory did not see the GPU copy into its source");
        check(AgcDriver::DriverDetail::copiesToHostMemory.load() == copiesBefore + 1, "a copy into host memory drained instead of syncing its source");
    }
    GuestHeap::GuestHeapFree_nid_postfix(source);
    GuestHeap::GuestHeapFree_nid_postfix(produced);
}

void testSmallCopyBehindPendingSource() {
    alignas(64) static volatile std::uint32_t copied = 0, settledCopied = 0, hostCopied = 0;
    constexpr std::size_t words = 32768;
    constexpr auto bytes = static_cast<std::uint32_t>(words * 4);
    constexpr std::size_t smallWords = 1024;
    constexpr auto smallBytes = static_cast<std::uint32_t>(smallWords * 4);
    auto* produced = static_cast<std::uint32_t*>(GuestHeap::GuestHeapAlignForGpu_nid_postfix(65536, bytes));
    auto* source = static_cast<std::uint32_t*>(GuestHeap::GuestHeapAlignForGpu_nid_postfix(65536, bytes));
    auto* destination = static_cast<std::uint32_t*>(GuestHeap::GuestHeapAlignForGpu_nid_postfix(65536, smallBytes));
    for (std::size_t i = 0; i < words; ++i) {
        produced[i] = 0xc3000000u | static_cast<std::uint32_t>(i);
        source[i] = 0x3c000000u | static_cast<std::uint32_t>(i);
    }
    std::fill(destination, destination + smallWords, 0u);
    const auto before = AgcDriver::DriverDetail::copiesBehindPendingSources.load();
    submit(0, commands(memoryCopy(source, produced, bytes), memoryCopy(destination, source, smallBytes), writeData(&copied, 1)));
    waitFor(&copied, 1, "the label after a small copy behind a pending source never landed");
    AgcDriverWaitIdle_nid_postfix();
    check(std::equal(destination, destination + smallWords, produced), "a small copy did not see the GPU copy into its source before it");
    check(AgcDriver::DriverDetail::copiesBehindPendingSources.load() == before, "a small copy queued behind the pipeline write into its source was recorded inline");
    std::fill(destination, destination + smallWords, 0u);
    const auto settled = AgcDriver::DriverDetail::copiesBehindPendingSources.load();
    submit(0, commands(memoryCopy(destination, produced, smallBytes), writeData(&settledCopied, 1)));
    waitFor(&settledCopied, 1, "the label after a small copy of a settled source never landed");
    AgcDriverWaitIdle_nid_postfix();
    check(std::equal(destination, destination + smallWords, produced), "a small copy of a settled source lost bytes");
    check(AgcDriver::DriverDetail::copiesBehindPendingSources.load() == settled, "a small copy of a settled source was recorded on the GPU");
    std::vector<std::uint32_t> hostDestination(smallWords);
    for (std::size_t i = 0; i < words; ++i) source[i] = 0x3c000000u | static_cast<std::uint32_t>(i);
    const auto hostCopies = AgcDriver::DriverDetail::copiesBehindPendingSources.load();
    const auto aliasChecks = AgcDriver::VulkanDevice::CopyAliasCounts().noSource;
    submit(0, commands(memoryCopy(source, produced, bytes), memoryCopy(hostDestination.data(), source, smallBytes), writeData(&hostCopied, 1)));
    waitFor(&hostCopied, 1, "the label after a small copy into memory that is not imported never landed");
    AgcDriverWaitIdle_nid_postfix();
    check(std::equal(hostDestination.begin(), hostDestination.end(), produced), "a small copy into memory that is not imported did not see the GPU copy into its source");
    check(AgcDriver::DriverDetail::copiesBehindPendingSources.load() == hostCopies, "a small copy into memory that is not imported was recorded on the GPU");
    check(AgcDriver::VulkanDevice::CopyAliasCounts().noSource == aliasChecks + 1, "a small copy into memory that is not imported went through the GPU copy before its CPU path");
    GuestHeap::GuestHeapFree_nid_postfix(destination);
    GuestHeap::GuestHeapFree_nid_postfix(source);
    GuestHeap::GuestHeapFree_nid_postfix(produced);
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

void testDispatchFloatMode() {
    using AgcDriver::DriverDetail::DispatchFloatMode;
    using AgcDriver::DriverDetail::ShaderSnapshot;
    using AgcDriver::DriverDetail::RegisteredShaderState;
    const AgcDriver::Registers live{{0x212, (0xc0u << 12u) | (1u << 21u) | (1u << 26u)}};
    ShaderSnapshot raw{0x20000, 0, 0, {}, {}};
    const auto fromLive = DispatchFloatMode(raw, live);
    check(fromLive.has_value() && fromLive->floatMode == 0xc0u && fromLive->dx10Clamp && !fromLive->ieeeMode && fromLive->fp16Overflow, "an unregistered compute shader does not take its float mode from the live COMPUTE_PGM_RSRC1");
    check(!DispatchFloatMode(raw, AgcDriver::Registers{}).has_value(), "an unregistered compute shader without a live COMPUTE_PGM_RSRC1 has a float mode");
    auto state = std::make_shared<RegisteredShaderState>();
    state->shader.emplace(0x212, 1u << 23u);
    ShaderSnapshot registered{0x20000, 0, 0, {}, {}};
    registered.registeredState = state;
    const auto fromRegistered = DispatchFloatMode(registered, live);
    check(fromRegistered.has_value() && fromRegistered->ieeeMode && fromRegistered->floatMode == 0u, "a registered compute shader takes its float mode from the live registers");
    registered.registeredState = std::make_shared<RegisteredShaderState>();
    check(!DispatchFloatMode(registered, live).has_value(), "a registered compute shader without RSRC1 takes its float mode from the live registers");
}

void testProgramSnapshots() {
    alignas(256) static std::array<std::uint32_t, 64> registered{};
    registered.fill(0xbf800000);
    registered[0] = 0xbf810000;
    alignas(256) static std::array<std::uint32_t, 64> raw{};
    raw.fill(0xbf800000);
    raw[1] = 0xbf810000;
    const auto registeredAddress = reinterpret_cast<std::uintptr_t>(registered.data());
    const auto rawAddress = reinterpret_cast<std::uintptr_t>(raw.data());
    AgcDriver::DriverDetail::ShaderRegistry shaders;
    AgcDriver::DriverDetail::ShaderSnapshot snapshot{registeredAddress, 0x1000, 2, {}, {}};
    snapshot.code.assign(registered.begin(), registered.end());
    snapshot.header.resize(sizeof(Shader));
    const auto entry = std::make_shared<const AgcDriver::DriverDetail::ShaderSnapshot>(std::move(snapshot));
    shaders[registeredAddress] = {entry};
    check(AgcDriver::DriverDetail::ProgramSnapshot(shaders, registeredAddress, {2}) == entry, "a registered program was not resolved to its registration");
    check(AgcDriver::DriverDetail::ProgramSnapshot(shaders, registeredAddress + 4 * 8, {2}) == entry, "an entry inside registered code was not resolved to its registration");
    const auto unregistered = AgcDriver::DriverDetail::ProgramSnapshot(shaders, rawAddress, {2});
    check(unregistered != entry && unregistered->codeAddress == rawAddress && unregistered->header.empty() && unregistered->code.size() == 2, "an unregistered program beside registered shaders was not read as raw code");
    check(AgcDriver::DriverDetail::ProgramSnapshot({}, rawAddress, {2}) == unregistered, "an empty registry did not read the program as raw code");
    check(!expectFailure([&] { static_cast<void>(AgcDriver::DriverDetail::ProgramSnapshot(shaders, registeredAddress, {1})); }).empty(), "a registered program was accepted as another shader type");
    check(AgcDriver::DriverDetail::ProgramSnapshot({}, rawAddress, {2}) == unregistered, "unchanged raw code lost its snapshot identity");
    check(!expectFailure([] { static_cast<void>(AgcDriver::DriverDetail::ProgramSnapshot({}, 0, {2})); }).empty(), "an unmapped unregistered program was accepted");
    AgcDriver::DriverDetail::DrawDecode decode{};
    decode.programs.resize(2);
    decode.programs[0].snapshot = unregistered;
    decode.programs[1].snapshot = entry;
    decode.programs[1].codeOffset = 8;
    check(AgcDriver::DriverDetail::DecodeProgramsCurrent(decode, shaders), "an unchanged decode was not current");
    registered[8] = 0xbf810000;
    check(AgcDriver::DriverDetail::DecodeProgramsCurrent(decode, shaders), "a rewritten registered program made a decode stale");
    raw[0] = 0xbf810000;
    check(!AgcDriver::DriverDetail::DecodeProgramsCurrent(decode, shaders), "a rewritten unregistered program left its decode current");
    decode.programs[0].snapshot = AgcDriver::DriverDetail::ProgramSnapshot({}, rawAddress, {2});
    check(AgcDriver::DriverDetail::DecodeProgramsCurrent(decode, shaders), "a decode with the rewritten program was not current");
}

void testRawGraphicsRanges() {
    using AgcDriver::DriverDetail::RawGraphicsDrawnIn;
    constexpr std::uint64_t base = 0x7e0000000000ull;
    const auto note = [](std::uint64_t address, std::size_t dwords) {
        AgcDriver::DriverDetail::ShaderSnapshot snapshot{address, 0, 0, std::vector<std::uint32_t>(dwords, 0xbf810000u), {}};
        AgcDriver::DriverDetail::NoteRawGraphicsDraw(snapshot);
        check(snapshot.rawGraphicsNoted->load(), "a raw graphics program was not marked as noted");
        AgcDriver::DriverDetail::NoteRawGraphicsDraw(snapshot);
    };
    note(base + 0x100, 0x10);
    note(base, 0x100);
    note(base + 0x3f0, 0x8);
    note(base + 0x800, 0x1);
    check(RawGraphicsDrawnIn(base + 0x200, base + 0x210), "code inside a raw program that contains a smaller one was not reported");
    check(RawGraphicsDrawnIn(base + 0x40c, base + 0x500), "the end of a raw program that overlaps another was not reported");
    check(!RawGraphicsDrawnIn(base + 0x410, base + 0x800), "code between raw programs was reported");
    check(!RawGraphicsDrawnIn(base - 4, base), "code before a raw program was reported");
    check(RawGraphicsDrawnIn(base + 0x700, base + 0x900), "a range covering a raw program was not reported");
    check(!RawGraphicsDrawnIn(base + 0x804, base + 0x808) && !RawGraphicsDrawnIn(base + 0x200, base + 0x200), "code after a raw program or an empty range was reported");
}

void testProgramWorkgroupUse() {
    const auto snapshot = [](std::vector<std::uint32_t> code) {
        AgcDriver::DriverDetail::ShaderSnapshot result{0x1000, 0, 2, std::move(code), {}};
        return result;
    };
    const auto plain = snapshot({0xbf800000, 0xbf810000});
    check(!AgcDriver::DriverDetail::ProgramUsesWorkgroup(plain, 0) && !AgcDriver::DriverDetail::ProgramUsesWorkgroup(plain, 0), "a program without LDS or barriers needs a workgroup");
    const auto barrier = snapshot({0xbf8a0000, 0xbf810000});
    check(AgcDriver::DriverDetail::ProgramUsesWorkgroup(barrier, 0), "an s_barrier did not need a workgroup");
    const auto lds = snapshot({0xd8340000, 0x00000100, 0xbf810000});
    check(AgcDriver::DriverDetail::ProgramUsesWorkgroup(lds, 0) && AgcDriver::DriverDetail::ProgramUsesWorkgroup(lds, 0), "an LDS write did not need a workgroup");
    const auto entry = snapshot({0xbf8a0000, 0xbf810000, 0xbf800000, 0xbf810000});
    check(!AgcDriver::DriverDetail::ProgramUsesWorkgroup(entry, 2) && AgcDriver::DriverDetail::ProgramUsesWorkgroup(entry, 0), "the workgroup check did not start at the program's entry");
}

void testVertexTailSnapshots() {
    using namespace AgcDriver::DriverDetail;
    alignas(256) static std::array<std::uint32_t, 64> prolog{0xbe802008u, 0xbf9f0000u};
    alignas(256) static std::array<std::uint32_t, 64> continuation{0xbf800000u, 0xbf810000u};
    const auto address = reinterpret_cast<std::uintptr_t>(prolog.data());
    const auto next = reinterpret_cast<std::uintptr_t>(continuation.data());
    const std::array<std::uint32_t, 2> data{static_cast<std::uint32_t>(next), static_cast<std::uint32_t>(next >> 32u)};
    const auto source = ReadRawShader(address);
    check(source->userDataTailRegister == 8u, "raw tail metadata is missing");
    const auto target = ReadUserDataTailContinuation(source->code, 8u, data);
    check(target && target->codeAddress == next, "tail did not read its user-data continuation");
    DrawDecode decode{};
    decode.programs.resize(1);
    decode.programs[0].snapshot = source;
    decode.programs[0].tailContinuation = target;
    check(DecodeProgramsCurrent(decode, {}), "unchanged tail snapshot became stale");
    continuation[0] = 0xbf8a0000u;
    check(!DecodeProgramsCurrent(decode, {}), "rewriting the continuation did not invalidate the draw cache");
    AgcDriver::QueueState queue{};
    queue.context[0x2d5] = 0x2000u;
    queue.shader = {{0xc8, static_cast<std::uint32_t>(address >> 8u)}, {0xc9, static_cast<std::uint32_t>(address >> 40u)},
        {0x8b, 4u}, {0x8c, data[0]}, {0x8d, data[1]}};
    check(VertexWorkgroupRequired(queue, {}), "a barrier in the continuation was hidden from workgroup routing");
    queue.context[0x2d5] |= 0x02000000u;
    check(!VertexWorkgroupRequired(queue, {}), "a passthrough program was rerouted to a workgroup");
    const std::array<std::uint32_t, 2> nullAddress{};
    check(expectFailure([&] { static_cast<void>(ReadUserDataTailContinuation(source->code, 8u, nullAddress)); }).find("address is null") != std::string::npos, "null tail address was accepted");
    const std::array<std::uint32_t, 2> unaligned{data[0] + 1u, data[1]};
    check(expectFailure([&] { static_cast<void>(ReadUserDataTailContinuation(source->code, 8u, unaligned)); }).find("dword aligned") != std::string::npos, "unaligned tail address was accepted");
    const std::array<std::uint32_t, 2> unmapped{0x1000u, 0u};
    check(!expectFailure([&] { static_cast<void>(ReadUserDataTailContinuation(source->code, 8u, unmapped)); }).empty(), "unmapped tail address was accepted");
}

void testRegisteredVertexWorkgroupRouting() {
    using namespace AgcDriver::DriverDetail;
    ShaderSnapshot snapshot{0x20000, 0x30000, 2, {0xd8340000u, 0x00000100u, 0xbf810000u, 0xbf810000u}, {}};
    snapshot.header.resize(sizeof(Shader));
    const ShaderRegistry registry{{snapshot.codeAddress, {std::make_shared<const ShaderSnapshot>(std::move(snapshot))}}};
    AgcDriver::QueueState queue{};
    queue.shader = {{0xc8, 0x200}, {0xc9, 0}};
    queue.context[0x2d5] = 0x2000;
    check(VertexWorkgroupRequired(queue, registry), "a registered NGG vertex program using LDS did not need a workgroup");
    for (const auto routing : {0u, 0x2020u, 0x2004u, 0x02002000u}) {
        queue.context[0x2d5] = routing;
        check(!VertexWorkgroupRequired(queue, registry), "legacy, geometry, tessellation or passthrough routing changed to an NGG vertex workgroup");
    }
    queue.context[0x2d5] = 0x2000;
    auto unaligned = registry;
    const auto aligned = unaligned.begin()->second.front();
    ShaderSnapshot shifted{0x1ffff, 0x30000, 2, aligned->code, aligned->header};
    unaligned.clear();
    unaligned[shifted.codeAddress].push_back(std::make_shared<const ShaderSnapshot>(std::move(shifted)));
    check(expectFailure([&] { static_cast<void>(VertexWorkgroupRequired(queue, unaligned)); }).find("not dword aligned") != std::string::npos, "an unaligned registered entry was accepted");
    queue.shader[0xc9] = 0x100;
    check(expectFailure([&] { static_cast<void>(VertexWorkgroupRequired(queue, registry)); }).find("reserved graphics program address") != std::string::npos, "reserved NGG program address bits were accepted");
}

void testRegisteredFastLaunchWorkgroupVertex() {
    alignas(256) static const std::array<std::uint32_t, 6> code{
        0xd8340000u, 0x00000100u, 0xbf8a0000u, 0xf80008cfu, 0x00000000u, 0xbf810000u,
    };
    struct Header {
        Shader shader{};
        ShaderUserData userData{};
        std::array<ShaderRegister, 4> registers{};
    };
    alignas(8) static Header header{};
    const auto address = reinterpret_cast<std::uintptr_t>(code.data());
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18u;
    header.shader.header_size = sizeof(Header);
    header.shader.shader_size = sizeof(code);
    header.shader.code = code.data();
    header.shader.type = 2u;
    header.shader.user_data = &header.userData;
    header.registers = {{{0x0c8u, static_cast<std::uint32_t>(address >> 8u)}, {0x0c9u, static_cast<std::uint32_t>(address >> 40u)}, {0x08au, 0x022c0245u}, {0x08bu, 0x0009002au}}};
    header.shader.sh_registers = header.registers.data();
    header.shader.num_sh_registers = header.registers.size();
    AgcDriverRegisterShader_nid_postfix(&header.shader);
    const std::array<const Shader*, 1> stages{&header.shader};
    const std::array<ShaderRegister, 6> context{{
        {0x2d5u, 0x00482030u}, {0x1ffu, 32u}, {0x2ceu, 32u}, {0x2d3u, 64u}, {0x29bu, 2u}, {0x2abu, 4u},
    }};
    const std::array<ShaderRegister, 2> primitive{{{0x242u, 1u}, {0x25bu, 0x201u}}};
    AgcDriverResolveGraphicsStagesAbi_nid_postfix(stages, context, primitive);
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
        if (argc == 2 && std::string_view(argv[1]) == "--eop-interrupts") {
            testEndOfPipeInterrupts();
            LibcRunShutdown_nid_postfix();
            std::puts("AGC end-of-pipe interrupt queue mode tests passed");
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--readonly-resummarize") {
            testReadOnlyResummarize();
            LibcRunShutdown_nid_postfix();
            std::puts("Read-only resummarize submission passed");
            return 0;
        }
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (argc == 2 && std::string_view(argv[1]) == "--small-copy-sync") {
            testHeaderWithoutProgramAddress();
            testSmallCopyBehindPendingSource();
            LibcRunShutdown_nid_postfix();
            std::puts("Small copies with APS5_COPY_SYNC passed");
            return 0;
        }
        alignas(256) std::array<std::uint32_t, 64> rawCode{};
        rawCode.fill(0xbf800000);
        rawCode[0] = 0xbe8003ff;
        rawCode[1] = 0xbf810000;
        rawCode[2] = 0xbf810000;
        const auto rawAddress = reinterpret_cast<std::uintptr_t>(rawCode.data());
        const auto literal = AgcDriver::DriverDetail::ReadRawShader(rawAddress);
        check(literal->code.size() == 3 && literal->header.empty(), "raw compute stopped at an instruction literal");
        check(AgcDriver::DriverDetail::ReadRawShader(rawAddress) == literal, "unchanged raw compute code lost its snapshot identity");
        rawCode[0] = 0xbf820002;
        rawCode[1] = 0xbf810000;
        rawCode[2] = 0xbf800000;
        rawCode[3] = 0xbf810000;
        const auto branched = AgcDriver::DriverDetail::ReadRawShader(rawAddress);
        check(branched->code.size() == 4 && literal->code[0] == 0xbe8003ff, "raw compute lost branch targets or modified an earlier snapshot");
        check(branched != literal, "changed raw compute code reused a stale snapshot");
        std::array<std::shared_ptr<const AgcDriver::DriverDetail::ShaderSnapshot>, 8> concurrent;
        std::vector<std::thread> readers;
        for (auto& snapshot : concurrent) readers.emplace_back([&snapshot, rawAddress] {
            snapshot = AgcDriver::DriverDetail::ReadRawShader(rawAddress);
        });
        for (auto& reader : readers) reader.join();
        for (const auto& snapshot : concurrent) check(snapshot == branched, "concurrent raw compute readers lost snapshot reuse");
        struct alignas(256) RawProgram : std::array<std::uint32_t, 64> {};
        std::vector<RawProgram> programs(4097);
        for (auto& program : programs) program[0] = 0xbf810000;
        const auto firstAddress = reinterpret_cast<std::uintptr_t>(programs.front().data());
        readers.clear();
        for (auto& snapshot : concurrent) readers.emplace_back([&snapshot, firstAddress] {
            snapshot = AgcDriver::DriverDetail::ReadRawShader(firstAddress);
        });
        for (auto& reader : readers) reader.join();
        for (const auto& snapshot : concurrent) check(snapshot == concurrent.front(), "concurrent raw compute misses duplicated snapshots");
        const auto evicted = AgcDriver::DriverDetail::ReadRawShader(firstAddress);
        for (std::size_t i = 1; i <= 400; ++i)
            AgcDriver::DriverDetail::ReadRawShader(reinterpret_cast<std::uintptr_t>(programs[i].data()));
        check(AgcDriver::DriverDetail::ReadRawShader(firstAddress) == evicted, "raw shader cache evicted a program while a title used only 401");
        for (std::size_t i = 1; i < programs.size(); ++i)
            AgcDriver::DriverDetail::ReadRawShader(reinterpret_cast<std::uintptr_t>(programs[i].data()));
        check(AgcDriver::DriverDetail::ReadRawShader(firstAddress) != evicted && evicted->code[0] == 0xbf810000,
            "raw compute cache eviction lost snapshot lifetime or exceeded its entry limit");
        check(!expectFailure([&] { AgcDriver::DriverDetail::ReadRawShader(rawAddress + 4); }).empty(), "raw compute accepted a misaligned entry");
        check(!expectFailure([] { AgcDriver::DriverDetail::ReadRawShader(0); }).empty(), "raw compute accepted an unmapped entry");
        alignas(256) std::array<std::uint32_t, 128> straddling{};
        straddling.fill(0xbf800000);
        straddling[63] = 0xf4000000;
        straddling[64] = 0xfa000000;
        straddling[65] = 0xbf810000;
        const auto straddled = AgcDriver::DriverDetail::ReadRawShader(reinterpret_cast<std::uintptr_t>(straddling.data()));
        check(straddled->code.size() == 66 && straddled->code[64] == 0xfa000000, "raw compute stopped at a memory instruction across its first read window");
#ifdef _WIN32
        auto* mapping = static_cast<std::uint32_t*>(VirtualAlloc(nullptr, 8192, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        check(mapping != nullptr, "cannot allocate raw compute boundary test");
        DWORD protection = 0;
        check(VirtualProtect(mapping + 1024, 4096, PAGE_NOACCESS, &protection) != 0, "cannot protect raw compute boundary");
        auto* boundedCode = mapping + 1024 - 64;
        std::fill_n(boundedCode, 64, 0xbf800000u);
        const auto boundedAddress = reinterpret_cast<std::uintptr_t>(boundedCode);
        const auto unterminated = expectFailure([&] { AgcDriver::DriverDetail::ReadRawShader(boundedAddress); });
        boundedCode[63] = 0xbf810000;
        const auto bounded = AgcDriver::DriverDetail::ReadRawShader(boundedAddress);
        check(VirtualProtect(mapping + 1024, 4096, PAGE_READWRITE, &protection) != 0, "cannot extend raw compute mapping");
        boundedCode[63] = 0xbf800000;
        boundedCode[64] = 0xbf810000;
        const auto extended = AgcDriver::DriverDetail::ReadRawShader(boundedAddress);
        check(extended != bounded && extended->code.size() == 65, "raw compute reused code before its end changed");
        check(VirtualProtect(mapping + 1024, 4096, PAGE_NOACCESS, &protection) != 0, "cannot revoke cached raw compute tail");
        check(!expectFailure([&] { AgcDriver::DriverDetail::ReadRawShader(boundedAddress); }).empty(), "raw compute reused an inaccessible cached tail");
        check(VirtualProtect(mapping, 4096, PAGE_NOACCESS, &protection) != 0, "cannot revoke cached raw compute code");
        check(!expectFailure([&] { AgcDriver::DriverDetail::ReadRawShader(boundedAddress); }).empty(), "raw compute reused inaccessible cached code");
        check(VirtualFree(mapping, 0, MEM_RELEASE) != 0, "cannot release raw compute boundary test");
        check(!unterminated.empty() && bounded->code.size() == 64, "raw compute crossed inaccessible memory or missed its last instruction");
#endif
        testProgramSnapshots();
        testRawGraphicsRanges();
        testProgramWorkgroupUse();
        testVertexTailSnapshots();
        testRegisteredVertexWorkgroupRouting();
        testRegisteredFastLaunchWorkgroupVertex();
        testEvents();
        testValidation();
        testThreadDimensionGroups();
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
        testQueue0SubmissionAfterEarlierWaitFreeWork();
        testQueue0SubmissionBehindWaitingQueue();
        testMultiSubmissions();
        testShaderHeaderAlignment();
        testHeaderWithoutProgramAddress();
        testCopyIntoHostMemory();
        testLargeDataFill();
        testSmallCopyBehindPendingSource();
        testRegisteredFloatMode();
        testDispatchFloatMode();
        testReadOnlyResummarize();
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
