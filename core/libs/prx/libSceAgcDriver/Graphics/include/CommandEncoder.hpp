#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_COMMANDENCODER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_COMMANDENCODER_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <source_location>
#include <string>
#include <thread>
#include <vector>

namespace AgcDriver::Graphics {

class CommandEncoder {
public:
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
            if (Failed() && !reported) deliver();
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
    static bool Enabled();
    static constexpr std::size_t Slots = 4;
    static std::unique_ptr<CommandEncoder> Attach(VkDevice device, DeviceFunctions& table);
    ~CommandEncoder();
    CommandEncoder(const CommandEncoder&) = delete;
    CommandEncoder& operator=(const CommandEncoder&) = delete;
    void Open(VkCommandBuffer commands, Recording& recording, std::source_location where = std::source_location::current());
    void Close(std::source_location where = std::source_location::current());
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
    std::byte* Append(Run run, std::size_t bytes);
    std::mutex& PoolMutex() { return poolMutex; }
    const DeviceFunctions& Real() const { return real; }
    struct Statistics {
        std::uint64_t posts = 0;
        std::uint64_t commands = 0;
        std::uint64_t waits = 0;
        std::uint64_t waitNs = 0;
        std::uint64_t encodeNs = 0;
        std::uint64_t parks = 0;
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
    void printSites(bool final) noexcept;
    const bool profiled;
    std::chrono::steady_clock::time_point printedAt;
    mutable std::mutex siteMutex;
    std::array<SiteWaits, TrackedSites> sites{};
    std::size_t siteCount = 0;
};

}

#endif
