#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_COMMANDENCODER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_COMMANDENCODER_HPP

#include "prx/libSceAgcDriver/Graphics/include/CommandStream.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace AgcDriver::Graphics {

class EncodeTicket {
public:
    EncodeTicket(std::unique_ptr<CommandStream> stream, std::uint64_t traceId);
    EncodeTicket(const EncodeTicket&) = delete;
    EncodeTicket& operator=(const EncodeTicket&) = delete;

    const CommandStream& Stream() const { return *stream; }
    std::uint64_t TraceId() const { return traceId; }
    void Complete(VkCommandBuffer encoded, std::uint32_t encoderIndex);
    void Fail(std::exception_ptr reason);
    bool Finished() const { return state.load(std::memory_order_acquire) != Pending; }
    bool Failed() const { return state.load(std::memory_order_acquire) == Failure; }
    void Wait();
    VkCommandBuffer Commands() const { return commands; }
    std::uint32_t Encoder() const { return encoder; }
    std::exception_ptr Error() const { return error; }
    std::unique_ptr<CommandStream> TakeStream() { return std::move(stream); }

private:
    static constexpr std::uint8_t Pending = 0, Encoded = 1, Failure = 2;
    void finish(std::uint8_t outcome);

    std::unique_ptr<CommandStream> stream;
    std::uint64_t traceId;
    std::atomic<std::uint8_t> state{Pending};
    VkCommandBuffer commands = VK_NULL_HANDLE;
    std::uint32_t encoder = 0;
    std::exception_ptr error;
    std::mutex mutex;
    std::condition_variable done;
    bool sleeping = false;
};

class CommandEncoders {
public:
    static constexpr unsigned MaximumEncoders = 3;
    static constexpr std::size_t SpareCommandBuffers = 64;
    static unsigned Count();
    static bool Inline();
    struct Statistics {
        std::uint64_t streams, inlineStreams, failures, ops, bytes, busyNs, inlineNs, waits, waitNs;
    };
    static Statistics Totals();
    static std::string TotalsText();
    static void CountInline(const CommandStream& stream, std::uint64_t nanoseconds);
    static void CountWait(std::uint64_t nanoseconds);

    CommandEncoders(const Context& context, unsigned count);
    ~CommandEncoders();
    CommandEncoders(const CommandEncoders&) = delete;
    CommandEncoders& operator=(const CommandEncoders&) = delete;

    void Encode(std::shared_ptr<EncodeTicket> ticket) noexcept;
    void Recycle(std::uint32_t encoder, VkCommandBuffer commands) noexcept;

private:
    struct Functions {
        PFN_vkCreateCommandPool createPool = nullptr;
        PFN_vkDestroyCommandPool destroyPool = nullptr;
        PFN_vkAllocateCommandBuffers allocate = nullptr;
        PFN_vkFreeCommandBuffers free = nullptr;
        PFN_vkBeginCommandBuffer begin = nullptr;
        PFN_vkEndCommandBuffer end = nullptr;
    };
    class Encoder;
    void stop() noexcept;

    Functions functions;
    VkDevice device = VK_NULL_HANDLE;
    std::mutex mutex;
    std::condition_variable wake;
    std::deque<std::shared_ptr<EncodeTicket>> queue;
    std::atomic<std::uint32_t> queued{0};
    std::atomic<std::uint32_t> spinning{0};
    unsigned sleepers = 0;
    bool stopping = false;
    std::vector<std::unique_ptr<Encoder>> encoders;
};

}

#endif
