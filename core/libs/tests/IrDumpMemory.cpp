#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaMemoryOpDecoder.hpp"
#include <array>
#include <cstdio>
#include <initializer_list>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ShaderRecompiler;

static void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}

static RdnaInstruction Smem(std::uint32_t pc, std::uint32_t opcode, std::uint32_t destination, std::uint32_t base, std::uint32_t offset) {
    const std::array<std::uint32_t, 2> code{(0x3du << 26u) | (opcode << 18u) | (destination << 6u) | (base / 2u), (125u << 25u) | offset};
    return DecodeRdnaSmem(pc, code, 0u);
}

static RdnaInstruction Ds(std::uint32_t pc, std::uint32_t opcode, std::uint32_t offset0, std::uint32_t offset1, std::uint32_t address, std::uint32_t destination) {
    const std::array<std::uint32_t, 2> code{(0x36u << 26u) | (opcode << 18u) | (offset1 << 8u) | offset0, (destination << 24u) | address};
    return DecodeRdnaDs(pc, code, 0u);
}

static void Translate(IrProgram& program, std::initializer_list<RdnaInstruction> instructions) {
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    program.BlockOrder() = {&block};
    program.Metadata().blockInfo.resize(1);
    TranslationContext context(program, block, 256);
    for (const auto& instruction : instructions) {
        context.TranslateInstruction(instruction);
    }
}

static std::vector<std::string> Lines(const std::string& dump, const std::string& opcode) {
    std::vector<std::string> lines;
    std::istringstream stream(dump);
    for (std::string line; std::getline(stream, line);) {
        if (line.find(" " + opcode + " ") != std::string::npos) lines.push_back(line);
    }
    return lines;
}

static void RequireLines(const std::string& dump, const std::string& opcode, std::initializer_list<const char*> suffixes) {
    const auto lines = Lines(dump, opcode);
    Require(lines.size() == suffixes.size(), dump + "\nexpected " + std::to_string(suffixes.size()) + " " + opcode + " lines, found " + std::to_string(lines.size()));
    std::size_t index = 0;
    for (const std::string suffix : suffixes) {
        const auto& line = lines[index++];
        Require(line.size() >= suffix.size() && line.compare(line.size() - suffix.size(), suffix.size(), suffix) == 0, dump + "\nexpected \"" + line + "\" to end with \"" + suffix + "\"");
    }
}

static void CheckOffsets() {
    IrProgram program;
    Translate(program, {
        Smem(0x12de0u, 0x00u, 4u, 0u, 0x54u),
        Smem(0x12de8u, 0x0au, 12u, 8u, 0x60u),
        Ds(0x12df0u, 0x37u, 3u, 9u, 1u, 2u),
    });
    const auto dump = ProgramToString(program);
    RequireLines(dump, "LoadAddressU32", {" [pc=0x00012de0 off=0x54]"});
    RequireLines(dump, "ReadConstBuffer", {" [pc=0x00012de8 off=0x60]", " [pc=0x00012de8 off=0x64]", " [pc=0x00012de8 off=0x68]", " [pc=0x00012de8 off=0x6c]"});
    RequireLines(dump, "LoadSharedU32", {" [pc=0x00012df0 off=0xc]", " [pc=0x00012df0 off=0x24]"});
    Require(dump.find("  %1 = GetScalarRegister s0 (U32; uses=1)\n") != std::string::npos, dump + "\nexpected register reads without memory fields");
}

static void CheckMissingMemoryInfo() {
    IrProgram program;
    Translate(program, {Smem(0x40u, 0x00u, 4u, 0u, 0x54u)});
    program.Resources().memoryInfo.clear();
    const auto dump = ProgramToString(program);
    RequireLines(dump, "LoadAddressU32", {" [pc=0x00000040 mem=?]"});
    bool rejected = false;
    try {
        ValidateProgram(program, false);
    } catch (const std::runtime_error& error) {
        rejected = std::string(error.what()) == "LoadAddressU32 has an invalid memory-info index";
        Require(rejected, std::string("unexpected validation failure: ") + error.what());
    }
    Require(rejected, "validation accepted a memory instruction without memory info");
}

static void CheckScalarStore() {
    IrProgram program;
    Translate(program, {
        Smem(0x80u, 0x10u, 8u, 0u, 0x20u),
        Smem(0x88u, 0x11u, 10u, 2u, 0x24u),
        Smem(0x90u, 0x12u, 12u, 4u, 0x28u),
    });
    const auto dump = ProgramToString(program);
    RequireLines(dump, "StoreAddressU32", {" [pc=0x00000080 off=0x20]"});
    RequireLines(dump, "StoreAddressU32x2", {" [pc=0x00000088 off=0x24]"});
    RequireLines(dump, "StoreAddressU32x4", {" [pc=0x00000090 off=0x28]"});
    Require(Lines(dump, "LoadAddressU32").empty(), dump + "\nexpected no loads from a store");
}

static void CheckScalarAtomic() {
    IrProgram program;
    Translate(program, {
        Smem(0x100u, 0x82u, 6u, 0u, 0x18u),
        Smem(0x110u, 0x81u, 8u, 2u, 0x1cu),
        Smem(0x120u, 0xa2u, 10u, 4u, 0x20u),
        Smem(0x130u, 0x8bu, 16u, 0u, 0x24u),
    });
    const auto dump = ProgramToString(program);
    RequireLines(dump, "SharedAtomicIAdd32", {" [pc=0x00000100 off=0x18]"});
    RequireLines(dump, "SharedAtomicCmpst32", {" [pc=0x00000110 off=0x1c]"});
    RequireLines(dump, "SharedAtomicIAdd64", {" [pc=0x00000120 off=0x20]"});
    RequireLines(dump, "SharedAtomicInc32", {" [pc=0x00000130 off=0x24]"});
    Require(Lines(dump, "IAdd32").size() == 4u, dump + "\nexpected one byte-address add per atomic");
    Require(Lines(dump, "LoadAddressU32").empty(), dump + "\nexpected no global loads from an LDS atomic");
    Require(dump.find("SetScalarRegister s6") != std::string::npos, dump + "\nexpected the 32-bit old value in s6");
    Require(dump.find("SetScalarRegister s10") != std::string::npos, dump + "\nexpected the 64-bit old value in s10");
    ValidateProgram(program, false);
}

int main() {
    try {
        CheckOffsets();
        CheckMissingMemoryInfo();
        CheckScalarStore();
        CheckScalarAtomic();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    return 0;
}
