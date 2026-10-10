#include <Testing/Test.hpp>
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "../execution/VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/QueueState.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include "prx/libSceAgcDriver/Eq/include/Query.hpp"
#include "prx/libSceAgcDriver/Eq/include/Event.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <source_location>
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

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;
using Testing::RequireThrows;
using AgcDriver::DriverDetail::ReadRawComputeShader;

void RequireDevice() {
    static_cast<void>(SharedVulkanTestDevice());
}

template<typename TAction>
std::string RequireRuntimeError(const TAction& action, std::string_view message, std::source_location location = std::source_location::current()) {
    return RequireThrows<std::runtime_error>(action, message, location).what();
}

template<typename TAction>
void RequireRejected(const TAction& action, std::string_view reason, std::source_location location = std::source_location::current()) {
    const auto message = RequireRuntimeError(action, "expected a rejection mentioning \"" + std::string(reason) + "\"", location);
    Require(message.find(reason) != std::string::npos, "rejection \"" + message + "\" does not mention \"" + std::string(reason) + "\"", location);
}

class EventQueue final {
public:
    explicit EventQueue(const char* name) {
        RequireEqual(sceKernelCreateEqueue(&queue, name), 0, "event queue creation result");
        owner = EqueuePin_nid_postfix(queue);
    }
    EventQueue(const EventQueue&) = delete;
    EventQueue& operator=(const EventQueue&) = delete;
    ~EventQueue() {
        owner.reset();
        if (queue == 0) return;
        try {
            sceKernelDeleteEqueue(queue);
        } catch (...) {
        }
    }

    KernelEqueue Handle() const noexcept { return queue; }
    int Triggered(std::array<KernelEvent, 2>& events) { return owner->GetTriggeredEvents(events.data(), 2); }

    void Delete(std::source_location location = std::source_location::current()) {
        owner.reset();
        const auto handle = queue;
        queue = 0;
        RequireEqual(sceKernelDeleteEqueue(handle), 0, "event queue deletion result", location);
    }

private:
    KernelEqueue queue = 0;
    KernelEqueueRef owner;
};

#ifdef _WIN32
class GuardedMapping final {
public:
    GuardedMapping() : memory(static_cast<std::uint32_t*>(VirtualAlloc(nullptr, 8192, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE))) {
        Require(memory != nullptr, "cannot allocate raw compute boundary test");
    }
    GuardedMapping(const GuardedMapping&) = delete;
    GuardedMapping& operator=(const GuardedMapping&) = delete;
    ~GuardedMapping() { VirtualFree(memory, 0, MEM_RELEASE); }

    std::uint32_t* Words() const noexcept { return memory; }

    void Protect(std::size_t firstWord, DWORD protection, std::string_view message) {
        DWORD previous = 0;
        Require(VirtualProtect(memory + firstWord, 4096, protection, &previous) != 0, message);
    }

private:
    std::uint32_t* memory;
};
#endif

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

std::array<std::uint32_t, 8> endOfPipeLabel(volatile std::uint32_t* address, std::uint32_t value) {
    const auto target = reinterpret_cast<std::uintptr_t>(address);
    return {0xc0064900, 0x514, (1u << 29u) | (2u << 24u), static_cast<std::uint32_t>(target), static_cast<std::uint32_t>(static_cast<std::uint64_t>(target) >> 32u), value, 0, 0};
}

void submit(std::uint32_t queue, const std::vector<std::uint32_t>& words, std::source_location location = std::source_location::current()) {
    Packet packet{const_cast<std::uint32_t*>(words.data()), static_cast<std::uint32_t>(words.size()), 0, {}};
    RequireEqual(queue == 0 ? sceAgcDriverSubmitDcb(&packet) : sceAgcDriverSubmitAcb(queue, &packet), 0, "label submit result", location);
}

std::chrono::milliseconds waitFor(volatile std::uint32_t* address, std::uint32_t value, std::string_view message, std::source_location location = std::source_location::current()) {
    const auto start = std::chrono::steady_clock::now();
    while (*address != value) {
        Require(std::chrono::steady_clock::now() - start < std::chrono::seconds(10), message, location);
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

using RawCode = std::array<std::uint32_t, 64>;

std::uintptr_t literalProgram(RawCode& code) {
    code.fill(0xbf800000);
    code[0] = 0xbe8003ff;
    code[1] = 0xbf810000;
    code[2] = 0xbf810000;
    return reinterpret_cast<std::uintptr_t>(code.data());
}

void makeBranchedProgram(RawCode& code) {
    code[0] = 0xbf820002;
    code[1] = 0xbf810000;
    code[2] = 0xbf800000;
    code[3] = 0xbf810000;
}

const Case rawLiteral{"ReadRawComputeShader_InstructionLiteral_DoesNotEndTheProgram", [] {
    RequireDevice();
    alignas(256) RawCode code{};
    const auto address = literalProgram(code);
    const auto literal = ReadRawComputeShader(address);
    Require(literal->code.size() == 3 && literal->header.empty(), "raw compute stopped at an instruction literal");
    Require(ReadRawComputeShader(address) == literal, "unchanged raw compute code lost its snapshot identity");
}};

const Case rawChanged{"ReadRawComputeShader_ChangedCode_GetsANewSnapshot", [] {
    RequireDevice();
    alignas(256) RawCode code{};
    const auto address = literalProgram(code);
    const auto literal = ReadRawComputeShader(address);
    makeBranchedProgram(code);
    const auto branched = ReadRawComputeShader(address);
    Require(branched->code.size() == 4 && literal->code[0] == 0xbe8003ff, "raw compute lost branch targets or modified an earlier snapshot");
    Require(branched != literal, "changed raw compute code reused a stale snapshot");
}};

const Case rawConcurrentHit{"ReadRawComputeShader_ConcurrentReadersOfCachedCode_ShareItsSnapshot", [] {
    RequireDevice();
    alignas(256) RawCode code{};
    const auto address = literalProgram(code);
    makeBranchedProgram(code);
    const auto branched = ReadRawComputeShader(address);
    std::array<std::shared_ptr<const AgcDriver::DriverDetail::ShaderSnapshot>, 8> concurrent;
    std::vector<std::thread> readers;
    for (auto& snapshot : concurrent) readers.emplace_back([&snapshot, address] { snapshot = ReadRawComputeShader(address); });
    for (auto& reader : readers) reader.join();
    for (const auto& snapshot : concurrent) Require(snapshot == branched, "concurrent raw compute readers lost snapshot reuse");
}};

const Case rawConcurrentMiss{"ReadRawComputeShader_ConcurrentMisses_ShareOneSnapshot", [] {
    RequireDevice();
    alignas(256) static RawCode program{};
    program[0] = 0xbf810000;
    const auto address = reinterpret_cast<std::uintptr_t>(program.data());
    std::array<std::shared_ptr<const AgcDriver::DriverDetail::ShaderSnapshot>, 8> concurrent;
    std::vector<std::thread> readers;
    for (auto& snapshot : concurrent) readers.emplace_back([&snapshot, address] { snapshot = ReadRawComputeShader(address); });
    for (auto& reader : readers) reader.join();
    for (const auto& snapshot : concurrent) Require(snapshot == concurrent.front(), "concurrent raw compute misses duplicated snapshots");
}};

const Case rawEviction{"ReadRawComputeShader_MoreProgramsThanTheEntryLimit_EvictsTheOldest", [] {
    RequireDevice();
    alignas(256) static std::array<RawCode, 65> programs{};
    for (auto& program : programs) program[0] = 0xbf810000;
    const auto firstAddress = reinterpret_cast<std::uintptr_t>(programs.front().data());
    const auto evicted = ReadRawComputeShader(firstAddress);
    for (std::size_t i = 1; i < programs.size(); ++i) ReadRawComputeShader(reinterpret_cast<std::uintptr_t>(programs[i].data()));
    Require(ReadRawComputeShader(firstAddress) != evicted && evicted->code[0] == 0xbf810000,
            "raw compute cache eviction lost snapshot lifetime or exceeded its entry limit");
}};

const Case rawInvalidEntry{"ReadRawComputeShader_MisalignedOrUnmappedEntry_Throws", [] {
    RequireDevice();
    alignas(256) RawCode code{};
    const auto address = literalProgram(code);
    Require(!RequireRuntimeError([&] { ReadRawComputeShader(address + 4); }, "raw compute accepted a misaligned entry").empty(), "a misaligned entry was rejected without a reason");
    Require(!RequireRuntimeError([] { ReadRawComputeShader(0); }, "raw compute accepted an unmapped entry").empty(), "an unmapped entry was rejected without a reason");
}};

#ifdef _WIN32
const Case rawBoundary{"ReadRawComputeShader_InaccessibleMemory_EndsTheReadAndInvalidatesTheCache", [] {
    RequireDevice();
    GuardedMapping mapping;
    mapping.Protect(1024, PAGE_NOACCESS, "cannot protect raw compute boundary");
    auto* boundedCode = mapping.Words() + 1024 - 64;
    std::fill_n(boundedCode, 64, 0xbf800000u);
    const auto boundedAddress = reinterpret_cast<std::uintptr_t>(boundedCode);
    const auto unterminated = RequireRuntimeError([&] { ReadRawComputeShader(boundedAddress); }, "raw compute crossed inaccessible memory");
    Require(!unterminated.empty(), "raw compute crossed inaccessible memory without a reason");
    boundedCode[63] = 0xbf810000;
    const auto bounded = ReadRawComputeShader(boundedAddress);
    RequireEqual(bounded->code.size(), std::size_t{64}, "raw compute instructions before inaccessible memory");
    mapping.Protect(1024, PAGE_READWRITE, "cannot extend raw compute mapping");
    boundedCode[63] = 0xbf800000;
    boundedCode[64] = 0xbf810000;
    const auto extended = ReadRawComputeShader(boundedAddress);
    Require(extended != bounded && extended->code.size() == 65, "raw compute reused code before its end changed");
    mapping.Protect(1024, PAGE_NOACCESS, "cannot revoke cached raw compute tail");
    Require(!RequireRuntimeError([&] { ReadRawComputeShader(boundedAddress); }, "raw compute reused an inaccessible cached tail").empty(), "an inaccessible cached tail was rejected without a reason");
    mapping.Protect(0, PAGE_NOACCESS, "cannot revoke cached raw compute code");
    Require(!RequireRuntimeError([&] { ReadRawComputeShader(boundedAddress); }, "raw compute reused inaccessible cached code").empty(), "inaccessible cached code was rejected without a reason");
}};
#endif

const Case eventType{"GetEqEventType_GraphicsAndOtherEvents_ReadTheirField", [] {
    RequireDevice();
    KernelEvent event{};
    event.filter = -14;
    event.ident = 0x40;
    event.data = 123;
    RequireEqual(sceAgcDriverGetEqEventType(&event), 0x40, "graphics event type");
    event.filter = -1;
    event.data = -17;
    RequireEqual(sceAgcDriverGetEqEventType(&event), -17, "non-graphics event type");
    event.data = std::numeric_limits<std::intptr_t>::max();
    RequireThrows<std::runtime_error>([&] { sceAgcDriverGetEqEventType(&event); }, "an out-of-range non-graphics event type was accepted");
    event.filter = -14;
    event.ident = std::numeric_limits<std::uintptr_t>::max();
    RequireThrows<std::runtime_error>([&] { sceAgcDriverGetEqEventType(&event); }, "an out-of-range graphics event type was accepted");
    RequireThrows<std::runtime_error>([] { sceAgcDriverGetEqEventType(nullptr); }, "a null event was accepted");
    RequireThrows<std::runtime_error>([&] { sceAgcDriverGetEqEventType(reinterpret_cast<const KernelEvent*>(reinterpret_cast<const std::byte*>(&event) + 1)); },
                                      "a misaligned event was accepted");
}};

const Case eventContext{"GetEqContextId_GraphicsEvent_ReadsItsIdent", [] {
    RequireDevice();
    KernelEvent event{};
    event.filter = -14;
    event.ident = 0x29;
    RequireEqual(sceAgcDriverGetEqContextId(&event), 0x29, "graphics event context id");
    event.ident = std::numeric_limits<std::uintptr_t>::max();
    RequireThrows<std::runtime_error>([&] { sceAgcDriverGetEqContextId(&event); }, "an out-of-range context id was accepted");
    event.ident = 1;
    event.filter = -1;
    RequireThrows<std::runtime_error>([&] { sceAgcDriverGetEqContextId(&event); }, "a non-graphics event reported a context id");
    RequireThrows<std::runtime_error>([] { sceAgcDriverGetEqContextId(nullptr); }, "a null event was accepted");
    RequireThrows<std::runtime_error>([&] { sceAgcDriverGetEqContextId(reinterpret_cast<const KernelEvent*>(reinterpret_cast<const std::byte*>(&event) + 1)); },
                                      "a misaligned event was accepted");
}};

const Case validation{"Submit_InvalidPacketOrQueue_IsRejected", [] {
    RequireDevice();
    std::array<std::uint32_t, 3> words{0xc0017600, 0x20c, 0};
    Packet packet{words.data(), 3, 0, {}};
    RequireThrows<std::runtime_error>([] { sceAgcDriverSubmitDcb(nullptr); }, "a null DCB was accepted");
    RequireThrows<std::runtime_error>([] { sceAgcDriverAgrSubmitDcb(nullptr); }, "a null AGR DCB was accepted");
    RequireThrows<std::runtime_error>([] { sceAgcDriverSubmitAcb(0x20, nullptr); }, "a null ACB was accepted");
    RequireThrows<std::runtime_error>([&] { sceAgcDriverSubmitAcb(0, &packet); }, "an ACB on queue 0 was accepted");
    RequireThrows<std::runtime_error>([&] { sceAgcDriverSubmitAcb(0x58, &packet); }, "an ACB on queue 0x58 was accepted");
    packet.dw_num = 2;
    RequireThrows<std::runtime_error>([&] { sceAgcDriverSubmitDcb(&packet); }, "a truncated packet was accepted");
    packet.dw_num = 3;
    packet.flags = 1;
    RequireThrows<std::runtime_error>([&] { sceAgcDriverSubmitDcb(&packet); }, "a packet with flags was accepted");
    packet.flags = 0;
    words[0] = 0xc001ff00;
    RequireThrows<std::runtime_error>([&] { sceAgcDriverSubmitDcb(&packet); }, "an unknown opcode was accepted");
    words[0] = 0xc001105c;
    RequireThrows<std::runtime_error>([&] { sceAgcDriverSubmitDcb(&packet); }, "a malformed flip was accepted");
    words[0] = 0xc0017608;
    RequireThrows<std::runtime_error>([&] { sceAgcDriverSubmitDcb(&packet); }, "a packet with header flags was accepted");
    words[0] = 0xc0017600;
    words[1] = 0x10000;
    RequireThrows<std::runtime_error>([&] { sceAgcDriverSubmitDcb(&packet); }, "an out-of-range register was accepted");
    packet.addr = reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uintptr_t>(words.data()) + 1);
    RequireThrows<std::runtime_error>([&] { sceAgcDriverSubmitDcb(&packet); }, "a misaligned command buffer was accepted");
    packet.addr = reinterpret_cast<std::uint32_t*>(std::numeric_limits<std::uintptr_t>::max() - 3);
    RequireThrows<std::runtime_error>([&] { sceAgcDriverSubmitDcb(&packet); }, "a wrapping command buffer was accepted");
    packet.addr = reinterpret_cast<std::uint32_t*>(0x1000);
    RequireThrows<std::runtime_error>([&] { sceAgcDriverSubmitDcb(&packet); }, "an unmapped command buffer was accepted");
    AgcDriverWaitIdle_nid_postfix();
}};

const Case clearContext{"ClearContext_QueueState_ResetsOnlyContextRegisters", [] {
    RequireDevice();
    AgcDriver::QueueState graphics{{{0x20c, 1}}, {{0x10, 17}, {0x11, 23}}, {{0x242, 5}}};
    const auto shader = graphics.shader;
    const auto userConfig = graphics.userConfig;
    graphics.ClearContext();
    Require(graphics.context == AgcDriver::InitialContextRegisters(), "CLEAR_STATE retained context registers");
    Require(graphics.shader == shader && graphics.userConfig == userConfig, "CLEAR_STATE reset unrelated registers");
    Require(graphics.context.count(0x1b3) == 1 && graphics.context.at(0x1b3) == 0 && graphics.context.count(0x1b4) == 1 && graphics.context.at(0x1b4) == 0, "CLEAR_STATE left SPI_PS_INPUT_ENA/ADDR unset");
    graphics.context.emplace(0x10, 31);
    graphics.ClearContext();
    Require(graphics.context == AgcDriver::InitialContextRegisters(), "repeated CLEAR_STATE retained context registers");
}};

const Case clearStatePackets{"Submit_ClearStatePacket_IsValidatedAndAccepted", [] {
    RequireDevice();
    std::array<std::uint32_t, 3> words{0xc0001200, 0, 0};
    Packet packet{words.data(), 2, 0, {}};
    RequireThrows<std::runtime_error>([&] { sceAgcDriverSubmitAcb(0x20, &packet); }, "CLEAR_STATE on a compute queue was accepted");
    words[1] = 0x10;
    RequireThrows<std::runtime_error>([&] { sceAgcDriverSubmitDcb(&packet); }, "an out-of-range CLEAR_STATE was accepted");
    words[1] = 0;
    words[0] = 0xc0011200;
    packet.dw_num = 3;
    RequireThrows<std::runtime_error>([&] { sceAgcDriverSubmitDcb(&packet); }, "an oversized CLEAR_STATE was accepted");
    words[0] = 0xc0001202;
    packet.dw_num = 2;
    RequireThrows<std::runtime_error>([&] { sceAgcDriverSubmitDcb(&packet); }, "CLEAR_STATE with header flags was accepted");
    words[0] = 0xc0001200;
    packet.dw_num = 1;
    RequireThrows<std::runtime_error>([&] { sceAgcDriverSubmitDcb(&packet); }, "a truncated CLEAR_STATE was accepted");
    packet.dw_num = 2;
    for (std::uint32_t state = 0; state <= 0xf; ++state) {
        words[1] = state;
        RequireEqual(sceAgcDriverSubmitDcb(&packet), 0, "CLEAR_STATE " + std::to_string(state) + " submit result");
    }
    AgcDriverWaitIdle_nid_postfix();
}};

const Case submissions{"Submit_ConcurrentProducersOnEveryQueue_AreAccepted", [] {
    RequireDevice();
    std::vector<std::thread> producers;
    std::array<std::exception_ptr, 4> errors{};
    for (std::uint32_t i = 0; i < errors.size(); ++i) {
        producers.emplace_back([&, i] {
            try {
                for (std::uint32_t j = 0; j < 100; ++j) {
                    std::array<std::uint32_t, 5> words{0xc0017600, 0x240, j, 0xc0001000, 0};
                    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
                    if (i == 0) RequireEqual(sceAgcDriverSubmitDcb(&packet), 0, "DCB submit result");
                    else if (i == 1) RequireEqual(sceAgcDriverAgrSubmitDcb(&packet), 0, "AGR submit result");
                    else RequireEqual(sceAgcDriverSubmitAcb(i == 2 ? 0x20 : 0x57, &packet), 0, "ACB submit result");
                    words.fill(0xffffffffu);
                }
            } catch (...) {
                errors[i] = std::current_exception();
            }
        });
    }
    for (auto& producer : producers) producer.join();
    for (const auto& error : errors) {
        if (error) std::rethrow_exception(error);
    }
    AgcDriverWaitIdle_nid_postfix();
    Packet empty{};
    RequireEqual(sceAgcDriverSubmitDcb(&empty), 0, "empty submit result");
    AgcDriverWaitIdle_nid_postfix();
}};

const Case endOfPipeInterrupts{"ReleaseMem_EndOfPipeInterrupt_ReachesOnlyItsRegisteredQueue", [] {
    RequireDevice();
    EventQueue eq("AGC test");
    int graphicsTag = 0;
    int computeTag = 0;
    RequireEqual(sceAgcDriverAddEqEvent(eq.Handle(), 0, &graphicsTag), 0, "graphics event registration result");
    RequireEqual(sceAgcDriverAddEqEvent(eq.Handle(), 0x20, &computeTag), 0, "compute event registration result");
    RequireThrows<std::runtime_error>([] { sceAgcDriverAddEqEvent(0, 0, nullptr); }, "an event on a null queue was accepted");
    std::array<std::uint32_t, 8> words{0xc0064900, 0, 1u << 24u, 0, 0, 0, 0, 0};
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0 && sceAgcDriverSubmitDcb(&packet) == 0, "interrupt submit failed");
    AgcDriverWaitIdle_nid_postfix();
    std::array<KernelEvent, 2> events{};
    RequireEqual(eq.Triggered(events), 1, "graphics end-of-pipe interrupts delivered");
    Require(events[0].filter == -14 && events[0].udata == &graphicsTag && events[0].data == 2 && sceAgcDriverGetEqEventType(events.data()) == 0, "graphics end-of-pipe event encoding is wrong");
    RequireEqual(sceAgcDriverGetEqContextId(events.data()), 0, "graphics end-of-pipe event context");
    RequireEqual(eq.Triggered(events), 0, "interrupts left after delivery");
    RequireEqual(sceAgcDriverSubmitAcb(0x20, &packet), 0, "compute interrupt submit result");
    AgcDriverWaitIdle_nid_postfix();
    Require(eq.Triggered(events) == 1 && events[0].udata == &computeTag && sceAgcDriverGetEqEventType(events.data()) == 0x20, "compute end-of-pipe interrupt missing");
    RequireEqual(sceAgcDriverGetEqContextId(events.data()), 0x20, "compute end-of-pipe event context");
    words[2] = 0;
    RequireEqual(sceAgcDriverSubmitDcb(&packet), 0, "plain release submit result");
    AgcDriverWaitIdle_nid_postfix();
    RequireEqual(eq.Triggered(events), 0, "interrupts raised by a release without INT_SEL");
    alignas(8) static volatile std::uint64_t label = 0;
    const auto labelAddress = reinterpret_cast<std::uintptr_t>(&label);
    words = {0xc0064900, 0x528, (3u << 29u) | (3u << 24u) | (1u << 16u), static_cast<std::uint32_t>(labelAddress), static_cast<std::uint32_t>(static_cast<std::uint64_t>(labelAddress) >> 32u), 0x89abcdefu, 0x01234567u, 0};
    RequireEqual(sceAgcDriverSubmitDcb(&packet), 0, "send-data release submit result");
    AgcDriverWaitIdle_nid_postfix();
    Require(label != 0, "send-data release did not write its label");
    RequireEqual(eq.Triggered(events), 0, "interrupts raised by a release with INT_SEL send data after write confirm");
    words = {0xc0064900, 0, 1u << 24u, 0, 0, 0, 0, 0};
    RequireEqual(sceAgcDriverDeleteEqEvent(eq.Handle(), 0), 0, "graphics event deletion result");
    RequireThrows<std::runtime_error>([&] { sceAgcDriverDeleteEqEvent(eq.Handle(), 0); }, "a deleted event was deleted again");
    words[2] = 1u << 24u;
    RequireEqual(sceAgcDriverSubmitDcb(&packet), 0, "interrupt submit result after deletion");
    AgcDriverWaitIdle_nid_postfix();
    RequireEqual(eq.Triggered(events), 0, "interrupts received by a deleted event");
    RequireEqual(sceAgcDriverDeleteEqEvent(eq.Handle(), 0x20), 0, "compute event deletion result");
    eq.Delete();
}};

const Case labelStoredSinceSubmission{"WaitRegMem_LabelStoredAfterSubmission_SatisfiesTheWait", [] {
    RequireDevice();
    alignas(64) static volatile std::uint32_t gate = 0, label = 0, done = 0, late = 0;
    submit(0x20, commands(waitEqual(&gate, 1), waitEqual(&label, 1), writeData(&done, 1)));
    submit(0, commands(writeData(&label, 1)));
    waitFor(&label, 1, "producer label never landed");
    label = 0;
    gate = 1;
    Require(waitFor(&done, 1, "consumer never passed its waits") < std::chrono::milliseconds(500), "a label stored after the wait's submission did not satisfy it");
    AgcDriverWaitIdle_nid_postfix();
    submit(0x20, commands(waitEqual(&label, 1), writeData(&late, 1)));
    Require(waitFor(&late, 1, "consumer never passed its wait") >= std::chrono::milliseconds(900), "a label stored before the wait's submission satisfied it");
    AgcDriverWaitIdle_nid_postfix();
}};

const Case endOfPipeLabelsWithoutWork{"ReleaseMem_EndOfPipeLabelsWithoutWork_LandWithTheirInterrupt", [] {
    RequireDevice();
    alignas(64) static volatile std::uint32_t first = 0, second = 0, done = 0;
    EventQueue eq("AGC EOP labels");
    int tag = 0;
    RequireEqual(sceAgcDriverAddEqEvent(eq.Handle(), 0, &tag), 0, "graphics event registration result");
    submit(0x20, commands(waitEqual(&second, 2), writeData(&done, 1)));
    submit(0, commands(endOfPipeLabel(&first, 1), endOfPipeLabel(&second, 2)));
    waitFor(&done, 1, "an end-of-pipe label with no work before it never landed");
    Require(first == 1 && second == 2, "end-of-pipe labels with no work before them landed wrong");
    AgcDriverWaitIdle_nid_postfix();
    std::array<KernelEvent, 2> events{};
    Require(eq.Triggered(events) == 1 && events[0].udata == &tag && events[0].data == 2, "end-of-pipe labels with no work before them lost their interrupts");
    RequireEqual(sceAgcDriverDeleteEqEvent(eq.Handle(), 0), 0, "graphics event deletion result");
    eq.Delete();
}};

const Case labelHeldAtSubmission{"WaitRegMem_LabelHeldAtSubmission_SatisfiesTheWait", [] {
    RequireDevice();
    alignas(64) static volatile std::uint32_t gate = 0, label = 1, done = 0, reset = 0;
    submit(0x20, commands(waitEqual(&gate, 1), waitEqual(&label, 1), writeData(&done, 1)));
    label = 0;
    gate = 1;
    Require(waitFor(&done, 1, "consumer never passed its waits") < std::chrono::milliseconds(500), "a label held when the wait was submitted did not satisfy it");
    AgcDriverWaitIdle_nid_postfix();
    gate = 0;
    label = 1;
    submit(0x20, commands(waitEqual(&gate, 1), writeData(&label, 0), waitEqual(&label, 1), writeData(&reset, 1)));
    gate = 1;
    Require(waitFor(&reset, 1, "consumer never passed its wait") >= std::chrono::milliseconds(900), "a label its own queue stored first counted as held at the submission");
    AgcDriverWaitIdle_nid_postfix();
}};

const Case wideLabelStoredSinceSubmission{"WaitRegMem64_LowDwordStoredAfterSubmission_SatisfiesOnlyAMatchingWait", [] {
    RequireDevice();
    alignas(64) static volatile std::uint32_t gate = 0, done = 0, late = 0;
    alignas(64) static volatile std::uint32_t label[2] = {0, 0x5eed};
    submit(0x20, commands(waitEqual(&gate, 1), waitEqual64(label, 1, 0xffffffffu), writeData(&done, 1)));
    submit(0, commands(writeData(label, 1)));
    waitFor(label, 1, "producer label never landed");
    label[0] = 0;
    gate = 1;
    Require(waitFor(&done, 1, "consumer never passed its waits") < std::chrono::milliseconds(500), "a 32-bit label stored after a low-dword 64-bit wait's submission did not satisfy it");
    AgcDriverWaitIdle_nid_postfix();
    gate = 0;
    submit(0x20, commands(waitEqual(&gate, 1), waitEqual64(label, 1, ~0ull), writeData(&late, 1)));
    submit(0, commands(writeData(label, 1)));
    waitFor(label, 1, "producer label never landed");
    label[0] = 0;
    gate = 1;
    Require(waitFor(&late, 1, "consumer never passed its wait") >= std::chrono::milliseconds(900), "a 32-bit store satisfied a 64-bit wait whose high dword never matched");
    AgcDriverWaitIdle_nid_postfix();
}};

const Case waitFreeAfterEarlierWork{"SubmitAcb_WaitFreeSubmission_RunsAfterEarlierQueue0Work", [] {
    RequireDevice();
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
    RequireEqual(static_cast<std::uint32_t>(first), writes, "queue 0 writes finished before the wait-free submission ran");
    AgcDriverWaitIdle_nid_postfix();
}};

const Case queue0WaitsOnWaitFree{"SubmitAcb_HeldWaitFreeSubmission_ReleasesQueue0WaitAtOnce", [] {
    RequireDevice();
    alignas(64) static volatile std::uint32_t label = 0, done = 0;
    submit(0, commands(waitEqual(&label, 1), writeData(&done, 1)));
    submit(0x20, commands(writeData(&label, 1)));
    Require(waitFor(&done, 1, "queue 0 never passed the wait the held submission satisfies") < std::chrono::milliseconds(500), "queue 0's wait on a held wait-free submission's label was not released at once");
    AgcDriverWaitIdle_nid_postfix();
}};

const Case multiAcbs{"SubmitMultiAcbs_CommandBuffers_RunInOrderOnTheirQueue", [] {
    RequireDevice();
    alignas(64) static volatile std::uint32_t value = 0, done = 0, gate = 0;
    RequireEqual(sceAgcDriverSubmitMultiAcbs(0x20, nullptr, nullptr, 0), 0, "empty multi-ACB submit result");
    auto write = writeData(&value, 1);
    std::array<std::uint32_t*, 1> addresses{write.data()};
    std::array<std::uint32_t, 1> sizes{static_cast<std::uint32_t>(write.size())};
    RequireThrows<std::runtime_error>([&] { sceAgcDriverSubmitMultiAcbs(0x1f, addresses.data(), sizes.data(), 1); }, "a multi-ACB submit to queue 0x1f was accepted");
    RequireThrows<std::runtime_error>([&] { sceAgcDriverSubmitMultiAcbs(0x58, addresses.data(), sizes.data(), 1); }, "a multi-ACB submit to queue 0x58 was accepted");
    RequireThrows<std::runtime_error>([&] { sceAgcDriverSubmitMultiAcbs(0, addresses.data(), sizes.data(), 1); }, "a multi-ACB submit to queue 0 was accepted");
    RequireEqual(sceAgcDriverSubmitMultiAcbs(0, nullptr, nullptr, 0), 0, "empty multi-ACB submit result on queue 0");
    RequireThrows<std::runtime_error>([&] { sceAgcDriverSubmitMultiAcbs(0x20, nullptr, sizes.data(), 1); }, "a multi-ACB submit without addresses was accepted");
    RequireThrows<std::runtime_error>([&] { sceAgcDriverSubmitMultiAcbs(0x20, addresses.data(), nullptr, 1); }, "a multi-ACB submit without sizes was accepted");
    Require(value == 0, "a rejected multi-ACB submit ran a command buffer");
    for (std::uint32_t queue : {0x20u, 0x57u}) {
        value = 0;
        done = 0;
        auto first = writeData(&value, 1);
        auto second = writeData(&value, 2);
        auto third = writeData(&done, 1);
        std::array<std::uint32_t*, 3> buffers{first.data(), second.data(), third.data()};
        std::array<std::uint32_t, 3> lengths{static_cast<std::uint32_t>(first.size()), static_cast<std::uint32_t>(second.size()), static_cast<std::uint32_t>(third.size())};
        RequireEqual(sceAgcDriverSubmitMultiAcbs(queue, buffers.data(), lengths.data(), 3), 0, "multi-ACB submit result on queue " + std::to_string(queue));
        waitFor(&done, 1, "the last ACB of a multi-ACB submit never ran");
        RequireEqual(static_cast<std::uint32_t>(value), 2u, "value after a multi-ACB submit on queue " + std::to_string(queue));
        AgcDriverWaitIdle_nid_postfix();
    }
    done = 0;
    value = 0;
    auto wait = waitEqual(&gate, 1);
    auto finish = writeData(&done, 1);
    std::array<std::uint32_t*, 2> buffers{wait.data(), finish.data()};
    std::array<std::uint32_t, 2> lengths{static_cast<std::uint32_t>(wait.size()), static_cast<std::uint32_t>(finish.size())};
    RequireEqual(sceAgcDriverSubmitMultiAcbs(0x21, buffers.data(), lengths.data(), 2), 0, "waiting multi-ACB submit result");
    submit(0x21, commands(writeData(&value, 1)));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    Require(value == 0 && done == 0, "a later submission on the same compute queue overtook a multi-ACB submit");
    gate = 1;
    waitFor(&value, 1, "a submission queued behind a multi-ACB submit never ran");
    Require(done == 1, "a multi-ACB submit did not run before a later submission on its queue");
    AgcDriverWaitIdle_nid_postfix();
}};

const Case waitFreeBehindHeld{"SubmitAcb_LabelBehindAHeldSubmission_ReleasesQueue0WaitAtOnce", [] {
    RequireDevice();
    alignas(64) static volatile std::uint32_t other = 0, label = 0, done = 0;
    submit(0, commands(waitEqual(&label, 1), writeData(&done, 1)));
    submit(0x20, commands(writeData(&other, 1)));
    submit(0x20, commands(writeData(&label, 1)));
    Require(waitFor(&done, 1, "queue 0 never passed the wait a later submission of the held queue satisfies") < std::chrono::milliseconds(500), "queue 0's wait on a label stored behind a held submission was not released at once");
    Require(other == 1, "the held submission did not run before the one behind it");
    AgcDriverWaitIdle_nid_postfix();
}};

const Case waitFreeCpuWaitsFor{"SubmitAcb_HeldSubmissionTheCpuWaitsFor_IsReleased", [] {
    RequireDevice();
    alignas(64) static volatile std::uint32_t stored = 0, flag = 0, done = 0;
    submit(0, commands(waitEqual(&flag, 1), writeData(&done, 1)));
    submit(0x20, commands(writeData(&stored, 1)));
    std::exception_ptr titleError;
    std::thread title([&titleError] {
        try {
            waitFor(&stored, 1, "the held submission never ran while queue 0 waited on the CPU");
        } catch (...) {
            titleError = std::current_exception();
        }
        flag = 1;
    });
    std::chrono::milliseconds waited{};
    std::exception_ptr waitError;
    try {
        waited = waitFor(&done, 1, "queue 0 never passed a wait the CPU satisfies after the held submission");
    } catch (...) {
        waitError = std::current_exception();
    }
    title.join();
    if (titleError) std::rethrow_exception(titleError);
    if (waitError) std::rethrow_exception(waitError);
    Require(waited < std::chrono::milliseconds(500), "a held submission the CPU waits for was not released while queue 0 waited on the CPU");
    AgcDriverWaitIdle_nid_postfix();
}};

const Case multiSubmissions{"SubmitMultiDcbs_CommandBuffers_RunInOrder", [] {
    RequireDevice();
    alignas(64) static volatile std::uint32_t value = 0, done = 0;
    RequireEqual(sceAgcDriverSubmitMultiDcbs(nullptr, nullptr, 0), 0, "empty multi-DCB submit result");
    RequireEqual(sceAgcDriverAgrSubmitMultiDcbs(nullptr, nullptr, 0), 0, "empty AGR multi-DCB submit result");
    std::array<std::uint32_t, 3> invalid{0xc0017600, 0x20c, 0};
    std::array<std::uint32_t*, 1> invalidAddresses{invalid.data()};
    std::array<std::uint32_t, 1> invalidSizes{2};
    RequireThrows<std::runtime_error>([&] { sceAgcDriverSubmitMultiDcbs(nullptr, invalidSizes.data(), 1); }, "a multi-DCB submit without addresses was accepted");
    RequireThrows<std::runtime_error>([&] { sceAgcDriverAgrSubmitMultiDcbs(invalidAddresses.data(), nullptr, 1); }, "an AGR multi-DCB submit without sizes was accepted");
    RequireThrows<std::runtime_error>([&] { sceAgcDriverSubmitMultiDcbs(invalidAddresses.data(), invalidSizes.data(), 1); }, "a truncated multi-DCB packet was accepted");
    RequireThrows<std::runtime_error>([&] { sceAgcDriverAgrSubmitMultiDcbs(invalidAddresses.data(), invalidSizes.data(), 1); }, "a truncated AGR multi-DCB packet was accepted");
    for (bool agr : {false, true}) {
        value = 0;
        done = 0;
        auto first = writeData(&value, 1);
        auto second = writeData(&value, 2);
        auto third = writeData(&done, 1);
        std::array<std::uint32_t*, 3> addresses{first.data(), second.data(), third.data()};
        std::array<std::uint32_t, 3> sizes{static_cast<std::uint32_t>(first.size()), static_cast<std::uint32_t>(second.size()), static_cast<std::uint32_t>(third.size())};
        const int result = agr ? sceAgcDriverAgrSubmitMultiDcbs(addresses.data(), sizes.data(), 3) : sceAgcDriverSubmitMultiDcbs(addresses.data(), sizes.data(), 3);
        const std::string name = agr ? "AGR multi-DCB" : "multi-DCB";
        RequireEqual(result, 0, name + " submit result");
        waitFor(&done, 1, "the last DCB of a multi-DCB submit never ran");
        RequireEqual(static_cast<std::uint32_t>(value), 2u, "value after a " + name + " submit");
        AgcDriverWaitIdle_nid_postfix();
    }
}};

const Case shaderHeaderAlignment{"RegisterShader_HeaderAlignment_AcceptsUnalignedAndRejectsInvalidHeaders", [] {
    RequireDevice();
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
    RequireRejected([] { AgcDriverRegisterShader_nid_postfix(nullptr); }, "null or misaligned address");
    RequireRejected([] { AgcDriverRegisterShader_nid_postfix(reinterpret_cast<const Shader*>(0x1001)); }, "not readable");
    Shader misplaced = shader;
    misplaced.code = code.data() + 1;
    RequireRejected([&] { AgcDriverRegisterShader_nid_postfix(at(4, misplaced)); }, "null or misaligned address");
    Shader older = shader;
    older.version = 0x17;
    RequireRejected([&] { AgcDriverRegisterShader_nid_postfix(at(1, older)); }, "invalid shader header");
    Shader truncated = shader;
    truncated.header_size = sizeof(Shader) - 4;
    RequireRejected([&] { AgcDriverRegisterShader_nid_postfix(at(4, truncated)); }, "smaller than its fixed fields");
}};

const Case headerWithoutProgramAddress{"RegisterShader_HeaderWithoutProgramAddress_IsAccepted", [] {
    RequireDevice();
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
}};

const Case registeredFloatMode{"RegisteredFloatMode_EveryStage_DecodesItsRsrc1", [] {
    RequireDevice();
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
        Require(astro.has_value() && astro->floatMode == 0xc0u && astro->dx10Clamp && !astro->ieeeMode && !astro->fp16Overflow, name + ": FLOAT_MODE 0xc0 with DX10_CLAMP decoded wrong");
        const auto ieee = mode(1u << 23u);
        Require(ieee && ieee->ieeeMode && ieee->floatMode == 0u && !ieee->dx10Clamp, name + ": IEEE_MODE decoded wrong");
        const auto overflow = mode(1u << stage.fp16OverflowBit);
        Require(overflow && overflow->fp16Overflow, name + ": FP16_OVFL not read from bit " + std::to_string(stage.fp16OverflowBit));
        for (const auto bit : otherBits) {
            if (bit == stage.fp16OverflowBit) continue;
            const auto other = mode(1u << bit);
            Require(other && !other->fp16Overflow, name + ": bit " + std::to_string(bit) + " read as FP16_OVFL");
        }
        ShaderSnapshot missing{0x20000, 0, stage.type, {}, {}};
        Require(!RegisteredFloatMode(missing).has_value(), name + ": a snapshot without registered state has a float mode");
        missing.registeredState = std::make_shared<RegisteredShaderState>();
        Require(!RegisteredFloatMode(missing).has_value(), name + ": a missing RSRC1 has a float mode");
    }
}};

const Case workerFailure{"WaitIdle_DispatchWithoutShaderRegisters_PropagatesToEverySubmit", [] {
    RequireDevice();
    std::array<std::uint32_t, 5> words{0xc0031500, 1, 1, 1, 0x41};
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    RequireEqual(sceAgcDriverSubmitAcb(0x21, &packet), 0, "dispatch submit result");
    std::array<std::string, 4> messages;
    std::vector<std::thread> waiters;
    for (auto& message : messages) {
        waiters.emplace_back([&message] {
            try {
                AgcDriverWaitIdle_nid_postfix();
            } catch (const std::runtime_error& error) {
                message = error.what();
            }
        });
    }
    for (auto& waiter : waiters) waiter.join();
    for (const auto& message : messages) Require(message.find("required shader register") != std::string::npos, "worker failure was lost: \"" + message + "\"");
    RequireEqual(RequireRuntimeError([&] { sceAgcDriverSubmitDcb(&packet); }, "a DCB after a worker failure was accepted"), messages[0], "failure reported by a subsequent DCB");
    RequireEqual(RequireRuntimeError([&] { sceAgcDriverAgrSubmitDcb(&packet); }, "an AGR DCB after a worker failure was accepted"), messages[0], "failure reported by a subsequent AGR DCB");
    RequireEqual(RequireRuntimeError([&] { sceAgcDriverSubmitAcb(0x20, &packet); }, "an ACB after a worker failure was accepted"), messages[0], "failure reported by a subsequent ACB");
}};

const Case shutdown{"LibcRunShutdown_AfterWorkerFailure_ReportsIt", [] {
    RequireDevice();
    RequireRejected([] { LibcRunShutdown_nid_postfix(); }, "required shader register");
}};

} // namespace
