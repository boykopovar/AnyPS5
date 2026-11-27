#include "prx/libSceAgcDriver/Graphics/include/RecordingPool.hpp"

#include <atomic>
#include <unordered_map>

namespace AgcDriver::Graphics {

namespace {

constexpr std::uint32_t RecordingThreads = 4;

std::atomic<std::uint64_t> recordedBuffers{0};

std::mutex& DevicePoolsMutex() {
    static std::mutex poolsMutex;
    return poolsMutex;
}

std::unordered_map<VkDevice, std::unique_ptr<RecordingPool>>& DevicePools() {
    static auto* pools = new std::unordered_map<VkDevice, std::unique_ptr<RecordingPool>>();
    return *pools;
}

}

std::uint64_t RecordingPool::RecordedBuffers() {
    return recordedBuffers.load(std::memory_order_relaxed);
}

struct RecordingPool::Returned {
    std::mutex mutex;
    std::vector<VkCommandBuffer> buffers;
};

struct RecordingPool::Release {
    Release(std::shared_ptr<Returned> returned, VkCommandBuffer commands) : returned(std::move(returned)), commands(commands) {}
    Release(const Release&) = delete;
    Release& operator=(const Release&) = delete;
    ~Release() {
        std::lock_guard lock(returned->mutex);
        returned->buffers.push_back(commands);
    }
    std::shared_ptr<Returned> returned;
    VkCommandBuffer commands;
};

RecordingPool::RecordingPool(const Context& context)
    : device(context.device),
      queueFamily(context.queueFamily),
      createPool(context.Function<PFN_vkCreateCommandPool>("vkCreateCommandPool")),
      destroyPool(context.Function<PFN_vkDestroyCommandPool>("vkDestroyCommandPool")),
      allocateBuffers(context.Function<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers")),
      resetBuffer(context.Function<PFN_vkResetCommandBuffer>("vkResetCommandBuffer")),
      beginBuffer(context.Function<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")),
      endBuffer(context.Function<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")) {}

void RecordingPool::Start(std::uint32_t threads) {
    std::lock_guard lock(mutex);
    if (!workers.empty() || closing) return;
    workers.reserve(threads);
    pools.reserve(threads);
    for (std::uint32_t index = 0; index < threads; ++index) {
        VkCommandPoolCreateInfo info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        info.queueFamilyIndex = queueFamily;
        VkCommandPool pool = VK_NULL_HANDLE;
        Check(createPool(device, &info, nullptr, &pool), "vkCreateCommandPool recording worker");
        pools.push_back(pool);
        workers.emplace_back([this, pool, returned = std::make_shared<Returned>()] { run(pool, returned); });
    }
}

void RecordingPool::Stop() {
    {
        std::lock_guard lock(mutex);
        if (closing) return;
        closing = true;
    }
    wake.notify_all();
    for (auto& worker : workers) worker.join();
    workers.clear();
    for (const auto pool : pools) destroyPool(device, pool, nullptr);
    pools.clear();
}

void RecordingPool::Record(OrderedAssembly<Result>& assembly, std::uint64_t sequence, const Inheritance& inheritance, std::function<void(VkCommandBuffer)> record) {
    {
        std::lock_guard lock(mutex);
        Require(!closing && !workers.empty(), "a draw was recorded on a stopped recording pool");
        tasks.push_back(Task{&assembly, sequence, inheritance, std::move(record)});
    }
    wake.notify_one();
}

VkCommandBuffer RecordingPool::acquire(VkCommandPool pool, Returned& returned) const {
    {
        std::lock_guard lock(returned.mutex);
        if (!returned.buffers.empty()) {
            const auto commands = returned.buffers.back();
            returned.buffers.pop_back();
            return commands;
        }
    }
    VkCommandBufferAllocateInfo allocation{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocation.commandPool = pool;
    allocation.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    allocation.commandBufferCount = 1;
    VkCommandBuffer commands = VK_NULL_HANDLE;
    Check(allocateBuffers(device, &allocation, &commands), "vkAllocateCommandBuffers recording worker");
    return commands;
}

void RecordingPool::run(VkCommandPool pool, const std::shared_ptr<Returned>& returned) {
    for (;;) {
        Task task;
        {
            std::unique_lock lock(mutex);
            wake.wait(lock, [&] { return closing || !tasks.empty(); });
            if (tasks.empty()) return;
            task = std::move(tasks.front());
            tasks.pop_front();
        }
        Result result;
        bool recording = false;
        try {
            result.commands = acquire(pool, *returned);
            const std::vector<VkFormat> colors(task.inheritance.colorAttachments, VK_FORMAT_UNDEFINED);
            VkCommandBufferInheritanceRenderingInfoKHR rendering{VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR};
            rendering.colorAttachmentCount = task.inheritance.colorAttachments;
            rendering.pColorAttachmentFormats = colors.empty() ? nullptr : colors.data();
            rendering.depthAttachmentFormat = task.inheritance.depthFormat;
            rendering.stencilAttachmentFormat = task.inheritance.stencilFormat;
            rendering.rasterizationSamples = task.inheritance.samples;
            VkCommandBufferInheritanceInfo inheritance{VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, &rendering};
            VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
            begin.pInheritanceInfo = &inheritance;
            Check(beginBuffer(result.commands, &begin), "vkBeginCommandBuffer recording worker");
            recording = true;
            task.record(result.commands);
            recording = false;
            Check(endBuffer(result.commands), "vkEndCommandBuffer recording worker");
            result.release = std::make_shared<Release>(returned, result.commands);
            recordedBuffers.fetch_add(1, std::memory_order_relaxed);
        } catch (...) {
            if (recording) resetBuffer(result.commands, 0);
            if (result.commands != VK_NULL_HANDLE) {
                std::lock_guard lock(returned->mutex);
                returned->buffers.push_back(result.commands);
            }
            result = Result{};
            result.error = std::current_exception();
        }
        task.assembly->Submit(task.sequence, std::move(result));
    }
}

RecordingPool& DeviceRecordingPool(const Context& context) {
    std::lock_guard lock(DevicePoolsMutex());
    auto& pool = DevicePools()[context.device];
    if (pool == nullptr) {
        auto started = std::make_unique<RecordingPool>(context);
        started->Start(RecordingThreads);
        pool = std::move(started);
    }
    return *pool;
}

void DestroyRecordingPools(const Context& context) {
    std::unique_ptr<RecordingPool> pool;
    {
        std::lock_guard lock(DevicePoolsMutex());
        auto& pools = DevicePools();
        const auto found = pools.find(context.device);
        if (found == pools.end()) return;
        pool = std::move(found->second);
        pools.erase(found);
    }
    if (pool != nullptr) pool->Stop();
}

}
