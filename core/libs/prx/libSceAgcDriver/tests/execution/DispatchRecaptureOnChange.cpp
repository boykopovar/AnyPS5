#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/SynchronizationStatistics.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::uint32_t Lanes = 32;
constexpr std::uint32_t BufferBytes = Lanes * 4u;
constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t LabelWord = 0x400;

alignas(256) constexpr std::array<std::uint32_t, 11> TableCode{0xf4080200u, 0xfa000000u, 0xbf8cc07fu, 0x34020082u, 0xe0301000u, 0x80020201u, 0xbf8c3f70u, 0x4a060481u, 0xe0701000u, 0x80010301u, 0xbf810000u};

void Expect(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error("dispatch recapture: " + what);
}

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint32_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, PAGE_READWRITE, BlockBytes);
#else
        constexpr std::uintptr_t alignment = 65536;
        void* mapped = mmap(nullptr, BlockBytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + alignment - 1) & ~(alignment - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + BlockBytes != begin + BlockBytes + alignment) munmap(reinterpret_cast<void*>(aligned + BlockBytes), begin + alignment - aligned);
            block = reinterpret_cast<std::uint32_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, BlockBytes);
        }
#endif
        Expect(block != nullptr, "cannot allocate a guest block");
        std::memset(block, 0, BlockBytes);
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint32_t* Words() const { return block; }

private:
    std::uint32_t* block = nullptr;
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), BufferBytes, 0x31016facu};
}

const GuestBlock* Table = nullptr;
const GuestBlock* Moved = nullptr;
std::atomic<bool> TableChangeArmed{false};
std::atomic<bool> WaitArmed{false};
std::atomic<int> TableChanges{0};
std::array<std::uint32_t, 7> WaitPacket{};

void ArmWait(const GuestBlock& block, std::uint32_t reference) {
    const auto label = reinterpret_cast<std::uintptr_t>(block.Words() + LabelWord);
    WaitPacket = {0xc0053c00u, 0x13u, static_cast<std::uint32_t>(label), static_cast<std::uint32_t>(label >> 32u), reference, 0xffffffffu, 10u};
    WaitArmed.store(true);
}

void ChangeTableBeforeCheck() {
    if (WaitArmed.exchange(false)) AgcDriver::DriverDetail::Driver::NotePendingWaitForTest(WaitPacket);
    if (!TableChangeArmed.exchange(false)) return;
    const auto descriptor = BufferDescriptor(Moved->Words());
    std::memcpy(Table->Words(), descriptor.data(), sizeof(descriptor));
    TableChanges.fetch_add(1);
}

void Dispatch(const GuestBlock& table, const GuestBlock& output) {
    const auto codeAddress = reinterpret_cast<std::uintptr_t>(TableCode.data());
    const auto tableAddress = reinterpret_cast<std::uintptr_t>(table.Words());
    const auto stored = BufferDescriptor(output.Words());
    std::vector<std::uint32_t> commands{
        0xc0027600u, 0x20cu, static_cast<std::uint32_t>(codeAddress >> 8u), static_cast<std::uint32_t>(codeAddress >> 40u),
        0xc0037600u, 0x207u, Lanes, 1u, 1u,
        0xc0017600u, 0x213u, 8u << 1u,
        0xc0087600u, 0x240u, static_cast<std::uint32_t>(tableAddress), static_cast<std::uint32_t>(tableAddress >> 32u), 0u, 0u, stored[0], stored[1], stored[2], stored[3],
        0xc0031500u, 1u, 1u, 1u, 0x8041u,
    };
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Expect(sceAgcDriverSubmitDcb(&packet) == 0, "the dispatch submission failed");
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(output.Words()), BufferBytes);
}

void SubmitWait(const GuestBlock& block, std::uint32_t reference) {
    const auto label = reinterpret_cast<std::uintptr_t>(block.Words() + LabelWord);
    std::vector<std::uint32_t> commands{0xc0053c00u, 0x13u, static_cast<std::uint32_t>(label), static_cast<std::uint32_t>(label >> 32u), reference, 0xffffffffu, 10u};
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Expect(sceAgcDriverSubmitDcb(&packet) == 0, "the wait submission failed");
    AgcDriverWaitIdle_nid_postfix();
}

std::uint32_t Lane(const GuestBlock& output, std::uint32_t lane) {
    return output.Words()[lane];
}

struct Counts {
    std::uint64_t retries, checks, skipped;
};

Counts CaptureCounts() {
    return {AgcDriver::DriverDetail::captureChangeRetries.load(), AgcDriver::DriverDetail::captureChecks.load(), AgcDriver::DriverDetail::captureChecksSkipped.load()};
}

Counts Since(const Counts& before) {
    const auto now = CaptureCounts();
    return {now.retries - before.retries, now.checks - before.checks, now.skipped - before.skipped};
}

void ExpectSkipped(const GuestBlock& table, const GuestBlock& output, const std::string& what) {
    const auto before = CaptureCounts();
    Dispatch(table, output);
    const auto counts = Since(before);
    std::printf("%s: lane 0 stored 0x%x, %llu capture check(s) run, %llu skipped, %llu packet(s) prepared again\n", what.c_str(), Lane(output, 0), static_cast<unsigned long long>(counts.checks), static_cast<unsigned long long>(counts.skipped), static_cast<unsigned long long>(counts.retries));
    Expect(counts.checks == 0, what + ": the dispatch compared its captured memory with no unsatisfied wait");
    Expect(counts.skipped >= 1, what + ": the dispatch did not skip its capture check");
    Expect(counts.retries == 0, what + ": a dispatch whose captured memory did not change was prepared again");
}

void ExpectCompared(const GuestBlock& table, const GuestBlock& output, std::uint32_t reference, const std::string& what) {
    const auto before = CaptureCounts();
    ArmWait(table, reference);
    Dispatch(table, output);
    const auto counts = Since(before);
    std::printf("%s: lane 0 stored 0x%x, %llu capture check(s) run, %llu packet(s) prepared again\n", what.c_str(), Lane(output, 0), static_cast<unsigned long long>(counts.checks), static_cast<unsigned long long>(counts.retries));
    Expect(counts.checks == 1, what + ": the dispatch ran " + std::to_string(counts.checks) + " capture checks, not one");
    Expect(counts.retries == 0, what + ": a dispatch whose captured memory did not change was prepared again");
}

void ExpectStillCompared(const GuestBlock& table, const GuestBlock& output, const std::string& what) {
    const auto before = CaptureCounts();
    Dispatch(table, output);
    const auto counts = Since(before);
    std::printf("%s: lane 0 stored 0x%x, %llu capture check(s) run, %llu skipped\n", what.c_str(), Lane(output, 0), static_cast<unsigned long long>(counts.checks), static_cast<unsigned long long>(counts.skipped));
    Expect(counts.checks == 1, what + ": the dispatch ran " + std::to_string(counts.checks) + " capture checks, not one");
    Expect(counts.retries == 0, what + ": a dispatch whose captured memory did not change was prepared again");
}

void ExpectOutput(const GuestBlock& output, std::uint32_t base, const std::string& what) {
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) Expect(Lane(output, lane) == base + lane + 1u, what + ": lane " + std::to_string(lane) + " stored " + std::to_string(Lane(output, lane)) + ", expected " + std::to_string(base + lane + 1u));
}

}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        Shader shader{};
        shader.file_header = 0x34333231;
        shader.version = 0x18;
        shader.header_size = sizeof(Shader);
        shader.shader_size = sizeof(TableCode);
        shader.code = TableCode.data();
        AgcDriverRegisterShader_nid_postfix(&shader);
        GuestBlock table;
        GuestBlock first;
        GuestBlock moved;
        GuestBlock output;
        for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
            first.Words()[lane] = 0x1000u + lane;
            moved.Words()[lane] = 0x2000u + lane;
        }
        const auto initial = BufferDescriptor(first.Words());
        std::memcpy(table.Words(), initial.data(), sizeof(initial));
        Table = &table;
        Moved = &moved;
        AgcDriver::DriverDetail::Driver::SetCaptureCheckHook(&ChangeTableBeforeCheck);

        Dispatch(table, output);
        ExpectOutput(output, 0x1000u, "the first dispatch through the table");
        std::printf("the first dispatch stored 0x%x in lane 0 through the table's first V#\n", Lane(output, 0));

        const auto retries = AgcDriver::DriverDetail::captureChangeRetries.load();
        ArmWait(table, 1);
        TableChangeArmed.store(true);
        Dispatch(table, output);
        const auto redone = AgcDriver::DriverDetail::captureChangeRetries.load() - retries;
        std::printf("the same dispatch with its table's V# moved to another buffer after its capture: the table changed %d time(s), lane 0 stored 0x%x (%s), %llu packet(s) prepared again\n", TableChanges.load(), Lane(output, 0), Lane(output, 0) == 0x2001u ? "the moved V# was bound" : "the V# captured before the change was bound", static_cast<unsigned long long>(redone));
        Expect(TableChanges.load() == 1, "the check seam did not run before the dispatch was recorded");
        ExpectOutput(output, 0x2000u, "the dispatch whose table changed before it was recorded");
        Expect(redone == 1, "the dispatch was prepared again " + std::to_string(redone) + " times, not once");

        ExpectCompared(table, output, 1, "the same dispatch behind the unsatisfied wait with its table unchanged");
        ExpectOutput(output, 0x2000u, "the dispatch behind an unsatisfied wait with an unchanged table");

        table.Words()[LabelWord] = 1;
        ExpectSkipped(table, output, "the same dispatch after memory satisfied the wait, table unchanged");
        ExpectOutput(output, 0x2000u, "the dispatch with an unchanged table");

        table.Words()[LabelWord] = 0;
        ExpectCompared(table, output, 1, "a dispatch behind a wait for 1 on a label holding 0");
        ExpectCompared(table, output, 2, "a dispatch behind a later wait for 2 on the same label");
        table.Words()[LabelWord] = 2;
        ExpectSkipped(table, output, "the same dispatch after memory satisfied the later wait, which replaced the earlier one");
        ExpectOutput(output, 0x2000u, "the dispatch after the later wait was satisfied");

        table.Words()[LabelWord] = 0;
        ExpectCompared(table, output, 2, "a dispatch behind a wait for 2 on a label holding 0");
        table.Words()[LabelWord] = 1;
        SubmitWait(table, 1);
        ExpectStillCompared(table, output, "a dispatch after memory satisfied a graphics wait for 1 on that label but not the noted wait for 2");
        ExpectOutput(output, 0x2000u, "the dispatch behind the wait for 2 memory has not satisfied");
        table.Words()[LabelWord] = 2;
        ExpectSkipped(table, output, "the same dispatch after memory satisfied the wait for 2");

        AgcDriver::DriverDetail::Driver::SetCaptureCheckHook(nullptr);
        AgcDriverShutdown_nid_postfix();
        std::puts("dispatch recapture: a dispatch whose captured descriptor table changed before it was recorded binds the table in memory");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { AgcDriverShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}
