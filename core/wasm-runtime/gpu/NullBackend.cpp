#include "Backend.hpp"
#include "ShaderCapture.hpp"
#include <atomic>
#include <cstdio>
#include <cstdlib>

namespace WasmGpu {
namespace {

class NullBackend final : public IBackend {
public:
    void RegisterShader(const Shader* shader) override {
        if (shaders.fetch_add(1) == 0) std::fprintf(stderr, "[gpu] first shader registered at %p\n", static_cast<const void*>(shader));
    }

    void Draw(std::uint32_t queue, std::span<const std::uint32_t> packet, AgcDriver::QueueState& state) override {
        if (ShaderCaptureEnabled()) CaptureDraw(packet, state);
        if (draws.fetch_add(1) == 0) std::fprintf(stderr, "[gpu] first draw on queue 0x%x (opcode 0x%x)\n", queue, (packet[0] >> 8u) & 0xffu);
    }

    void Dispatch(std::uint32_t queue, std::span<const std::uint32_t> packet, AgcDriver::QueueState&) override {
        if (dispatches.fetch_add(1) == 0) std::fprintf(stderr, "[gpu] first dispatch on queue 0x%x (opcode 0x%x)\n", queue, (packet[0] >> 8u) & 0xffu);
    }

    void AfterWork(std::function<void()> completion) override {
        completion();
    }

    void PresentBuffer(const AgcDriver::DisplayBuffer& buffer) override {
        const auto count = presents.fetch_add(1);
        if (count == 0 || std::getenv("APS5_TRACE_GPU") != nullptr) {
            std::fprintf(stderr, "[gpu] present %llu: buffer 0x%llx %ux%u format 0x%llx (draws %llu, dispatches %llu)\n", static_cast<unsigned long long>(count + 1), static_cast<unsigned long long>(buffer.address), buffer.width, buffer.height,
                static_cast<unsigned long long>(buffer.pixelFormat), static_cast<unsigned long long>(draws.load()), static_cast<unsigned long long>(dispatches.load()));
        }
    }

    void PresentClear(bool) override {
        presents.fetch_add(1);
    }

private:
    std::atomic<std::uint64_t> shaders{0};
    std::atomic<std::uint64_t> draws{0};
    std::atomic<std::uint64_t> dispatches{0};
    std::atomic<std::uint64_t> presents{0};
};

}  // namespace

IBackend& Backend() {
    static NullBackend backend;
    return backend;
}

}  // namespace WasmGpu
