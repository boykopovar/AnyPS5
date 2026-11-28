#include "prx/libSceAgcDriver/Graphics/include/CommandStream.hpp"
#include <cstdint>
#include <memory>
#include <span>
#include <string>

using AgcDriver::Graphics::CommandStream;

void StreamGuardedPayloads(CommandStream& stream) {
#if defined(APS5_GUARD_OWNING_PAYLOAD)
    stream.Emplace(nullptr, std::shared_ptr<int>());
#elif defined(APS5_GUARD_OWNING_ARRAY)
    const std::string names[1];
    stream.Emplace(nullptr, std::uint32_t{1}, std::span<const std::string>(names));
#elif defined(APS5_GUARD_POINTER_ARGUMENT)
    CommandStream::CheckPlain<VkCommandBuffer, std::uint32_t, const float*>();
#else
    const std::uint32_t words[2]{};
    stream.Emplace(nullptr, std::uint64_t{0}, std::span<const std::uint32_t>(words));
    CommandStream::CheckPlain<VkPipeline, VkBuffer, std::uint32_t, float>();
#endif
}
