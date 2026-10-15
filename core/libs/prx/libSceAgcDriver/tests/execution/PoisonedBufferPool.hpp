#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_POISONEDBUFFERPOOL_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_POISONEDBUFFERPOOL_HPP

#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace PoisonedBufferPool {

inline constexpr std::byte PoisonByte{0xa5};

inline void PoisonPool(const AgcDriver::VulkanDevice& device) {
    using namespace AgcDriver::Graphics;
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    std::vector<VkBufferUsageFlags> usages{VK_BUFFER_USAGE_STORAGE_BUFFER_BIT};
    if (context.bufferDeviceAddress) usages.push_back(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    std::vector<std::shared_ptr<Buffer>> held;
    for (const auto usage : usages) {
        for (std::size_t size = 256; size <= 8192; size *= 2u) {
            for (std::uint32_t copy = 0; copy < 2u; ++copy) {
                auto buffer = std::make_shared<Buffer>(context, size, usage);
                const auto bytes = buffer->Bytes();
                std::fill(bytes.begin(), bytes.end(), PoisonByte);
                held.push_back(std::move(buffer));
            }
        }
    }
    held.clear();
    Buffer probe(context, 256, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    Require(probe.Bytes()[0] == PoisonByte && probe.Bytes()[255] == PoisonByte, "buffer direct bounds: the pooled buffers were not reused after being poisoned");
}

}

#endif
