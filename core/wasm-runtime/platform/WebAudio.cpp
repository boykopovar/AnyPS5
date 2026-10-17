#include "AudioRing.hpp"
#include "wasm-runtime/Platform.hpp"
#include <emscripten/em_asm.h>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <string>

namespace {

WasmPlatform::AudioState state;
std::mutex streams;

}  // namespace

int WasmAudio::Open(std::uint32_t frequency) {
    if (frequency != Frequency) {
        throw std::runtime_error("WasmAudio: " + std::to_string(frequency) + " Hz output is not supported, the browser stream runs at " + std::to_string(Frequency) + " Hz");
    }
    static std::once_flag attached;
    std::call_once(attached, [] {
        MAIN_THREAD_ASYNC_EM_ASM({ globalThis.aps5Platform.attachAudio(wasmMemory.buffer, $0); }, static_cast<double>(reinterpret_cast<std::uintptr_t>(&state)));
    });
    std::lock_guard lock(streams);
    const int stream = WasmPlatform::OpenRing(state);
    if (stream < 0) throw std::runtime_error("WasmAudio: all " + std::to_string(WasmPlatform::AudioRings) + " browser audio streams are open");
    return stream;
}

bool WasmAudio::Running() {
    return state.running.load(std::memory_order_acquire) != 0;
}

std::uint32_t WasmAudio::Queued(int stream) {
    return WasmPlatform::QueuedFrames(WasmPlatform::RingAt(state, stream));
}

std::uint32_t WasmAudio::Write(int stream, const Samples& samples) {
    return WasmPlatform::WriteFrames(WasmPlatform::RingAt(state, stream), samples);
}

void WasmAudio::Close(int stream) {
    std::lock_guard lock(streams);
    WasmPlatform::RingAt(state, stream).active.store(0, std::memory_order_release);
}
