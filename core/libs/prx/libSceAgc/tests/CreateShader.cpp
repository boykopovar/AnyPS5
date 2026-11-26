#include "prx/libSceAgc/Shader/include/CreateShader.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

const Shader* registered = nullptr;
unsigned registrations = 0;

alignas(256) const std::array<std::uint32_t, 64> code{0xbf810000u};
alignas(256) const std::array<std::uint32_t, 64> movedCode{0xbf810000u};

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename T>
T* relative(T* target, const void* field) {
    return reinterpret_cast<T*>(reinterpret_cast<std::uintptr_t>(target) - reinterpret_cast<std::uintptr_t>(field));
}

struct Binary {
    Shader shader{};
    std::array<ShaderRegister, 3> registers{};
    std::array<ShaderRegister, 1> context{};
    ShaderSpecialRegs specials{};
    ShaderUserData userData{};
    std::array<std::uint16_t, 2> direct{};
    std::array<ShaderSharp, 4> sharps{};
    std::array<ShaderSemantic, 1> inputs{};
    std::array<ShaderSemantic, 1> outputs{};
};

void fill(Binary& binary, ShaderRegs::ShaderBinaryType type, std::uint32_t low) {
    binary.shader.file_header = ShaderRegs::SHADER_FILE_HEADER_MAGIC;
    binary.shader.version = ShaderRegs::SHADER_VERSION;
    binary.shader.header_size = sizeof(Shader);
    binary.shader.shader_size = sizeof(code);
    binary.shader.type = static_cast<std::uint8_t>(type);
    binary.registers = {{{low, 0x10u}, {low + 1u, 0xabcdef00u}, {0x20au, 0x1234u}}};
    binary.shader.num_sh_registers = static_cast<std::uint8_t>(binary.registers.size());
    binary.shader.sh_registers = relative(binary.registers.data(), &binary.shader.sh_registers);
    binary.shader.specials = relative(&binary.specials, &binary.shader.specials);
}

Shader* create(Binary& binary, const std::array<std::uint32_t, 64>& at = code) {
    registered = nullptr;
    registrations = 0;
    Shader* created = nullptr;
    check(sceAgcCreateShader(&created, &binary.shader, at.data()) == 0, "sceAgcCreateShader failed");
    check(created == &binary.shader && registered == &binary.shader && registrations == 1, "the created shader is not the header, registered once");
    check(binary.shader.code == at.data(), "the code address was not stored in the header");
    return created;
}

void testCompute() {
    Binary binary;
    fill(binary, ShaderRegs::ShaderBinaryType::Cs, ShaderRegs::COMPUTE_PGM_LO);
    create(binary);
    check(binary.shader.sh_registers == binary.registers.data() && binary.shader.specials == &binary.specials, "the relative register and special pointers were not resolved");
    check(binary.shader.user_data == nullptr && binary.shader.cx_registers == nullptr && binary.shader.input_semantics == nullptr && binary.shader.output_semantics == nullptr, "a null relative pointer did not stay null");
    const auto program = reinterpret_cast<std::uint64_t>(code.data()) + 0x1000u;
    check(binary.registers[0].value == static_cast<std::uint32_t>(program >> 8u), "COMPUTE_PGM_LO does not hold the code address plus its offset");
    check(binary.registers[1].value == (0xabcdef00u | static_cast<std::uint32_t>((program >> 40u) & 0xffu)), "COMPUTE_PGM_HI did not keep its upper bits around the address byte");
    check(binary.registers[2].offset == 0x20au && binary.registers[2].value == 0x1234u, "a register other than the program address changed");
}

void testPixelWithUserData() {
    Binary binary;
    fill(binary, ShaderRegs::ShaderBinaryType::Ps, ShaderRegs::SPI_SHADER_PGM_LO_PS);
    binary.shader.user_data = relative(&binary.userData, &binary.shader.user_data);
    binary.shader.cx_registers = relative(binary.context.data(), &binary.shader.cx_registers);
    binary.shader.num_cx_registers = 1;
    binary.shader.input_semantics = relative(binary.inputs.data(), &binary.shader.input_semantics);
    binary.shader.output_semantics = relative(binary.outputs.data(), &binary.shader.output_semantics);
    binary.userData.direct_resource_offset = relative(binary.direct.data(), &binary.userData.direct_resource_offset);
    for (std::size_t i = 0; i < binary.sharps.size(); ++i) binary.userData.sharp_resource_offset[i] = relative(&binary.sharps[i], &binary.userData.sharp_resource_offset[i]);
    create(binary);
    check(binary.shader.user_data == &binary.userData && binary.shader.cx_registers == binary.context.data(), "the relative user data and context register pointers were not resolved");
    check(binary.shader.input_semantics == binary.inputs.data() && binary.shader.output_semantics == binary.outputs.data(), "the relative semantic pointers were not resolved");
    check(binary.userData.direct_resource_offset == binary.direct.data(), "the user data's direct resource pointer was not resolved");
    for (std::size_t i = 0; i < binary.sharps.size(); ++i) check(binary.userData.sharp_resource_offset[i] == &binary.sharps[i], "a user data sharp resource pointer was not resolved");
    const auto program = reinterpret_cast<std::uint64_t>(code.data()) + 0x1000u;
    check(binary.registers[0].value == static_cast<std::uint32_t>(program >> 8u) && binary.registers[1].value == (0xabcdef00u | static_cast<std::uint32_t>((program >> 40u) & 0xffu)), "SPI_SHADER_PGM_LO_PS and _HI_PS do not hold the code address");
}

void testCreatedAgain() {
    Binary binary;
    fill(binary, ShaderRegs::ShaderBinaryType::Ps, ShaderRegs::SPI_SHADER_PGM_LO_PS);
    binary.shader.user_data = relative(&binary.userData, &binary.shader.user_data);
    binary.userData.direct_resource_offset = relative(binary.direct.data(), &binary.userData.direct_resource_offset);
    create(binary);
    create(binary);
    check(binary.shader.sh_registers == binary.registers.data() && binary.shader.specials == &binary.specials && binary.shader.user_data == &binary.userData, "a second create resolved the header's pointers again");
    check(binary.userData.direct_resource_offset == binary.direct.data(), "a second create resolved the user data's pointers again");
    const auto program = reinterpret_cast<std::uint64_t>(code.data()) + 0x1000u;
    check(binary.registers[0].value == static_cast<std::uint32_t>(program >> 8u) && binary.registers[1].value == (0xabcdef00u | static_cast<std::uint32_t>((program >> 40u) & 0xffu)), "a second create from the same code moved the program address");
    create(binary, movedCode);
    const auto moved = reinterpret_cast<std::uint64_t>(movedCode.data()) + 0x1000u;
    check(binary.registers[0].value == static_cast<std::uint32_t>(moved >> 8u) && binary.registers[1].value == (0xabcdef00u | static_cast<std::uint32_t>((moved >> 40u) & 0xffu)), "a create from other code did not rebase the program address onto it");
}

void testWithoutProgramAddress() {
    Binary binary;
    fill(binary, ShaderRegs::ShaderBinaryType::Fs, 0x30u);
    const auto registers = binary.registers;
    create(binary);
    check(binary.registers[0].offset == registers[0].offset && binary.registers[0].value == registers[0].value && binary.registers[1].value == registers[1].value && binary.registers[2].value == registers[2].value, "a fetch shader's registers changed");
}

void testInvalidDst() {
    Binary binary;
    fill(binary, ShaderRegs::ShaderBinaryType::Cs, ShaderRegs::COMPUTE_PGM_LO);
    const auto originalSh = binary.shader.sh_registers;
    const auto originalLo = binary.registers[0].value;
    registered = nullptr;
    registrations = 0;
    errno = 71;
    try {
        sceAgcCreateShader(reinterpret_cast<Shader**>(std::uintptr_t{1}), &binary.shader, code.data());
        check(false, "unmapped dst pointer accepted");
    } catch (const std::runtime_error&) {}
    check(errno == 71, "unmapped dst mutated errno");
    check(registrations == 0 && binary.shader.code == nullptr && binary.shader.sh_registers == originalSh && binary.registers[0].value == originalLo,
          "unmapped dst mutated shader header or registered shader");

    if (GuestArena::GuestArenaAvailable_nid_postfix()) {
        constexpr std::size_t kArenaBytes = 0x10000;
        void* const page = GuestArena::GuestArenaAllocate_nid_postfix(kArenaBytes, kArenaBytes);
        check(page != nullptr, "allocating write-watched page failed");
#ifdef _WIN32
        const HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE, 0, kArenaBytes, nullptr);
        check(section != nullptr, "creating write-watched section failed");
        GuestArena::GuestArenaMap_nid_postfix(page, kArenaBytes, section, 0, PAGE_READWRITE);
        CloseHandle(section);
        const bool watchedWrites = GuestArena::GuestArenaWriteWatched_nid_postfix();
#else
        GuestArena::GuestArenaCommit_nid_postfix(page, kArenaBytes, 0x04, kArenaBytes);
        const bool watchedWrites = GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix();
        if (watchedWrites) {
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(page, kArenaBytes);
        }
#endif
        const auto collectDirty = [&]() -> bool {
            if (!watchedWrites) {
                return true;
            }
#ifdef _WIN32
            std::array<void*, 16> pages{};
            std::size_t count = pages.size();
            check(GuestArena::GuestArenaCollectWrites_nid_postfix(reinterpret_cast<std::uintptr_t>(page), kArenaBytes, pages.data(), &count, true),
                  "collecting write-watched writes failed");
            return count != 0;
#else
            bool dirty = false;
            check(GuestWriteWatch::GuestWriteWatchCollect_nid_postfix(reinterpret_cast<std::uintptr_t>(page), kArenaBytes,
                [](void* context, std::uintptr_t, std::uintptr_t) { *static_cast<bool*>(context) = true; }, &dirty),
                "collecting write-watched writes failed");
            return dirty;
#endif
        };
        const auto armWatch = [&](void* target) {
            if (!watchedWrites) {
                return;
            }
            static_cast<void>(collectDirty());
            check(!collectDirty(), "write-watched page was still dirty");
#ifdef _WIN32
            MEMORY_BASIC_INFORMATION mbi{};
            check(VirtualQuery(target, &mbi, sizeof(mbi)) == sizeof(mbi) && mbi.Protect == PAGE_READONLY, "arming write-watched page failed");
#else
            static_cast<void>(target);
#endif
        };
        auto* const watchedDst = static_cast<Shader**>(page);
        *watchedDst = nullptr;
        armWatch(watchedDst);
        errno = 71;
        check(sceAgcCreateShader(watchedDst, &binary.shader, code.data()) == 0, "write-watched dst pointer failed");
        check(errno == 71, "write-watched dst mutated errno");
        check(*watchedDst == &binary.shader && registered == &binary.shader && registrations == 1,
              "write-watched dst pointer did not receive registered shader");
        check(collectDirty(), "write-watched dst pointer did not mark page dirty");

        Binary roBinary;
        fill(roBinary, ShaderRegs::ShaderBinaryType::Cs, ShaderRegs::COMPUTE_PGM_LO);
        const auto roOriginalSh = roBinary.shader.sh_registers;
        const auto roOriginalLo = roBinary.registers[0].value;
        *watchedDst = nullptr;
        GuestArena::GuestArenaSetProtection_nid_postfix(reinterpret_cast<std::uintptr_t>(page), kArenaBytes, 0x02);
        registered = nullptr;
        registrations = 0;
        errno = 71;
        try {
            sceAgcCreateShader(watchedDst, &roBinary.shader, code.data());
            check(false, "read-only dst pointer accepted");
        } catch (const std::runtime_error&) {}
        check(errno == 71, "read-only dst mutated errno");
        check(*watchedDst == nullptr && registrations == 0 && roBinary.shader.code == nullptr &&
              roBinary.shader.sh_registers == roOriginalSh && roBinary.registers[0].value == roOriginalLo,
              "read-only dst mutated header, destination, or registered shader");
        GuestArena::GuestArenaSetProtection_nid_postfix(reinterpret_cast<std::uintptr_t>(page), kArenaBytes, 0x04);
#ifndef _WIN32
        if (watchedWrites) {
            GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(page, kArenaBytes);
        }
#endif
        GuestArena::GuestArenaReset_nid_postfix(page, kArenaBytes);
        GuestArena::GuestArenaRelease_nid_postfix(page, kArenaBytes);
    }
}

}

extern "C" void AgcDriverRegisterShader_nid_postfix(const Shader* shader) {
    registered = shader;
    ++registrations;
}

int main() {
    try {
        testCompute();
        testPixelWithUserData();
        testCreatedAgain();
        testWithoutProgramAddress();
        testInvalidDst();
        std::puts("AGC create shader tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
