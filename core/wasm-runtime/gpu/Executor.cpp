#include "Backend.hpp"
#include "Shaders.hpp"
#include "prx/libSceAgcDriver/Eq/include/Event.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderPreparationScope.hpp"
#include "prx/libSceAgcDriver/Execution/include/VideoOutput.hpp"
#include "SceTypes.hpp"
#include "wasm-runtime/Guest.hpp"
#include <atomic>
#include <bit>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <exception>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace WasmGpu {
namespace {

using AgcDriver::FlipPacketHeader;
using AgcDriver::RenderingWaitPacketHeader;

void Require(bool condition, const std::string& reason) {
    if (!condition) throw std::runtime_error("AGC driver: " + reason);
}

bool Trace() {
    static const bool enabled = std::getenv("APS5_TRACE_GPU") != nullptr;
    return enabled;
}

std::uint32_t Opcode(std::uint32_t header) {
    return (header >> 8u) & 0xffu;
}

bool EopRelease(std::span<const std::uint32_t> words) {
    return words.size() >= 8 && (words[0] >> 30u) == 3u && AgcDriver::Pm4::PacketWords(words[0]) == 8u && Opcode(words[0]) == 0x49u && ((words[2] >> 24u) & 7u) == 2u;
}

bool EndOfPipeInterrupt(std::span<const std::uint32_t> packet) {
    if (Opcode(packet[0]) != 0x49u || packet.size() < 3) return false;
    const auto select = (packet[2] >> 24u) & 7u;
    return select != 0 && select != 3;
}

struct Submission {
    std::uint32_t queue = 0;
    std::uint64_t serial = 0;
    bool suspend = false;
    std::vector<std::uint32_t> commands;
    std::map<std::size_t, std::size_t> conditionalEnds;
    std::map<std::size_t, std::shared_ptr<AgcDriver::IFlipRequest>> flips;
    std::map<std::size_t, std::shared_ptr<AgcDriver::IRenderingWait>> renderingWaits;
    std::map<std::size_t, std::vector<std::uint32_t>> registerLists;
    const std::uint32_t* rewindTail = nullptr;
    std::size_t rewindWords = 0;
};

struct EopFlip {
    std::uint32_t handle;
    std::shared_ptr<AgcDriver::IFlipRequest> request;
};

struct Worker {
    std::deque<Submission> pending;
    std::thread thread;
    bool busy = false;
};

class Executor {
public:
    static Executor& Get() {
        static Executor executor;
        return executor;
    }

    void Submit(const Packet* packet, std::uint32_t queue) {
        rethrow();
        Require(queue == 0 || (queue >= 0x20 && queue < 0x58), "unsupported compute queue");
        AgcDriver::GuestMemory::CheckRange(packet, sizeof(Packet), alignof(Packet));
        const auto descriptor = *packet;
        Require(descriptor.flags == 0, "nonzero submission flags are not implemented");
        Submission submission;
        submission.queue = queue;
        if (descriptor.dw_num != 0) {
            AgcDriver::GuestMemory::CheckRange(descriptor.addr, static_cast<std::size_t>(descriptor.dw_num) * sizeof(std::uint32_t), alignof(std::uint32_t));
            std::size_t budget = std::size_t{1} << 26u;
            copySegment(submission, descriptor.addr, descriptor.dw_num, budget);
        }
        readRegisterLists(submission);
        waitForFlipRoom(submission);
        {
            std::lock_guard lock(mutex);
            reserveOutputs(submission);
            submission.serial = ++accepted;
            if (submission.serial == 1 || Trace()) std::fprintf(stderr, "[gpu] submission %llu on queue 0x%x: %zu dwords\n", static_cast<unsigned long long>(submission.serial), queue, submission.commands.size());
            enqueue(std::move(submission));
        }
        changed.notify_all();
    }

    void SuspendPoint() {
        Submission boundary;
        boundary.suspend = true;
        std::lock_guard lock(mutex);
        boundary.serial = ++accepted;
        enqueue(std::move(boundary));
        changed.notify_all();
    }

    void WaitIdle() {
        std::unique_lock lock(mutex);
        changed.wait(lock, [&] {
            if (failure) return true;
            for (const auto& [queue, worker] : workers) {
                if (!worker.pending.empty() || worker.busy) return false;
            }
            return true;
        });
        if (failure) std::rethrow_exception(failure);
    }

    void RegisterOutput(std::uint32_t handle, const std::shared_ptr<AgcDriver::IVideoOutput>& output) {
        Require(output != nullptr, "null video output");
        std::lock_guard lock(mutex);
        Require(outputs.emplace(handle, output).second, "video output already registered");
    }

    void UnregisterOutput(std::uint32_t handle, const std::shared_ptr<AgcDriver::IVideoOutput>& output) {
        std::lock_guard lock(mutex);
        const auto found = outputs.find(handle);
        Require(found != outputs.end() && found->second == output, "video output registration mismatch");
        std::erase_if(eopFlips, [handle](const auto& entry) { return entry.second.handle == handle; });
        outputs.erase(found);
    }

    std::uint32_t ReserveEopFlip(const AgcDriver::FlipInfo& info) {
        std::lock_guard lock(mutex);
        Require(nextEopToken <= AgcDriver::EopFlipTokenLimit, "EOP flip token space exhausted");
        const auto output = outputs.find(info.handle);
        Require(output != outputs.end(), "EOP flip references an unregistered video output");
        auto request = output->second->TryReserve(info);
        if (!request) return 0;
        const auto token = nextEopToken++;
        eopFlips.emplace(token, EopFlip{info.handle, std::move(request)});
        return token;
    }

private:
    void rethrow() {
        std::lock_guard lock(mutex);
        if (failure) std::rethrow_exception(failure);
    }

    bool copySegment(Submission& submission, const std::uint32_t* guest, std::size_t words, std::size_t& budget) {
        Require(words <= budget, "command buffer jumps exceed the copy limit");
        budget -= words;
        std::vector<std::pair<std::size_t, std::size_t>> guarded;
        const auto reach = [&](std::size_t cursor) {
            std::erase_if(guarded, [&](const auto& range) {
                if (range.first != cursor) return false;
                submission.conditionalEnds.emplace(range.second, submission.commands.size());
                return true;
            });
        };
        for (std::size_t cursor = 0; cursor < words;) {
            const auto header = guest[cursor];
            if (AgcDriver::Pm4::FillerPacket(header)) {
                reach(cursor);
                submission.commands.push_back(header);
                ++cursor;
                continue;
            }
            const auto count = (header & 0xc0000000u) == 0xc0000000u ? AgcDriver::Pm4::PacketWords(header) : words - cursor;
            if ((header & 0xc0000000u) != 0xc0000000u || count > words - cursor) {
                submission.commands.insert(submission.commands.end(), guest + cursor, guest + words);
                return false;
            }
            reach(cursor);
            const auto opcode = Opcode(header);
            if (opcode == 0x3fu) {
                const bool conditional = count == 14;
                Require(conditional || count == 4, "invalid INDIRECT_BUFFER size");
                const auto low = conditional ? guest[cursor + 8] : guest[cursor + 1];
                const auto high = conditional ? guest[cursor + 9] : guest[cursor + 2];
                const auto size = conditional ? guest[cursor + 10] : guest[cursor + 3];
                const auto* target = reinterpret_cast<const std::uint32_t*>(static_cast<std::uintptr_t>(low & ~3u) | (static_cast<std::uintptr_t>(high & 0xffffu) << 32u));
                const std::size_t targetWords = size & 0xfffffu;
                const bool chain = !conditional && (size & (1u << 20u)) != 0;
                Require(!AgcDriver::Pm4::Predicated(header), "predicated indirect buffers are not implemented in WebAssembly");
                if (targetWords != 0) {
                    AgcDriver::GuestMemory::CheckRange(target, targetWords * sizeof(std::uint32_t), alignof(std::uint32_t));
                    if (copySegment(submission, target, targetWords, budget)) return true;
                }
                if (chain) return false;
                cursor += count;
                continue;
            }
            if (opcode == 0x22u && count == 5) guarded.emplace_back(cursor + count + AgcDriver::Pm4::ConditionalWords(std::span(guest + cursor, count)), submission.commands.size());
            submission.commands.insert(submission.commands.end(), guest + cursor, guest + cursor + count);
            cursor += count;
            if (opcode == 0x59u) {
                submission.rewindTail = guest + cursor;
                submission.rewindWords = words - cursor;
                return true;
            }
        }
        reach(words);
        Require(guarded.empty(), "conditional execution range exceeds its command buffer");
        return false;
    }

    void readRegisterLists(Submission& submission) {
        for (std::size_t cursor = 0; cursor < submission.commands.size(); cursor += AgcDriver::Pm4::PacketWords(submission.commands[cursor])) {
            const auto header = submission.commands[cursor];
            if ((header >> 30u) != 3u || !AgcDriver::Pm4::IndirectRegisterOpcode(Opcode(header))) continue;
            submission.registerLists.emplace(cursor, AgcDriver::Pm4::ReadIndirectRegisters(std::span<const std::uint32_t>(submission.commands).subspan(cursor, AgcDriver::Pm4::PacketWords(header))));
        }
    }

    void waitForFlipRoom(const Submission& submission) {
        for (std::size_t cursor = 0; cursor < submission.commands.size(); cursor += AgcDriver::Pm4::PacketWords(submission.commands[cursor])) {
            if (submission.commands[cursor] != FlipPacketHeader) continue;
            std::shared_ptr<AgcDriver::IVideoOutput> output;
            {
                std::lock_guard lock(mutex);
                const auto found = outputs.find(submission.commands[cursor + 1]);
                Require(found != outputs.end(), "flip references an unregistered video output");
                output = found->second;
            }
            output->WaitForFlipRoom();
        }
    }

    void reserveOutputs(Submission& submission) {
        std::set<std::uint32_t> eopTokens;
        for (std::size_t cursor = 0; cursor < submission.commands.size(); cursor += AgcDriver::Pm4::PacketWords(submission.commands[cursor])) {
            const auto* words = submission.commands.data() + cursor;
            if (words[0] == RenderingWaitPacketHeader) {
                const auto output = outputs.find(words[1]);
                Require(output != outputs.end(), "rendering wait references an unregistered video output");
                submission.renderingWaits.emplace(cursor, output->second->CaptureRenderingWait(words[2]));
            }
            if (EopRelease(std::span(submission.commands).subspan(cursor))) {
                const auto token = words[7];
                const auto flip = eopFlips.find(token);
                if (flip != eopFlips.end()) {
                    Require(eopTokens.insert(token).second, "EOP flip token repeated in a submission");
                    submission.flips.emplace(cursor, flip->second.request);
                }
            }
            if (words[0] == FlipPacketHeader) {
                const auto output = outputs.find(words[1]);
                Require(output != outputs.end(), "flip references an unregistered video output");
                const AgcDriver::FlipInfo info{words[1], std::bit_cast<std::int32_t>(words[2]), words[3], std::bit_cast<std::int64_t>(static_cast<std::uint64_t>(words[4]) | (static_cast<std::uint64_t>(words[5]) << 32u))};
                auto request = output->second->Reserve(info);
                Require(request != nullptr, "video output returned a null flip reservation");
                submission.flips.emplace(cursor, std::move(request));
            }
        }
        for (const auto token : eopTokens) eopFlips.erase(token);
    }

    void enqueue(Submission submission) {
        const auto queue = submission.queue;
        auto& worker = workers[queue];
        worker.pending.push_back(std::move(submission));
        if (!worker.thread.joinable()) worker.thread = std::thread([this, queue] { run(queue); });
    }

    void run(std::uint32_t queue) {
        for (;;) {
            Submission submission;
            {
                std::unique_lock lock(mutex);
                auto& worker = workers[queue];
                changed.wait(lock, [&] { return !worker.pending.empty(); });
                submission = std::move(worker.pending.front());
                worker.pending.pop_front();
                worker.busy = true;
            }
            try {
                execute(submission);
            } catch (const std::exception& error) {
                WasmRuntime::GuestFault("GPU queue 0x%x failed in submission %llu: %s", queue, static_cast<unsigned long long>(submission.serial), error.what());
            }
            {
                std::lock_guard lock(mutex);
                workers[queue].busy = false;
            }
            changed.notify_all();
        }
    }

    void finishFlip(std::shared_ptr<AgcDriver::IFlipRequest> request, std::uint32_t queue) {
        Backend().AfterWork([request = std::move(request), queue] {
            AgcDriverDeliverEopInterrupt(queue);
            request->GpuReady(nullptr);
        });
    }

    void execute(Submission& submission) {
        if (submission.suspend) return;
        auto& state = queues[submission.queue];
        const auto& commands = submission.commands;
        for (std::size_t cursor = 0; cursor < commands.size();) {
            const auto header = commands[cursor];
            if (AgcDriver::Pm4::FillerPacket(header)) {
                ++cursor;
                continue;
            }
            const auto count = AgcDriver::Pm4::PacketWords(header);
            Require(count <= commands.size() - cursor, "packet " + AgcDriver::Pm4::Name(header) + " exceeds its command buffer");
            const auto packet = std::span(commands).subspan(cursor, count);
            const auto opcode = Opcode(header);
            auto next = cursor + count;
            AgcDriver::GuestMemory::SetCurrentPacket(header == FlipPacketHeader ? 0xffffu : opcode, submission.queue);
            if (AgcDriver::Pm4::Predicated(header) && state.predication.operation != 0 && !AgcDriver::Pm4::PredicationPasses(state)) {
                const auto end = submission.conditionalEnds.find(cursor);
                cursor = end != submission.conditionalEnds.end() ? end->second : next;
                continue;
            }
            if (opcode == 0x3f && (header >> 30u) == 3u) {
                cursor = next;
                continue;
            }
            if (header == RenderingWaitPacketHeader) {
                submission.renderingWaits.at(cursor)->Wait();
            } else if (const auto flip = submission.flips.find(cursor); flip != submission.flips.end()) {
                if (opcode == 0x49) AgcDriver::Pm4::Execute(packet, state);
                finishFlip(flip->second, submission.queue);
            } else if (opcode == 0x15 || opcode == 0x16) {
                Backend().Dispatch(submission.queue, packet, state);
            } else if (AgcDriver::Pm4::DrawOpcode(opcode)) {
                Backend().Draw(submission.queue, packet, state);
            } else if (opcode == 0x22) {
                if (AgcDriver::Pm4::ReadCondition(packet) == 0) next = submission.conditionalEnds.at(cursor);
            } else if (opcode == 0x42 || opcode == 0x46 || opcode == 0x58) {
            } else if (AgcDriver::Pm4::IndirectRegisterOpcode(opcode)) {
                AgcDriver::Pm4::ExecuteIndirectRegisters(packet, submission.registerLists.at(cursor), state);
            } else {
                AgcDriver::Pm4::Execute(packet, state);
                if (EndOfPipeInterrupt(packet)) {
                    const auto queue = submission.queue;
                    Backend().AfterWork([queue] { AgcDriverDeliverEopInterrupt(queue); });
                }
            }
            cursor = next;
        }
        if (submission.rewindTail != nullptr) executeRewindTail(submission);
    }

    void executeRewindTail(const Submission& stalled) {
        std::atomic_ref<std::uint32_t> control(*const_cast<std::uint32_t*>(stalled.rewindTail - 1));
        while ((control.load(std::memory_order_acquire) & AgcDriver::Pm4::RewindValid) == 0) std::this_thread::sleep_for(std::chrono::microseconds(100));
        Submission tail;
        tail.queue = stalled.queue;
        tail.serial = stalled.serial;
        std::size_t budget = std::size_t{1} << 26u;
        copySegment(tail, stalled.rewindTail, stalled.rewindWords, budget);
        readRegisterLists(tail);
        waitForFlipRoom(tail);
        {
            std::lock_guard lock(mutex);
            reserveOutputs(tail);
        }
        execute(tail);
    }

    std::mutex mutex;
    std::condition_variable changed;
    std::map<std::uint32_t, Worker> workers;
    std::map<std::uint32_t, AgcDriver::QueueState> queues;
    std::map<std::uint32_t, std::shared_ptr<AgcDriver::IVideoOutput>> outputs;
    std::map<std::uint32_t, EopFlip> eopFlips;
    std::uint32_t nextEopToken = AgcDriver::EopFlipTokenBase;
    std::uint64_t accepted = 0;
    std::exception_ptr failure;
};

}  // namespace
}  // namespace WasmGpu

void AgcDriver::Submit(const Packet* packet, std::uint32_t queue) {
    WasmGpu::Executor::Get().Submit(packet, queue);
}

extern "C" {

void AgcDriverWaitIdle_nid_postfix() {
    WasmGpu::Executor::Get().WaitIdle();
}

void AgcDriverRaiseWorkerThreadPriority_nid_postfix(const char*) {}

void AgcDriverLockVulkanLoader_nid_postfix() {}

void AgcDriverUnlockVulkanLoader_nid_postfix() {}

void AgcDriverShutdown_nid_postfix() {
    WasmGpu::Executor::Get().WaitIdle();
}

void AgcDriverSuspendPoint_nid_postfix() {
    WasmGpu::Executor::Get().SuspendPoint();
}

void AgcDriverRegisterShader_nid_postfix(const Shader* shader) {
    WasmGpu::RegisterShader(shader);
    WasmGpu::Backend().RegisterShader(shader);
}

void AgcDriverResolveShaderAbi_nid_postfix(const Shader*, std::span<const ShaderRegister>, std::span<const ShaderRegister>) {}

void AgcDriverResolveGraphicsStagesAbi_nid_postfix(std::span<const Shader* const>, std::span<const ShaderRegister>, std::span<const ShaderRegister>) {}

void AgcDriverResolveGraphicsAbi_nid_postfix(const Shader*, const Shader*, std::uint32_t) {}

AgcDriver::DriverDetail::ShaderPreparationTransaction* AgcDriverBeginShaderPreparation_nid_postfix(const Shader* const*, std::size_t) {
    return nullptr;
}

void AgcDriverCommitShaderPreparation_nid_postfix(AgcDriver::DriverDetail::ShaderPreparationTransaction*) {}

void AgcDriverEndShaderPreparation_nid_postfix(AgcDriver::DriverDetail::ShaderPreparationTransaction*) noexcept {}

std::uint32_t AgcDriverReserveEopFlip_nid_postfix(const AgcDriver::FlipInfo& info) {
    return WasmGpu::Executor::Get().ReserveEopFlip(info);
}

void AgcDriverRegisterVideoOutput_nid_postfix(std::uint32_t handle, const std::shared_ptr<AgcDriver::IVideoOutput>& output) {
    WasmGpu::Executor::Get().RegisterOutput(handle, output);
}

void AgcDriverUnregisterVideoOutput_nid_postfix(std::uint32_t handle, const std::shared_ptr<AgcDriver::IVideoOutput>& output) {
    WasmGpu::Executor::Get().UnregisterOutput(handle, output);
}

}

extern "C" void AgcDriverWasmPresentBuffer_nid_postfix(const AgcDriver::DisplayBuffer& buffer) {
    WasmGpu::Backend().PresentBuffer(buffer);
}

extern "C" void AgcDriverWasmPresentClear_nid_postfix(bool opaque) {
    WasmGpu::Backend().PresentClear(opaque);
}
