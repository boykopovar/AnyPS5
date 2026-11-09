#include "prx/libSceAgcDriver/Graphics/include/CommandEncoder.hpp"
#include "prx/libSceAgcDriver/Execution/include/HandoffSpin.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/ThreadPriority.hpp"
#include "prx/libSceAgcDriver/Execution/include/TracyZone.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <stdexcept>
#include <thread>
#include <utility>
#ifdef _WIN32
#include "prx/libc/include/general/ThreadName.hpp"
#endif

namespace AgcDriver::Graphics {

namespace {

constexpr const char* EncoderNames[CommandEncoders::MaximumEncoders] = {"command encoder", "command encoder 2", "command encoder 3"};

struct EncodeCounters {
    std::atomic<std::uint64_t> streams{0};
    std::atomic<std::uint64_t> inlineStreams{0};
    std::atomic<std::uint64_t> failures{0};
    std::atomic<std::uint64_t> ops{0};
    std::atomic<std::uint64_t> bytes{0};
    std::atomic<std::uint64_t> busyNs{0};
    std::atomic<std::uint64_t> inlineNs{0};
    std::atomic<std::uint64_t> waits{0};
    std::atomic<std::uint64_t> waitNs{0};
};

EncodeCounters counters;

std::uint64_t nanosecondsSince(std::chrono::steady_clock::time_point start) {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count());
}

void checkEncode(VkResult result, const char* operation) {
    if (result != VK_SUCCESS) throw std::runtime_error(std::string("AGC graphics: ") + operation + ": Vulkan result " + std::to_string(result));
}

void prepareEncoderThread(const char* name) {
    try {
        TracyZone::NameThread(name);
#ifdef _WIN32
        ThreadName::SetCurrent(name);
#endif
        PerformanceTrace::BindThread(name);
        RaiseWorkerThreadPriority(name);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "[encode] %s: %s\n", name, error.what());
    }
}

}

EncodeTicket::EncodeTicket(std::unique_ptr<CommandStream> stream, std::uint64_t traceId) : stream(std::move(stream)), traceId(traceId) {}

void EncodeTicket::Complete(VkCommandBuffer encoded, std::uint32_t encoderIndex) {
    commands = encoded;
    encoder = encoderIndex;
    finish(Encoded);
}

void EncodeTicket::Fail(std::exception_ptr reason) {
    error = std::move(reason);
    finish(Failure);
}

void EncodeTicket::finish(std::uint8_t outcome) {
    bool wake = false;
    {
        std::lock_guard lock(mutex);
        state.store(outcome, std::memory_order_release);
        wake = sleeping;
    }
    if (wake) done.notify_all();
}

void EncodeTicket::Wait() {
    if (SpinUntil(HandoffSpinWindow(), [&] { return Finished(); })) return;
    std::unique_lock lock(mutex);
    sleeping = true;
    done.wait(lock, [&] { return Finished(); });
    sleeping = false;
}

class CommandEncoders::Encoder {
public:
    Encoder(CommandEncoders& owner, std::uint32_t queueFamily, std::uint32_t index) : owner(owner), index(index) {
        VkCommandPoolCreateInfo info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        info.queueFamilyIndex = queueFamily;
        Check(owner.functions.createPool(owner.device, &info, nullptr, &pool), "vkCreateCommandPool encoder");
        try {
            spares.reserve(SpareCommandBuffers + 1);
            thread = std::thread([this] { run(); });
        } catch (...) {
            owner.functions.destroyPool(owner.device, pool, nullptr);
            throw;
        }
    }

    ~Encoder() {
        if (thread.joinable()) thread.join();
        if (!spares.empty()) owner.functions.free(owner.device, pool, static_cast<std::uint32_t>(spares.size()), spares.data());
        owner.functions.destroyPool(owner.device, pool, nullptr);
    }

    Encoder(const Encoder&) = delete;
    Encoder& operator=(const Encoder&) = delete;

    void Join() {
        if (thread.joinable()) thread.join();
    }

    void Recycle(VkCommandBuffer commands) noexcept {
        std::lock_guard lock(spareMutex);
        try {
            spares.push_back(commands);
        } catch (...) {
        }
    }

private:
    void run() {
        prepareEncoderThread(EncoderNames[index]);
        for (;;) {
            std::shared_ptr<EncodeTicket> ticket;
            owner.spinning.fetch_add(1, std::memory_order_acq_rel);
            SpinUntil(HandoffSpinWindow(), [&] { return owner.queued.load(std::memory_order_acquire) != 0; });
            owner.spinning.fetch_sub(1, std::memory_order_acq_rel);
            {
                std::unique_lock lock(owner.mutex);
                while (owner.queue.empty() && !owner.stopping) {
                    ++owner.sleepers;
                    owner.wake.wait(lock);
                    --owner.sleepers;
                }
                if (owner.queue.empty()) return;
                ticket = std::move(owner.queue.front());
                owner.queue.pop_front();
                owner.queued.fetch_sub(1, std::memory_order_acq_rel);
            }
            encode(*ticket);
        }
    }

    VkCommandBuffer takeSpare() noexcept {
        std::lock_guard lock(spareMutex);
        if (spares.size() > SpareCommandBuffers) {
            owner.functions.free(owner.device, pool, static_cast<std::uint32_t>(spares.size() - SpareCommandBuffers), spares.data() + SpareCommandBuffers);
            spares.resize(SpareCommandBuffers);
        }
        if (spares.empty()) return VK_NULL_HANDLE;
        const auto commands = spares.back();
        spares.pop_back();
        return commands;
    }

    void encode(EncodeTicket& ticket) noexcept {
        const auto start = std::chrono::steady_clock::now();
        TraceBatchTag = ticket.TraceId();
        const auto& functions = owner.functions;
        auto commands = takeSpare();
        try {
            PerformanceTimer timing("Graphics.Encode");
            if (commands == VK_NULL_HANDLE) {
                VkCommandBufferAllocateInfo allocation{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
                allocation.commandPool = pool;
                allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                allocation.commandBufferCount = 1;
                checkEncode(functions.allocate(owner.device, &allocation, &commands), "vkAllocateCommandBuffers encoder");
            }
            VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            checkEncode(functions.begin(commands, &begin), "vkBeginCommandBuffer encoder");
            ticket.Stream().Replay(commands);
            checkEncode(functions.end(commands), "vkEndCommandBuffer encoder");
        } catch (...) {
            if (commands != VK_NULL_HANDLE) functions.free(owner.device, pool, 1, &commands);
            counters.failures.fetch_add(1, std::memory_order_relaxed);
            TraceBatchTag = 0;
            ticket.Fail(std::current_exception());
            return;
        }
        counters.streams.fetch_add(1, std::memory_order_relaxed);
        counters.ops.fetch_add(ticket.Stream().Ops(), std::memory_order_relaxed);
        counters.bytes.fetch_add(ticket.Stream().Bytes(), std::memory_order_relaxed);
        counters.busyNs.fetch_add(nanosecondsSince(start), std::memory_order_relaxed);
        TraceBatchTag = 0;
        ticket.Complete(commands, index);
    }

    CommandEncoders& owner;
    std::uint32_t index;
    VkCommandPool pool = VK_NULL_HANDLE;
    std::mutex spareMutex;
    std::vector<VkCommandBuffer> spares;
    std::thread thread;
};

unsigned CommandEncoders::Count() {
    static const unsigned count = [] {
        const char* text = std::getenv("APS5_ENCODERS");
        if (text == nullptr || text[0] == '\0') return 2u;
        const auto value = std::strtoul(text, nullptr, 10);
        if (value < 1) return 1u;
        if (value > MaximumEncoders) return MaximumEncoders;
        return static_cast<unsigned>(value);
    }();
    return count;
}

bool CommandEncoders::Inline() {
    static const bool verify = std::getenv("APS5_VERIFY_DEFERRED_ENCODE") != nullptr;
    return verify;
}

CommandEncoders::Statistics CommandEncoders::Totals() {
    const auto load = [](const std::atomic<std::uint64_t>& value) { return value.load(std::memory_order_relaxed); };
    return {load(counters.streams), load(counters.inlineStreams), load(counters.failures), load(counters.ops), load(counters.bytes), load(counters.busyNs), load(counters.inlineNs), load(counters.waits), load(counters.waitNs)};
}

std::string CommandEncoders::TotalsText() {
    const auto totals = Totals();
    const auto streams = totals.streams + totals.inlineStreams;
    char text[384];
    std::snprintf(text, sizeof(text), "%llu streams on encoder threads (%llu failed), %llu encoded inline; %llu ops, %.1f MiB, %.1f bytes per op; encoder threads busy %.1f ms (%.1f us per stream), inline encodes %.1f ms; the submitter waited for %llu encodes, %.1f ms",
                  static_cast<unsigned long long>(totals.streams), static_cast<unsigned long long>(totals.failures), static_cast<unsigned long long>(totals.inlineStreams), static_cast<unsigned long long>(totals.ops),
                  static_cast<double>(totals.bytes) / (1024.0 * 1024.0), totals.ops != 0 ? static_cast<double>(totals.bytes) / static_cast<double>(totals.ops) : 0.0,
                  static_cast<double>(totals.busyNs) / 1e6, totals.streams != 0 ? static_cast<double>(totals.busyNs) / 1e3 / static_cast<double>(totals.streams) : 0.0,
                  static_cast<double>(totals.inlineNs) / 1e6, static_cast<unsigned long long>(totals.waits), static_cast<double>(totals.waitNs) / 1e6);
    return streams != 0 || totals.failures != 0 ? std::string(text) : std::string();
}

void CommandEncoders::CountInline(const CommandStream& stream, std::uint64_t nanoseconds) {
    counters.inlineStreams.fetch_add(1, std::memory_order_relaxed);
    counters.ops.fetch_add(stream.Ops(), std::memory_order_relaxed);
    counters.bytes.fetch_add(stream.Bytes(), std::memory_order_relaxed);
    counters.inlineNs.fetch_add(nanoseconds, std::memory_order_relaxed);
}

void CommandEncoders::CountWait(std::uint64_t nanoseconds) {
    counters.waits.fetch_add(1, std::memory_order_relaxed);
    counters.waitNs.fetch_add(nanoseconds, std::memory_order_relaxed);
}

CommandEncoders::CommandEncoders(const Context& context, unsigned count) : device(context.device) {
    functions.createPool = context.Function<PFN_vkCreateCommandPool>("vkCreateCommandPool");
    functions.destroyPool = context.Function<PFN_vkDestroyCommandPool>("vkDestroyCommandPool");
    functions.allocate = context.Function<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers");
    functions.free = context.Function<PFN_vkFreeCommandBuffers>("vkFreeCommandBuffers");
    functions.begin = context.Function<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer");
    functions.end = context.Function<PFN_vkEndCommandBuffer>("vkEndCommandBuffer");
    if (count < 1) count = 1;
    if (count > MaximumEncoders) count = MaximumEncoders;
    try {
        encoders.reserve(count);
        for (unsigned index = 0; index < count; ++index) encoders.push_back(std::make_unique<Encoder>(*this, context.queueFamily, index));
    } catch (...) {
        stop();
        encoders.clear();
        throw;
    }
}

CommandEncoders::~CommandEncoders() {
    stop();
    encoders.clear();
}

void CommandEncoders::stop() noexcept {
    {
        std::lock_guard lock(mutex);
        stopping = true;
    }
    wake.notify_all();
    for (auto& encoder : encoders) encoder->Join();
}

void CommandEncoders::Encode(std::shared_ptr<EncodeTicket> ticket) noexcept {
    bool wakeOne = false;
    try {
        std::lock_guard lock(mutex);
        queue.push_back(ticket);
        queued.fetch_add(1, std::memory_order_acq_rel);
        wakeOne = sleepers != 0 && spinning.load(std::memory_order_acquire) == 0;
    } catch (...) {
        ticket->Fail(std::current_exception());
        return;
    }
    if (wakeOne) wake.notify_one();
}

void CommandEncoders::Recycle(std::uint32_t encoder, VkCommandBuffer commands) noexcept {
    if (commands == VK_NULL_HANDLE || encoder >= encoders.size()) return;
    encoders[encoder]->Recycle(commands);
}

}
