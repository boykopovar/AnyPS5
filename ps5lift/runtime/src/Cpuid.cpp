#include <ps5lift/State.h>

#include <cinttypes>
#include <cstdint>

namespace {

constexpr std::uint32_t Bit(const unsigned index) {
    return std::uint32_t{1} << index;
}

constexpr std::uint32_t MaximumLeaf = 0xd;
constexpr std::uint32_t MaximumExtendedLeaf = 0x80000001;
constexpr std::uint32_t VendorEbx = 0x68747541;
constexpr std::uint32_t VendorEdx = 0x69746e65;
constexpr std::uint32_t VendorEcx = 0x444d4163;
constexpr std::uint32_t Signature = 0x00840f70;
constexpr std::uint32_t Leaf1Ebx = 16u << 16 | 8u << 8;
constexpr std::uint32_t Leaf1Ecx = Bit(0) | Bit(1) | Bit(9) | Bit(12) | Bit(13) | Bit(19) | Bit(20) | Bit(22) | Bit(23) | Bit(25) | Bit(26) | Bit(27) | Bit(28) | Bit(29) | Bit(30);
constexpr std::uint32_t Leaf1Edx = 0x178bfbff;
constexpr std::uint32_t Leaf7Ebx = Bit(3) | Bit(5) | Bit(8) | Bit(18) | Bit(19) | Bit(23) | Bit(24) | Bit(29);
constexpr std::uint32_t Leaf7Ecx = Bit(22);
constexpr std::uint64_t EnabledState = 0x7;
constexpr std::uint32_t XsaveSize = 0x340;
constexpr std::uint32_t XsaveFeatures = Bit(0) | Bit(1) | Bit(2) | Bit(3);
constexpr std::uint32_t AvxStateSize = 0x100;
constexpr std::uint32_t AvxStateOffset = 0x240;
constexpr std::uint32_t Leaf80000001Ecx = Bit(0) | Bit(5) | Bit(6) | Bit(7) | Bit(8);
constexpr std::uint32_t Leaf80000001Edx = 0x2fd3fbff;

struct Answer {
    std::uint32_t Eax;
    std::uint32_t Ebx;
    std::uint32_t Ecx;
    std::uint32_t Edx;
};

Answer Identify(const std::uint64_t address, const std::uint32_t leaf, const std::uint32_t subleaf) {
    switch (leaf) {
    case 0x0: return {MaximumLeaf, VendorEbx, VendorEcx, VendorEdx};
    case 0x1: return {Signature, Leaf1Ebx, Leaf1Ecx, Leaf1Edx};
    case 0x2: case 0x3: case 0x4: case 0x8: case 0x9: case 0xa: case 0xb: case 0xc: return {};
    case 0x7: return subleaf == 0 ? Answer{0, Leaf7Ebx, Leaf7Ecx, 0} : Answer{};
    case 0xd:
        if (subleaf == 0) return {static_cast<std::uint32_t>(EnabledState), XsaveSize, XsaveSize, 0};
        if (subleaf == 1) return {XsaveFeatures, XsaveSize, 0, 0};
        if (subleaf == 2) return {AvxStateSize, AvxStateOffset, 0, 0};
        return {};
    case 0x80000000: return {MaximumExtendedLeaf, VendorEbx, VendorEcx, VendorEdx};
    case 0x80000001: return {Signature, 0, Leaf80000001Ecx, Leaf80000001Edx};
    default:
        if (leaf <= MaximumLeaf || (leaf >= 0x80000000 && leaf <= MaximumExtendedLeaf)) ps5lift_fail("cpuid leaf 0x%" PRIx32 " at 0x%" PRIx64 " has no PS5 value", leaf, address);
        return {};
    }
}

}

extern "C" void ps5lift_cpuid(Ps5LiftState* state, const std::uint64_t address) {
    const auto answer = Identify(address, static_cast<std::uint32_t>(state->Gpr[PS5LIFT_RAX]), static_cast<std::uint32_t>(state->Gpr[PS5LIFT_RCX]));
    state->Gpr[PS5LIFT_RAX] = answer.Eax;
    state->Gpr[PS5LIFT_RBX] = answer.Ebx;
    state->Gpr[PS5LIFT_RCX] = answer.Ecx;
    state->Gpr[PS5LIFT_RDX] = answer.Edx;
}

extern "C" std::uint64_t ps5lift_xgetbv(const std::uint64_t address, const std::uint32_t index) {
    if (index > 1) ps5lift_fail("xgetbv register 0x%" PRIx32 " at 0x%" PRIx64 " has no PS5 value", index, address);
    return EnabledState;
}

extern "C" void GuestCpuCpuid_nid_no_patch(const std::uint32_t leaf, const std::uint32_t subleaf, std::uint32_t* const registers) {
    const auto answer = Identify(0, leaf, subleaf);
    registers[0] = answer.Eax;
    registers[1] = answer.Ebx;
    registers[2] = answer.Ecx;
    registers[3] = answer.Edx;
}

extern "C" std::uint64_t GuestCpuXgetbv_nid_no_patch(const std::uint32_t index) {
    return ps5lift_xgetbv(0, index);
}
