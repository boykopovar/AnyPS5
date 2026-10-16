#ifndef CORE_WASM_RUNTIME_GPU_BACKEND_HPP
#define CORE_WASM_RUNTIME_GPU_BACKEND_HPP

#include "prx/libSceAgcDriver/Execution/include/DisplayBuffer.hpp"
#include "prx/libSceAgcDriver/Execution/include/QueueState.hpp"
#include <cstdint>
#include <functional>
#include <span>

struct Shader;

namespace WasmGpu {

class IBackend {
public:
    virtual ~IBackend() = default;
    virtual void RegisterShader(const Shader* shader) = 0;
    virtual void Draw(std::uint32_t queue, std::span<const std::uint32_t> packet, AgcDriver::QueueState& state) = 0;
    virtual void Dispatch(std::uint32_t queue, std::span<const std::uint32_t> packet, AgcDriver::QueueState& state) = 0;
    virtual void AfterWork(std::function<void()> completion) = 0;
    virtual void PresentBuffer(const AgcDriver::DisplayBuffer& buffer) = 0;
    virtual void PresentClear(bool opaque) = 0;
};

IBackend& Backend();

}  // namespace WasmGpu

#endif
