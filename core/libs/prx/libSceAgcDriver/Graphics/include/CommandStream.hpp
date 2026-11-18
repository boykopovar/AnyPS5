#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_COMMANDSTREAM_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_COMMANDSTREAM_HPP

#include <vulkan/vulkan.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <memory>
#include <mutex>
#include <new>
#include <span>
#include <type_traits>
#include <vector>

namespace AgcDriver::Graphics {

struct CommandFunctions;
class WorkBudget;
class CommandStream;

class StreamMaterializer {
public:
    virtual VkCommandBuffer Begin(CommandStream& stream) = 0;

protected:
    ~StreamMaterializer() = default;
};

struct Demotion {
    const char* command = nullptr;
    VkStructureType chained = VK_STRUCTURE_TYPE_MAX_ENUM;
};

bool OnReplayThread() noexcept;

class ReplayThreadScope {
public:
    ReplayThreadScope() noexcept;
    ~ReplayThreadScope();
    ReplayThreadScope(const ReplayThreadScope&) = delete;
    ReplayThreadScope& operator=(const ReplayThreadScope&) = delete;

private:
    bool previous;
};

class CommandStream {
public:
    using Run = void (*)(const std::byte* payload, VkCommandBuffer commands, const CommandFunctions& functions);
    static constexpr std::size_t RecordAlign = 16;
    static constexpr std::size_t ChunkBytes = 64 * 1024;
    static constexpr std::size_t Aligned(std::size_t bytes) { return (bytes + RecordAlign - 1) / RecordAlign * RecordAlign; }

    explicit CommandStream(const CommandFunctions& functions, WorkBudget* budget = nullptr, StreamMaterializer* materializer = nullptr);
    ~CommandStream();
    CommandStream(const CommandStream&) = delete;
    CommandStream& operator=(const CommandStream&) = delete;

    VkCommandBuffer Handle() const noexcept { return reinterpret_cast<VkCommandBuffer>(reinterpret_cast<std::uintptr_t>(this) | HandleTag); }
    static bool IsHandle(VkCommandBuffer commands) noexcept { return (reinterpret_cast<std::uintptr_t>(commands) & HandleTag) != 0; }
    static CommandStream& From(VkCommandBuffer commands) noexcept { return *reinterpret_cast<CommandStream*>(reinterpret_cast<std::uintptr_t>(commands) & ~HandleTag); }

    template<typename... A>
    static constexpr void CheckPlain() {
        static_assert(((!std::is_pointer_v<A> || !std::is_const_v<std::remove_pointer_t<A>>) && ...), "AGC graphics: a streamed command copies its arguments by value, so a pointer-to-const argument would be read after the caller's storage is gone; stream it with its pointed-to data");
    }

    template<typename Payload, typename... T>
    void Emplace(Run run, const Payload& payload, std::span<const T>... arrays) {
        static_assert((std::is_trivially_copyable_v<T> && ...), "AGC graphics: a streamed command's arrays are copied as bytes, so their elements must be trivially copyable; keep owners in batch storage");
        static_assert(std::is_trivially_destructible_v<Payload>, "AGC graphics: a streamed command's payload is never destroyed, so it must be trivially destructible; keep owners in batch storage");
        static_assert(std::is_nothrow_copy_constructible_v<Payload>, "AGC graphics: a streamed command's payload must be built without throwing");
        const std::size_t bytes = Aligned(sizeof(Payload)) + (Aligned(sizeof(T) * arrays.size()) + ... + 0);
        EmplaceBytes(run, bytes, [&](std::byte* at) noexcept {
            new (at) Payload(payload);
            at += Aligned(sizeof(Payload));
            ((at = copyInto(at, arrays)), ...);
        });
    }

    template<typename Fill>
    void EmplaceBytes(Run run, std::size_t payloadBytes, Fill&& fill) {
        static_assert(std::is_nothrow_invocable_v<Fill, std::byte*>, "AGC graphics: a streamed command's bytes must be written without throwing");
        payloadBytes = Aligned(payloadBytes);
        if (auto* at = reserve(run, payloadBytes); at != nullptr) [[likely]] {
            fill(at);
            ops.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        auto scratch = pastCap(payloadBytes);
        fill(scratch.get());
        runPastCap(run, scratch.get());
    }

    const CommandFunctions& Functions() const noexcept { return functions; }
    VkCommandBuffer Materialized() const noexcept { return materialized; }
    const Demotion& DemotedBy() const noexcept { return demotion; }
    VkCommandBuffer Demote(const char* command, VkStructureType chained = VK_STRUCTURE_TYPE_MAX_ENUM);
    void Poison(std::exception_ptr reason) noexcept;
    bool Poisoned() const noexcept { return poisoned.load(std::memory_order_acquire); }
    std::exception_ptr Reason() const;
    void Seal() noexcept { sealed.store(true, std::memory_order_release); }
    bool Sealed() const noexcept { return sealed.load(std::memory_order_acquire); }
    std::uint64_t Ops() const noexcept { return ops.load(std::memory_order_relaxed); }
    std::size_t Bytes() const noexcept { return bytes.load(std::memory_order_relaxed); }
    std::size_t Charged() const noexcept { return charged; }
    std::size_t Cap() const noexcept { return cap; }
    void ReleaseMemory() noexcept;

    template<typename Visit>
    void ForEach(Visit&& visit) const {
        for (const auto& chunk : chunks) {
            std::size_t offset = 0;
            while (offset < chunk->used) {
                Record record;
                std::memcpy(&record, chunk->bytes.get() + offset, sizeof(Record));
                visit(record.run, chunk->bytes.get() + offset + RecordHeader);
                offset += RecordHeader + record.bytes;
            }
        }
    }

private:
    static constexpr std::uintptr_t HandleTag = 1;
    struct Record {
        Run run;
        std::size_t bytes;
    };
    static constexpr std::size_t RecordHeader = (sizeof(Record) + RecordAlign - 1) / RecordAlign * RecordAlign;
    struct Chunk {
        explicit Chunk(std::size_t capacity);
        std::unique_ptr<std::byte[]> bytes;
        std::size_t capacity;
        std::size_t used = 0;
    };
    std::byte* reserve(Run run, std::size_t payloadBytes);
    std::unique_ptr<std::byte[]> pastCap(std::size_t payloadBytes);
    void runPastCap(Run run, const std::byte* payload);
    template<typename T>
    static std::byte* copyInto(std::byte* at, std::span<const T> array) noexcept {
        if (!array.empty()) std::memcpy(at, array.data(), sizeof(T) * array.size());
        return at + Aligned(sizeof(T) * array.size());
    }
    const CommandFunctions& functions;
    WorkBudget* budget;
    StreamMaterializer* materializer;
    VkCommandBuffer materialized = VK_NULL_HANDLE;
    Demotion demotion;
    std::vector<std::unique_ptr<Chunk>> chunks;
    std::size_t charged = 0;
    std::size_t cap;
    std::atomic<std::uint64_t> ops{0};
    std::atomic<std::size_t> bytes{0};
    std::atomic<bool> sealed{false};
    std::atomic<bool> poisoned{false};
    mutable std::mutex reasonMutex;
    std::exception_ptr reason;
};

void Replay(const CommandStream& stream, VkCommandBuffer target, const CommandFunctions& real);

}

#endif
