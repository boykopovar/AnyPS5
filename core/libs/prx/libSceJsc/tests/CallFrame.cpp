#include "prx/libSceJsc/libSceJsc.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "CallFrame check failed at line %d\n", line);
        std::abort();
    }
}

}

int main() {
    std::uint64_t frame[6] = {};
    Check(!_ZN3JSC9CallFrame15isAnyWasmCalleeEv(frame), __LINE__);

    std::uint64_t cell[2] = {};
    frame[3] = reinterpret_cast<std::uintptr_t>(cell);
    Check(!_ZN3JSC9CallFrame15isAnyWasmCalleeEv(frame), __LINE__);

    frame[3] |= 2;
    Check(!_ZN3JSC9CallFrame15isAnyWasmCalleeEv(frame), __LINE__);

    bool threw = false;
    try {
        _ZN3JSC9CallFrame15isAnyWasmCalleeEv(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Check(threw, __LINE__);

    std::puts("JSC CallFrame isAnyWasmCallee tests passed");
    return 0;
}
