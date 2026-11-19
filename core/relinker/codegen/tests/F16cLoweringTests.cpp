#include "VectorExecution.hpp"
#include <codegen/IAmd64OnlyConverter.hpp>
#include <codegen/x86/F16cLowering.hpp>
#include <codegen/x86/IAmd64OnlyInstructionMatcher.hpp>
#include <codegen/CodegenException.hpp>
#include <array>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using LoweringExecution::Bytes;

void require(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

Bytes ph2psReg(const bool wide256, const std::uint8_t dst, const std::uint8_t src) {
    return {0xC4, static_cast<std::uint8_t>(((dst & 8) ? 0 : 0x80) | 0x40 | ((src & 8) ? 0 : 0x20) | 2),
        static_cast<std::uint8_t>(0x78 | (wide256 ? 4 : 0) | 1), 0x13,
        static_cast<std::uint8_t>(0xC0 | ((dst & 7) << 3) | (src & 7))};
}

Bytes ph2psMemRax(const bool wide256, const std::uint8_t dst) {
    return {0xC4, static_cast<std::uint8_t>(((dst & 8) ? 0 : 0x80) | 0x60 | 2),
        static_cast<std::uint8_t>(0x78 | (wide256 ? 4 : 0) | 1), 0x13,
        static_cast<std::uint8_t>((dst & 7) << 3)};
}

Bytes ph2psRip(const bool wide256, const std::uint8_t dst) {
    return {0xC4, static_cast<std::uint8_t>(((dst & 8) ? 0 : 0x80) | 0x60 | 2),
        static_cast<std::uint8_t>(0x78 | (wide256 ? 4 : 0) | 1), 0x13,
        static_cast<std::uint8_t>(((dst & 7) << 3) | 5), 0, 0, 0, 0};
}

Bytes ph2psRspDisp8(const bool wide256, const std::uint8_t dst, const std::int8_t disp) {
    return {0xC4, static_cast<std::uint8_t>(((dst & 8) ? 0 : 0x80) | 0x60 | 2),
        static_cast<std::uint8_t>(0x78 | (wide256 ? 4 : 0) | 1), 0x13,
        static_cast<std::uint8_t>(0x44 | ((dst & 7) << 3)), 0x24, static_cast<std::uint8_t>(disp)};
}

Bytes ps2phReg(const bool wide256, const std::uint8_t dst, const std::uint8_t src, const std::uint8_t imm) {
    return {0xC4, static_cast<std::uint8_t>(((src & 8) ? 0 : 0x80) | 0x40 | ((dst & 8) ? 0 : 0x20) | 3),
        static_cast<std::uint8_t>(0x78 | (wide256 ? 4 : 0) | 1), 0x1D,
        static_cast<std::uint8_t>(0xC0 | ((src & 7) << 3) | (dst & 7)), imm};
}

Bytes ps2phMemRax(const bool wide256, const std::uint8_t src, const std::uint8_t imm) {
    return {0xC4, static_cast<std::uint8_t>(((src & 8) ? 0 : 0x80) | 0x60 | 3),
        static_cast<std::uint8_t>(0x78 | (wide256 ? 4 : 0) | 1), 0x1D,
        static_cast<std::uint8_t>((src & 7) << 3), imm};
}

Bytes ps2phRip(const bool wide256, const std::uint8_t src, const std::uint8_t imm) {
    return {0xC4, static_cast<std::uint8_t>(((src & 8) ? 0 : 0x80) | 0x60 | 3),
        static_cast<std::uint8_t>(0x78 | (wide256 ? 4 : 0) | 1), 0x1D,
        static_cast<std::uint8_t>(((src & 7) << 3) | 5), 0, 0, 0, 0, imm};
}

Bytes ps2phRspDisp8(const bool wide256, const std::uint8_t src, const std::int8_t disp, const std::uint8_t imm) {
    return {0xC4, static_cast<std::uint8_t>(((src & 8) ? 0 : 0x80) | 0x60 | 3),
        static_cast<std::uint8_t>(0x78 | (wide256 ? 4 : 0) | 1), 0x1D,
        static_cast<std::uint8_t>(0x44 | ((src & 7) << 3)), 0x24, static_cast<std::uint8_t>(disp), imm};
}

void decoding() {
    for (const bool wide : {false, true}) {
        const auto phReg = ph2psReg(wide, 11, 5);
        const auto decodedPh = Codegen::DecodeF16c(phReg.data(), phReg.size());
        require(decodedPh && decodedPh->Operation == Codegen::F16cOperation::HalfToSingle && decodedPh->Vector256 == wide && decodedPh->Destination == 11 && decodedPh->Source == 5 && !decodedPh->Memory,
            "VCVTPH2PS register form did not decode");
        const auto phMem = ph2psMemRax(wide, 9);
        const auto decodedPhMem = Codegen::DecodeF16c(phMem.data(), phMem.size());
        require(decodedPhMem && decodedPhMem->Operation == Codegen::F16cOperation::HalfToSingle && decodedPhMem->Vector256 == wide && decodedPhMem->Destination == 9 && decodedPhMem->Memory,
            "VCVTPH2PS memory form did not decode");
        const auto psReg = ps2phReg(wide, 13, 6, 0x04);
        const auto decodedPs = Codegen::DecodeF16c(psReg.data(), psReg.size());
        require(decodedPs && decodedPs->Operation == Codegen::F16cOperation::SingleToHalf && decodedPs->Vector256 == wide && decodedPs->Destination == 13 && decodedPs->Source == 6 && decodedPs->Immediate == 0x04 && !decodedPs->Memory,
            "VCVTPS2PH register form did not decode");
        const auto psMem = ps2phMemRax(wide, 14, 0x02);
        const auto decodedPsMem = Codegen::DecodeF16c(psMem.data(), psMem.size());
        require(decodedPsMem && decodedPsMem->Operation == Codegen::F16cOperation::SingleToHalf && decodedPsMem->Vector256 == wide && decodedPsMem->Source == 14 && decodedPsMem->Immediate == 0x02 && decodedPsMem->Memory,
            "VCVTPS2PH memory form did not decode");
    }
    const std::array<Bytes, 7> rejected{{
        {0xC4, 0xE2, 0xF9, 0x13, 0xC1},
        {0xC4, 0xE2, 0x71, 0x13, 0xC1},
        {0xC4, 0xE2, 0x78, 0x13, 0xC1},
        {0xC4, 0xE3, 0xF9, 0x1D, 0xC1, 0x00},
        {0xC4, 0xE3, 0x71, 0x1D, 0xC1, 0x00},
        {0xC4, 0xE3, 0x78, 0x1D, 0xC1, 0x00},
        {0x90},
    }};
    for (const auto& bytes : rejected)
        require(!Codegen::DecodeF16c(bytes.data(), bytes.size()), "an invalid or unrelated encoding was decoded as F16C");

    const std::array<Bytes, 3> malformed{{
        {0xC4, 0xE2, 0x79, 0x13, 0xC1, 0x90},
        {0xC4, 0xE3, 0x79, 0x1D, 0xC1},
        {0xC4, 0xE3, 0x79, 0x1D, 0xC1, 0x00, 0x90},
    }};
    for (const auto& bytes : malformed) {
        bool threw = false;
        try { (void)Codegen::DecodeF16c(bytes.data(), bytes.size()); }
        catch (const Codegen::CodegenException&) { threw = true; }
        require(threw, "malformed F16C instruction was accepted");
    }
}

void matching() {
    const auto avx = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Avx);
    const auto rosetta = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Rosetta);
    const auto intel = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Intel);
    const auto ph = ph2psReg(true, 0, 1);
    const auto ps = ps2phReg(false, 2, 3, 0);
    for (const auto& [bytes, name] : std::array<std::pair<Bytes, std::string>, 2>{{{ph, "VCVTPH2PS"}, {ps, "VCVTPS2PH"}}}) {
        require(!rosetta->Match(bytes.data(), bytes.size()), "Rosetta mode lowered " + name);
        require(!intel->Match(bytes.data(), bytes.size()), "Intel mode lowered " + name);
        const auto match = avx->Match(bytes.data(), bytes.size());
        require(match && match->InstructionName == name && match->Lowering == Codegen::Amd64OnlyLowering::Trampoline && match->RelocatableWindow,
            "AVX mode did not lower " + name + " through a movable trampoline");
    }
    const std::array<std::span<const std::uint8_t>, 2> sequence{ph, ps};
    const auto seq = avx->MatchSequence(sequence, {});
    require(seq && seq->InstructionName == "VCVTPH2PS" && seq->Lowering == Codegen::Amd64OnlyLowering::Trampoline, "AVX MatchSequence did not lower F16C sequence");

    const auto converter = Codegen::MakeAmd64OnlyConverter(Codegen::Amd64OnlyTarget::Avx);
    Bytes text;
    const auto phRip = ph2psRip(true, 4);
    const auto psRip = ps2phRip(false, 4, 2);
    text.insert(text.end(), phRip.begin(), phRip.end());
    text.insert(text.end(), psRip.begin(), psRip.end());
    text.push_back(0xC3);
    Bytes file(0x300 + text.size(), 0xCC);
    std::copy(text.begin(), text.end(), file.begin() + 0x200);
    const Domain::ProgramHeader codeSegment{1, 5, 0x200, 0x1000, 0, text.size(), text.size(), 16};
    const auto converted = converter->Convert(file, {codeSegment});
    require(converted.Trampolines.size() == 2 && converted.KeptCount == 0 && converted.Reports.size() == 2, "AVX converter did not lower RIP-relative F16C instructions");
    require(converted.Reports[0].InstructionName == "VCVTPH2PS" && converted.Reports[1].InstructionName == "VCVTPS2PH", "AVX converter reported unexpected F16C names");
    require(converted.Trampolines[0].Relocations.size() == 1 && converted.Trampolines[1].Relocations.size() == 1, "AVX converter did not record RIP-relative F16C stub relocations");
}

#if LOWERING_EXECUTION_AVAILABLE

using VectorExecution::Machine;
using VectorExecution::Runner;
using Vector = std::array<std::uint64_t, 4>;

Bytes lowered(const Bytes& instruction) {
    Codegen::StubBodyBuilder body;
    Codegen::EmitF16c(body, *Codegen::DecodeF16c(instruction.data(), instruction.size()));
    const auto finished = body.Finish();
    return Bytes(finished.Bytes.begin(), finished.Bytes.begin() + static_cast<std::ptrdiff_t>(finished.ReturnBranchOffset));
}

std::uint32_t sdmHalfToSingle(const std::uint16_t h) {
    const std::uint32_t sign = static_cast<std::uint32_t>(h & 0x8000u) << 16;
    const std::uint32_t exp = (h >> 10) & 0x1Fu;
    const std::uint32_t mant = h & 0x3FFu;
    if (exp == 0) {
        if (mant == 0) return sign;
        unsigned k = 0;
        for (unsigned bit = 0; bit < 10; ++bit)
            if ((mant >> bit) & 1u) k = bit;
        const std::uint32_t normMant = (mant << (23 - k)) & 0x7FFFFFu;
        return sign | ((103u + k) << 23) | normMant;
    }
    if (exp == 31) {
        if (mant == 0) return sign | 0x7F800000u;
        return sign | 0x7FC00000u | (mant << 13);
    }
    return sign | ((exp + 112u) << 23) | (mant << 13);
}

std::uint16_t sdmSingleToHalf(const std::uint32_t u, const unsigned rc) {
    const std::uint32_t sign = u >> 31;
    const std::uint16_t hSign = static_cast<std::uint16_t>(sign << 15);
    const std::uint32_t absU = u & 0x7FFFFFFFu;
    if (absU == 0) return hSign;
    if (absU > 0x7F800000u) return static_cast<std::uint16_t>(hSign | 0x7E00u | ((u & 0x7FFFFFu) >> 13));
    if (absU == 0x7F800000u) return static_cast<std::uint16_t>(hSign | 0x7C00u);
    const std::uint32_t exp = absU >> 23;
    const std::uint32_t mant = absU & 0x7FFFFFu;
    const auto roundUp = [&](const std::uint32_t q, const bool r, const bool s) {
        if (rc == 0) return r && (s || (q & 1u) != 0);
        if (rc == 1) return sign != 0 && (r || s);
        if (rc == 2) return sign == 0 && (r || s);
        return false;
    };
    if (exp > 142u) {
        const bool toInf = rc == 0 || (rc == 1 && sign != 0) || (rc == 2 && sign == 0);
        return static_cast<std::uint16_t>(hSign | (toInf ? 0x7C00u : 0x7BFFu));
    }
    if (exp >= 113u) {
        const std::uint32_t q = ((exp - 112u) << 10) | (mant >> 13);
        const bool r = ((mant >> 12) & 1u) != 0;
        const bool s = (mant & 0xFFFu) != 0;
        return static_cast<std::uint16_t>(hSign | (q + (roundUp(q, r, s) ? 1u : 0u)));
    }
    if (exp >= 102u) {
        const std::uint32_t sig = 0x00800000u | mant;
        const unsigned shift = 126u - exp;
        const std::uint32_t q = sig >> shift;
        const bool r = ((sig >> (shift - 1u)) & 1u) != 0;
        const bool s = (sig & ((1u << (shift - 1u)) - 1u)) != 0;
        return static_cast<std::uint16_t>(hSign | (q + (roundUp(q, r, s) ? 1u : 0u)));
    }
    return static_cast<std::uint16_t>(hSign | (roundUp(0, false, true) ? 1u : 0u));
}

Vector vectorFromFloats(const std::array<std::uint32_t, 8>& lanes) {
    Vector out{};
    std::memcpy(out.data(), lanes.data(), sizeof(out));
    return out;
}

Vector vectorFromHalves(const std::array<std::uint16_t, 8>& lanes, const bool wide256) {
    std::array<std::uint16_t, 16> raw{};
    const std::size_t count = wide256 ? 8u : 4u;
    for (std::size_t i = 0; i < count; ++i) raw[i] = lanes[i];
    Vector out{};
    std::memcpy(out.data(), raw.data(), sizeof(out));
    return out;
}

void verifyMachine(const Machine& start, const Machine& end, const std::optional<std::uint8_t> dstReg, const Vector& wantedReg, const std::optional<Vector>& wantedMem, const std::string& describe) {
    for (std::uint8_t reg = 0; reg < 16; ++reg) {
        const auto expected = (dstReg && reg == *dstReg) ? wantedReg : start.Ymm[reg];
        if (end.Ymm[reg] != expected) throw std::runtime_error(describe + ": ymm" + std::to_string(reg) + " mismatch");
    }
    const auto expectedMem = wantedMem ? *wantedMem : start.Memory;
    if (end.Memory != expectedMem) throw std::runtime_error(describe + ": memory mismatch");
    for (const auto slot : end.RedZone) require(slot == LoweringExecution::kSentinel, describe + " overwrote the red zone");
}

Bytes wrapWithMxcsr(const Bytes& body, const std::uint32_t clearMask, const std::uint32_t setMask) {
    Bytes wrapped;
    const auto push32 = [&](const std::uint32_t value) {
        for (unsigned shift = 0; shift < 32; shift += 8) wrapped.push_back(static_cast<std::uint8_t>(value >> shift));
    };
    wrapped.insert(wrapped.end(), {0x48, 0x8D, 0xA4, 0x24});
    push32(static_cast<std::uint32_t>(-144));
    wrapped.insert(wrapped.end(), {0x0F, 0xAE, 0x1C, 0x24});
    wrapped.insert(wrapped.end(), {0x8B, 0x0C, 0x24});
    wrapped.insert(wrapped.end(), {0x81, 0xE1});
    push32(~clearMask);
    wrapped.insert(wrapped.end(), {0x81, 0xC9});
    push32(setMask);
    wrapped.insert(wrapped.end(), {0x89, 0x4C, 0x24, 0x04});
    wrapped.insert(wrapped.end(), {0x0F, 0xAE, 0x54, 0x24, 0x04});
    wrapped.insert(wrapped.end(), body.begin(), body.end());
    wrapped.insert(wrapped.end(), {0x0F, 0xAE, 0x14, 0x24});
    wrapped.insert(wrapped.end(), {0x48, 0x8D, 0xA4, 0x24});
    push32(144u);
    return wrapped;
}

Bytes wrapWithMxcsrRc(const Bytes& body, const unsigned rc) {
    return wrapWithMxcsr(body, 0x6000u, (rc & 3u) << 13);
}

void executionPh2Ps(const bool hostF16c) {
    const auto stub256 = lowered(ph2psReg(true, 3, 12));
    const Runner runner256(stub256);
    std::unique_ptr<Runner> hw256;
    if (hostF16c) hw256 = std::make_unique<Runner>(ph2psReg(true, 3, 12));

    for (std::uint32_t base = 0; base < 65536u; base += 8u) {
        auto machine = VectorExecution::Initial();
        std::array<std::uint16_t, 8> input{};
        std::array<std::uint32_t, 8> expected{};
        for (std::size_t i = 0; i < 8; ++i) {
            input[i] = static_cast<std::uint16_t>(base + i);
            expected[i] = sdmHalfToSingle(input[i]);
        }
        machine.Ymm[12] = vectorFromHalves(input, true);
        const auto start = machine;
        runner256.Run(machine);
        verifyMachine(start, machine, 3, vectorFromFloats(expected), std::nullopt, "VCVTPH2PS exhaustive base=" + std::to_string(base));
        if (hw256) {
            auto hwMachine = start;
            hw256->Run(hwMachine);
            require(hwMachine.Ymm[3] == machine.Ymm[3], "VCVTPH2PS differs from hardware at base=" + std::to_string(base));
        }
    }

    const std::array<std::array<std::uint8_t, 2>, 5> pairs{{{0, 1}, {5, 5}, {15, 14}, {14, 15}, {8, 0}}};
    const std::array<std::uint16_t, 8> sample{0x0000, 0x8000, 0x0001, 0x03FF, 0x3C00, 0x7BFF, 0x7C00, 0xFE05};
    std::array<std::uint32_t, 8> expectedFull{};
    std::array<std::uint32_t, 8> expectedLow{};
    for (std::size_t i = 0; i < 8; ++i) {
        expectedFull[i] = sdmHalfToSingle(sample[i]);
        expectedLow[i] = i < 4 ? expectedFull[i] : 0u;
    }
    for (const bool wide : {false, true}) {
        for (const auto pair : pairs) {
            auto machine = VectorExecution::Initial();
            machine.Ymm[pair[1]] = vectorFromHalves(sample, true);
            const auto start = machine;
            Runner(lowered(ph2psReg(wide, pair[0], pair[1]))).Run(machine);
            verifyMachine(start, machine, pair[0], vectorFromFloats(wide ? expectedFull : expectedLow), std::nullopt, "VCVTPH2PS reg pair");
        }
        for (const std::uint8_t dst : {std::uint8_t{0}, std::uint8_t{7}, std::uint8_t{15}}) {
            auto machine = VectorExecution::Initial();
            machine.Memory = vectorFromHalves(sample, true);
            const auto start = machine;
            Runner(lowered(ph2psMemRax(wide, dst))).Run(machine);
            verifyMachine(start, machine, dst, vectorFromFloats(wide ? expectedFull : expectedLow), std::nullopt, "VCVTPH2PS mem");
        }
    }
}

std::vector<std::uint32_t> singleToHalfInputs() {
    std::vector<std::uint32_t> values{
        0x00000000u, 0x80000000u, 0x7F800000u, 0xFF800000u,
        0x7F800001u, 0xFF800001u, 0x7F95A5A5u, 0xFF95A5A5u,
        0x7FC00000u, 0xFFC00000u, 0x7FD55555u, 0xFFD55555u,
        0x7FFFFFFFu, 0xFFFFFFFFu,
        0x00000001u, 0x80000001u, 0x007FFFFFu, 0x807FFFFFu,
        0x00800000u, 0x80800000u, 0x32FFFFFFu, 0xB2FFFFFFu,
        0x477FDFFFu, 0x477FE000u, 0x477FE001u, 0x477FEFFFu,
        0x477FF000u, 0x477FF001u, 0x477FFFFFu, 0x47800000u,
        0x47800001u, 0x7F7FFFFFu,
        0xC77FDFFFu, 0xC77FE000u, 0xC77FE001u, 0xC77FEFFFu,
        0xC77FF000u, 0xC77FF001u, 0xC77FFFFFu, 0xC7800000u,
        0xC7800001u, 0xFF7FFFFFu,
        0x3F800000u, 0xBF800000u, 0x3F800FFFu, 0x3F801000u,
        0x3F801001u, 0x3F802FFFu, 0x3F803000u, 0x3F803001u,
        0xBF800FFFu, 0xBF801000u, 0xBF801001u, 0xBF803000u,
        0x387FC000u, 0x387FDFFFu, 0x387FE000u, 0x387FE001u,
        0x387FFFFFu, 0x38800000u, 0x38800FFFu, 0x38801000u,
        0x38801001u, 0xB87FE000u, 0xB87FFFFFu, 0xB8801000u,
    };
    for (std::uint32_t exp = 102u; exp <= 113u; ++exp) {
        const unsigned shift = exp >= 113u ? 13u : 126u - exp;
        const std::uint32_t half = 1u << (shift - 1u);
        for (const std::uint32_t sign : {0u, 0x80000000u}) {
            for (const std::uint32_t baseMant : {0u, shift < 23u ? (1u << shift) : 0u, (0x7FFFFFu & ~((1u << shift) - 1u))}) {
                for (const std::int32_t delta : {-1, 0, 1}) {
                    const auto mant = static_cast<std::uint32_t>(static_cast<std::int64_t>(baseMant) + static_cast<std::int64_t>(half) + delta);
                    if (mant <= 0x7FFFFFu) values.push_back(sign | (exp << 23) | mant);
                }
            }
            values.push_back(sign | (exp << 23));
            values.push_back(sign | (exp << 23) | 1u);
            values.push_back(sign | (exp << 23) | 0x7FFFFFu);
        }
    }
    while (values.size() % 8 != 0) values.push_back(0x3F800000u);
    return values;
}

void executionPs2Ph(const bool hostF16c) {
    const auto inputs = singleToHalfInputs();
    for (const std::uint8_t imm : {std::uint8_t{0}, std::uint8_t{1}, std::uint8_t{2}, std::uint8_t{3}, std::uint8_t{0xF3}}) {
        const unsigned rc = imm & 3u;
        const auto instr256 = ps2phReg(true, 4, 11, imm);
        const Runner runner256(lowered(instr256));
        std::unique_ptr<Runner> hw256;
        if (hostF16c) hw256 = std::make_unique<Runner>(instr256);

        for (std::size_t offset = 0; offset < inputs.size(); offset += 8) {
            std::array<std::uint32_t, 8> chunk{};
            std::array<std::uint16_t, 8> expected{};
            for (std::size_t i = 0; i < 8; ++i) {
                chunk[i] = inputs[offset + i];
                expected[i] = sdmSingleToHalf(chunk[i], rc);
            }
            auto machine = VectorExecution::Initial();
            machine.Ymm[11] = vectorFromFloats(chunk);
            const auto start = machine;
            runner256.Run(machine);
            verifyMachine(start, machine, 4, vectorFromHalves(expected, true), std::nullopt, "VCVTPS2PH imm=" + std::to_string(imm) + " offset=" + std::to_string(offset));
            if (hw256) {
                auto hwMachine = start;
                hw256->Run(hwMachine);
                require(hwMachine.Ymm[4] == machine.Ymm[4], "VCVTPS2PH differs from hardware at imm=" + std::to_string(imm) + " offset=" + std::to_string(offset));
            }
        }
    }

    for (unsigned rc = 0; rc < 4; ++rc) {
        for (const std::uint8_t imm : {std::uint8_t{4}, std::uint8_t{0xF4}}) {
            const auto instr = ps2phReg(true, 6, 9, imm);
            const Runner runner(wrapWithMxcsrRc(lowered(instr), rc));
            std::unique_ptr<Runner> hw;
            if (hostF16c) hw = std::make_unique<Runner>(wrapWithMxcsrRc(instr, rc));

            for (std::size_t offset = 0; offset < inputs.size(); offset += 8) {
                std::array<std::uint32_t, 8> chunk{};
                std::array<std::uint16_t, 8> expected{};
                for (std::size_t i = 0; i < 8; ++i) {
                    chunk[i] = inputs[offset + i];
                    expected[i] = sdmSingleToHalf(chunk[i], rc);
                }
                auto machine = VectorExecution::Initial();
                machine.Ymm[9] = vectorFromFloats(chunk);
                const auto start = machine;
                runner.Run(machine);
                verifyMachine(start, machine, 6, vectorFromHalves(expected, true), std::nullopt, "VCVTPS2PH MXCSR rc=" + std::to_string(rc));
                if (hw) {
                    auto hwMachine = start;
                    hw->Run(hwMachine);
                    require(hwMachine.Ymm[6] == machine.Ymm[6], "VCVTPS2PH MXCSR differs from hardware at rc=" + std::to_string(rc));
                }
            }
        }
    }

    const std::array<std::uint32_t, 8> sample{0x3F800000u, 0xBF800000u, 0x477FF000u, 0xC77FF000u, 0x387FE000u, 0x7F800001u, 0x00000001u, 0x80000000u};
    std::array<std::uint16_t, 8> expected{};
    for (std::size_t i = 0; i < 8; ++i) expected[i] = sdmSingleToHalf(sample[i], 0);
    const std::array<std::array<std::uint8_t, 2>, 5> pairs{{{0, 1}, {5, 5}, {15, 14}, {14, 15}, {8, 0}}};
    for (const bool wide : {false, true}) {
        for (const auto pair : pairs) {
            auto machine = VectorExecution::Initial();
            machine.Ymm[pair[1]] = vectorFromFloats(sample);
            const auto start = machine;
            Runner(lowered(ps2phReg(wide, pair[0], pair[1], 0))).Run(machine);
            verifyMachine(start, machine, pair[0], vectorFromHalves(expected, wide), std::nullopt, "VCVTPS2PH reg pair");
        }
        for (const std::uint8_t src : {std::uint8_t{0}, std::uint8_t{7}, std::uint8_t{15}}) {
            auto machine = VectorExecution::Initial();
            machine.Ymm[src] = vectorFromFloats(sample);
            const auto start = machine;
            Vector expectedMem = start.Memory;
            std::memcpy(expectedMem.data(), expected.data(), wide ? 16u : 8u);
            Runner(lowered(ps2phMemRax(wide, src, 0))).Run(machine);
            verifyMachine(start, machine, std::nullopt, {}, expectedMem, "VCVTPS2PH mem");
        }
    }
}

void executionDazFtz(const bool hostF16c) {
    const std::array<std::uint16_t, 8> subHalf{0x0001, 0x03FF, 0x8001, 0x83FF, 0x0000, 0x8000, 0x3C00, 0x7BFF};
    std::array<std::uint32_t, 8> expectedPh{};
    for (std::size_t i = 0; i < 8; ++i) expectedPh[i] = sdmHalfToSingle(subHalf[i]);
    const auto phInstr = ph2psReg(true, 2, 10);
    auto machine = VectorExecution::Initial();
    machine.Ymm[10] = vectorFromHalves(subHalf, true);
    const auto phStart = machine;
    Runner(wrapWithMxcsr(lowered(phInstr), 0u, 0x8040u)).Run(machine);
    verifyMachine(phStart, machine, 2, vectorFromFloats(expectedPh), std::nullopt, "VCVTPH2PS with DAZ+FTZ");
    if (hostF16c) {
        auto hwMachine = phStart;
        Runner(wrapWithMxcsr(phInstr, 0u, 0x8040u)).Run(hwMachine);
        require(hwMachine.Ymm[2] == machine.Ymm[2], "VCVTPH2PS with DAZ+FTZ differs from hardware");
    }

    const std::array<std::uint32_t, 8> dazInput{0x00000001u, 0x80000001u, 0x007FFFFFu, 0x807FFFFFu, 0x00800000u, 0x80800000u, 0x33800001u, 0xB3800001u};
    for (const std::uint32_t flags : {0x0040u, 0x8000u, 0x8040u}) {
        const bool daz = (flags & 0x0040u) != 0;
        for (const std::uint8_t imm : {std::uint8_t{0}, std::uint8_t{1}, std::uint8_t{2}, std::uint8_t{3}}) {
            const auto psInstr = ps2phReg(true, 5, 13, imm);
            std::array<std::uint16_t, 8> expected{};
            for (std::size_t i = 0; i < 8; ++i) {
                std::uint32_t u = dazInput[i];
                if (daz && ((u & 0x7F800000u) == 0u)) u &= 0x80000000u;
                expected[i] = sdmSingleToHalf(u, imm & 3u);
            }
            auto psMachine = VectorExecution::Initial();
            psMachine.Ymm[13] = vectorFromFloats(dazInput);
            const auto psStart = psMachine;
            Runner(wrapWithMxcsr(lowered(psInstr), 0x8040u, flags)).Run(psMachine);
            verifyMachine(psStart, psMachine, 5, vectorFromHalves(expected, true), std::nullopt, "VCVTPS2PH DAZ/FTZ");
            if (hostF16c) {
                auto hwMachine = psStart;
                Runner(wrapWithMxcsr(psInstr, 0x8040u, flags)).Run(hwMachine);
                require(hwMachine.Ymm[5] == psMachine.Ymm[5], "VCVTPS2PH DAZ/FTZ differs from hardware");
            }
        }
    }
}

void gprFlagsRipAndStack() {
    const auto runCheck = [](const Bytes& instruction, const std::uint64_t memoryValue, const bool ripRelative, const std::size_t trailing, const std::string& describe) {
        auto state = LoweringExecution::InitialState();
        const LoweringExecution::Execution exec(instruction, memoryValue, ripRelative, [](Codegen::StubBodyBuilder& body, const Bytes& relocated) {
            Codegen::EmitF16c(body, *Codegen::DecodeF16c(relocated.data(), relocated.size()));
        }, trailing);
        exec.Run(state);
        for (std::size_t reg = 0; reg < 16; ++reg)
            require(state.Output[reg] == state.Input[reg], describe + ": GPR " + std::to_string(reg) + " changed");
        require((state.Flags & LoweringExecution::kFlagMask) == (state.InputFlags & LoweringExecution::kFlagMask), describe + ": RFLAGS changed");
        for (const auto slot : state.RedZone)
            require(slot == LoweringExecution::kSentinel, describe + ": red zone corrupted");
    };

    const Bytes phDeepRsp{0xC4, 0xE2, 0x7D, 0x13, 0x9C, 0x24, 0x38, 0xFF, 0xFF, 0xFF};
    const Bytes psDeepRsp{0xC4, 0xE3, 0x7D, 0x1D, 0x9C, 0x24, 0x38, 0xFF, 0xFF, 0xFF, 0x00};

    runCheck(ph2psRip(false, 2), 0x3C00400042004400ull, true, 0, "VCVTPH2PS 128 RIP");
    runCheck(ph2psRip(true, 15), 0x3C00400042004400ull, true, 0, "VCVTPH2PS 256 RIP");
    runCheck(ph2psRspDisp8(false, 3, -64), 0, false, 0, "VCVTPH2PS 128 RSP red zone");
    runCheck(ph2psRspDisp8(true, 15, -32), 0, false, 0, "VCVTPH2PS 256 RSP red zone");
    runCheck(phDeepRsp, 0, false, 0, "VCVTPH2PS 256 RSP -200");
    runCheck(ps2phRip(false, 2, 0), 0, true, 1, "VCVTPS2PH 128 RIP");
    runCheck(ps2phRip(true, 15, 4), 0, true, 1, "VCVTPS2PH 256 RIP");
    runCheck(ps2phRspDisp8(false, 3, 16, 1), 0, false, 0, "VCVTPS2PH 128 RSP");
    runCheck(ps2phRspDisp8(true, 15, 32, 2), 0, false, 0, "VCVTPS2PH 256 RSP");
    runCheck(psDeepRsp, 0, false, 0, "VCVTPS2PH 256 RSP -200");

    const Bytes phSibR8R9{0xC4, 0x82, 0x7D, 0x13, 0x44, 0x48, 0x08};
    const Bytes psSibR8R9{0xC4, 0x83, 0x7D, 0x1D, 0x44, 0x48, 0x08, 0x00};
    const auto withSibSetup = [](const Bytes& stub) {
        Bytes body{0x49, 0x89, 0xC0, 0x49, 0xC7, 0xC1, 0x04, 0x00, 0x00, 0x00, 0x49, 0x83, 0xE8, 0x10};
        body.insert(body.end(), stub.begin(), stub.end());
        return body;
    };
    const std::array<std::uint16_t, 8> halfSample{0x3C00, 0xBC00, 0x4000, 0xC000, 0x0001, 0x03FF, 0x7BFF, 0x7C00};
    std::array<std::uint32_t, 8> floatExpected{};
    for (std::size_t i = 0; i < 8; ++i) floatExpected[i] = sdmHalfToSingle(halfSample[i]);
    auto phSibMachine = VectorExecution::Initial();
    phSibMachine.Memory = vectorFromHalves(halfSample, true);
    const auto phSibStart = phSibMachine;
    Runner(withSibSetup(lowered(phSibR8R9))).Run(phSibMachine);
    verifyMachine(phSibStart, phSibMachine, 0, vectorFromFloats(floatExpected), std::nullopt, "VCVTPH2PS SIB [r8+r9*2+8]");

    auto psSibMachine = VectorExecution::Initial();
    psSibMachine.Ymm[0] = vectorFromFloats(floatExpected);
    const auto psSibStart = psSibMachine;
    Vector expectedSibMem = psSibStart.Memory;
    std::memcpy(expectedSibMem.data(), halfSample.data(), 16u);
    Runner(withSibSetup(lowered(psSibR8R9))).Run(psSibMachine);
    verifyMachine(psSibStart, psSibMachine, std::nullopt, {}, expectedSibMem, "VCVTPS2PH SIB [r8+r9*2+8]");
}

void pageEndMemory() {
    const auto pageSize = LoweringExecution::PageSize();
    auto* area = LoweringExecution::AllocatePages(pageSize * 2);
    require(area != nullptr, "cannot allocate guarded operand pages");
    require(LoweringExecution::ProtectPages(area + pageSize, pageSize, false, false), "cannot guard the second page");

    const std::array<std::uint16_t, 8> halfSample{0x3C00, 0xBC00, 0x0001, 0x03FF, 0x7BFF, 0xFBFF, 0x7C00, 0xFC00};
    std::array<std::uint32_t, 8> floatFromHalf{};
    std::array<std::uint32_t, 8> floatFromHalfLow{};
    for (std::size_t i = 0; i < 8; ++i) {
        floatFromHalf[i] = sdmHalfToSingle(halfSample[i]);
        floatFromHalfLow[i] = i < 4 ? floatFromHalf[i] : 0u;
    }

    const auto withRax = [](void* address, const Bytes& stub) {
        Bytes body{0x48, 0xB8};
        const auto value = reinterpret_cast<std::uintptr_t>(address);
        for (unsigned shift = 0; shift < 64; shift += 8) body.push_back(static_cast<std::uint8_t>(value >> shift));
        body.insert(body.end(), stub.begin(), stub.end());
        return body;
    };

    for (const bool wide : {false, true}) {
        const std::size_t width = wide ? 16u : 8u;
        auto* operand = area + pageSize - width;
        std::memcpy(operand, halfSample.data(), width);
        auto machine = VectorExecution::Initial();
        const auto start = machine;
        Runner(withRax(operand, lowered(ph2psMemRax(wide, 5)))).Run(machine);
        verifyMachine(start, machine, 5, vectorFromFloats(wide ? floatFromHalf : floatFromHalfLow), std::nullopt,
            wide ? "VCVTPH2PS 256 at page end" : "VCVTPH2PS 128 at page end");
    }

    const std::array<std::uint32_t, 8> floatSample{0x3F800000u, 0xBF800000u, 0x33800000u, 0x387FC000u, 0x477FE000u, 0xC77FE000u, 0x7F800000u, 0xFF800000u};
    std::array<std::uint16_t, 8> expectedHalf{};
    for (std::size_t i = 0; i < 8; ++i) expectedHalf[i] = sdmSingleToHalf(floatSample[i], 0);
    for (const bool wide : {false, true}) {
        const std::size_t width = wide ? 16u : 8u;
        auto* operand = area + pageSize - width;
        std::memset(operand, 0xAA, width);
        auto machine = VectorExecution::Initial();
        machine.Ymm[6] = vectorFromFloats(floatSample);
        const auto start = machine;
        Runner(withRax(operand, lowered(ps2phMemRax(wide, 6, 0)))).Run(machine);
        verifyMachine(start, machine, std::nullopt, {}, std::nullopt, wide ? "VCVTPS2PH 256 at page end" : "VCVTPS2PH 128 at page end");
        require(std::memcmp(operand, expectedHalf.data(), width) == 0, wide ? "VCVTPS2PH 256 stored wrong bytes at page end" : "VCVTPS2PH 128 stored wrong bytes at page end");
    }

    LoweringExecution::FreePages(area, pageSize * 2);
}

void execution() {
    const bool hostAvx = LoweringExecution::HostExecutes({0xC5, 0xFC, 0x57, 0xC0});
    if (!hostAvx) {
        std::cout << "f16c execution checks skipped: host does not execute AVX\n";
        return;
    }
    const bool hostF16c = LoweringExecution::HostExecutes(ph2psReg(false, 0, 0)) && LoweringExecution::HostExecutes(ps2phReg(false, 0, 0, 0));
    executionPh2Ps(hostF16c);
    executionPs2Ph(hostF16c);
    executionDazFtz(hostF16c);
    gprFlagsRipAndStack();
    pageEndMemory();
    std::cout << (hostF16c ? "f16c execution verified against SDM and hardware\n" : "f16c execution verified against SDM\n");
}

#else

void execution() { std::cout << "f16c execution checks require an x86-64 Linux, macOS or Windows build\n"; }

#endif

}

int main() {
    try {
        decoding();
        matching();
        execution();
        std::cout << "f16c lowering checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
