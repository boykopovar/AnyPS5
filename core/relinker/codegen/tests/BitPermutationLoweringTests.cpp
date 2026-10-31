#include <codegen/x86/BitPermutationLowering.hpp>
#include <codegen/x86/IAmd64OnlyInstructionMatcher.hpp>
#include <codegen/IAmd64OnlyConverter.hpp>
#include <codegen/CodegenException.hpp>
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <tuple>
#include <vector>
#if defined(_WIN32)
#include <windows.h>
#elif (defined(__linux__) || defined(__APPLE__)) && defined(__x86_64__)
#include <sys/mman.h>
#endif

namespace {

using Bytes = std::vector<std::uint8_t>;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

Bytes instruction(bool deposit, bool wide, std::uint8_t destination, std::uint8_t source, std::uint8_t mask) {
    return {0xC4, static_cast<std::uint8_t>(0xE2 ^ ((destination & 8) << 4) ^ ((mask & 8) << 2)),
        static_cast<std::uint8_t>((wide ? 0x80 : 0) | ((~source & 15) << 3) | (deposit ? 3 : 2)), 0xF5,
        static_cast<std::uint8_t>(0xC0 | ((destination & 7) << 3) | (mask & 7))};
}

void decoding() {
    for (const bool deposit : {false, true}) for (const bool wide : {false, true})
        for (std::uint8_t destination = 0; destination < 16; ++destination)
            for (std::uint8_t source = 0; source < 16; ++source)
                for (std::uint8_t mask = 0; mask < 16; ++mask) {
                    const auto bytes = instruction(deposit, wide, destination, source, mask);
                    const auto operands = Codegen::DecodeBitPermutation(bytes.data(), bytes.size());
                    require(operands && operands->Deposit == deposit && operands->Wide == wide && operands->Destination == destination && operands->Source == source && operands->Mask == mask && !operands->Memory, "Register decoding changed an operand");
                }
    for (const Bytes bytes : {Bytes{0x90}, Bytes{0xC4, 0xE2, 0xF2}, Bytes{0xC4, 0xE1, 0xF2, 0xF5, 0xC0}, Bytes{0xC4, 0xE2, 0xF0, 0xF5, 0xC0}})
        require(!Codegen::DecodeBitPermutation(bytes.data(), bytes.size()), "Unrelated or truncated opcode matched PEXT/PDEP");
    for (const Bytes bytes : {Bytes{0xC4, 0xE2, 0xF6, 0xF5, 0xC0}, Bytes{0xC4, 0xE2, 0xF2, 0xF5, 0x84}}) {
        bool rejected = false;
        try { (void)Codegen::DecodeBitPermutation(bytes.data(), bytes.size()); }
        catch (const Codegen::CodegenException&) { rejected = true; }
        require(rejected, "Malformed PEXT/PDEP was accepted");
    }
    for (const auto segment : {std::uint8_t{0x64}, std::uint8_t{0x65}}) {
        const Bytes site{segment, 0x67, 0xC4, 0x02, 0xAA, 0xF5, 0x8C, 0xAC, 0x10, 0, 0, 0};
        const auto operands = Codegen::DecodeBitPermutation(site.data(), site.size());
        require(operands && operands->Destination == 9 && operands->Source == 10 && operands->Memory && operands->Memory->RexIndexBase == 3 && operands->Memory->Prefixes == Bytes{segment, 0x67}, "Memory extensions or segment/address prefixes were lost");
        bool rejected = false;
        try {
            Codegen::StubBodyBuilder builder;
            Codegen::EmitBitPermutation(builder, *operands);
        } catch (const Codegen::CodegenException&) { rejected = true; }
        require(rejected, "A segmented or address-size-overridden mask was accepted");
    }
    for (const bool deposit : {false, true}) for (const bool wide : {false, true}) {
        for (const auto [displacement, rejects32, rejects64] : {std::tuple<std::int32_t, bool, bool>{-215, false, true}, {-211, true, true}, {-208, true, true}, {-137, true, true}, {-136, false, false}}) {
            Bytes site{0xC4, 0xE2, static_cast<std::uint8_t>((wide ? 0xF0 : 0x70) | (deposit ? 3 : 2)), 0xF5, 0x84, 0x24};
            for (unsigned shift = 0; shift < 32; shift += 8) site.push_back(static_cast<std::uint8_t>(static_cast<std::uint32_t>(displacement) >> shift));
            bool rejected = false;
            try {
                Codegen::StubBodyBuilder builder;
                Codegen::EmitBitPermutation(builder, *Codegen::DecodeBitPermutation(site.data(), site.size()));
            } catch (const Codegen::CodegenException&) { rejected = true; }
            const auto operands = *Codegen::DecodeBitPermutation(site.data(), site.size());
            require(Codegen::CanLowerBitPermutation(operands) != rejected, "Matcher eligibility disagrees with spill bounds");
            require(rejected == (wide ? rejects64 : rejects32), "Spill overlap detection has an incorrect boundary");
        }
        const Bytes overflow{0xC4, 0xE2, static_cast<std::uint8_t>((wide ? 0xF0 : 0x70) | (deposit ? 3 : 2)), 0xF5, 0x84, 0x24, 0xFF, 0xFF, 0xFF, 0x7F};
        bool rejected = false;
        try {
            Codegen::StubBodyBuilder builder;
            Codegen::EmitBitPermutation(builder, *Codegen::DecodeBitPermutation(overflow.data(), overflow.size()));
        } catch (const Codegen::CodegenException&) { rejected = true; }
        require(rejected, "An overflowing stack displacement was silently wrapped");
    }
}

void rejectedMemory() {
    for (const bool deposit : {false, true}) for (const bool wide : {false, true})
        for (std::uint8_t base = 0; base < 16; ++base) {
            if (base == 4) continue;
            auto site = instruction(deposit, wide, 0, 1, base);
            site[4] = static_cast<std::uint8_t>(0x80 | (base & 7));
            if ((base & 7) == 4) site.push_back(0x24);
            site.insert(site.end(), {0, 0, 0, 0});
            bool rejected = false;
            try {
                Codegen::StubBodyBuilder builder;
                Codegen::EmitBitPermutation(builder, *Codegen::DecodeBitPermutation(site.data(), site.size()));
            } catch (const Codegen::CodegenException&) { rejected = true; }
            require(rejected, "A dynamic memory mask could alias the spill frame");
            const auto matcher = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Rosetta, true);
            require(!matcher->Match(site.data(), site.size()), "Unsupported memory mask was not left native");
        }
}

void matching() {
    const auto intel = Codegen::MakeAmd64OnlyInstructionMatcher();
    const auto rosetta = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Rosetta, true);
    for (const bool deposit : {false, true}) {
        const auto site = instruction(deposit, true, 0, 1, 6);
        const auto name = deposit ? "PDEP" : "PEXT";
        require(!Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Rosetta)->Match(site.data(), site.size()), "Default Rosetta mode rewrote PEXT/PDEP");
        require(!intel->Match(site.data(), site.size()), "Intel mode rewrote PEXT/PDEP");
        const auto match = rosetta->Match(site.data(), site.size());
        require(match && match->InstructionName == name && match->Lowering == Codegen::Amd64OnlyLowering::Trampoline && !match->Optional, "Rosetta mode did not require a PEXT/PDEP trampoline");
        const std::array<std::span<const std::uint8_t>, 2> sequence{site, site};
        require(rosetta->MatchSequence(sequence, {}).has_value(), "A PEXT/PDEP sequence could not move out of line");
        Bytes file(0x300, 0xCC);
        std::copy(site.begin(), site.end(), file.begin() + 0x200);
        const Domain::ProgramHeader segment{1, 5, 0x200, 0x1000, 0, site.size(), site.size(), 16};
        const auto converted = Codegen::MakeAmd64OnlyConverter(Codegen::Amd64OnlyTarget::Rosetta, {}, true)->Convert(file, {segment});
        require(converted.Trampolines.size() == 1 && converted.Reports.size() == 1 && converted.Reports[0].InstructionName == name, "Converter did not report a PEXT/PDEP trampoline");
        require(converted.Trampolines[0].Offset == 0x200 && converted.Trampolines[0].Length == site.size() && converted.Bytes == file, "Converter changed the original PEXT/PDEP site");
    }
}

#if (defined(_WIN32) || defined(__linux__) || defined(__APPLE__)) && defined(__x86_64__)

struct State {
    std::array<std::uint64_t, 16> Input{};
    std::array<std::uint64_t, 16> Output{};
    std::uint64_t Flags = 0;
    std::array<std::uint64_t, 16> RedZone{};
    std::uint64_t InputFlags = 0x202;
};

class Execution {
public:
    Execution(const Bytes& site, std::uint64_t memoryValue) {
        auto operands = Codegen::DecodeBitPermutation(site.data(), site.size());
        require(operands.has_value(), "Execution site was not bit permutation");
        for (auto reg : {3, 5, 6, 7, 12, 13, 14, 15}) push(static_cast<std::uint8_t>(reg));
#if defined(_WIN32)
        bytes.insert(bytes.end(), {0x48, 0x89, 0xCF});
#endif
        memory(0x8D, 4, 4, -128);
        rip({0x48, 0x89, 0x3D}, 0);
        rip({0x48, 0x89, 0x25}, 8);
        memory(0x89, 4, 7, 32);
        memory(0x8B, 0, 7, 49 * 8);
        push(0);
        bytes.push_back(0x9D);
        bytes.insert(bytes.end(), {0x48, 0xB8});
        append64(kSentinel);
        for (int displacement = -128; displacement < 0; displacement += 8)
            memory(0x89, 0, 4, displacement);
        memory(0x8B, 0, 7, 16 * 8);
        memory(0x89, 0, 4, 64);
        for (std::uint8_t reg = 0; reg < 16; ++reg)
            if (reg != 4 && reg != 7) memory(0x8B, reg, 7, reg * 8);
        memory(0x8B, 7, 7, 7 * 8);
        const auto siteOffset = bytes.size();
        if (operands->Memory && operands->Memory->RipRelative) {
            auto relocated = site;
            const auto displacement = static_cast<std::int32_t>(4096 + 24 - (siteOffset + site.size()));
            std::memcpy(relocated.data() + relocated.size() - 4, &displacement, sizeof(displacement));
            operands = Codegen::DecodeBitPermutation(relocated.data(), relocated.size());
        }
        Codegen::StubBodyBuilder builder;
        Codegen::EmitBitPermutation(builder, *operands);
        auto body = builder.Finish();
        const auto stubOffset = bytes.size();
        Codegen::ApplyStubRelocations(body.Bytes, body.Relocations, siteOffset, stubOffset, 0);
        bytes.insert(bytes.end(), body.Bytes.begin(), body.Bytes.begin() + static_cast<std::ptrdiff_t>(body.ReturnBranchOffset));
        rip({0x48, 0x89, 0x25}, 16);
        rip({0x48, 0x8B, 0x25}, 8);
        memory(0x8D, 4, 4, -256);
        for (std::uint8_t reg = 0; reg < 16; ++reg) if (reg != 4) push(reg);
        bytes.push_back(0x9C);
        rip({0x48, 0x8B, 0x3D}, 0);
        memory(0x8B, 0, 4, 0);
        memory(0x89, 0, 7, 32 * 8);
        int offset = 8;
        for (int reg = 15; reg >= 0; --reg) {
            if (reg == 4) continue;
            memory(0x8B, 0, 4, offset);
            memory(0x89, 0, 7, (16 + reg) * 8);
            offset += 8;
        }
        rip({0x48, 0x8B, 0x05}, 16);
        memory(0x89, 0, 7, 20 * 8);
        for (int slot = 0; slot < 16; ++slot) {
            memory(0x8B, 0, 4, 256 + 128 - 128 + slot * 8);
            memory(0x89, 0, 7, (33 + slot) * 8);
        }
        rip({0x48, 0x8B, 0x25}, 8);
        memory(0x8D, 4, 4, 128);
        for (auto reg : {15, 14, 13, 12, 7, 6, 5, 3}) pop(static_cast<std::uint8_t>(reg));
        bytes.push_back(0xC3);
        require(bytes.size() < 4096, "Execution wrapper exceeded its code page");
#if defined(_WIN32)
        code = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 8192, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        require(code != nullptr, "Cannot allocate execution test pages");
#else
        code = static_cast<std::uint8_t*>(mmap(nullptr, 8192, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        require(code != MAP_FAILED, "Cannot allocate execution test pages");
#endif
        std::memcpy(code, bytes.data(), bytes.size());
        std::memcpy(code + 4096 + 24, &memoryValue, sizeof(memoryValue));
#if defined(_WIN32)
        DWORD previous = 0;
        require(VirtualProtect(code, 4096, PAGE_EXECUTE_READ, &previous) != 0, "Cannot make bit permutation test code executable");
        require(FlushInstructionCache(GetCurrentProcess(), code, bytes.size()) != 0, "Cannot flush execution test code");
#else
        require(mprotect(code, 4096, PROT_READ | PROT_EXEC) == 0, "Cannot make bit permutation test code executable");
#endif
    }

    ~Execution() {
#if defined(_WIN32)
        if (code) VirtualFree(code, 0, MEM_RELEASE);
#else
        if (code) munmap(code, 8192);
#endif
    }

    void Run(State& state) const { reinterpret_cast<void (*)(State*)>(code)(&state); }
    const void* Memory() const { return code + 4096 + 24; }
    static constexpr std::uint64_t kSentinel = 0xBADC0FFEE0DDF00Dull;

private:
    Bytes bytes;
    std::uint8_t* code = nullptr;

    void append32(std::int32_t value) {
        for (unsigned shift = 0; shift < 32; shift += 8) bytes.push_back(static_cast<std::uint8_t>(static_cast<std::uint32_t>(value) >> shift));
    }
    void append64(std::uint64_t value) {
        for (unsigned shift = 0; shift < 64; shift += 8) bytes.push_back(static_cast<std::uint8_t>(value >> shift));
    }
    void rip(std::initializer_list<std::uint8_t> opcode, int slot) {
        bytes.insert(bytes.end(), opcode);
        append32(static_cast<std::int32_t>(4096 + slot - (bytes.size() + 4)));
    }
    void push(std::uint8_t reg) {
        if (reg >= 8) bytes.push_back(0x41);
        bytes.push_back(static_cast<std::uint8_t>(0x50 | (reg & 7)));
    }
    void pop(std::uint8_t reg) {
        if (reg >= 8) bytes.push_back(0x41);
        bytes.push_back(static_cast<std::uint8_t>(0x58 | (reg & 7)));
    }
    void memory(std::uint8_t opcode, std::uint8_t reg, std::uint8_t base, int displacement) {
        bytes.insert(bytes.end(), {static_cast<std::uint8_t>(0x48 | ((reg & 8) >> 1) | ((base & 8) >> 3)), opcode,
            static_cast<std::uint8_t>(0x80 | ((reg & 7) << 3) | (base & 7))});
        if ((base & 7) == 4) bytes.push_back(0x24);
        append32(displacement);
    }
};

std::uint64_t expectedResult(std::uint64_t source, std::uint64_t mask, bool wide, bool deposit) {
    std::uint64_t result = 0;
    unsigned compact = 0;
    const unsigned bits = wide ? 64 : 32;
    for (unsigned bit = 0; bit < bits; ++bit) {
        if (((mask >> bit) & 1) == 0) continue;
        if (deposit) result |= ((source >> compact) & 1) << bit;
        else result |= ((source >> bit) & 1) << compact;
        ++compact;
    }
    return result;
}

void check(const State& state, const Codegen::BitPermutationOperands& operands, std::uint64_t mask) {
    const auto expected = expectedResult(state.Input[operands.Source], mask, operands.Wide, operands.Deposit);
    for (std::size_t reg = 0; reg < 16; ++reg)
        require(state.Output[reg] == (reg == operands.Destination ? expected : state.Input[reg]), "PEXT/PDEP changed a register or produced a wrong result");
    require((state.Flags & 0x8D5u) == (state.InputFlags & 0x8D5u), "PEXT/PDEP changed arithmetic flags");
    for (const auto value : state.RedZone) require(value == Execution::kSentinel, "PEXT/PDEP overwrote the caller's red zone");
}

State initialState() {
    State state;
    for (std::size_t reg = 0; reg < 16; ++reg) state.Input[reg] = 0xABCD123456789000ull + reg * 0x1234567ull;
    return state;
}

void execution() {
    std::size_t count = 0;
    for (const bool deposit : {false, true}) for (const bool wide : {false, true}) {
        for (std::uint8_t destination = 0; destination < 16; ++destination)
            for (std::uint8_t source = 0; source < 16; ++source)
                for (std::uint8_t mask = 0; mask < 16; ++mask) {
                    const auto site = instruction(deposit, wide, destination, source, mask);
                    Execution executable(site, 0);
                    const auto operands = *Codegen::DecodeBitPermutation(site.data(), site.size());
                    for (int variant = 0; variant < 4; ++variant) {
                        auto state = initialState();
                        if (variant == 1) state.Input.fill(0);
                        if (variant == 2) state.Input.fill(~std::uint64_t{0});
                        if (variant == 3) {
                            auto seed = static_cast<std::uint64_t>(1 + destination * 256 + source * 16 + mask);
                            for (auto& value : state.Input) {
                                seed ^= seed << 13;
                                seed ^= seed >> 7;
                                seed ^= seed << 17;
                                value = seed;
                            }
                        }
                        state.InputFlags |= variant & 1 ? 0x8D5 : 0;
                        executable.Run(state);
                        check(state, operands, state.Input[mask]);
                        ++count;
                    }
                }
        for (const std::uint8_t base : {std::uint8_t{4}}) for (std::uint8_t destination = 0; destination < 16; ++destination) {
            auto site = instruction(deposit, wide, destination, 2, base);
            site[4] = static_cast<std::uint8_t>(0x80 | ((destination & 7) << 3) | (base & 7));
            if ((base & 7) == 4) site.push_back(0x24);
            site.insert(site.end(), {static_cast<std::uint8_t>(base == 4 ? 64 : 16), 0, 0, 0});
            constexpr std::uint64_t value = 0xFEDCBA9876543210ull;
            Execution executable(site, value);
            auto state = initialState();
            state.Input[base] = reinterpret_cast<std::uint64_t>(executable.Memory()) - 16;
            state.Output[0] = value;
            state.InputFlags |= 0x895;
            executable.Run(state);
            check(state, *Codegen::DecodeBitPermutation(site.data(), site.size()), value);
            ++count;
        }
        const auto site = instruction(deposit, wide, 2, 0, 2);
        Execution executable(site, 0);
        for (std::uint64_t bits = 0; bits < 64; ++bits) {
            auto state = initialState();
            for (unsigned bit = 0; bit < 6; ++bit)
                if ((bits >> bit) & 1) state.InputFlags |= std::uint64_t{1} << std::array{0, 2, 4, 6, 7, 11}[bit];
            executable.Run(state);
            check(state, *Codegen::DecodeBitPermutation(site.data(), site.size()), state.Input[2]);
            ++count;
        }
    }
    std::cout << "Executed " << count << " PEXT/PDEP cases with register, flag and red-zone checks\n";
}
#else
void execution() { std::cout << "PEXT/PDEP execution checks require an x86-64 build\n"; }
#endif

}

int main() {
    try {
        decoding();
        rejectedMemory();
        matching();
        execution();
        std::cout << "PEXT/PDEP lowering checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
