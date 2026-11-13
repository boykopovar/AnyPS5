#include "LoweringExecution.hpp"
#include <codegen/x86/Avx2TwinLowering.hpp>
#include <codegen/x86/IAmd64OnlyInstructionMatcher.hpp>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using LoweringExecution::Bytes;
using Vector = std::array<std::uint32_t, 8>;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

struct Case {
    const char* Name;
    Bytes Site;
    Bytes Replacement;
};

std::vector<Case> cases(std::uint8_t imm) {
    return {
        {"VINSERTI128", {0xC4, 0xE3, 0x75, 0x38, 0xC2, imm}, {0xC4, 0xE3, 0x75, 0x18, 0xC2, imm}},
        {"VEXTRACTI128", {0xC4, 0xE3, 0x7D, 0x39, 0xC8, imm}, {0xC4, 0xE3, 0x7D, 0x19, 0xC8, imm}},
        {"VPERM2I128", {0xC4, 0xE3, 0x75, 0x46, 0xC2, imm}, {0xC4, 0xE3, 0x75, 0x06, 0xC2, imm}},
        {"VPBLENDD", {0xC4, 0xE3, 0x75, 0x02, 0xC2, imm}, {0xC4, 0xE3, 0x75, 0x0C, 0xC2, imm}},
        {"VPBLENDD", {0xC4, 0xE3, 0x71, 0x02, 0xC2, imm}, {0xC4, 0xE3, 0x71, 0x0C, 0xC2, imm}},
        {"VBROADCASTI128", {0xC4, 0xE2, 0x7D, 0x5A, 0x00}, {0xC4, 0xE2, 0x7D, 0x1A, 0x00}},
        {"VPBROADCASTD", {0xC4, 0xE2, 0x7D, 0x58, 0x00}, {0xC4, 0xE2, 0x7D, 0x18, 0x00}},
        {"VPBROADCASTD", {0xC4, 0xE2, 0x79, 0x58, 0x00}, {0xC4, 0xE2, 0x79, 0x18, 0x00}},
        {"VPBROADCASTQ", {0xC4, 0xE2, 0x7D, 0x59, 0x00}, {0xC4, 0xE2, 0x7D, 0x19, 0x00}},
        {"VPBROADCASTQ", {0xC4, 0xE2, 0x79, 0x59, 0x00}, {0xC4, 0xE1, 0x7B, 0x12, 0x00}},
    };
}

void decoding() {
    for (const auto& item : cases(0x5B)) {
        const auto twin = Codegen::DecodeAvx2Twin(item.Site.data(), item.Site.size());
        require(twin && std::string(twin->Name) == item.Name && twin->Replacement == item.Replacement, std::string(item.Name) + " was not rewritten to its AVX1 twin");
    }
    const Bytes prefixed{0x64, 0x67, 0xC4, 0x62, 0x7D, 0x58, 0x05, 0x10, 0x20, 0x30, 0x40};
    const auto rip = Codegen::DecodeAvx2Twin(prefixed.data(), prefixed.size());
    require(rip && rip->Replacement == Bytes{0x64, 0x67, 0xC4, 0x62, 0x7D, 0x18, 0x05, 0x10, 0x20, 0x30, 0x40}, "A prefixed, extended or RIP-relative VPBROADCASTD lost a byte");
    const Bytes extended{0xC4, 0x42, 0x79, 0x59, 0x0C, 0x88};
    const auto movddup = Codegen::DecodeAvx2Twin(extended.data(), extended.size());
    require(movddup && movddup->Replacement == Bytes{0xC4, 0x41, 0x7B, 0x12, 0x0C, 0x88}, "VPBROADCASTQ xmm lost its register extensions in VMOVDDUP");
    for (const Bytes bytes : {
             Bytes{0xC4, 0xE2, 0x7D, 0x58, 0xC1},
             Bytes{0xC4, 0xE2, 0x7D, 0x59, 0xC1},
             Bytes{0xC4, 0xE3, 0xFD, 0x00, 0xC1, 0x1B},
             Bytes{0xC4, 0xE3, 0x71, 0x38, 0xC2, 0x01},
             Bytes{0xC4, 0xE3, 0x79, 0x39, 0xC8, 0x01},
             Bytes{0xC4, 0xE3, 0x71, 0x46, 0xC2, 0x01},
             Bytes{0xC4, 0xE2, 0x79, 0x5A, 0x00},
             Bytes{0xC4, 0xE3, 0xF5, 0x02, 0xC2, 0x01},
             Bytes{0xC4, 0xE3, 0x74, 0x02, 0xC2, 0x01},
             Bytes{0xC5, 0xFD, 0xFE, 0xC1},
             Bytes{0xC4, 0xE2, 0x7D, 0x78, 0x00},
             Bytes{0xC4, 0xE3},
             Bytes{0x90}})
        require(!Codegen::DecodeAvx2Twin(bytes.data(), bytes.size()), "An instruction without an AVX1 twin was rewritten");
}

void matching() {
    const auto intel = Codegen::MakeAmd64OnlyInstructionMatcher();
    const auto rosetta = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Rosetta);
    const auto avx = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Avx);
    for (const auto& item : cases(1)) {
        require(!intel->Match(item.Site.data(), item.Site.size()) && !rosetta->Match(item.Site.data(), item.Site.size()), std::string("Intel or Rosetta mode rewrote ") + item.Name);
        const auto match = avx->Match(item.Site.data(), item.Site.size());
        require(match && match->Lowering == Codegen::Amd64OnlyLowering::InPlace && match->ReplacementBytes == item.Replacement && match->InstructionName == item.Name, std::string("AVX mode did not rewrite ") + item.Name + " in place");
        Codegen::StubBodyBuilder body;
        avx->EmitOutOfLine(body, item.Site);
        const auto lowered = body.Finish();
        require(Bytes(lowered.Bytes.begin(), lowered.Bytes.begin() + static_cast<std::ptrdiff_t>(lowered.ReturnBranchOffset)) == item.Replacement, std::string("A moved ") + item.Name + " was not emitted as its twin");
    }
}

#if LOWERING_EXECUTION_AVAILABLE

struct Registers {
    Vector Ymm0;
    Vector Ymm1;
    Vector Ymm2;
    Vector Memory;
};

Registers run(const Bytes& instruction, const Registers& input) {
    Bytes code{0xC5, 0xFE, 0x6F, 0x07, 0xC5, 0xFE, 0x6F, 0x4F, 0x20, 0xC5, 0xFE, 0x6F, 0x57, 0x40, 0x48, 0x8D, 0x47, 0x60};
    code.insert(code.end(), instruction.begin(), instruction.end());
    code.insert(code.end(), {0xC5, 0xFE, 0x7F, 0x07, 0xC5, 0xF8, 0x77, 0xC3});
    const auto entry = LoweringExecution::AppendHostEntry(code);
    auto* page = LoweringExecution::AllocatePages(4096);
    if (page == nullptr) throw std::runtime_error("Cannot allocate the twin execution page");
    std::memcpy(page, code.data(), code.size());
    if (!LoweringExecution::ProtectPages(page, 4096, true, false)) {
        LoweringExecution::FreePages(page, 4096);
        throw std::runtime_error("Cannot make the twin execution page executable");
    }
    auto state = input;
    LoweringExecution::Call(page, entry, &state);
    LoweringExecution::FreePages(page, 4096);
    return state;
}

Vector lane(const Vector& source, unsigned index) {
    Vector out{};
    for (unsigned i = 0; i < 4; ++i) out[i] = source[index * 4 + i];
    return out;
}

Vector expected(const std::string& name, const Bytes& site, const Registers& in, std::uint8_t imm) {
    const bool wide = (site[2] & 4) != 0;
    Vector out{};
    if (name == "VINSERTI128") {
        out = in.Ymm1;
        for (unsigned i = 0; i < 4; ++i) out[(imm & 1) * 4 + i] = in.Ymm2[i];
    } else if (name == "VEXTRACTI128") {
        out = lane(in.Ymm1, imm & 1);
    } else if (name == "VPERM2I128") {
        for (unsigned half = 0; half < 2; ++half) {
            const unsigned control = (imm >> (half * 4)) & 0xF;
            if ((control & 8) != 0) continue;
            const auto& source = (control & 2) != 0 ? in.Ymm2 : in.Ymm1;
            for (unsigned i = 0; i < 4; ++i) out[half * 4 + i] = source[(control & 1) * 4 + i];
        }
    } else if (name == "VPBLENDD") {
        for (unsigned i = 0; i < (wide ? 8u : 4u); ++i) out[i] = ((imm >> i) & 1) != 0 ? in.Ymm2[i] : in.Ymm1[i];
    } else if (name == "VBROADCASTI128") {
        for (unsigned i = 0; i < 8; ++i) out[i] = in.Memory[i % 4];
    } else if (name == "VPBROADCASTD") {
        for (unsigned i = 0; i < (wide ? 8u : 4u); ++i) out[i] = in.Memory[0];
    } else if (name == "VPBROADCASTQ") {
        for (unsigned i = 0; i < (wide ? 8u : 4u); ++i) out[i] = in.Memory[i % 2];
    }
    return out;
}

void execution() {
    Registers input{};
    for (unsigned i = 0; i < 8; ++i) {
        input.Ymm0[i] = 0xDEAD0000u + i;
        input.Ymm1[i] = 0x11110000u + i;
        input.Ymm2[i] = 0x22220000u + i;
        input.Memory[i] = 0x33330000u + i;
    }
    for (const std::uint8_t imm : {0x00, 0x01, 0x02, 0x03, 0x12, 0x20, 0x31, 0x5B, 0x88, 0xA5, 0xFF}) {
        for (const auto& item : cases(imm)) {
            const auto output = run(item.Replacement, input);
            const auto wanted = expected(item.Name, item.Site, input, imm);
            if (output.Ymm0 != wanted)
                throw std::runtime_error(std::string(item.Name) + (((item.Site[2] & 4) != 0) ? " ymm" : " xmm") + " with immediate " + std::to_string(imm) + " does not match the AVX2 instruction's result");
        }
    }
}

#else

void execution() { std::cout << "AVX2 twin execution checks require a Linux/macOS x86-64 build\n"; }

#endif

}

int main() {
    try {
        decoding();
        matching();
        execution();
        std::cout << "AVX2 twin lowering checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
