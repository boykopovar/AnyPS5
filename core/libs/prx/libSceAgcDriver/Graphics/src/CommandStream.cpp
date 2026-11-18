#include "prx/libSceAgcDriver/Graphics/include/CommandStream.hpp"
#include "prx/libSceAgcDriver/Graphics/include/CommandList.hpp"
#include "prx/libSceAgcDriver/Graphics/include/SealedBatch.hpp"
#include "prx/libc/include/HostThreadSlot.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace AgcDriver::Graphics {

namespace {

struct ReplayThreadTag;
using ReplayThreadSlot = HostThreadSlot<bool, ReplayThreadTag>;

[[noreturn]] void refuseReplayThreadAppend() {
    std::fputs("AGC graphics: a command was appended to a stream from a replay thread; replay reads only its stream and the real functions\n", stderr);
    std::fflush(stderr);
    std::abort();
}

}

bool OnReplayThread() noexcept {
    return ReplayThreadSlot::Get();
}

ReplayThreadScope::ReplayThreadScope() noexcept : previous(ReplayThreadSlot::Get()) {
    ReplayThreadSlot::Set(true);
}

ReplayThreadScope::~ReplayThreadScope() {
    ReplayThreadSlot::Set(previous);
}

CommandStream::Chunk::Chunk(std::size_t capacity) : bytes(new std::byte[capacity]), capacity(capacity) {}

CommandStream::CommandStream(const CommandFunctions& functions, WorkBudget* budget, StreamMaterializer* materializer) : functions(functions), budget(budget), materializer(materializer), cap(budget != nullptr ? budget->StreamCap() : SIZE_MAX) {}

CommandStream::~CommandStream() = default;

std::byte* CommandStream::reserve(Run run, std::size_t payloadBytes) {
    if (OnReplayThread()) refuseReplayThreadAppend();
    try {
        if (Sealed()) throw std::logic_error("AGC graphics: a command was appended to a sealed stream");
        if (materialized != VK_NULL_HANDLE) throw std::logic_error("AGC graphics: a command was appended to a stream after it was demoted to a command buffer");
        if (Poisoned()) std::rethrow_exception(Reason());
        const std::size_t need = RecordHeader + payloadBytes;
        if (chunks.empty() || chunks.back()->capacity - chunks.back()->used < need) {
            const std::size_t capacity = std::max(ChunkBytes, need);
            if (capacity > cap - std::min(charged, cap)) return nullptr;
            chunks.push_back(std::make_unique<Chunk>(capacity));
            charged += capacity;
            if (budget != nullptr) budget->Charge(capacity);
        }
        auto& chunk = *chunks.back();
        auto* at = chunk.bytes.get() + chunk.used;
        const Record record{run, payloadBytes};
        std::memcpy(at, &record, sizeof(Record));
        chunk.used += need;
        bytes.fetch_add(need, std::memory_order_relaxed);
        return at + RecordHeader;
    } catch (...) {
        Poison(std::current_exception());
        throw;
    }
}

std::unique_ptr<std::byte[]> CommandStream::pastCap(std::size_t payloadBytes) {
    Demote("stream byte cap");
    try {
        return std::unique_ptr<std::byte[]>(new std::byte[std::max<std::size_t>(payloadBytes, RecordAlign)]);
    } catch (...) {
        Poison(std::current_exception());
        throw;
    }
}

void CommandStream::runPastCap(Run run, const std::byte* payload) {
    run(payload, materialized, functions);
}

void CommandStream::Poison(std::exception_ptr why) noexcept {
    {
        std::lock_guard lock(reasonMutex);
        if (reason == nullptr) reason = why;
    }
    poisoned.store(true, std::memory_order_release);
}

std::exception_ptr CommandStream::Reason() const {
    std::lock_guard lock(reasonMutex);
    return reason;
}

VkCommandBuffer CommandStream::Demote(const char* command, VkStructureType chained) {
    if (materialized != VK_NULL_HANDLE) return materialized;
    if (OnReplayThread()) refuseReplayThreadAppend();
    try {
        if (Sealed()) throw std::logic_error("AGC graphics: a sealed stream was demoted to a command buffer");
        if (Poisoned()) std::rethrow_exception(Reason());
        if (materializer == nullptr) throw std::logic_error(std::string("AGC graphics: ") + command + " needs a command buffer, and this stream has no way to demote to one");
        const auto target = materializer->Begin(*this);
        {
            const ReplayThreadScope scope;
            ForEach([&](Run run, const std::byte* payload) { run(payload, target, functions); });
        }
        materialized = target;
        demotion = Demotion{command, chained};
        chunks.clear();
        return target;
    } catch (...) {
        Poison(std::current_exception());
        throw;
    }
}

void CommandStream::ReleaseMemory() noexcept {
    chunks.clear();
}

void Replay(const CommandStream& stream, VkCommandBuffer target, const CommandFunctions& real) {
    if (!stream.Sealed()) throw std::logic_error("AGC graphics: a stream was replayed before it was sealed");
    if (stream.Poisoned()) std::rethrow_exception(stream.Reason());
    if (stream.Materialized() != VK_NULL_HANDLE) throw std::logic_error("AGC graphics: a stream demoted to a command buffer was replayed");
    if (CommandStream::IsHandle(target)) throw std::logic_error("AGC graphics: a stream was replayed into a stream handle instead of a command buffer");
    const ReplayThreadScope scope;
    stream.ForEach([&](CommandStream::Run run, const std::byte* payload) { run(payload, target, real); });
}

}
