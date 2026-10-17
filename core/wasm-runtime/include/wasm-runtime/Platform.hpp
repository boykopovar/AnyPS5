#ifndef CORE_WASM_RUNTIME_INCLUDE_WASM_RUNTIME_PLATFORM_HPP
#define CORE_WASM_RUNTIME_INCLUDE_WASM_RUNTIME_PLATFORM_HPP

#include <cstdint>

namespace WasmInput {
void Start();
}

namespace WasmAudio {
inline constexpr std::uint32_t Frequency = 48000;

struct Samples {
    const void* data = nullptr;
    bool isFloat = false;
    std::uint32_t channels = 0;
    std::uint32_t frames = 0;
};

int Open(std::uint32_t frequency);
bool Running();
std::uint32_t Queued(int stream);
std::uint32_t Write(int stream, const Samples& samples);
void Close(int stream);
}

namespace WasmStorage {
void Commit();
}

#endif
