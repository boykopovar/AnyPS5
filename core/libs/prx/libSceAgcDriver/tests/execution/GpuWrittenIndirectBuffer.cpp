#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VideoOutput.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
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
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

constexpr std::array<std::uint32_t, 12> Code{0x7e000204u, 0x7e020205u, 0x7e040206u, 0x7e060207u, 0x7e080208u, 0x7e0a0280u, 0xe0781000u, 0x80000005u, 0xe0701010u, 0x80000405u, 0xbf8c3f70u, 0xbf810000u};
constexpr std::size_t CommandsOffset = 4096;
constexpr std::size_t CommandWords = 64;
constexpr std::size_t MarkerOffset = 8192;
constexpr std::size_t LabelOffset = 8196;
constexpr std::size_t GateOffset = 8200;
constexpr std::size_t ChildOffset = CommandsOffset + CommandWords * 4;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t Filler = 0xffff1000u;

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, PAGE_READWRITE, BlockBytes);
#else
        void* mapped = mmap(nullptr, BlockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            block = static_cast<std::uint8_t*>(mapped);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, BlockBytes);
        }
#endif
        Require(block != nullptr, "GPU-written indirect buffer: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint64_t At(std::size_t offset) const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block + offset)); }
    std::uint8_t* Bytes(std::size_t offset) const { return block + offset; }

private:
    std::uint8_t* block = nullptr;
};

std::vector<std::uint32_t> Pm4(std::uint32_t opcode, std::initializer_list<std::uint32_t> body) {
    std::vector<std::uint32_t> words{0xc0000000u | (static_cast<std::uint32_t>(body.size() - 1) << 16u) | (opcode << 8u)};
    words.insert(words.end(), body);
    return words;
}

std::uint32_t Low(std::uint64_t address) { return static_cast<std::uint32_t>(address); }
std::uint32_t High(std::uint64_t address) { return static_cast<std::uint32_t>(address >> 32u); }

std::vector<std::uint32_t> Dispatching(const GuestBlock& block, const std::vector<std::uint32_t>& writeData, std::size_t destination = CommandsOffset) {
    const auto kernel = block.At(0);
    const auto commands = block.At(destination);
    std::vector<std::uint32_t> words;
    for (const auto& packet : {
             Pm4(0x76, {0x20c, static_cast<std::uint32_t>(kernel >> 8u)}),
             Pm4(0x76, {0x20d, static_cast<std::uint32_t>(kernel >> 40u) & 0xffu}),
             Pm4(0x76, {0x207, 1}), Pm4(0x76, {0x208, 1}), Pm4(0x76, {0x209, 1}),
             Pm4(0x76, {0x213, 9u << 1u}),
             Pm4(0x76, {0x240, Low(commands), High(commands) & 0xffffu, CommandWords * 4u, 0x31016facu, writeData[0], writeData[1], writeData[2], writeData[3], writeData[4]}),
             Pm4(0x15, {1, 1, 1, 0x41}),
             Pm4(0x46, {0x407}),
         }) {
        words.insert(words.end(), packet.begin(), packet.end());
    }
    return words;
}

std::uint32_t Load(const GuestBlock& block, std::size_t offset) {
    return std::atomic_ref<std::uint32_t>(*reinterpret_cast<std::uint32_t*>(block.Bytes(offset))).load();
}

void Store(const GuestBlock& block, std::size_t offset, std::uint32_t value) {
    std::atomic_ref<std::uint32_t>(*reinterpret_cast<std::uint32_t*>(block.Bytes(offset))).store(value);
}

void HeldWaitAfterRewrite(const GuestBlock& block) {
    std::array<std::uint32_t, CommandWords> fillers{};
    fillers.fill(Filler);
    std::memcpy(block.Bytes(CommandsOffset), fillers.data(), sizeof(fillers));
    Store(block, MarkerOffset, 0);
    Store(block, LabelOffset, 1);
    Store(block, GateOffset, 0);
    const auto commands = block.At(CommandsOffset);
    const auto label = block.At(LabelOffset);
    const auto gate = block.At(GateOffset);
    const auto marker = block.At(MarkerOffset);
    auto first = Pm4(0x3c, {0x13, Low(gate), High(gate), 1, 0xffffffffu, 0x19});
    const auto dispatch = Dispatching(block, Pm4(0x37, {0x500u, Low(label), High(label), 0}));
    first.insert(first.end(), dispatch.begin(), dispatch.end());
    const auto branch = Pm4(0x3f, {1, 0, 0, 0, 0, 0, 0, Low(commands), High(commands) & 0xffffu, CommandWords, 0, 0, 0});
    first.insert(first.end(), branch.begin(), branch.end());
    auto second = Pm4(0x3c, {0x13, Low(label), High(label), 1, 0xffffffffu, 0x19});
    const auto done = Pm4(0x37, {0x500u, Low(marker), High(marker), 1});
    second.insert(second.end(), done.begin(), done.end());
    Packet firstDcb{first.data(), static_cast<std::uint32_t>(first.size()), 0, {}};
    Packet secondDcb{second.data(), static_cast<std::uint32_t>(second.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&firstDcb) == 0 && sceAgcDriverSubmitDcb(&secondDcb) == 0, "GPU-written indirect buffer: the label submissions failed");
    Store(block, GateOffset, 1);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    bool blocked = false;
    std::uint32_t early = 0;
    while (!blocked && early == 0 && std::chrono::steady_clock::now() < deadline) {
        std::array<AgcDriver::AwaitedWait, 8> waits{};
        const auto count = AgcDriverAwaitedWaits_nid_postfix(waits.data(), waits.size());
        blocked = std::any_of(waits.begin(), waits.begin() + static_cast<std::ptrdiff_t>(count), [&](const AgcDriver::AwaitedWait& wait) { return wait.address == label && wait.reference == 1; });
        AgcDriver::GuestMemory::FlushGpuWrites(marker, sizeof(std::uint32_t));
        early = Load(block, MarkerOffset);
        if (!blocked && early == 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    AgcDriver::GuestMemory::FlushGpuWrites(label, sizeof(std::uint32_t));
    const auto rewritten = Load(block, LabelOffset);
    Store(block, LabelOffset, 1);
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::FlushGpuWrites(marker, sizeof(std::uint32_t));
    Require(early == 0, "a wait on a label that a rewritten indirect buffer before it sets to 0 passed on the value 1 it held at submission");
    Require(blocked, "the wait after the rewritten indirect buffer did not block on the label within 10 s");
    Require(rewritten == 0, "the wait after the rewritten indirect buffer blocked before the rewritten WRITE_DATA set the label to 0: the label is " + std::to_string(rewritten));
    Require(Load(block, MarkerOffset) == 1, "the wait after the rewritten indirect buffer did not pass once the label was 1 again");
}

void NestedRewrite(const GuestBlock& block, std::uint32_t marker) {
    std::array<std::uint32_t, CommandWords> fillers{};
    fillers.fill(Filler);
    std::memcpy(block.Bytes(CommandsOffset), fillers.data(), sizeof(fillers));
    std::memcpy(block.Bytes(ChildOffset), fillers.data(), sizeof(fillers));
    Store(block, MarkerOffset, 0);
    Store(block, GateOffset, 0);
    const auto parent = block.At(CommandsOffset);
    const auto child = block.At(ChildOffset);
    const auto target = block.At(MarkerOffset);
    const auto gate = block.At(GateOffset);
    auto words = Pm4(0x3c, {0x13, Low(gate), High(gate), 1, 0xffffffffu, 0x19});
    const auto run = Pm4(0x3f, {1, 0, 0, 0, 0, 0, 0, Low(parent), High(parent) & 0xffffu, CommandWords, 0, 0, 0});
    words.insert(words.end(), run.begin(), run.end());
    Packet dcb{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&dcb) == 0, "GPU-written indirect buffer: the nested submission failed");
    auto rewritten = Dispatching(block, Pm4(0x37, {0x500u, Low(target), High(target), marker}), ChildOffset);
    const auto call = Pm4(0x3f, {Low(child), High(child) & 0xffffu, CommandWords});
    rewritten.insert(rewritten.end(), call.begin(), call.end());
    Require(rewritten.size() <= CommandWords, "GPU-written indirect buffer: the rewritten parent does not fit its buffer");
    std::memcpy(block.Bytes(CommandsOffset), rewritten.data(), rewritten.size() * sizeof(std::uint32_t));
    Store(block, GateOffset, 1);
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::FlushGpuWrites(target, sizeof(std::uint32_t));
    const auto seen = Load(block, MarkerOffset);
    Require(seen == marker, "an indirect buffer called from a rewritten COND_INDIRECT_BUFFER ran as it was when the parent was read, not as the dispatch before the call wrote it: the marker is " + std::to_string(seen) + ", expected " + std::to_string(marker));
}

void InvalidRewrite(const GuestBlock& block) {
    std::array<std::uint32_t, CommandWords> fillers{};
    fillers.fill(Filler);
    std::memcpy(block.Bytes(CommandsOffset), fillers.data(), sizeof(fillers));
    Store(block, MarkerOffset, 0);
    Store(block, LabelOffset, 0);
    Store(block, GateOffset, 0);
    const auto commands = block.At(CommandsOffset);
    const auto marker = block.At(MarkerOffset);
    const auto label = block.At(LabelOffset);
    const auto gate = block.At(GateOffset);
    auto words = Pm4(0x3c, {0x13, Low(gate), High(gate), 1, 0xffffffffu, 0x19});
    const auto run = Pm4(0x3f, {1, 0, 0, 0, 0, 0, 0, Low(commands), High(commands) & 0xffffu, CommandWords, 0, 0, 0});
    words.insert(words.end(), run.begin(), run.end());
    const auto after = Pm4(0x37, {0x500u, Low(label), High(label), 1});
    words.insert(words.end(), after.begin(), after.end());
    Packet dcb{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&dcb) == 0, "GPU-written indirect buffer: the submission of the invalid rewrite failed");
    std::vector<std::uint32_t> rewritten{0x00000000u, 0x00002000u};
    const auto store = Pm4(0x37, {0x500u, Low(marker), High(marker), 1});
    rewritten.insert(rewritten.end(), store.begin(), store.end());
    std::memcpy(block.Bytes(CommandsOffset), rewritten.data(), rewritten.size() * sizeof(std::uint32_t));
    Store(block, GateOffset, 1);
    try {
        AgcDriverWaitIdle_nid_postfix();
    } catch (const std::exception& error) {
        throw std::runtime_error(std::string("a COND_INDIRECT_BUFFER rewritten after its submission with contents that do not validate failed the queue instead of running as it was copied: ") + error.what());
    }
    AgcDriver::GuestMemory::FlushGpuWrites(label, sizeof(std::uint32_t));
    AgcDriver::GuestMemory::FlushGpuWrites(marker, sizeof(std::uint32_t));
    Require(Load(block, LabelOffset) == 1, "the packet after a rewritten COND_INDIRECT_BUFFER whose contents do not validate did not run");
    Require(Load(block, MarkerOffset) == 0, "a rewritten COND_INDIRECT_BUFFER whose contents do not validate ran part of them instead of running as it was copied");
}

void Run(const GuestBlock& block, std::uint32_t marker, bool conditional) {
    std::array<std::uint32_t, CommandWords> fillers{};
    fillers.fill(Filler);
    std::memcpy(block.Bytes(CommandsOffset), fillers.data(), sizeof(fillers));
    std::memset(block.Bytes(MarkerOffset), 0, sizeof(std::uint32_t));
    const auto commands = block.At(CommandsOffset);
    const auto target = block.At(MarkerOffset);
    const auto writeData = Pm4(0x37, {0x500u, Low(target), High(target), marker});
    auto words = Dispatching(block, writeData);
    const auto run = conditional ? Pm4(0x3f, {1, 0, 0, 0, 0, 0, 0, Low(commands), High(commands) & 0xffffu, CommandWords, 0, 0, 0}) : Pm4(0x3f, {Low(commands), High(commands) & 0xffffu, CommandWords});
    words.insert(words.end(), run.begin(), run.end());
    Packet dcb{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&dcb) == 0, "GPU-written indirect buffer: the submission failed");
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::FlushGpuWrites(commands, CommandWords * sizeof(std::uint32_t));
    AgcDriver::GuestMemory::FlushGpuWrites(target, sizeof(std::uint32_t));
    std::array<std::uint32_t, 5> written{};
    std::memcpy(written.data(), block.Bytes(CommandsOffset), sizeof(written));
    Require(std::equal(written.begin(), written.end(), writeData.begin()), "GPU-written indirect buffer: the dispatch did not write its WRITE_DATA packet into the command buffer");
    std::uint32_t seen = 0;
    std::memcpy(&seen, block.Bytes(MarkerOffset), sizeof(seen));
    const std::string kind = conditional ? "a COND_INDIRECT_BUFFER" : "an INDIRECT_BUFFER";
    Require(seen == marker, kind + " ran the command buffer as it was at submission, not as the dispatch before it wrote it: the marker is " + std::to_string(seen) + ", the WRITE_DATA the dispatch wrote stores " + std::to_string(marker));
}

}

struct Tickets {
    std::mutex mutex;
    std::condition_variable changed;
    std::array<std::uint64_t, 16> issued{};
    std::array<std::uint64_t, 16> completed{};
    std::vector<std::uint64_t> waited;
    int timedOut = 0;
    int ready = 0;
};

class TicketWait final : public AgcDriver::IRenderingWait {
public:
    TicketWait(std::shared_ptr<Tickets> tickets, std::uint32_t index, std::uint64_t ticket) : tickets(std::move(tickets)), index(index), ticket(ticket) {}
    void Wait() override {
        std::unique_lock lock(tickets->mutex);
        tickets->waited.push_back(ticket);
        if (!tickets->changed.wait_for(lock, std::chrono::seconds(3), [&] { return tickets->completed[index] >= ticket; })) ++tickets->timedOut;
    }
private:
    std::shared_ptr<Tickets> tickets;
    std::uint32_t index;
    std::uint64_t ticket;
};

class TicketFlip final : public AgcDriver::IFlipRequest {
public:
    TicketFlip(std::shared_ptr<Tickets> tickets, std::uint32_t index, std::uint64_t ticket) : tickets(std::move(tickets)), index(index), ticket(ticket) {}
    void GpuReady(const std::shared_ptr<AgcDriver::FrameTiming>&) override {
        {
            std::lock_guard lock(tickets->mutex);
            tickets->completed[index] = std::max(tickets->completed[index], ticket);
            ++tickets->ready;
        }
        tickets->changed.notify_all();
    }
    void Fail(std::exception_ptr) noexcept override {}
private:
    std::shared_ptr<Tickets> tickets;
    std::uint32_t index;
    std::uint64_t ticket;
};

class TicketOutput final : public AgcDriver::IVideoOutput {
public:
    std::shared_ptr<Tickets> tickets = std::make_shared<Tickets>();
    std::shared_ptr<AgcDriver::IRenderingWait> CaptureRenderingWait(std::uint32_t index) override {
        std::lock_guard lock(tickets->mutex);
        return std::make_shared<TicketWait>(tickets, index, tickets->issued.at(index));
    }
    std::shared_ptr<AgcDriver::IFlipRequest> Reserve(const AgcDriver::FlipInfo& info) override {
        std::lock_guard lock(tickets->mutex);
        return std::make_shared<TicketFlip>(tickets, info.index, ++tickets->issued.at(info.index));
    }
    void Fail(std::exception_ptr) noexcept override {}
};

void RewrittenWaitBeforeOwnFlip(const GuestBlock& block) {
    constexpr std::uint32_t Handle = 7;
    const std::array<std::uint32_t, 4> renderingWait{0xc0021018u, Handle, 0, 0};
    const std::array<std::uint32_t, 6> flip{0xc004105cu, Handle, 0, 1, 0, 0};
    auto output = std::make_shared<TicketOutput>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);
    std::vector<std::uint32_t> first(renderingWait.begin(), renderingWait.end());
    first.insert(first.end(), flip.begin(), flip.end());
    Packet firstDcb{first.data(), static_cast<std::uint32_t>(first.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&firstDcb) == 0, "GPU-written indirect buffer: the first flip submission failed");
    AgcDriverWaitIdle_nid_postfix();
    std::array<std::uint32_t, CommandWords> fillers{};
    fillers.fill(Filler);
    std::memcpy(block.Bytes(CommandsOffset), fillers.data(), sizeof(fillers));
    Store(block, GateOffset, 0);
    const auto commands = block.At(CommandsOffset);
    const auto gate = block.At(GateOffset);
    auto second = Pm4(0x3c, {0x13, Low(gate), High(gate), 1, 0xffffffffu, 0x19});
    const auto branch = Pm4(0x3f, {1, 0, 0, 0, 0, 0, 0, Low(commands), High(commands) & 0xffffu, CommandWords, 0, 0, 0});
    second.insert(second.end(), branch.begin(), branch.end());
    second.insert(second.end(), flip.begin(), flip.end());
    Packet secondDcb{second.data(), static_cast<std::uint32_t>(second.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&secondDcb) == 0, "GPU-written indirect buffer: the second flip submission failed");
    std::memcpy(block.Bytes(CommandsOffset), renderingWait.data(), sizeof(renderingWait));
    const auto opened = std::chrono::steady_clock::now();
    Store(block, GateOffset, 1);
    AgcDriverWaitIdle_nid_postfix();
    const auto took = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - opened).count();
    int timedOut = 0;
    int ready = 0;
    std::vector<std::uint64_t> waited;
    {
        std::lock_guard lock(output->tickets->mutex);
        timedOut = output->tickets->timedOut;
        ready = output->tickets->ready;
        waited = output->tickets->waited;
    }
    AgcDriverUnregisterVideoOutput_nid_postfix(Handle, output);
    Require(timedOut == 0, "a rendering wait written into an indirect buffer after its submission waited for the flip after that buffer in the same submission (" + std::to_string(took) + " ms, timed out after 3 s)");
    Require(ready == 2, "the two flips did not complete: " + std::to_string(ready) + " completed");
    std::string tickets;
    for (const auto ticket : waited) tickets += (tickets.empty() ? "" : ", ") + std::to_string(ticket);
    Require(waited == std::vector<std::uint64_t>{0, 1}, "the rendering waits did not wait for the first submission's ticket 0 and then the rewritten wait's ticket 1 of the first flip; they waited for {" + tickets + "}");
}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        GuestBlock block;
        std::memcpy(block.Bytes(0), Code.data(), sizeof(Code));
        Run(block, 0x5eed0001u, true);
        Run(block, 0x5eed0002u, false);
        HeldWaitAfterRewrite(block);
        NestedRewrite(block, 0x5eed0003u);
        InvalidRewrite(block);
        RewrittenWaitBeforeOwnFlip(block);
        AgcDriverShutdown_nid_postfix();
        std::puts("GPU-written indirect buffer: indirect buffers run what the dispatch before them wrote");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { AgcDriverShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}
