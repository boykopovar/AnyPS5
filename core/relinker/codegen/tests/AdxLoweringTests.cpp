#include <codegen/x86/AdxLowering.hpp>
#include <codegen/x86/IAmd64OnlyInstructionMatcher.hpp>
#include <codegen/IAmd64OnlyConverter.hpp>
#include <codegen/CodegenException.hpp>
#include <array>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>
#if (defined(__linux__) || defined(__APPLE__)) && defined(__x86_64__)
#include <cpuid.h>
#include <sys/mman.h>
#endif

namespace {

using Bytes = std::vector<std::uint8_t>;

void require(const bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

void decoding() {
    const Bytes adcx32{0x66, 0x0F, 0x38, 0xF6, 0xC1};
    const Bytes adcx64{0x66, 0x4C, 0x0F, 0x38, 0xF6, 0xC1};
    const Bytes adox64{0xF3, 0x4C, 0x0F, 0x38, 0xF6, 0xC2};
    const Bytes adcxStack{0x66, 0x48, 0x0F, 0x38, 0xF6, 0x44, 0x24, 0x08};
    const Bytes adcxRip{0x66, 0x48, 0x0F, 0x38, 0xF6, 0x05, 0x10, 0x00, 0x00, 0x00};
    const Bytes adcxAddress32{0x67, 0x66, 0x0F, 0x38, 0xF6, 0x05, 0x10, 0x00, 0x00, 0x00};
    const auto first = Codegen::DecodeAdx(adcx32.data(), adcx32.size());
    const auto second = Codegen::DecodeAdx(adcx64.data(), adcx64.size());
    const auto third = Codegen::DecodeAdx(adox64.data(), adox64.size());
    const auto fourth = Codegen::DecodeAdx(adcxStack.data(), adcxStack.size());
    const auto fifth = Codegen::DecodeAdx(adcxRip.data(), adcxRip.size());
    const auto sixth = Codegen::DecodeAdx(adcxAddress32.data(), adcxAddress32.size());
    require(first && !first->Overflow && !first->Wide && first->Destination == 0 && first->Source == 1, "ADCX 32-bit registers decoded incorrectly");
    require(second && !second->Overflow && second->Wide && second->Destination == 8 && second->Source == 1, "ADCX REX extensions decoded incorrectly");
    require(third && third->Overflow && third->Wide && third->Destination == 8 && third->Source == 2, "ADOX REX extensions decoded incorrectly");
    require(fourth && fourth->Memory && fourth->Memory->StackBase && fourth->Memory->Displacement == 8, "ADCX stack operand decoded incorrectly");
    require(fifth && fifth->Memory && fifth->Memory->RipRelative, "ADCX RIP-relative source was not identified");
    require(sixth && sixth->Memory && !sixth->Memory->RipRelative, "ADCX address-size source was mistaken for RIP-relative");
    for (const auto& bytes : {Bytes{0x0F, 0x38, 0xF6, 0xC1}, Bytes{0xF2, 0x0F, 0x38, 0xF6, 0xC1}, Bytes{0x66, 0xF3, 0x0F, 0x38, 0xF6, 0xC1}})
        require(!Codegen::DecodeAdx(bytes.data(), bytes.size()), "An instruction without the mandatory ADX prefix matched");
    const auto rosetta = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Rosetta);
    const auto intel = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Intel);
    require(rosetta->Match(adcx32.data(), adcx32.size()).has_value(), "Rosetta mode did not lower ADCX");
    require(rosetta->Match(adox64.data(), adox64.size()).has_value(), "Rosetta mode did not lower ADOX");
    require(rosetta->Match(adcxRip.data(), adcxRip.size())->Relocations.size() == 1, "ADCX RIP-relative source has no relocation");
    require(!intel->Match(adcx32.data(), adcx32.size()), "Intel mode rewrote ADCX");
    bool rejected = false;
    const Bytes rspDestination{0x66, 0x0F, 0x38, 0xF6, 0xE1};
    try {
        (void)rosetta->Match(rspDestination.data(), rspDestination.size());
    } catch (const Codegen::CodegenException&) {
        rejected = true;
    }
    require(rejected, "ADCX with an RSP destination was silently accepted");
}

void conversion() {
    const auto converter = Codegen::MakeAmd64OnlyConverter(Codegen::Amd64OnlyTarget::Rosetta);
    const std::array<std::uint8_t, 3> adxOpcode{0x0F, 0x38, 0xF6};
    for (const auto& instruction : {Bytes{0x66, 0x0F, 0x38, 0xF6, 0xC1}, Bytes{0xF3, 0x0F, 0x38, 0xF6, 0xC1}}) {
        Bytes file(0x300, 0xCC);
        std::copy(instruction.begin(), instruction.end(), file.begin() + 0x200);
        const auto trailing = instruction.size() == 4 ? std::uint8_t{0x90} : std::uint8_t{0xC3};
        file[0x200 + instruction.size()] = trailing;
        file[0x200 + instruction.size() + 1] = 0xC3;
        const Domain::ProgramHeader segment{1, 5, 0x200, 0x1000, 0, instruction.size() + (trailing == 0x90 ? 2 : 1), instruction.size() + (trailing == 0x90 ? 2 : 1), 16};
        const auto result = converter->Convert(file, {segment});
        require(result.Trampolines.size() == 1 && result.Reports.size() == 1, "The converter did not lower an ADX instruction");
        const auto& trampoline = result.Trampolines.front();
        require(result.Reports.front().InstructionName == (instruction.front() == 0x66 ? "ADCX" : "ADOX"), "The converter reported the wrong ADX instruction");
        require(std::search(trampoline.Body.begin(), trampoline.Body.begin() + static_cast<std::ptrdiff_t>(trampoline.ReturnBranchOffset), adxOpcode.begin(), adxOpcode.end()) == trampoline.Body.begin() + static_cast<std::ptrdiff_t>(trampoline.ReturnBranchOffset), "The converter left an unsupported ADX opcode in the trampoline");
    }
}

#if (defined(__linux__) || defined(__APPLE__)) && defined(__x86_64__)

struct Run {
    std::uint64_t Value;
    std::uint64_t Flags;
    std::uint64_t Stack;
};

Run executeCode(Bytes codeBytes, const std::uint64_t destination, const std::uint64_t source, const std::uint64_t initialFlags, const std::uint64_t memorySource = 0, const std::optional<std::size_t> returnBranchOffset = std::nullopt) {
    const auto ret = codeBytes.size();
    if (returnBranchOffset) {
        const auto displacement = static_cast<std::int32_t>(ret - (*returnBranchOffset + 5));
        std::memcpy(codeBytes.data() + *returnBranchOffset + 1, &displacement, sizeof(displacement));
    }
    codeBytes.push_back(0xC3);
    auto* code = static_cast<std::uint8_t*>(mmap(nullptr, 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    require(code != MAP_FAILED, "Cannot allocate executable memory for an ADX stub");
    std::memcpy(code, codeBytes.data(), codeBytes.size());
    Run result{};
    asm volatile(
        "mov %[destination], %%rax\n\t"
        "mov %[source], %%rcx\n\t"
        "mov %[source], %%r9\n\t"
        "sub $128, %%rsp\n\t"
        "mov %[memorySource], (%%rsp)\n\t"
        "lea -8(%%rsp), %[stack]\n\t"
        "mov %[initialFlags], %%r11\n\t"
        "push %%r11\n\t"
        "popfq\n\t"
        "call *%[code]\n\t"
        "pushfq\n\t"
        "pop %[flags]\n\t"
        "add $128, %%rsp\n\t"
        "mov %%rax, %[value]\n\t"
        : [value] "=&r"(result.Value), [flags] "=&r"(result.Flags), [stack] "=&r"(result.Stack)
        : [destination] "r"(destination), [source] "r"(source), [initialFlags] "r"(initialFlags), [memorySource] "r"(memorySource), [code] "r"(code)
        : "rax", "rcx", "r9", "r11", "memory", "cc");
    munmap(code, 4096);
    return result;
}

Run execute(const Bytes& site, const std::uint64_t destination, const std::uint64_t source, const std::uint64_t initialFlags, const std::uint64_t memorySource = 0) {
    const auto matcher = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Rosetta);
    const auto match = matcher->Match(site.data(), site.size());
    require(match && match->Lowering == Codegen::Amd64OnlyLowering::Trampoline, "An ADX trampoline was not produced");
    return executeCode(match->StubBody, destination, source, initialFlags, memorySource, match->ReturnBranchOffset);
}

Run executeNative(const bool wide, const bool adox, const std::uint64_t destination, const std::uint64_t source, const std::uint64_t initialFlags) {
    Bytes instruction{static_cast<std::uint8_t>(adox ? 0xF3 : 0x66)};
    if (wide)
        instruction.push_back(0x48);
    instruction.insert(instruction.end(), {0x0F, 0x38, 0xF6, 0xC1, 0xC3});
    return executeCode(std::move(instruction), destination, source, initialFlags);
}

std::uint64_t expectedResult(const bool wide, const std::uint64_t destination, const std::uint64_t source, const std::uint64_t carry) {
    if (wide)
        return destination + source + carry;
    return static_cast<std::uint32_t>(destination) + static_cast<std::uint32_t>(source) + static_cast<std::uint32_t>(carry);
}

void execution() {
    for (const bool wide : {false, true}) {
        for (const bool adox : {false, true}) {
            Bytes site{static_cast<std::uint8_t>(adox ? 0xF3 : 0x66)};
            if (wide)
                site.push_back(0x48);
            site.insert(site.end(), {0x0F, 0x38, 0xF6, 0xC1});
            const auto mask = wide ? std::numeric_limits<std::uint64_t>::max() : std::uint64_t{0xFFFFFFFF};
            const auto high = wide ? std::numeric_limits<std::uint64_t>::max() : std::uint64_t{0xFFFFFFFF};
            const std::array<std::uint64_t, 4> values{0, 1, wide ? 0x7FFFFFFFFFFFFFFFull : 0x7FFFFFFFull, high};
            for (const auto destination : values) {
                for (const auto source : values) {
                    for (const auto inputFlag : {std::uint64_t{0}, std::uint64_t{1}}) {
                        const auto flagMask = adox ? std::uint64_t{0x800} : std::uint64_t{1};
                        const auto initial = (std::uint64_t{0x8D7} & ~flagMask) | (inputFlag ? flagMask : 0);
                        const auto run = execute(site, destination, source, initial);
                        const auto expected = expectedResult(wide, destination, source, inputFlag) & mask;
                        require(run.Value == expected, "ADX register lowering produced the wrong sum");
                        const bool carry = wide
                            ? (expected < destination || (inputFlag != 0 && expected == destination))
                            : ((static_cast<std::uint64_t>(static_cast<std::uint32_t>(destination)) + static_cast<std::uint32_t>(source) + inputFlag) >> 32) != 0;
                        const auto expectedFlags = (initial & ~flagMask) | (carry ? flagMask : 0);
                        require((run.Flags & 0x8D5) == (expectedFlags & 0x8D5), "ADX lowering produced the wrong carry flag or changed an unrelated arithmetic flag");
                    }
                }
            }
        }
    }
    unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
    const bool hasAdx = __get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx) != 0 && (ebx & (1u << 19)) != 0;
    if (hasAdx) {
        for (const bool wide : {false, true}) {
            for (const bool adox : {false, true}) {
                const auto high = wide ? std::numeric_limits<std::uint64_t>::max() : std::uint64_t{0xFFFFFFFF};
                const std::array<std::uint64_t, 4> values{0, 1, wide ? 0x7FFFFFFFFFFFFFFFull : 0x7FFFFFFFull, high};
                for (const auto destination : values) {
                    for (const auto source : values) {
                        for (const auto inputFlag : {std::uint64_t{0}, std::uint64_t{1}}) {
                            const auto flagMask = adox ? std::uint64_t{0x800} : std::uint64_t{1};
                            const auto initial = (std::uint64_t{0x8D7} & ~flagMask) | (inputFlag ? flagMask : 0);
                            Bytes site{static_cast<std::uint8_t>(adox ? 0xF3 : 0x66)};
                            if (wide)
                                site.push_back(0x48);
                            site.insert(site.end(), {0x0F, 0x38, 0xF6, 0xC1});
                            const auto lowered = execute(site, destination, source, initial);
                            const auto native = executeNative(wide, adox, destination, source, initial);
                            require(lowered.Value == native.Value && (lowered.Flags & 0x8D5) == (native.Flags & 0x8D5), "ADX lowering differs from native ADX");
                        }
                    }
                }
            }
        }
    }
    const Bytes stackSource{0x66, 0x48, 0x0F, 0x38, 0xF6, 0x44, 0x24, 0x08};
    const auto stackRun = execute(stackSource, 5, 0, 0x202, 7);
    require(stackRun.Value == 12 && (stackRun.Flags & 1) == 0, "ADX lowering did not compensate an RSP-relative memory source");
    const Bytes rspSource{0x66, 0x48, 0x0F, 0x38, 0xF6, 0xC4};
    const auto rspRun = execute(rspSource, 5, 0, 0xA03);
    require(rspRun.Value == rspRun.Stack + 6, "ADX lowering did not preserve the original RSP source value and carry-in");
    require(((rspRun.Flags ^ 0xA03) & 0x8D4) == 0, "ADX lowering did not preserve unrelated flags with RSP as the source");
    const Bytes highSource{0x66, 0x49, 0x0F, 0x38, 0xF6, 0xC1};
    const auto highRun = execute(highSource, 3, 5, 0x202);
    require(highRun.Value == 8, "ADX lowering did not preserve the high source-register extension");
}

#else

void execution() {}

#endif

}

int main() {
    try {
        decoding();
        conversion();
        execution();
        std::cout << "ADX lowering tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
