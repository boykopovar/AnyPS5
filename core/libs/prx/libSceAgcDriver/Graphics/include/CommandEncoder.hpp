#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_COMMANDENCODER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_COMMANDENCODER_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <source_location>
#include <span>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

namespace AgcDriver::Graphics {

class CommandEncoder {
public:
    template<typename TFunction>
    static TFunction Unwrap(TFunction function) { return function; }
    template<typename TFunction>
    static TFunction Unwrap(const TrackedFunction<TFunction>& function) { return function.function; }
    using Run = void (*)(const std::byte* payload, VkCommandBuffer commands, const DeviceFunctions& functions);
    enum class Wait : std::uint8_t { Quiesce, Settle, Open, Close, Lookup, Direct, Behind, ChunkPool, Teardown, Unwind, OtherSites };
    struct WaitSite {
        Wait kind = Wait::OtherSites;
        const char* detail = nullptr;
        std::uint32_t line = 0;
        static WaitSite At(Wait kind, std::source_location where) { return {kind, where.function_name(), where.line()}; }
    };
    struct SiteWaits {
        WaitSite site;
        std::uint64_t waits = 0;
        std::uint64_t waitNs = 0;
        std::uint64_t flushes = 0;
    };
    static constexpr std::size_t TrackedSites = 64;
    static std::string Describe(const WaitSite& site);
    static std::string Report(const std::vector<SiteWaits>& sites, std::size_t limit);
    class Recording {
    public:
        Recording() = default;
        Recording(const Recording&) = delete;
        Recording& operator=(const Recording&) = delete;
        bool Failed() const { return failed.load(std::memory_order_acquire); }
        void Report() {
            if (Failed()) refuse();
        }
        void Finish() {
            if (Failed()) refuse();
        }

    private:
        friend class CommandEncoder;
        [[noreturn]] void deliver();
        [[noreturn]] void refuse();
        std::exception_ptr error;
        std::atomic<bool> failed{false};
        bool reported = false;
    };
    enum class Handoff : std::uint8_t { Always, Adaptive, Never };
    static constexpr std::uint64_t ProbeBlock = 64;
    static constexpr std::uint64_t ProbeWindows = 4 * ProbeBlock;
    static constexpr std::uint64_t SettledSpanMin = 1024;
    static constexpr std::uint64_t SettledSpanMax = 16384;
    struct HandoffChoice {
        bool encode = true;
        bool timed = false;
    };
    struct HandoffDecision {
        bool encode = true;
        std::uint64_t encodedNs = 0;
        std::uint64_t inlineNs = 0;
        std::uint64_t windows = 0;
        std::uint64_t span = 0;
    };
    class HandoffPolicy {
    public:
        HandoffChoice Next() {
            if (Complete()) static_cast<void>(Decide());
            if (!probing) {
                if (settled < span) {
                    ++settled;
                    return {encode, false};
                }
                probing = true;
                probed = 0;
                blockNs = {};
                charged = false;
            }
            return {probeEncodes(), true};
        }
        void ChargeOutside(std::uint64_t ns) {
            if (probing && charged) blockNs[lastBlock] += ns;
        }
        void Charge(std::uint64_t ns) {
            if (!probing || Complete()) return;
            lastBlock = probed / ProbeBlock;
            blockNs[lastBlock] += ns;
            charged = true;
            ++probed;
        }
        bool Complete() const { return probing && probed == ProbeWindows; }
        bool LastEncoded() const { return ((lastBlock % 2) == 0) == encode; }
        HandoffDecision Decide() {
            const auto chosen = std::min(blockNs[0], blockNs[2]);
            const auto other = std::min(blockNs[1], blockNs[3]);
            const auto encodedNs = encode ? chosen : other;
            const auto inlineNs = encode ? other : chosen;
            const bool winner = encodedNs <= inlineNs;
            span = winner == encode && span != 0 ? std::min(SettledSpanMax, span * 2) : SettledSpanMin;
            encode = winner;
            probing = false;
            settled = 0;
            return HandoffDecision{encode, encodedNs, inlineNs, ProbeBlock, span};
        }
        bool Encodes() const { return encode; }

    private:
        bool probeEncodes() const { return ((probed / ProbeBlock) % 2 == 0) == encode; }
        bool encode = true;
        bool probing = false;
        bool charged = false;
        std::size_t lastBlock = 0;
        std::uint64_t probed = 0;
        std::uint64_t settled = 0;
        std::uint64_t span = 0;
        std::array<std::uint64_t, ProbeWindows / ProbeBlock> blockNs{};
    };
    static bool Enabled();
    static constexpr std::size_t Slots = 4;
    static std::unique_ptr<CommandEncoder> Attach(VkDevice device, DeviceFunctions& table);
    ~CommandEncoder();
    CommandEncoder(const CommandEncoder&) = delete;
    CommandEncoder& operator=(const CommandEncoder&) = delete;
    void Open(VkCommandBuffer commands, Recording& recording, std::source_location where = std::source_location::current());
    void Close(std::source_location where = std::source_location::current());
    bool Admit(Handoff handoff, std::source_location where = std::source_location::current());
    void EndInline() noexcept;
    void Quiesce(std::source_location where = std::source_location::current()) noexcept { Quiesce(WaitSite::At(Wait::Quiesce, where)); }
    void Quiesce(WaitSite site) noexcept;
    bool Direct(const char* command) noexcept;
    void Settle(std::source_location where = std::source_location::current()) noexcept { Settle(WaitSite::At(Wait::Settle, where)); }
    void Settle(WaitSite site) noexcept;
    [[noreturn]] static void RefuseForeignProducer();
    bool OpenOnThisThread() const { return target.load(std::memory_order_acquire) != VK_NULL_HANDLE && owner.load(std::memory_order_relaxed) == std::this_thread::get_id(); }
    bool Defers(VkCommandBuffer commands) const {
        if (commands == VK_NULL_HANDLE || target.load(std::memory_order_acquire) != commands) return false;
        if (owner.load(std::memory_order_relaxed) != std::this_thread::get_id()) RefuseForeignProducer();
        return true;
    }
    bool Behind(VkCommandBuffer commands) const { return commands == recent.load(std::memory_order_relaxed) && executed.load(std::memory_order_acquire) < posted.load(std::memory_order_acquire); }
    static constexpr std::size_t RecordAlign = 16;
    static constexpr std::size_t Aligned(std::size_t bytes) { return (bytes + RecordAlign - 1) / RecordAlign * RecordAlign; }
    template<typename... A>
    static constexpr void CheckPlain() {
        static_assert(((!std::is_pointer_v<A> || !std::is_const_v<std::remove_pointer_t<A>>) && ...), "AGC graphics: a plain encoded command copies its arguments by value, so a pointer-to-const argument would be read after the caller's storage is gone; encode it with its pointed-to data");
    }
    template<typename Payload, typename... T>
    void Emplace(Run run, const Payload& payload, std::span<const T>... arrays) {
        static_assert((std::is_trivially_copyable_v<T> && ...), "AGC graphics: an encoded command's arrays are copied as bytes, so their elements must be trivially copyable; keep owners in batch storage");
        static_assert(std::is_trivially_destructible_v<Payload>, "AGC graphics: an encoded command's payload is never destroyed, so it must be trivially destructible; keep owners in batch storage");
        static_assert(std::is_nothrow_copy_constructible_v<Payload>, "AGC graphics: an encoded command's payload must be built without throwing");
        const std::size_t bytes = Aligned(sizeof(Payload)) + (Aligned(sizeof(T) * arrays.size()) + ... + 0);
        auto* at = reserve(run, bytes);
        new (at) Payload(payload);
        at += Aligned(sizeof(Payload));
        ((at = copyInto(at, arrays)), ...);
        publish();
    }
    std::mutex& PoolMutex() { return poolMutex; }
    const DeviceFunctions& Real() const { return real; }
    struct Statistics {
        std::uint64_t posts = 0;
        std::uint64_t commands = 0;
        std::uint64_t waits = 0;
        std::uint64_t waitNs = 0;
        std::uint64_t encodeNs = 0;
        std::uint64_t parks = 0;
        std::uint64_t postNs = 0;
        std::uint64_t inlined = 0;
    };
    Statistics Counts() const;
    std::vector<SiteWaits> WaitsBySite() const;
    bool ProfilesSites() const { return profiled; }
    static void SetDelayForTesting(std::function<void()> delay);
    class UnwindQuiesce {
    public:
        explicit UnwindQuiesce(CommandEncoder* encoder, std::source_location where = std::source_location::current()) : encoder(encoder), exceptions(std::uncaught_exceptions()), where(where) {}
        ~UnwindQuiesce() {
            if (encoder != nullptr && std::uncaught_exceptions() > exceptions) encoder->Quiesce(WaitSite::At(Wait::Unwind, where));
        }
        UnwindQuiesce(const UnwindQuiesce&) = delete;
        UnwindQuiesce& operator=(const UnwindQuiesce&) = delete;

    private:
        CommandEncoder* encoder;
        int exceptions;
        std::source_location where;
    };

private:
    struct Chunk {
        std::vector<std::byte> bytes;
        std::size_t used = 0;
        std::size_t records = 0;
        VkCommandBuffer target = VK_NULL_HANDLE;
        Recording* recording = nullptr;
        Chunk* next = nullptr;
    };
    struct Window {
        VkCommandBuffer commands;
        Recording* recording;
    };
    CommandEncoder(VkDevice device, DeviceFunctions& table, std::size_t slot);
    static std::size_t claim();
    bool post() noexcept;
    std::byte* reserve(Run run, std::size_t bytes);
    template<typename T>
    static std::byte* copyInto(std::byte* at, std::span<const T> values) {
        if (!values.empty()) std::memcpy(at, values.data(), values.size_bytes());
        return at + Aligned(values.size_bytes());
    }
    void publish() noexcept;
    void refuseEncoderThread() const noexcept;
    void run();
    void execute(Chunk& chunk);
    void await(const WaitSite& site, bool flushed) noexcept;
    void awaitSpare() noexcept;
    void note(const WaitSite& site, std::uint64_t ns, bool waited, bool flushed) noexcept;
    DeviceFunctions& table;
    DeviceFunctions real;
    VkDevice device = VK_NULL_HANDLE;
    std::size_t slot = Slots;
    std::vector<Window> windows;
    std::atomic<VkCommandBuffer> target{VK_NULL_HANDLE};
    std::atomic<VkCommandBuffer> recent{VK_NULL_HANDLE};
    Recording* recording = nullptr;
    Chunk* filling = nullptr;
    std::size_t reserved = 0;
    std::mutex queueMutex;
    Chunk* head = nullptr;
    Chunk* tail = nullptr;
    Chunk* spare = nullptr;
    std::size_t chunkCount = 0;
    std::atomic<std::uint64_t> posted{0};
    std::atomic<std::uint64_t> executed{0};
    std::atomic<bool> sleeping{false};
    std::atomic<bool> stopping{false};
    std::mutex poolMutex;
    std::thread worker;
    std::atomic<std::thread::id> owner{};
    std::atomic<std::uint64_t> postCount{0};
    std::atomic<std::uint64_t> parkCount{0};
    std::atomic<std::uint64_t> commandCount{0};
    std::atomic<std::uint64_t> waitCount{0};
    std::atomic<std::uint64_t> waitNs{0};
    std::atomic<std::uint64_t> encodeNs{0};
    std::atomic<std::uint64_t> postNs{0};
    std::atomic<std::uint64_t> inlinedCount{0};
    HandoffPolicy policy;
    std::uint32_t inlineDepth = 0;
    bool timedWindow = false;
    std::chrono::steady_clock::time_point windowStart;
    std::uint64_t waitsAtWindowEnd = 0;
    std::atomic<std::uint64_t> encodedDecisions{0};
    std::atomic<std::uint64_t> inlineDecisions{0};
    std::chrono::steady_clock::time_point decisionPrintedAt;
    bool lastDecisionEncoded = true;
    bool decline(std::source_location where) noexcept;
    void startTimedWindow() noexcept;
    void endTimedWindow() noexcept;
    void decide(std::source_location where) noexcept;
    void printSites(bool final) noexcept;
    const bool profiled;
    std::chrono::steady_clock::time_point printedAt;
    mutable std::mutex siteMutex;
    std::array<SiteWaits, TrackedSites> sites{};
    std::size_t siteCount = 0;
};

}

#endif
