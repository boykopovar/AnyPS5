#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_STAGEDMERGE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_STAGEDMERGE_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <cstdint>
#include <memory>

namespace AgcDriver::Graphics {

struct StagedTemporary {
    virtual ~StagedTemporary() = default;
    virtual VkImage Image() const = 0;
    virtual VkImageView View() const = 0;
};

std::shared_ptr<StagedTemporary> AcquireStagedTemporary(const Context& context, VkExtent2D extent, std::uint32_t samples);
void RecordStagedClear(const Context& context, VkCommandBuffer commands, StagedTemporary& temporary);
void RecordStagedMerge(const Context& context, VkCommandBuffer commands, StagedTemporary& temporary, VkImageView target, std::uint32_t mask, std::uint32_t samples, VkRect2D area);
void ClearStagedMerges(VkDevice device);

}

#endif
