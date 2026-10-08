#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_BINDINGPLAN_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_BINDINGPLAN_HPP

#include "prx/libSceAgcDriver/Graphics/include/Shaders.hpp"
#include <array>
#include <compare>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <ranges>
#include <vector>

namespace AgcDriver::Graphics {

class BindingPlan {
public:
    BindingPlan(const Context& context, std::span<const CompiledShader> shaders);
    BindingPlan(const BindingPlan&) = delete;
    BindingPlan& operator=(const BindingPlan&) = delete;

    struct Binding {
        VkDescriptorSetLayoutBinding layout;
        std::size_t shader;
        std::size_t source;
        ShaderRecompiler::DescriptorRole role;
        std::ranges::iota_view<std::size_t, std::size_t> allocations;
        std::ranges::iota_view<std::size_t, std::size_t> imageAllocations;
        std::size_t firstSampler = 0;
        std::size_t samplerCount = 0;
    };
    struct Buffer {
        bool written = true;
        bool atomic = false;
        std::int32_t pushByte = -1;
        std::int64_t dataAllocation = -1;
        std::uint32_t dataByte = 0;
    };

    std::vector<Binding> bindings;
    std::vector<Buffer> buffers;
    std::vector<VkDescriptorSetLayoutBinding> layout;
    std::vector<std::uint32_t> layoutKey;
    std::vector<VkDescriptorPoolSize> descriptorSizes;
    std::uint32_t sampledImages = 0;
    std::uint32_t storageImages = 0;
    std::uint32_t samplers = 0;
};

class BindingPlanCache {
public:
    explicit BindingPlanCache(const Context& context, std::size_t capacity = 1024);
    std::shared_ptr<const BindingPlan> Get(std::span<const CompiledShader> shaders);
    std::size_t Size() const;

private:
    struct StageKey {
        std::uint64_t variant = 0;
        ShaderRecompiler::ShaderStage stage{};
        std::uint32_t pushOffset = 0;
        auto operator<=>(const StageKey&) const = default;
    };
    using Key = std::array<StageKey, 6>;
    struct Entry {
        std::shared_ptr<const BindingPlan> plan;
        std::list<Key>::iterator order;
    };
    Context context;
    std::size_t capacity;
    mutable std::mutex mutex;
    std::list<Key> order;
    std::map<Key, Entry> entries;
};

}

#endif
