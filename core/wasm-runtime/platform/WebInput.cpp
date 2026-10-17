#include "InputRing.hpp"
#include "wasm-runtime/Platform.hpp"
#include <emscripten/em_asm.h>
#include <emscripten/threading.h>
#include <cstdint>
#include <limits>
#include <mutex>
#include <thread>

namespace {

WasmPlatform::InputRing ring;

void Consume() {
    for (;;) {
        const auto written = ring.write.load(std::memory_order_acquire);
        WasmPlatform::InputSlot slot;
        if (WasmPlatform::PopInput(ring, written, slot)) {
            PadPublishInput_nid_postfix(WasmPlatform::ToPadInputState(slot));
        } else {
            emscripten_futex_wait(&ring.write, written, std::numeric_limits<double>::infinity());
        }
    }
}

}  // namespace

void WasmInput::Start() {
    static std::once_flag started;
    std::call_once(started, [] {
        std::thread(Consume).detach();
        MAIN_THREAD_ASYNC_EM_ASM({ globalThis.aps5Platform.attachInput(wasmMemory.buffer, $0); }, static_cast<double>(reinterpret_cast<std::uintptr_t>(&ring)));
    });
}
