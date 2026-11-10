#include "prx/libSceAgc/Misc/include/Suspend.hpp"
#include "prx/libSceAgc/Misc/include/ShaderFusion.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"
#include "prx/libc/include/Shutdown.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {

constexpr int InvalidShaderHalves = static_cast<int>(0x8a6c0008u);
constexpr std::uint32_t SSetpcS6 = 0xbe802006u;
constexpr std::uint32_t GsStage = 1u << 22u;

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename TAction>
void expectFailure(TAction action) {
    try {
        action();
    } catch (const std::exception& error) {
        check(error.what()[0] != '\0', "empty exception message");
        return;
    }
    throw std::runtime_error("expected an exception");
}

void testSuspendErrorName() {
    check(GRAPHICS5_ERROR_SUSPEND_DISALLOWED == static_cast<int>(0x8a6c002fu), "suspend-disallowed value drifted");
    check(GRAPHICS5_ERROR_SUSPEND_DISALLOWED != ShaderRegs::GRAPHICS5_ERROR_INVALID_SHADER_PROGRAM,
        "suspend-disallowed aliases the invalid-program error");
}

void testSuspendCheckStatus() {
    expectFailure([] { sceAgcSuspendPointAndCheckStatus(nullptr); });
    expectFailure([] { sceAgcSuspendPointAndCheckStatus(reinterpret_cast<void*>(0x1)); });
    std::uint32_t token = 0x12345678u;
    check(sceAgcSuspendPointAndCheckStatus(&token) == 0, "readable suspend point was not accepted");
    check(sceAgcSuspendPoint() == 0, "empty suspend failed");
}

// Minimal valid Gs front/back pair: just enough header, code trailer and resource
// registers for the size queries, which never decode instructions without an
// embedded constant buffer and never register shaders.
struct Halves {
    std::array<std::uint32_t, 8> frontCode{SSetpcS6, 0, 0, 0xabcdu, 0u, 1u, 4u, 0u};
    std::array<std::uint32_t, 2> backCode{0xbe800380u, 0xbf810000u};
    std::array<ShaderRegister, 2> frontSh{{{0x08Au, 3u}, {0x08Bu, 6u << 1u}}};
    std::array<ShaderRegister, 2> backSh{{{0x08Au, 5u}, {0x08Bu, 2u << 1u}}};
    ShaderSpecialRegs frontSpecials{};
    ShaderSpecialRegs backSpecials{};
    Shader front{};
    Shader back{};

    Halves() {
        std::memcpy(frontCode.data() + 1, "barefoot", 8);
        frontSpecials.vgt_shader_stages_en.value = GsStage;
        backSpecials.vgt_shader_stages_en.value = GsStage;
        front.file_header = ShaderRegs::SHADER_FILE_HEADER_MAGIC;
        front.version = ShaderRegs::SHADER_VERSION;
        front.code = frontCode.data();
        front.shader_size = static_cast<std::uint32_t>(frontCode.size() * 4);
        front.sh_registers = frontSh.data();
        front.num_sh_registers = static_cast<std::uint8_t>(frontSh.size());
        front.specials = &frontSpecials;
        front.type = static_cast<std::uint8_t>(ShaderRegs::ShaderBinaryType::GsFront);
        back.file_header = ShaderRegs::SHADER_FILE_HEADER_MAGIC;
        back.version = ShaderRegs::SHADER_VERSION;
        back.code = backCode.data();
        back.shader_size = static_cast<std::uint32_t>(backCode.size() * 4);
        back.sh_registers = backSh.data();
        back.num_sh_registers = static_cast<std::uint8_t>(backSh.size());
        back.specials = &backSpecials;
        back.type = static_cast<std::uint8_t>(ShaderRegs::ShaderBinaryType::GsBack);
    }
};

std::uint32_t alignLow(const SizeAlign& size) {
    std::uint32_t low = 0;
    std::memcpy(&low, reinterpret_cast<const unsigned char*>(&size) + 8, sizeof(low));
    return low;
}

std::uint32_t alignHigh(const SizeAlign& size) {
    std::uint32_t high = 0;
    std::memcpy(&high, reinterpret_cast<const unsigned char*>(&size) + 12, sizeof(high));
    return high;
}

void testFusedSizeCanary() {
    Halves halves;
    SizeAlign size{};
    std::memset(&size, 0xa5, sizeof(size));
    check(sceAgcGetFusedShaderSize_0080(&size, &halves.front, &halves.back) == 0, "_0080 size query failed");
    check(size.m_size == halves.back.num_sh_registers * sizeof(ShaderRegister), "_0080 size is not the back half's register list");
    // The firmware writes the alignment as a 32-bit value at +8: the low 32 bits
    // read 4 while the +12 canary survives, all read back with memcpy over bytes.
    check(alignLow(size) == 4, "_0080 alignment low half is not 4");
    check(alignHigh(size) == 0xa5a5a5a5u, "_0080 size write clobbered +12");
}

void testNullPairOrder() {
    Halves halves;
    // Pair validation precedes the output check: a swapped pair with a null output
    // still reports invalid halves instead of rejecting the output.
    check(sceAgcGetFusedShaderSize_0080(nullptr, &halves.back, &halves.front) == InvalidShaderHalves,
        "_0080 checked its output before the halves");
    check(sceAgcGetFusedShaderSize(nullptr, &halves.back, &halves.front) == InvalidShaderHalves,
        "the unversioned size query checked its output before the halves");
    // A null output with valid halves is rejected where the console would fault.
    expectFailure([&] { sceAgcGetFusedShaderSize_0080(nullptr, &halves.front, &halves.back); });
    expectFailure([&] { sceAgcGetFusedShaderSize(nullptr, &halves.front, &halves.back); });
}

}

int main() {
    try {
        testSuspendErrorName();
        testSuspendCheckStatus();
        testFusedSizeCanary();
        testNullPairOrder();
        LibcRunShutdown_nid_postfix();
        std::puts("AGC suspend status tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try { LibcRunShutdown_nid_postfix(); }
        catch (const std::exception& shutdown) { std::fprintf(stderr, "shutdown: %s\n", shutdown.what()); }
        return 1;
    }
}
