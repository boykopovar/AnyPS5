#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_VERTEXPLANSCRATCH_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_VERTEXPLANSCRATCH_HPP

#include "prx/libSceAgcDriver/Execution/include/ThreadScratch.hpp"
#include "prx/libSceAgcDriver/Graphics/include/VertexInput.hpp"
#include <cstddef>
#include <memory>
#include <vector>

namespace AgcDriver::Graphics {

class Buffer;

struct VertexPlanScratch {
    std::vector<VertexFetch> fetches;
    std::vector<std::size_t> fetchValid;
    std::vector<std::size_t> fetchOf;
    std::vector<std::shared_ptr<Buffer>> zeroed;
    std::vector<bool> isSolo;
    std::vector<VertexFetch> plannedFetches;
    std::vector<std::size_t> plannedOf;
    std::vector<VkBuffer> rangeHandles;
    std::vector<VkDeviceSize> rangeOffsets;

    void clear() noexcept {
        fetches.clear();
        fetchValid.clear();
        fetchOf.clear();
        zeroed.clear();
        isSolo.clear();
        plannedFetches.clear();
        plannedOf.clear();
        rangeHandles.clear();
        rangeOffsets.clear();
    }
};

struct VertexPlanTag {};

using VertexPlanLease = AgcDriver::ExclusiveScratch<VertexPlanScratch, VertexPlanTag>;

}

#endif
