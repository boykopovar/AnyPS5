#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_ASYNCCOMPUTE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_ASYNCCOMPUTE_HPP

#include "Recompiler.hpp"

namespace AgcDriver::Graphics {

inline bool AsyncDispatchEligible(const ShaderRecompiler::RecompileResult& compiled, bool threadDimensions = false, bool sharedMarker = false) {
    if (threadDimensions || sharedMarker || compiled.workgroupMemoryDwords != 0 || compiled.functionLdsBuffer || compiled.meshWindowBuffer || compiled.bdaAbiVersion != 0) return false;
    using Role = ShaderRecompiler::DescriptorRole;
    for (const auto& binding : compiled.bindings) {
        switch (binding.role) {
            case Role::GuestBuffers:
            case Role::Gds:
            case Role::FaultBuffer:
            case Role::FlattenedSrt:
            case Role::ShaderData: break;
            default: return false;
        }
    }
    return true;
}

}

#endif
