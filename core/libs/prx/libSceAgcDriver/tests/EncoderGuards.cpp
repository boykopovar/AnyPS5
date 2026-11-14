#include "prx/libSceAgcDriver/Graphics/include/CommandEncoder.hpp"
#include <cstdint>
#include <memory>
#include <span>
#include <string>

using AgcDriver::Graphics::CommandEncoder;

void EncodeGuardedPayloads(CommandEncoder& encoder) {
#if defined(APS5_GUARD_OWNING_PAYLOAD)
    encoder.Emplace(nullptr, std::shared_ptr<int>());
#elif defined(APS5_GUARD_OWNING_ARRAY)
    const std::string names[1];
    encoder.Emplace(nullptr, std::uint32_t{1}, std::span<const std::string>(names));
#elif defined(APS5_GUARD_POINTER_ARGUMENT)
    CommandEncoder::CheckPlain<VkCommandBuffer, std::uint32_t, const float*>();
#else
    const std::uint32_t words[2]{};
    encoder.Emplace(nullptr, std::uint64_t{0}, std::span<const std::uint32_t>(words));
    CommandEncoder::CheckPlain<VkPipeline, VkBuffer, std::uint32_t, float>();
#endif
}
