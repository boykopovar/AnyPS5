#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_COMMANDENCODER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_COMMANDENCODER_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace AgcDriver::Graphics {

class CommandEncoder {
public:
    using Run = void (*)(const std::byte* payload, VkCommandBuffer commands, const DeviceFunctions& functions);
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
    void Open(VkCommandBuffer commands, Recording& recording);
    void Close();
    void Quiesce() noexcept;
    bool Direct() noexcept;
    void Settle() noexcept;
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
    };
    Statistics Counts() const;
    static void SetDelayForTesting(std::function<void()> delay);
    class UnwindQuiesce {
    public:
        explicit UnwindQuiesce(CommandEncoder* encoder) : encoder(encoder), exceptions(std::uncaught_exceptions()) {}
        ~UnwindQuiesce() {
            if (encoder != nullptr && std::uncaught_exceptions() > exceptions) encoder->Quiesce();
        }
        UnwindQuiesce(const UnwindQuiesce&) = delete;
        UnwindQuiesce& operator=(const UnwindQuiesce&) = delete;

    private:
        CommandEncoder* encoder;
        int exceptions;
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
    void post() noexcept;
    void run();
    void execute(Chunk& chunk);
    void await() noexcept;
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
    std::atomic<std::uint64_t> posted{0};
    std::atomic<std::uint64_t> executed{0};
    std::atomic<bool> sleeping{false};
    std::atomic<bool> stopping{false};
    std::mutex poolMutex;
    std::thread worker;
    std::atomic<std::thread::id> owner{};
    std::atomic<std::uint64_t> postCount{0};
    std::atomic<std::uint64_t> commandCount{0};
    std::atomic<std::uint64_t> waitCount{0};
    std::atomic<std::uint64_t> waitNs{0};
    std::atomic<std::uint64_t> encodeNs{0};
};

}

#endif
