#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DESCRIPTORHEAP_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DESCRIPTORHEAP_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <array>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

namespace AgcDriver::Graphics {

class DescriptorHeap {
public:
    static constexpr std::uint32_t MaxViews = 16u;
    enum class Array : std::uint32_t { Samplers, SampledImages, StorageImages, Count };
    struct Capacities {
        std::uint32_t samplers;
        std::uint32_t sampledImages;
        std::uint32_t storageImages;
    };

    DescriptorHeap(const Context& context, Capacities capacities);
    ~DescriptorHeap();
    DescriptorHeap(const DescriptorHeap&) = delete;
    DescriptorHeap& operator=(const DescriptorHeap&) = delete;

    static Capacities DeviceCapacities(const Context& context);
    VkDescriptorSetLayout Layout(VkShaderStageFlags stages);
    VkDescriptorSetLayout EmptyLayout();
    void SetHeader(VkBuffer buffer, VkDeviceSize bytes);
    VkDescriptorSet Set(VkShaderStageFlags stages);
    std::uint32_t Capacity(Array array) const { return arrays[static_cast<std::size_t>(array)].capacity; }
    std::uint32_t AllocateSampler(VkSampler sampler);
    std::uint32_t AllocateImage(Array array, VkImageView view, VkImageLayout layout);
    void Release(Array array, std::uint32_t slot, std::uint64_t retireSerial);
    std::uint32_t Collect(std::uint64_t completedSerial);
    bool Live(Array array, std::uint32_t slot) const;
    std::uint32_t LiveSlots(Array array) const;
    std::uint32_t PendingReleases() const;
    void Bind(VkCommandBuffer commands, VkPipelineBindPoint bindPoint, VkPipelineLayout pipelineLayout, VkShaderStageFlags stages);

private:
    struct View {
        VkShaderStageFlags stages;
        VkDescriptorSetLayout layout;
        VkDescriptorSet set;
    };
    struct Retired {
        std::uint32_t slot;
        std::uint64_t serial;
    };
    struct Slots {
        std::uint32_t capacity = 0;
        VkDescriptorType type = VK_DESCRIPTOR_TYPE_SAMPLER;
        std::vector<VkDescriptorImageInfo> entries;
        std::vector<std::uint32_t> generations;
        std::vector<bool> live;
        std::vector<std::uint32_t> free;
        std::deque<Retired> retired;
    };
    const View& view(VkShaderStageFlags stages);
    std::uint32_t allocate(Array array, const VkDescriptorImageInfo& entry);
    void write(const View& target, Array array, std::uint32_t first, std::uint32_t count);
    void writeHeader(const View& target);
    void release() noexcept;

    Context context;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkSampler nullSampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout emptyLayout = VK_NULL_HANDLE;
    VkDescriptorBufferInfo header{VK_NULL_HANDLE, 0, 0};
    mutable std::mutex mutex;
    std::deque<View> views;
    std::array<Slots, static_cast<std::size_t>(Array::Count)> arrays;
};

}

#endif
