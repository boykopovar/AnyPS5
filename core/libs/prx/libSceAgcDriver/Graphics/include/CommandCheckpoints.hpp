#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_COMMANDCHECKPOINTS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_COMMANDCHECKPOINTS_HPP

#include "prx/libSceAgcDriver/Execution/include/SubmissionSnapshots.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"

namespace AgcDriver::Graphics {

PFN_vkGetDeviceProcAddr CheckpointCommandKinds(VkDevice device, PFN_vkGetDeviceProcAddr resolver);
void NoteRecordedCheckpoint(VkCommandBuffer commands, AgcDriver::FaultToken token) noexcept;

}

#endif
