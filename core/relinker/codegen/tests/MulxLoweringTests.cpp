#include <codegen/x86/MulxLowering.hpp>
#include <codegen/x86/IAmd64OnlyInstructionMatcher.hpp>
#include <codegen/IAmd64OnlyConverter.hpp>
#include <codegen/CodegenException.hpp>
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <tuple>
#include <utility>
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

Bytes instruction(bool wide, std::uint8_t dst, std::uint8_t first, std::uint8_t second) {
    return {0xC4, static_cast<std::uint8_t>(0xE2 ^ ((dst & 8) << 4) ^ ((second & 8) << 2)),
        static_cast<std::uint8_t>((wide ? 0x80 : 0) | ((~first & 15) << 3) | 3), 0xF6,
        static_cast<std::uint8_t>(0xC0 | ((dst & 7) << 3) | (second & 7))};
}

void decoding() {
    for (bool wide : {false, true}) {
        for (std::uint8_t dst = 0; dst < 16; ++dst) {
            for (std::uint8_t first = 0; first < 16; ++first) {
                for (std::uint8_t second = 0; second < 16; ++second) {
                    const auto bytes = instruction(wide, dst, first, second);
                    const auto operands = Codegen::DecodeMulx(bytes.data(), bytes.size());
                    require(operands && operands->Wide == wide && operands->HighDestination == dst && operands->LowDestination == first && operands->Source == second && !operands->Memory, "MULX register decoding changed an operand");
                }
            }
        }
    }
    const Bytes memory{0x64, 0x67, 0xC4, 0x02, 0xAB, 0xF6, 0x8C, 0xAC, 0x10, 0, 0, 0};
    const auto operands = Codegen::DecodeMulx(memory.data(), memory.size());
    require(operands && operands->HighDestination == 9 && operands->LowDestination == 10 && operands->Memory && operands->Memory->RexIndexBase == 3 && operands->Memory->Prefixes == Bytes{0x64, 0x67}, "MULX memory extensions or prefixes were lost");
    for (const Bytes bytes : {Bytes{0x90}, Bytes{0xC4, 0xE2, 0xF0}, Bytes{0xC4, 0xE1, 0xF3, 0xF6, 0xC0}, Bytes{0xC4, 0xE2, 0xF0, 0xF6, 0xC0}})
        require(!Codegen::DecodeMulx(bytes.data(), bytes.size()), "Unrelated or truncated opcode matched MULX");
    for (const Bytes bytes : {Bytes{0xC4, 0xE2, 0xF7, 0xF6, 0xC0}, Bytes{0xC4, 0xE2, 0xF3, 0xF6, 0x84}}) {
        bool rejected = false;
        try { (void)Codegen::DecodeMulx(bytes.data(), bytes.size()); }
        catch (const Codegen::CodegenException&) { rejected = true; }
        require(rejected, "Malformed MULX was accepted");
    }
    const Bytes overflow{0xC4, 0xE2, 0xF3, 0xF6, 0x84, 0x24, 0xFF, 0xFF, 0xFF, 0x7F};
    bool rejected = false;
    try {
        Codegen::StubBodyBuilder body;
        Codegen::EmitMulx(body, *Codegen::DecodeMulx(overflow.data(), overflow.size()));
    } catch (const Codegen::CodegenException&) { rejected = true; }
    require(rejected, "An overflowing stack displacement was silently wrapped");
    for (const bool wide : {false, true}) {
        for (const auto [displacement, rejects32, rejects64] : {std::tuple<std::int32_t, bool, bool>{-199, false, true}, {-195, true, true}, {-192, true, true}, {-184, true, true}, {-176, true, true}, {-169, true, true}, {-168, false, false}}) {
            Bytes site{0xC4, 0xE2, static_cast<std::uint8_t>(wide ? 0xF3 : 0x73), 0xF6, 0x84, 0x24};
            for (unsigned shift = 0; shift < 32; shift += 8)
                site.push_back(static_cast<std::uint8_t>(static_cast<std::uint32_t>(displacement) >> shift));
            rejected = false;
            try {
                Codegen::StubBodyBuilder body;
                Codegen::EmitMulx(body, *Codegen::DecodeMulx(site.data(), site.size()));
            } catch (const Codegen::CodegenException&) { rejected = true; }
            require(rejected == (wide ? rejects64 : rejects32), "MULX spill alias detection has an incorrect boundary");
        }
    }
}

void matching() {
    const auto bytes = instruction(true, 0, 1, 6);
    const auto intel = Codegen::MakeAmd64OnlyInstructionMatcher();
    const auto rosetta = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Rosetta);
    require(!intel->Match(bytes.data(), bytes.size()), "Intel mode rewrote MULX");
    const auto match = rosetta->Match(bytes.data(), bytes.size());
    require(match && match->InstructionName == "MULX" && match->Lowering == Codegen::Amd64OnlyLowering::Trampoline && !match->Optional, "Rosetta mode did not require a MULX trampoline");
    const std::array<std::span<const std::uint8_t>, 2> sequence{bytes, bytes};
    require(rosetta->MatchSequence(sequence, {}).has_value(), "A MULX sequence could not move out of line");
    const auto converter = Codegen::MakeAmd64OnlyConverter(Codegen::Amd64OnlyTarget::Rosetta);
    for (const Bytes site : {bytes, Bytes{0xC4, 0xE2, 0xF3, 0xF6, 0x05, 0x40, 0, 0, 0}}) {
        Bytes file(0x300, 0xCC);
        std::copy(site.begin(), site.end(), file.begin() + 0x200);
        const Domain::ProgramHeader segment{1, 5, 0x200, 0x1000, 0, site.size(), site.size(), 16};
        const auto converted = converter->Convert(file, {segment});
        require(converted.Trampolines.size() == 1 && converted.Reports.size() == 1 && converted.Reports[0].InstructionName == "MULX", "Converter did not report a MULX trampoline");
        require(converted.Trampolines[0].Offset == 0x200 && converted.Trampolines[0].Length == site.size(), "Converter changed the MULX site extent");
        require(converted.Bytes == file, "MULX conversion changed the input image before patching");
        if (site.size() == 9) {
            const auto& relocations = converted.Trampolines[0].Relocations;
            require(relocations.size() == 1 && relocations[0].SiteTarget == 9 + 0x40, "MULX lost its RIP-relative memory target");
        }
        const auto intelConverted = Codegen::MakeAmd64OnlyConverter()->Convert(file, {segment});
        require(intelConverted.Reports.empty() && intelConverted.Bytes == file, "Intel conversion rewrote MULX");
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
        auto operands = Codegen::DecodeMulx(site.data(), site.size());
        require(operands.has_value(), "Execution site was not MULX");
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
            operands = Codegen::DecodeMulx(relocated.data(), relocated.size());
        }
        Codegen::StubBodyBuilder builder;
        Codegen::EmitMulx(builder, *operands);
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
        require(VirtualProtect(code, 4096, PAGE_EXECUTE_READ, &previous) != 0, "Cannot make MULX test code executable");
        require(FlushInstructionCache(GetCurrentProcess(), code, bytes.size()) != 0, "Cannot flush execution test code");
#else
        require(mprotect(code, 4096, PROT_READ | PROT_EXEC) == 0, "Cannot make MULX test code executable");
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

std::pair<std::uint64_t, std::uint64_t> product(std::uint64_t first, std::uint64_t second, bool wide) {
    if (!wide) {
        const auto result = (first & 0xFFFFFFFFull) * (second & 0xFFFFFFFFull);
        return {result >> 32, result & 0xFFFFFFFFull};
    }
    const auto low = (first & 0xFFFFFFFFull) * (second & 0xFFFFFFFFull);
    const auto middle = (first >> 32) * (second & 0xFFFFFFFFull) + (low >> 32);
    const auto carry = (first & 0xFFFFFFFFull) * (second >> 32) + (middle & 0xFFFFFFFFull);
    return {(first >> 32) * (second >> 32) + (middle >> 32) + (carry >> 32),
        (carry << 32) | (low & 0xFFFFFFFFull)};
}

void check(const State& state, const Codegen::MulxOperands& operands, std::uint64_t source) {
    const auto [high, low] = product(state.Input[2], source, operands.Wide);
    for (std::size_t reg = 0; reg < 16; ++reg) {
        const auto expected = reg == operands.HighDestination ? high : reg == operands.LowDestination ? low : state.Input[reg];
        require(state.Output[reg] == expected, "MULX changed an unrelated register or produced an incorrect result");
    }
    require((state.Flags & 0x8D5u) == (state.InputFlags & 0x8D5u), "MULX changed arithmetic flags");
    for (const auto value : state.RedZone) require(value == Execution::kSentinel, "MULX overwrote the caller's red zone");
}

State initialState() {
    State state;
    for (std::size_t reg = 0; reg < 16; ++reg) state.Input[reg] = 0xABCD123456789000ull + reg * 0x1234567ull;
    return state;
}

void execution() {
    for (bool wide : {false, true}) {
        for (std::uint8_t high = 0; high < 16; ++high) {
            for (std::uint8_t low = 0; low < 16; ++low) {
                for (std::uint8_t source = 0; source < 16; ++source) {
                    const auto site = instruction(wide, high, low, source);
                    Execution executable(site, 0);
                    for (int variant = 0; variant < 8; ++variant) {
                        auto state = initialState();
                        if (variant == 1) state.Input.fill(0);
                        if (variant == 2) state.Input.fill(~std::uint64_t{0});
                        if (variant == 3) state.Input[2] = 0x100000001ull;
                        if (variant >= 4) {
                            auto seed = static_cast<std::uint64_t>(variant * 4096 + high * 256 + low * 16 + source);
                            for (auto& value : state.Input) {
                                seed ^= seed << 13;
                                seed ^= seed >> 7;
                                seed ^= seed << 17;
                                value = seed;
                            }
                        }
                        state.InputFlags |= variant % 2 == 0 ? 0x8D5 : 0;
                        executable.Run(state);
                        check(state, *Codegen::DecodeMulx(site.data(), site.size()), state.Input[source]);
                    }
                }
            }
        }
        for (std::uint8_t base = 0; base < 16; ++base) {
            for (std::uint8_t high = 0; high < 16; ++high) {
                for (std::uint8_t low = 0; low < 16; ++low) {
                    auto site = instruction(wide, high, low, base);
                    site[4] = static_cast<std::uint8_t>(0x80 | ((high & 7) << 3) | (base & 7));
                    if ((base & 7) == 4) site.push_back(0x24);
                    site.insert(site.end(), {static_cast<std::uint8_t>(base == 4 ? 64 : 16), 0, 0, 0});
                    constexpr std::uint64_t value = 0xFEDCBA9876543210ull;
                    Execution executable(site, value);
                    auto state = initialState();
                    state.Input[base] = reinterpret_cast<std::uint64_t>(executable.Memory()) - 16;
                    state.Output[0] = value;
                    state.InputFlags |= 0x895;
                    executable.Run(state);
                    check(state, *Codegen::DecodeMulx(site.data(), site.size()), value);
                }
            }
        }
        for (const Bytes tail : {Bytes{0x06}, Bytes{0x46, 0x10}, Bytes{0x84, 0x24, 0x40, 0, 0, 0}, Bytes{0x04, 0xCA}, Bytes{0x85, 0x10, 0, 0, 0}, Bytes{0x05, 0, 0, 0, 0}}) {
            Bytes site{0xC4, 0xE2, static_cast<std::uint8_t>(wide ? 0xF3 : 0x73), 0xF6};
            site.insert(site.end(), tail.begin(), tail.end());
            constexpr std::uint64_t value = 0xFEDCBA9876543210ull;
            Execution executable(site, value);
            auto state = initialState();
            state.Input[6] = reinterpret_cast<std::uint64_t>(executable.Memory()) - (tail[0] == 0x46 ? 16 : 0);
            state.Input[2] = reinterpret_cast<std::uint64_t>(executable.Memory()) - 3 * 8;
            state.Input[1] = 3;
            state.Input[5] = reinterpret_cast<std::uint64_t>(executable.Memory()) - 16;
            state.Output[0] = value;
            executable.Run(state);
            check(state, *Codegen::DecodeMulx(site.data(), site.size()), value);
        }
        const auto site = instruction(wide, 2, 0, 2);
        Execution executable(site, 0);
        for (std::uint64_t bits = 0; bits < 64; ++bits) {
            auto state = initialState();
            for (unsigned bit = 0; bit < 6; ++bit)
                if ((bits >> bit) & 1) state.InputFlags |= std::uint64_t{1} << std::array{0, 2, 4, 6, 7, 11}[bit];
            executable.Run(state);
            check(state, *Codegen::DecodeMulx(site.data(), site.size()), state.Input[2]);
        }
    }
    std::cout << "Executed 73,868 MULX cases with register, flag and red-zone checks\n";
}
#else
void execution() { std::cout << "MULX execution checks require an x86-64 build\n"; }
#endif

}

int main() {
    try {
        decoding();
        matching();
        execution();
        std::cout << "MULX lowering checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
