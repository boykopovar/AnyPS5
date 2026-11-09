#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SHADERDEVICEPROFILE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SHADERDEVICEPROFILE_HPP

#include "Recompiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <string>
#include <vector>

namespace AgcDriver {

class ShaderDeviceProfile {
public:
    ShaderDeviceProfile(const ShaderRecompiler::SpirvTarget& target, const VkDeviceCreateInfo& deviceInfo, const VkPhysicalDeviceLimits& limits);
    ShaderDeviceProfile(const ShaderDeviceProfile&) = delete;
    ShaderDeviceProfile& operator=(const ShaderDeviceProfile&) = delete;
    ShaderRecompiler::SpirvTarget Target() const { return target; }
    const VkPhysicalDeviceLimits& Limits() const { return limits; }
    bool NullDescriptors() const { return nullDescriptors; }

private:
    ShaderRecompiler::SpirvTarget target;
    const VkPhysicalDeviceLimits limits;
    std::vector<std::uint32_t> capabilities;
    std::vector<std::string> extensions;
    std::vector<std::string_view> extensionViews;
    bool nullDescriptors = false;
};

}

#endif
