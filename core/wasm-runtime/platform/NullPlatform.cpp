#include "wasm-runtime/Platform.hpp"
#include <stdexcept>

void WasmInput::Start() {
}

int WasmAudio::Open(std::uint32_t) {
    return -1;
}

bool WasmAudio::Running() {
    return false;
}

std::uint32_t WasmAudio::Queued(int) {
    throw std::logic_error("WasmAudio: the Node runtime has no audio streams");
}

std::uint32_t WasmAudio::Write(int, const Samples&) {
    throw std::logic_error("WasmAudio: the Node runtime has no audio streams");
}

void WasmAudio::Close(int) {
    throw std::logic_error("WasmAudio: the Node runtime has no audio streams");
}

void WasmStorage::Commit() {
}
