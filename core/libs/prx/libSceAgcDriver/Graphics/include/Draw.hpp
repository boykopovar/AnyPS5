#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DRAW_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DRAW_HPP

#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include <memory>
#include <vector>

namespace AgcDriver::Graphics {

void Draw(const Context& context, const State& state, const Pm4::DrawParameters& draw, std::span<const CompiledShader> shaders, std::span<const GuestMemorySnapshot> snapshots = {});

// Recorded draws whose written guest buffers were copied (their results reach guest memory by a CPU
// write-back when their batch completes), listed until that write-back ran. It is the draw
// counterpart of VulkanDevice's State::copiedWriters for dispatches, which VulkanDevice.cpp consults
// in two places: DispatchIndirect (an indirect dispatch whose arguments such a writer produced reads
// them on the CPU, behind the write-back, instead of from the host import on the GPU) and the fill
// HLE (a vkCmdFillBuffer over a range a listed write-back will land on must wait for it, or the
// write-back overwrites the fill). Draws are listed here only under APS5_RECORD_COPIED_DRAWS=1,
// which is safe only once BOTH of those checks also test this list, i.e. each extends its
// `std::any_of(state->copiedWriters ...)` with
// `|| std::any_of(Graphics::DrawCopiedWriters()->begin(), Graphics::DrawCopiedWriters()->end(), [&](const auto& writer) { return writer->WritesOverlap(<its range>); })`;
// cleaner still, State::copiedWriters can become this list (one registry for both producers), after
// which the switch can turn into APS5_NO_RECORD_COPIED_DRAWS. Until then such draws are recorded and
// waited for at once. Read and written under GuestMemory::GpuMutex, like the device's list.
std::shared_ptr<std::vector<std::shared_ptr<ShaderResources>>> DrawCopiedWriters();

}

#endif
