#ifndef CORE_WASM_RUNTIME_GPU_SHADERCAPTURE_HPP
#define CORE_WASM_RUNTIME_GPU_SHADERCAPTURE_HPP

#include "prx/libSceAgcDriver/Execution/include/QueueState.hpp"
#include <cstdint>
#include <span>

namespace WasmGpu {

bool ShaderCaptureEnabled();
void CaptureDraw(std::span<const std::uint32_t> packet, const AgcDriver::QueueState& queue);

}  // namespace WasmGpu

#endif
