#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DESCRIPTORHEAPVERSIONS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DESCRIPTORHEAPVERSIONS_HPP

#include <vulkan/vulkan.h>
#include <cstdint>
#include <optional>

namespace AgcDriver {

constexpr bool DescriptorHeapVersionsSupported(std::uint32_t extensionRevision, std::optional<std::uint32_t> validationVersion = std::nullopt) {
    return extensionRevision >= VK_EXT_DESCRIPTOR_HEAP_SPEC_VERSION &&
           (!validationVersion.has_value() || *validationVersion >= VK_MAKE_API_VERSION(0, 1, 4, 363));
}

}

#endif
