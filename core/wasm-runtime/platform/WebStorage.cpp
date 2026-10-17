#include "wasm-runtime/Platform.hpp"
#include <emscripten/em_asm.h>

void WasmStorage::Commit() {
    MAIN_THREAD_ASYNC_EM_ASM({ globalThis.aps5Platform.commitStorage(); });
}
