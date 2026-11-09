#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"

namespace AgcDriver::Graphics {

VkDeviceAddress Buffer::DeviceAddress() const {
    Require(deviceAddress != 0, "buffer has no device address");
    return deviceAddress;
}

void Buffer::initializeAddress(VkBufferUsageFlags usage) {
    if ((usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) == 0) return;
    const VkBufferDeviceAddressInfo info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, buffer};
    deviceAddress = context.Function<PFN_vkGetBufferDeviceAddressKHR>("vkGetBufferDeviceAddressKHR")(context.device, &info);
    Require(deviceAddress != 0, "vkGetBufferDeviceAddressKHR returned a null address");
}

}
