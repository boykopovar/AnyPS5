#include <ps5lift/State.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>

extern "C" {
[[gnu::weak]] void JumpBufferSaveMask_nid_no_patch(unsigned char* buffer);
[[gnu::weak]] void JumpBufferRestoreMask_nid_no_patch(const unsigned char* buffer);
}

namespace {

constexpr std::size_t MaskSavedOffset = 88;
constexpr int SavedRegisters[] = {PS5LIFT_RBX, PS5LIFT_RSP, PS5LIFT_RBP, PS5LIFT_R12, PS5LIFT_R13, PS5LIFT_R14, PS5LIFT_R15};

template <typename T>
void Put(unsigned char* buffer, const std::size_t offset, const T value) {
    std::memcpy(buffer + offset, &value, sizeof(value));
}

template <typename T>
T Get(const unsigned char* buffer, const std::size_t offset) {
    T value;
    std::memcpy(&value, buffer + offset, sizeof(value));
    return value;
}

void Save(Ps5LiftState* state, const bool mask) {
    auto* buffer = reinterpret_cast<unsigned char*>(state->Gpr[PS5LIFT_RDI]);
    const auto rsp = state->Gpr[PS5LIFT_RSP];
    Put(buffer, 0, Get<std::uint64_t>(reinterpret_cast<const unsigned char*>(rsp), 0));
    for (std::size_t i = 0; i < std::size(SavedRegisters); ++i) {
        const auto reg = SavedRegisters[i];
        Put(buffer, 8 + 8 * i, reg == PS5LIFT_RSP ? rsp + 8 : state->Gpr[reg]);
    }
    Put(buffer, 64, ps5lift_read_mxcsr(state));
    Put(buffer, 68, state->X87Control);
    if (mask && JumpBufferSaveMask_nid_no_patch != nullptr) JumpBufferSaveMask_nid_no_patch(buffer);
    else Put(buffer, MaskSavedOffset, std::int32_t{0});
    state->Gpr[PS5LIFT_RAX] = 0;
}

void Jump(Ps5LiftState* state, const bool mask) {
    const auto* buffer = reinterpret_cast<const unsigned char*>(state->Gpr[PS5LIFT_RDI]);
    const auto value = static_cast<std::uint32_t>(state->Gpr[PS5LIFT_RSI]);
    if (mask && JumpBufferRestoreMask_nid_no_patch != nullptr) JumpBufferRestoreMask_nid_no_patch(buffer);
    for (std::size_t i = 0; i < std::size(SavedRegisters); ++i) state->Gpr[SavedRegisters[i]] = Get<std::uint64_t>(buffer, 8 + 8 * i);
    state->Gpr[PS5LIFT_RAX] = value != 0 ? value : 1;
    state->Mxcsr = Get<std::uint32_t>(buffer, 64);
    ps5lift_apply_mxcsr(state);
    state->X87Control = Get<std::uint16_t>(buffer, 68);
    state->ResumeRip = Get<std::uint64_t>(buffer, 0);
    state->ResumeRsp = state->Gpr[PS5LIFT_RSP];
    state->Unwinding = 1;
}

void SaveWithRequestedMask(Ps5LiftState* state) {
    Save(state, static_cast<std::uint32_t>(state->Gpr[PS5LIFT_RSI]) != 0);
}

}

extern "C" void ps5lift_import_setjmp(Ps5LiftState* state) { Save(state, true); }
extern "C" void ps5lift_import___setjmp(Ps5LiftState* state) { Save(state, false); }
extern "C" void ps5lift_import_sigsetjmp(Ps5LiftState* state) { SaveWithRequestedMask(state); }
extern "C" void ps5lift_import_longjmp(Ps5LiftState* state) { Jump(state, true); }
extern "C" void ps5lift_import___longjmp(Ps5LiftState* state) { Jump(state, false); }
extern "C" void ps5lift_import_siglongjmp(Ps5LiftState* state) { Jump(state, true); }

extern "C" void ps5lift_import_gNQ1V2vfXDE(Ps5LiftState* state) { Save(state, true); }
extern "C" void ps5lift_import_sjpkrhugvVI(Ps5LiftState* state) { Save(state, false); }
extern "C" void ps5lift_import_aRo9AhFUXcM(Ps5LiftState* state) { SaveWithRequestedMask(state); }
extern "C" void ps5lift_import_lKEN2IebgJ0(Ps5LiftState* state) { Jump(state, true); }
extern "C" void ps5lift_import__pF_p9hhi6k9Q(Ps5LiftState* state) { Jump(state, false); }
extern "C" void ps5lift_import_pebqbE5ws8s(Ps5LiftState* state) { Jump(state, true); }
