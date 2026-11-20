#include "prx/libSceJsc/include/CallFrameExports.hpp"

#include <stdexcept>

extern "C" {

bool APS5_VABI _ZN3JSC9CallFrame15isAnyWasmCalleeEv(void* self) {
    if (!self) throw std::invalid_argument("JSC::CallFrame::isAnyWasmCallee requires a CallFrame");
    return false;
}

}
