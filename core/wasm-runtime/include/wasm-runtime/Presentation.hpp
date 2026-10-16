#ifndef CORE_WASM_RUNTIME_INCLUDE_WASM_RUNTIME_PRESENTATION_HPP
#define CORE_WASM_RUNTIME_INCLUDE_WASM_RUNTIME_PRESENTATION_HPP

#include "prx/libSceAgcDriver/Execution/include/DisplayBuffer.hpp"

extern "C" void AgcDriverWasmPresentBuffer_nid_postfix(const AgcDriver::DisplayBuffer& buffer);
extern "C" void AgcDriverWasmPresentClear_nid_postfix(bool opaque);

#endif
