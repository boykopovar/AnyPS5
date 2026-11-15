#pragma once

#include <cstdint>
#include <initializer_list>

namespace GnmCommandWriters {
namespace {

constexpr int CommandFailed = -1;
constexpr std::uint32_t OpNop = 0x10;
constexpr std::uint32_t OpSetContextReg = 0x69;
constexpr std::uint32_t OpSetShReg = 0x76;

std::uint32_t* EmitPacket(std::uint32_t* at, std::uint32_t opcode, std::initializer_list<std::uint32_t> body) {
    *at++ = 0xc0000000u | ((static_cast<std::uint32_t>(body.size()) - 1) << 16) | (opcode << 8);
    for (const auto word : body) *at++ = word;
    return at;
}

constexpr std::uint32_t Header(std::uint32_t opcode, std::uint32_t bodyWords, bool compute = false, bool predicated = false) {
    return 0xc0000000u | ((bodyWords - 1) << 16) | (opcode << 8) | (compute ? 2u : 0u) | (predicated ? 1u : 0u);
}

std::uint32_t* Nop(std::uint32_t* at, std::uint32_t bodyWords) {
    at[0] = Header(OpNop, bodyWords);
    at[1] = 0;
    return at + 1 + bodyWords;
}

std::uint32_t* UpdatePsRegisters(std::uint32_t* cmdbuf, const std::uint32_t* regs) {
    auto* at = EmitPacket(cmdbuf, OpSetShReg, {8, regs[0], 0});
    at = EmitPacket(at, OpSetShReg, {10, regs[2], regs[3]});
    at = EmitPacket(at, OpNop, {0xc01e01c4u, regs[4], regs[5]});
    at = EmitPacket(at, OpNop, {0xc01e01b3u, regs[6], regs[7]});
    at = EmitPacket(at, OpNop, {0xc01e01b6u, regs[8]});
    at = EmitPacket(at, OpNop, {0xc01e01b8u, regs[9]});
    at = EmitPacket(at, OpNop, {0xc01e0203u, regs[10]});
    return EmitPacket(at, OpNop, {0xc01e008fu, regs[11]});
}

inline int DispatchDirect(std::uint32_t* cmdbuf, std::uint32_t size, std::uint32_t threadsX, std::uint32_t threadsY, std::uint32_t threadsZ, std::uint32_t flags) {
    if (!cmdbuf || size != 9 || static_cast<std::int32_t>(threadsX | threadsY | threadsZ) < 0) return CommandFailed;
    cmdbuf[0] = Header(0x15, 4, true, flags & 1);
    cmdbuf[1] = threadsX;
    cmdbuf[2] = threadsY;
    cmdbuf[3] = threadsZ;
    cmdbuf[4] = (flags & 0x18) + 1;
    Nop(cmdbuf + 5, 3);
    return 0;
}

inline int DispatchIndirect(std::uint32_t* cmdbuf, std::uint32_t size, std::uint32_t dataOffset, std::uint32_t flags) {
    if (!cmdbuf || size != 7) return CommandFailed;
    cmdbuf[0] = Header(0x16, 2, true, flags & 1);
    cmdbuf[1] = dataOffset;
    cmdbuf[2] = (flags & 0x18) + 1;
    Nop(cmdbuf + 3, 3);
    return 0;
}

inline int DispatchIndirectOnMec(std::uint32_t* cmdbuf, std::uint32_t size, std::uint64_t args, std::uint32_t modifier) {
    if (!cmdbuf || size != 8 || args == 0 || (args & 3) != 0) return CommandFailed;
    cmdbuf[0] = Header(0x16, 3, true, modifier & 1);
    cmdbuf[1] = static_cast<std::uint32_t>(args);
    cmdbuf[2] = static_cast<std::uint32_t>(args >> 32);
    cmdbuf[3] = (modifier & 0x18) | 1;
    cmdbuf[4] = Header(OpNop, 3);
    cmdbuf[5] = 0;
    return 0;
}

inline int SetCsShaderWithModifier(std::uint32_t* cmdbuf, std::uint32_t size, const std::uint32_t* regs, std::uint32_t modifier) {
    if (!cmdbuf || size <= 0x18 || !regs || (modifier & 0xfffffc3fu) != 0 || regs[1] != 0) return CommandFailed;
    const std::uint32_t rsrc1 = modifier == 0 ? regs[2] : (regs[2] & 0xfffffc3fu) | modifier;
    auto* at = EmitPacket(cmdbuf, OpSetShReg, {0x20c, regs[0], 0});
    at = EmitPacket(at, OpSetShReg, {0x212, rsrc1, regs[3]});
    at = EmitPacket(at, OpSetShReg, {0x207, regs[4], regs[5], regs[6]});
    Nop(at, 11);
    return 0;
}

inline int SetPsShader350(std::uint32_t* cmdbuf, std::uint32_t size, const std::uint32_t* regs) {
    if (!cmdbuf || size <= 0x27) return CommandFailed;
    if (!regs) {
        auto* at = EmitPacket(cmdbuf, OpSetShReg, {8, 0, 0});
        at = EmitPacket(at, OpSetContextReg, {0x203, 0});
        at = EmitPacket(at, OpSetContextReg, {0x8f, 0xf});
        Nop(at, 0x1d);
        return 0;
    }
    if (regs[1] != 0) return CommandFailed;
    auto* at = EmitPacket(cmdbuf, OpSetShReg, {8, regs[0], 0});
    at = EmitPacket(at, OpSetShReg, {10, regs[2], regs[3]});
    at = EmitPacket(at, OpSetContextReg, {0x1c4, regs[4], regs[5]});
    at = EmitPacket(at, OpSetContextReg, {0x1b3, regs[6], regs[7]});
    at = EmitPacket(at, OpSetContextReg, {0x1b6, regs[8]});
    at = EmitPacket(at, OpSetContextReg, {0x1b8, regs[9]});
    at = EmitPacket(at, OpSetContextReg, {0x203, regs[10]});
    at = EmitPacket(at, OpSetContextReg, {0x8f, regs[11]});
    Nop(at, 11);
    return 0;
}

inline int UpdateGsShader(std::uint32_t* cmdbuf, std::uint32_t size, const std::uint32_t* regs) {
    if (!cmdbuf || size < 0x1d || !regs || regs[1] != 0) return CommandFailed;
    auto* at = EmitPacket(cmdbuf, OpSetShReg, {0x88, regs[0], 0});
    at = EmitPacket(at, OpSetShReg, {0x8a, regs[2], regs[3]});
    at = EmitPacket(at, OpNop, {0xc01e02e5u, regs[4]});
    at = EmitPacket(at, OpNop, {0xc01e029bu, regs[5]});
    at = EmitPacket(at, OpNop, {0xc01e02e4u, regs[6]});
    Nop(at, 11);
    return 0;
}

inline int UpdateHsShader(std::uint32_t* cmdbuf, std::uint32_t size, const std::uint32_t* regs, std::uint32_t lsHsConfig) {
    if (!cmdbuf || size <= 0x1c || !regs || regs[1] != 0) return CommandFailed;
    auto* at = EmitPacket(cmdbuf, OpSetShReg, {0x108, regs[0], 0});
    at = EmitPacket(at, OpSetShReg, {0x10a, regs[2], regs[3]});
    at = EmitPacket(at, OpNop, {0xc01e0286u, regs[5], regs[6]});
    at = EmitPacket(at, OpNop, {0xc01e02dbu, regs[4]});
    at = EmitPacket(at, OpNop, {0xc01e02d6u, lsHsConfig});
    Nop(at, 11);
    return 0;
}

inline int UpdatePsShader(std::uint32_t* cmdbuf, std::uint32_t size, const std::uint32_t* regs) {
    if (!cmdbuf || size <= 0x27) return CommandFailed;
    if (!regs) {
        auto* at = EmitPacket(cmdbuf, OpSetShReg, {8, 0, 0});
        at = EmitPacket(at, OpNop, {0xc01e0203u, 0});
        Nop(at, 0x20);
        return 0;
    }
    if (regs[1] != 0) return CommandFailed;
    Nop(UpdatePsRegisters(cmdbuf, regs), 11);
    return 0;
}

inline int UpdatePsShader350(std::uint32_t* cmdbuf, std::uint32_t size, const std::uint32_t* regs) {
    if (!cmdbuf || size <= 0x27) return CommandFailed;
    if (!regs) {
        auto* at = EmitPacket(cmdbuf, OpSetShReg, {8, 0, 0});
        at = EmitPacket(at, OpNop, {0xc01e0203u, 0});
        at = EmitPacket(at, OpSetContextReg, {0x8f, 0xf});
        Nop(at, 0x1d);
        return 0;
    }
    if (regs[1] != 0) return CommandFailed;
    Nop(UpdatePsRegisters(cmdbuf, regs), 11);
    return 0;
}

inline int UpdateVsShader(std::uint32_t* cmdbuf, std::uint32_t size, const std::uint32_t* regs, std::uint32_t modifier) {
    if (!cmdbuf || size <= 0x1c || !regs || (modifier & 0xfcfffc3fu) != 0 || regs[1] != 0) return CommandFailed;
    const std::uint32_t rsrc1 = modifier == 0 ? regs[2] : (regs[2] & 0xfcfffc3fu) | modifier;
    auto* at = EmitPacket(cmdbuf, OpSetShReg, {0x48, regs[0], 0});
    at = EmitPacket(at, OpSetShReg, {0x4a, rsrc1, regs[3]});
    at = EmitPacket(at, OpNop, {0xc01e0207u, regs[6]});
    at = EmitPacket(at, OpNop, {0xc01e01b1u, regs[4]});
    at = EmitPacket(at, OpNop, {0xc01e01c3u, regs[5]});
    Nop(at, 11);
    return 0;
}

} // namespace
} // namespace GnmCommandWriters
