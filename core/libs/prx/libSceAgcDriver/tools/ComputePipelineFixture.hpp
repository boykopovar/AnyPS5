#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_TOOLS_COMPUTEPIPELINEFIXTURE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_TOOLS_COMPUTEPIPELINEFIXTURE_HPP

#include "prx/libSceAgcDriver/Execution/include/ComputePipelineCapture.hpp"

namespace AgcDriver {

inline ComputePipelineCapture ComputePipelineFixture() {
    ComputePipelineCapture capture;
    capture.spirv = {
        0x07230203u, 0x00010000u, 0u, 7u, 0u,
        0x00020011u, 1u,
        0x0003000eu, 0u, 1u,
        0x0005000fu, 5u, 3u, 0x6e69616du, 0u,
        0x00060010u, 3u, 17u, 1u, 1u, 1u,
        0x00040047u, 6u, 1u, 7u,
        0x00020013u, 1u,
        0x00030021u, 2u, 1u,
        0x00040015u, 5u, 32u, 0u,
        0x00040032u, 5u, 6u, 1u,
        0x00050036u, 1u, 3u, 0u, 2u,
        0x000200f8u, 4u,
        0x000100fdu,
        0x00010038u
    };
    capture.specialization.push_back({7u, 3u});
    capture.sets = {{0, {{3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2, VK_SHADER_STAGE_COMPUTE_BIT, 0}}}};
    capture.pushConstants = {{VK_SHADER_STAGE_COMPUTE_BIT, 16, 16}};
    capture.flags = VK_PIPELINE_CREATE_DISPATCH_BASE_BIT;
    return capture;
}

}

#endif
