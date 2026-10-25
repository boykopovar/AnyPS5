#include <codegen/x86/MaskedStoreLowering.hpp>
#include <codegen/x86/IAmd64OnlyInstructionMatcher.hpp>
#include <codegen/CodegenException.hpp>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>
#if (defined(__linux__) || defined(__APPLE__)) && defined(__x86_64__)
#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using Bytes = std::vector<std::uint8_t>;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct Form {
    std::uint8_t Opcode;
    bool W;
    const char* Name;
    std::uint8_t ElementSize;
};

constexpr std::array kForms{
    Form{0x2E, false, "VMASKMOVPS", 4},
    Form{0x2F, false, "VMASKMOVPD", 8},
    Form{0x8E, false, "VPMASKMOVD", 4},
    Form{0x8E, true, "VPMASKMOVQ", 8},
};

Bytes maskedStore(const Form& form, bool wide, std::uint8_t mask, std::uint8_t source, std::uint8_t rexIndexBase, Bytes memory, const Bytes& prefixes = {}) {
    Bytes bytes = prefixes;
    bytes.insert(bytes.end(), {0xC4,
        static_cast<std::uint8_t>(((source & 8) != 0 ? 0 : 0x80) | ((rexIndexBase & 2) != 0 ? 0 : 0x40) | ((rexIndexBase & 1) != 0 ? 0 : 0x20) | 0x02),
        static_cast<std::uint8_t>((form.W ? 0x80 : 0) | ((~mask & 15) << 3) | (wide ? 4 : 0) | 0x01), form.Opcode});
    memory.at(0) = static_cast<std::uint8_t>(memory[0] | ((source & 7) << 3));
    bytes.insert(bytes.end(), memory.begin(), memory.end());
    return bytes;
}

bool rejected(const Bytes& bytes) {
    try {
        (void)Codegen::DecodeMaskedStore(bytes.data(), bytes.size());
    } catch (const Codegen::CodegenException&) {
        return true;
    }
    return false;
}

void decoding() {
    for (const auto& form : kForms) {
        for (const bool wide : {false, true}) {
            for (std::uint8_t mask = 0; mask < 16; ++mask) {
                for (std::uint8_t source = 0; source < 16; ++source) {
                    const auto bytes = maskedStore(form, wide, mask, source, 0, {0x07});
                    const auto operands = Codegen::DecodeMaskedStore(bytes.data(), bytes.size());
                    require(operands && std::string(operands->Name) == form.Name && operands->ElementSize == form.ElementSize && operands->Wide == wide && operands->Mask == mask, "Masked store decoding changed the form, width or mask register");
                    require(operands->Memory.Rm == 7 && operands->Memory.Mod == 0 && operands->Memory.RexIndexBase == 0 && operands->Instruction == bytes, "Masked store decoding changed the memory operand");
                }
            }
        }
    }
    const auto extended = maskedStore(kForms[0], true, 3, 4, 3, {0x84, 0xD1, 0x78, 0x56, 0x34, 0x12}, {0x64, 0x67});
    const auto operands = Codegen::DecodeMaskedStore(extended.data(), extended.size());
    require(operands && operands->Memory.RexIndexBase == 3 && operands->Memory.Sib == 0xD1 && operands->Memory.Displacement == 0x12345678 && operands->Memory.Prefixes == Bytes{0x64, 0x67}, "Masked store extensions, SIB or prefixes were lost");
    const auto rip = maskedStore(kForms[1], false, 1, 2, 0, {0x05, 0x10, 0x00, 0x00, 0x00});
    const auto ripOperands = Codegen::DecodeMaskedStore(rip.data(), rip.size());
    require(ripOperands && ripOperands->Memory.RipRelative && ripOperands->DisplacementOffset == 5 && ripOperands->Memory.Displacement == 0x10, "RIP-relative masked store was not recognised");
    const auto stack = maskedStore(kForms[2], false, 1, 2, 0, {0x44, 0x24, 0x08});
    const auto stackOperands = Codegen::DecodeMaskedStore(stack.data(), stack.size());
    require(stackOperands && stackOperands->Memory.StackBase && stackOperands->DisplacementOffset == 6, "RSP-based masked store was not recognised");
    for (const auto& bytes : {
             Bytes{0xC4, 0xE2, 0x75, 0x2C, 0x07}, Bytes{0xC4, 0xE2, 0x75, 0x2D, 0x07}, Bytes{0xC4, 0xE2, 0x75, 0x8C, 0x07},
             Bytes{0xC4, 0xE2, 0xF5, 0x2E, 0x07}, Bytes{0xC4, 0xE2, 0xF5, 0x2F, 0x07}, Bytes{0xC4, 0xE2, 0x75, 0x2E, 0xC7},
             Bytes{0xC4, 0xE1, 0x75, 0x2E, 0x07}, Bytes{0xC4, 0xE2, 0x74, 0x2E, 0x07}, Bytes{0xC5, 0xF5, 0x2E, 0x07},
             Bytes{0xC4, 0xE2, 0x75, 0x2E}, Bytes{0x66, 0xC4, 0xE2, 0x75, 0x2E, 0x07}})
        require(!Codegen::DecodeMaskedStore(bytes.data(), bytes.size()), "A load, register form, other map or prefix matched a masked store");
    require(rejected(Bytes{0xC4, 0xE2, 0x75, 0x2E, 0x07, 0x00}), "A masked store with trailing bytes was accepted");
    require(rejected(Bytes{0xC4, 0xE2, 0x75, 0x2E, 0x47}), "A truncated masked store was accepted");
}

void body() {
    const auto site = maskedStore(kForms[1], false, 1, 2, 0, {0x07});
    Codegen::StubBodyBuilder builder;
    Codegen::EmitMaskedStore(builder, *Codegen::DecodeMaskedStore(site.data(), site.size()));
    const auto lowered = builder.Finish();
    const Bytes expected{
        0x48, 0x8D, 0x64, 0x24, 0x80, 0x9C, 0x50, 0x51, 0x48, 0x8D, 0x07, 0xC4, 0xE1, 0x79, 0x50, 0xC9,
        0xF6, 0xC1, 0x01, 0x74, 0x0A, 0xF0, 0x80, 0x48, 0x00, 0x00, 0xF0, 0x80, 0x48, 0x07, 0x00,
        0xF6, 0xC1, 0x02, 0x74, 0x0A, 0xF0, 0x80, 0x48, 0x08, 0x00, 0xF0, 0x80, 0x48, 0x0F, 0x00,
        0x59, 0x58, 0x9D, 0x48, 0x8D, 0xA4, 0x24, 0x80, 0x00, 0x00, 0x00, 0xC4, 0xE2, 0x71, 0x2F, 0x17};
    require(Bytes(lowered.Bytes.begin(), lowered.Bytes.begin() + static_cast<std::ptrdiff_t>(lowered.ReturnBranchOffset)) == expected, "VMASKMOVPD stub does not probe its elements before the store");
    const auto segment = maskedStore(kForms[0], true, 9, 2, 0, {0x44, 0x24, 0x08}, {0x64});
    Codegen::StubBodyBuilder segmentBuilder;
    Codegen::EmitMaskedStore(segmentBuilder, *Codegen::DecodeMaskedStore(segment.data(), segment.size()));
    const auto segmentBody = segmentBuilder.Finish().Bytes;
    const Bytes lea{0x64, 0x48, 0x8D, 0x84, 0x24, 0xA0, 0x00, 0x00, 0x00, 0xC4, 0xC1, 0x7C, 0x50, 0xC9};
    require(std::search(segmentBody.begin(), segmentBody.end(), lea.begin(), lea.end()) == segmentBody.begin() + 8, "RSP-based masked store address does not skip the spill frame");
    const Bytes probe{0x64, 0xF0, 0x80, 0x48, 0x1F, 0x00};
    require(std::search(segmentBody.begin(), segmentBody.end(), probe.begin(), probe.end()) != segmentBody.end(), "Masked store probes lost the segment prefix");
    const auto overflow = maskedStore(kForms[0], false, 1, 2, 0, {0x84, 0x24, 0xFF, 0xFF, 0xFF, 0x7F});
    bool thrown = false;
    try {
        Codegen::StubBodyBuilder overflowBuilder;
        Codegen::EmitMaskedStore(overflowBuilder, *Codegen::DecodeMaskedStore(overflow.data(), overflow.size()));
    } catch (const Codegen::CodegenException&) {
        thrown = true;
    }
    require(thrown, "An overflowing stack displacement was silently wrapped");
}

void matching() {
    const auto intel = Codegen::MakeAmd64OnlyInstructionMatcher();
    const auto rosetta = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Rosetta);
    for (const auto& form : kForms) {
        const auto bytes = maskedStore(form, true, 1, 0, 0, {0x00});
        require(!intel->Match(bytes.data(), bytes.size()), "Intel mode lowered a masked store");
        const auto match = rosetta->Match(bytes.data(), bytes.size());
        require(match && match->InstructionName == form.Name && match->Lowering == Codegen::Amd64OnlyLowering::Trampoline && !match->Optional, "Rosetta mode did not require a masked store trampoline");
        const std::array<std::span<const std::uint8_t>, 2> sequence{bytes, bytes};
        require(rosetta->MatchSequence(sequence, {}).has_value(), "A masked store sequence could not move out of line");
    }
    const Bytes load{0xC4, 0xE2, 0x75, 0x2C, 0x00};
    require(!rosetta->Match(load.data(), load.size()), "Rosetta mode lowered a masked load");
}

#if (defined(__linux__) || defined(__APPLE__)) && defined(__x86_64__)

constexpr std::size_t kPage = 4096;
constexpr std::size_t kDataPages = 16;
constexpr std::uint8_t kFill = 0xA5;
constexpr std::uint8_t kSentinel = 0x5A;

std::uint8_t* protectedPage = nullptr;
volatile sig_atomic_t faults = 0;

void onFault(int number, siginfo_t* info, void*) {
    const auto* address = static_cast<const std::uint8_t*>(info->si_addr);
    if (address < protectedPage || address >= protectedPage + kPage) {
        signal(number, SIG_DFL);
        return;
    }
    faults = faults + 1;
    mprotect(protectedPage, kPage, PROT_READ | PROT_WRITE);
}

struct alignas(32) Machine {
    std::uint8_t Ymm[16][32];
    std::uint64_t Gpr[7];
    std::uint64_t Flags;
    std::uint64_t Stack;
    std::uint64_t Code;
};

static_assert(offsetof(Machine, Gpr) == 512 && offsetof(Machine, Flags) == 568 && offsetof(Machine, Stack) == 576 && offsetof(Machine, Code) == 584);

void run(Machine& machine) {
    asm volatile(
        "mov %%rsp, %%r12\n\t"
        "mov %[machine], %%r13\n\t"
        "vmovdqu 0(%%r13), %%ymm0\n\t"
        "vmovdqu 32(%%r13), %%ymm1\n\t"
        "vmovdqu 64(%%r13), %%ymm2\n\t"
        "vmovdqu 96(%%r13), %%ymm3\n\t"
        "vmovdqu 128(%%r13), %%ymm4\n\t"
        "vmovdqu 160(%%r13), %%ymm5\n\t"
        "vmovdqu 192(%%r13), %%ymm6\n\t"
        "vmovdqu 224(%%r13), %%ymm7\n\t"
        "vmovdqu 256(%%r13), %%ymm8\n\t"
        "vmovdqu 288(%%r13), %%ymm9\n\t"
        "vmovdqu 320(%%r13), %%ymm10\n\t"
        "vmovdqu 352(%%r13), %%ymm11\n\t"
        "vmovdqu 384(%%r13), %%ymm12\n\t"
        "vmovdqu 416(%%r13), %%ymm13\n\t"
        "vmovdqu 448(%%r13), %%ymm14\n\t"
        "vmovdqu 480(%%r13), %%ymm15\n\t"
        "mov 512(%%r13), %%rax\n\t"
        "mov 520(%%r13), %%rcx\n\t"
        "mov 528(%%r13), %%rbx\n\t"
        "mov 536(%%r13), %%rsi\n\t"
        "mov 544(%%r13), %%rdi\n\t"
        "mov 552(%%r13), %%r9\n\t"
        "mov 560(%%r13), %%r10\n\t"
        "mov 576(%%r13), %%rsp\n\t"
        "pushq 568(%%r13)\n\t"
        "popfq\n\t"
        "call *584(%%r13)\n\t"
        "pushfq\n\t"
        "popq 568(%%r13)\n\t"
        "mov %%rsp, 576(%%r13)\n\t"
        "mov %%r12, %%rsp\n\t"
        "mov %%rax, 512(%%r13)\n\t"
        "mov %%rcx, 520(%%r13)\n\t"
        "mov %%rbx, 528(%%r13)\n\t"
        "mov %%rsi, 536(%%r13)\n\t"
        "mov %%rdi, 544(%%r13)\n\t"
        "mov %%r9, 552(%%r13)\n\t"
        "mov %%r10, 560(%%r13)\n\t"
        "vmovdqu %%ymm0, 0(%%r13)\n\t"
        "vmovdqu %%ymm1, 32(%%r13)\n\t"
        "vmovdqu %%ymm2, 64(%%r13)\n\t"
        "vmovdqu %%ymm3, 96(%%r13)\n\t"
        "vmovdqu %%ymm4, 128(%%r13)\n\t"
        "vmovdqu %%ymm5, 160(%%r13)\n\t"
        "vmovdqu %%ymm6, 192(%%r13)\n\t"
        "vmovdqu %%ymm7, 224(%%r13)\n\t"
        "vmovdqu %%ymm8, 256(%%r13)\n\t"
        "vmovdqu %%ymm9, 288(%%r13)\n\t"
        "vmovdqu %%ymm10, 320(%%r13)\n\t"
        "vmovdqu %%ymm11, 352(%%r13)\n\t"
        "vmovdqu %%ymm12, 384(%%r13)\n\t"
        "vmovdqu %%ymm13, 416(%%r13)\n\t"
        "vmovdqu %%ymm14, 448(%%r13)\n\t"
        "vmovdqu %%ymm15, 480(%%r13)\n\t"
        "vzeroupper\n\t"
        :
        : [machine] "r"(&machine)
        : "rax", "rcx", "rbx", "rsi", "rdi", "r9", "r10", "r12", "r13",
          "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
          "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15", "memory", "cc");
}

struct Addressing {
    Bytes Memory;
    std::uint8_t RexIndexBase;
    std::uint8_t Mask;
    std::uint8_t Source;
};

enum Register { Rax, Rcx, Rbx, Rsi, Rdi, R9, R10 };

const std::array kAddressings{
    Addressing{{0x07}, 0, 1, 2},
    Addressing{{0x44, 0x5E, 0x7F}, 0, 9, 14},
    Addressing{{0x84, 0x88, 0x00, 0xF0, 0xFF, 0xFF}, 0, 15, 0},
    Addressing{{0x04, 0xD1}, 3, 3, 8},
    Addressing{{0x44, 0x24, 0x40}, 0, 12, 5},
    Addressing{{0x05, 0x00, 0x00, 0x00, 0x00}, 0, 6, 11},
};

void execution() {
    const auto region = static_cast<std::uint8_t*>(mmap(nullptr, (kDataPages + 2) * kPage, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    require(region != MAP_FAILED, "Cannot map masked store test pages");
    auto* const code = region;
    auto* const data = region + kPage;
    protectedPage = data + kDataPages * kPage;
    struct sigaction action {};
    action.sa_sigaction = onFault;
    action.sa_flags = SA_SIGINFO;
    sigemptyset(&action.sa_mask);
    struct sigaction previousSegv {};
    struct sigaction previousBus {};
    sigaction(SIGSEGV, &action, &previousSegv);
    sigaction(SIGBUS, &action, &previousBus);
    const auto matcher = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Rosetta);
    const std::array<std::uint8_t, 6> patterns{0xFF, 0x00, 0x01, 0x80, 0x55, 0xFE};
    std::uint64_t runs = 0;
    for (const auto& form : kForms) {
        for (const bool wide : {false, true}) {
            const auto elements = (wide ? 32u : 16u) / form.ElementSize;
            for (std::size_t mode = 0; mode < kAddressings.size(); ++mode) {
                const auto& addressing = kAddressings[mode];
                const auto site = maskedStore(form, wide, addressing.Mask, addressing.Source, addressing.RexIndexBase, addressing.Memory);
                const auto match = matcher->Match(site.data(), site.size());
                require(match && match->Lowering == Codegen::Amd64OnlyLowering::Trampoline, "Masked store stub was not produced");
                for (const int offset : {-40, -16, -4, -2, 0, 8}) {
                    auto* const target = protectedPage + offset;
                    const bool stackBased = mode == 4;
                    auto* const stack = stackBased ? target + 8 - 0x40 : data + kDataPages / 2 * kPage;
                    auto body = match->StubBody;
                    const auto siteAddress = mode == 5 ? reinterpret_cast<std::uint64_t>(target) - site.size() : reinterpret_cast<std::uint64_t>(code);
                    Codegen::ApplyStubRelocations(body, match->Relocations, siteAddress, reinterpret_cast<std::uint64_t>(code), 0);
                    body.at(match->ReturnBranchOffset) = 0xC3;
                    require(body.size() <= kPage, "Masked store stub exceeds its code page");
                    std::memcpy(code, body.data(), body.size());
                    for (const auto pattern : patterns) {
                        Machine machine{};
                        for (unsigned reg = 0; reg < 16; ++reg)
                            for (unsigned byte = 0; byte < 32; ++byte)
                                machine.Ymm[reg][byte] = static_cast<std::uint8_t>(reg * 37 + byte * 11 + 1);
                        for (unsigned element = 0; element < 32u / form.ElementSize; ++element) {
                            const bool active = element >= elements || (pattern >> element & 1) != 0;
                            auto* lane = machine.Ymm[addressing.Mask] + element * form.ElementSize;
                            std::memset(lane, active ? 0x00 : 0xFF, form.ElementSize);
                            lane[0] = 0x01;
                            lane[form.ElementSize - 1] = active ? 0x80 : 0x7F;
                        }
                        machine.Gpr[Rax] = 0x1111111111111111ull;
                        machine.Gpr[Rcx] = 0x2222222222222222ull;
                        machine.Gpr[Rbx] = 8;
                        machine.Gpr[Rsi] = reinterpret_cast<std::uint64_t>(target) - 0x7F - 16;
                        machine.Gpr[Rdi] = reinterpret_cast<std::uint64_t>(target);
                        machine.Gpr[R9] = reinterpret_cast<std::uint64_t>(target) - 5 * 8;
                        machine.Gpr[R10] = 5;
                        if (mode == 2) {
                            machine.Gpr[Rcx] = 3;
                            machine.Gpr[Rax] = reinterpret_cast<std::uint64_t>(target) + 0x1000 - 12;
                        }
                        machine.Flags = (runs & 1) != 0 ? 0x8D7 : 0x002;
                        machine.Stack = reinterpret_cast<std::uint64_t>(stack);
                        machine.Code = reinterpret_cast<std::uint64_t>(code);
                        const auto input = machine;
                        require(mprotect(protectedPage, kPage, PROT_READ | PROT_WRITE) == 0, "Cannot unprotect the watched page");
                        std::memset(target - 16, kFill, 64);
                        std::memset(stack - 8 - 128, kSentinel, 128);
                        require(mprotect(protectedPage, kPage, PROT_READ) == 0, "Cannot protect the watched page");
                        faults = 0;
                        run(machine);
                        ++runs;
                        bool touchesProtected = false;
                        for (unsigned element = 0; element < elements; ++element) {
                            const bool active = (pattern >> element & 1) != 0;
                            for (unsigned byte = 0; byte < form.ElementSize; ++byte) {
                                const auto at = element * form.ElementSize + byte;
                                require(target[at] == (active ? input.Ymm[addressing.Source][at] : kFill), "Masked store wrote an inactive element or lost an active one");
                                touchesProtected = touchesProtected || (active && target + at >= protectedPage);
                            }
                        }
                        for (int at = -16; at < 0; ++at)
                            require(target[at] == kFill, "Masked store wrote before its operand");
                        for (unsigned at = elements * form.ElementSize; at < 48; ++at)
                            require(target[at] == kFill, "Masked store wrote past its operand");
                        require(faults == (touchesProtected ? 1 : 0), "Masked store stub did not fault exactly once on the protected page");
                        for (unsigned at = 0; at < 128; ++at)
                            require(stack[-8 - 128 + static_cast<int>(at)] == kSentinel, "Masked store stub wrote into the red zone");
                        require(std::memcmp(machine.Ymm, input.Ymm, sizeof(input.Ymm)) == 0, "Masked store stub changed a vector register");
                        require(std::memcmp(machine.Gpr, input.Gpr, sizeof(input.Gpr)) == 0, "Masked store stub changed a general register");
                        require((machine.Flags & 0x8D5) == (input.Flags & 0x8D5), "Masked store stub changed the flags");
                        require(machine.Stack == input.Stack, "Masked store stub did not restore RSP");
                    }
                }
            }
        }
    }
    sigaction(SIGSEGV, &previousSegv, nullptr);
    sigaction(SIGBUS, &previousBus, nullptr);
    munmap(region, (kDataPages + 2) * kPage);
    std::cout << runs << " masked store executions\n";
}
#else
void execution() { std::cout << "Masked store execution checks require a Linux/macOS x86-64 build\n"; }
#endif

}

int main() {
    try {
        decoding();
        body();
        matching();
        execution();
        std::cout << "Masked store lowering checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
