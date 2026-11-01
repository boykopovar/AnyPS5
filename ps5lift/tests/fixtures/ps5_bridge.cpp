#include <ps5lift/State.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" void ps5lift_import_YQ0navp_pYIc(Ps5LiftState* state) {
    const auto* text = reinterpret_cast<const char*>(state->Gpr[PS5LIFT_RDI]);
    state->Gpr[PS5LIFT_RAX] = static_cast<std::uint64_t>(std::puts(text));
    std::fflush(stdout);
}

extern "C" void ps5lift_import_uMei1W9uyNo(Ps5LiftState* state) {
    std::fflush(stdout);
    std::_Exit(static_cast<int>(state->Gpr[PS5LIFT_RDI]));
}
