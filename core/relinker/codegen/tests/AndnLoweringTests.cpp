#include <codegen/x86/AndnLowering.hpp>
#include <codegen/x86/IAmd64OnlyInstructionMatcher.hpp>
#include <codegen/CodegenException.hpp>
#include <array>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>
#if (defined(__linux__) || defined(__APPLE__)) && defined(__x86_64__)
#include <sys/mman.h>
#endif

namespace {

using Bytes = std::vector<std::uint8_t>;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

Bytes instruction(bool wide, std::uint8_t dst, std::uint8_t first, std::uint8_t second) {
    return {0xC4, static_cast<std::uint8_t>(0xE2 ^ ((dst & 8) << 4) ^ ((second & 8) << 2)),
        static_cast<std::uint8_t>((wide ? 0x80 : 0) | ((~first & 15) << 3)), 0xF2,
        static_cast<std::uint8_t>(0xC0 | ((dst & 7) << 3) | (second & 7))};
}

void decoding() {
    for (bool wide : {false, true}) {
        for (std::uint8_t dst = 0; dst < 16; ++dst) {
            for (std::uint8_t first = 0; first < 16; ++first) {
                for (std::uint8_t second = 0; second < 16; ++second) {
                    const auto bytes = instruction(wide, dst, first, second);
                    const auto operands = Codegen::DecodeAndn(bytes.data(), bytes.size());
                    require(operands && operands->Wide == wide && operands->Destination == dst && operands->InvertedSource == first && operands->Source == second && !operands->Memory, "ANDN register decoding changed an operand");
                }
            }
        }
    }
    const Bytes memory{0x64, 0x67, 0xC4, 0x02, 0xA8, 0xF2, 0x8C, 0xAC, 0x10, 0, 0, 0};
    const auto operands = Codegen::DecodeAndn(memory.data(), memory.size());
    require(operands && operands->Destination == 9 && operands->InvertedSource == 10 && operands->Memory && operands->Memory->RexIndexBase == 3 && operands->Memory->Prefixes == Bytes{0x64, 0x67}, "ANDN memory extensions or prefixes were lost");
    for (const Bytes bytes : {Bytes{0x90}, Bytes{0xC4, 0xE2, 0xF0}, Bytes{0xC4, 0xE1, 0xF0, 0xF2, 0xC0}})
        require(!Codegen::DecodeAndn(bytes.data(), bytes.size()), "Unrelated or truncated opcode matched ANDN");
    for (const Bytes bytes : {Bytes{0xC4, 0xE2, 0xF4, 0xF2, 0xC0}, Bytes{0xC4, 0xE2, 0xF1, 0xF2, 0xC0}, Bytes{0xC4, 0xE2, 0xF0, 0xF2, 0x84}}) {
        bool rejected = false;
        try { (void)Codegen::DecodeAndn(bytes.data(), bytes.size()); }
        catch (const Codegen::CodegenException&) { rejected = true; }
        require(rejected, "Malformed ANDN was accepted");
    }
    const Bytes overflow{0xC4, 0xE2, 0xF0, 0xF2, 0x84, 0x24, 0xFF, 0xFF, 0xFF, 0x7F};
    bool rejected = false;
    try {
        Codegen::StubBodyBuilder body;
        Codegen::EmitAndn(body, *Codegen::DecodeAndn(overflow.data(), overflow.size()));
    } catch (const Codegen::CodegenException&) { rejected = true; }
    require(rejected, "An overflowing stack displacement was silently wrapped");
}

void matching() {
    const auto bytes = instruction(true, 0, 1, 6);
    const auto intel = Codegen::MakeAmd64OnlyInstructionMatcher();
    const auto rosetta = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Rosetta);
    require(!intel->Match(bytes.data(), bytes.size()), "Intel mode rewrote ANDN");
    const auto match = rosetta->Match(bytes.data(), bytes.size());
    require(match && match->InstructionName == "ANDN" && match->Lowering == Codegen::Amd64OnlyLowering::Trampoline && !match->Optional, "Rosetta mode did not require an ANDN trampoline");
    const std::array<std::span<const std::uint8_t>, 2> sequence{bytes, bytes};
    require(rosetta->MatchSequence(sequence, {}).has_value(), "An ANDN sequence could not move out of line");
}

#if (defined(__linux__) || defined(__APPLE__)) && defined(__x86_64__)

struct State {
    std::array<std::uint64_t, 16> Input{};
    std::array<std::uint64_t, 16> Output{};
    std::uint64_t Flags = 0;
    std::array<std::uint64_t, 16> RedZone{};
};

class Execution {
public:
    Execution(const Bytes& site, std::uint64_t memoryValue) {
        auto operands = Codegen::DecodeAndn(site.data(), site.size());
        require(operands.has_value(), "Execution site was not ANDN");
        for (auto reg : {3, 5, 12, 13, 14, 15}) push(static_cast<std::uint8_t>(reg));
        memory(0x8D, 4, 4, -128);
        rip({0x48, 0x89, 0x3D}, 0);
        rip({0x48, 0x89, 0x25}, 8);
        memory(0x89, 4, 7, 32);
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
            operands = Codegen::DecodeAndn(relocated.data(), relocated.size());
        }
        Codegen::StubBodyBuilder builder;
        Codegen::EmitAndn(builder, *operands);
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
        for (auto reg : {15, 14, 13, 12, 5, 3}) pop(static_cast<std::uint8_t>(reg));
        bytes.push_back(0xC3);
        require(bytes.size() < 4096, "Execution wrapper exceeded its code page");
        code = static_cast<std::uint8_t*>(mmap(nullptr, 8192, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        require(code != MAP_FAILED, "Cannot allocate execution test pages");
        std::memcpy(code, bytes.data(), bytes.size());
        std::memcpy(code + 4096 + 24, &memoryValue, sizeof(memoryValue));
        require(mprotect(code, 4096, PROT_READ | PROT_EXEC) == 0, "Cannot make ANDN test code executable");
    }

    ~Execution() { if (code != MAP_FAILED) munmap(code, 8192); }

    void Run(State& state) const { reinterpret_cast<void (*)(State*)>(code)(&state); }
    const void* Memory() const { return code + 4096 + 24; }
    static constexpr std::uint64_t kSentinel = 0xBADC0FFEE0DDF00Dull;

private:
    Bytes bytes;
    std::uint8_t* code = reinterpret_cast<std::uint8_t*>(MAP_FAILED);

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

void check(const State& state, const Codegen::AndnOperands& operands, std::uint64_t second) {
    const auto mask = operands.Wide ? ~std::uint64_t{0} : 0xFFFFFFFFull;
    const auto expected = (~state.Input[operands.InvertedSource] & second) & mask;
    for (std::size_t reg = 0; reg < 16; ++reg)
        require(state.Output[reg] == (reg == operands.Destination ? expected : state.Input[reg]), "ANDN changed a source/scratch register or produced an incorrect result");
    const auto flags = (expected == 0 ? 0x40u : 0u) | ((expected >> (operands.Wide ? 63 : 31)) != 0 ? 0x80u : 0u);
    require((state.Flags & 0x8C1u) == flags, "ANDN did not preserve its defined flag semantics");
    for (const auto value : state.RedZone) require(value == Execution::kSentinel, "ANDN overwrote the caller's red zone");
}

State initialState() {
    State state;
    for (std::size_t reg = 0; reg < 16; ++reg) state.Input[reg] = 0xABCD123456789000ull + reg * 0x1234567ull;
    return state;
}

void execution() {
    for (bool wide : {false, true}) {
        for (std::uint8_t dst = 0; dst < 16; ++dst) {
            for (const auto pair : {std::pair<std::uint8_t, std::uint8_t>{1, 6}, {dst, 10}, {11, dst}, {dst, dst}, {4, 9}, {9, 4}}) {
                const auto site = instruction(wide, dst, pair.first, pair.second);
                Execution executable(site, 0);
                for (int variant = 0; variant < 3; ++variant) {
                    auto state = initialState();
                    if (variant == 1) state.Input.fill(0);
                    if (variant == 2) state.Input.fill(~std::uint64_t{0});
                    executable.Run(state);
                    check(state, *Codegen::DecodeAndn(site.data(), site.size()), state.Input[pair.second]);
                }
            }
        }
        for (const Bytes tail : {Bytes{0x06}, Bytes{0x46, 0x10}, Bytes{0x84, 0x24, 0x40, 0, 0, 0}, Bytes{0x04, 0xCA}, Bytes{0x85, 0x10, 0, 0, 0}, Bytes{0x05, 0, 0, 0, 0}}) {
            Bytes site{0xC4, 0xE2, static_cast<std::uint8_t>(wide ? 0xF0 : 0x70), 0xF2};
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
            check(state, *Codegen::DecodeAndn(site.data(), site.size()), value);
        }
    }
}
#else
void execution() { std::cout << "ANDN execution checks require a Linux/macOS x86-64 build\n"; }
#endif

}

int main() {
    try {
        decoding();
        matching();
        execution();
        std::cout << "ANDN lowering checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
