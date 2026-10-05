#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <Sse4aEmulation.hpp>
#include <codegen/IAmd64OnlyConverter.hpp>
#include <codegen/x86/IAmd64OnlyInstructionMatcher.hpp>
#include <codegen/x86/Sse4aOperands.hpp>
#include <elfpatcher/windows/WindowsLoadImage.hpp>
#include <elfpatcher/windows/WindowsPeWriter.hpp>
#include <elfpatcher/windows/WindowsTrampolineBuilder.hpp>
#include <io/BufferUtils.hpp>
#include <array>
#include <cstring>
#include <functional>
#include <iostream>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace Elfpatcher::Windows;
using Bytes = std::vector<std::uint8_t>;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void requireFailure(const std::function<void()>& operation, const char* message) {
    try {
        operation();
    } catch (const Domain::RelinkerException&) {
        return;
    }
    throw std::runtime_error(message);
}

template<typename TValue>
void write(Bytes& bytes, std::size_t offset, TValue value) {
    if (offset > bytes.size() || sizeof(value) > bytes.size() - offset) throw std::runtime_error("Test fixture write is out of bounds");
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

template<typename TValue>
TValue read(const Bytes& bytes, std::size_t offset) {
    TValue value;
    if (offset > bytes.size() || sizeof(value) > bytes.size() - offset) throw std::runtime_error("Test fixture read is out of bounds");
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

struct alignas(16) State {
    std::uint8_t Xmm[16][16];
    std::uint64_t FlagsIn;
    std::uint64_t FlagsOut;
    std::uint64_t RspBefore;
    std::uint64_t RspAfter;
    std::uint8_t RedZone[128];
};

constexpr std::uint64_t kCanary = 0xA5C3E17B9D24F608ull;
constexpr std::uint64_t kStatusFlags = 0x8D5;
constexpr std::uint32_t kXmmOffset = 0;
constexpr std::uint32_t kFlagsInOffset = 256;
constexpr std::uint32_t kFlagsOutOffset = 264;
constexpr std::uint32_t kRspBeforeOffset = 272;
constexpr std::uint32_t kRspAfterOffset = 280;
constexpr std::uint32_t kRedZoneOffset = 288;

static_assert(offsetof(State, FlagsIn) == kFlagsInOffset && offsetof(State, FlagsOut) == kFlagsOutOffset && offsetof(State, RspBefore) == kRspBeforeOffset && offsetof(State, RspAfter) == kRspAfterOffset && offsetof(State, RedZone) == kRedZoneOffset, "State layout drifted");

void emit(Bytes& code, std::initializer_list<std::uint8_t> bytes) {
    code.insert(code.end(), bytes.begin(), bytes.end());
}

void emitU32(Bytes& code, std::uint32_t value) {
    for (std::size_t index = 0; index < 4; ++index) code.push_back(static_cast<std::uint8_t>(value >> (index * 8)));
}

void movdquXmmRbx(Bytes& code, const std::uint8_t reg, const bool store) {
    code.push_back(0xF3);
    if (reg >= 8) code.push_back(0x44);
    emit(code, {0x0F, static_cast<std::uint8_t>(store ? 0x7F : 0x6F), static_cast<std::uint8_t>(0x83 | ((reg & 7) << 3))});
    emitU32(code, kXmmOffset + reg * 16);
}

void movdquXmmRsp(Bytes& code, const std::uint8_t reg, const bool store, const std::uint32_t offset) {
    code.push_back(0xF3);
    if (reg >= 8) code.push_back(0x44);
    emit(code, {0x0F, static_cast<std::uint8_t>(store ? 0x7F : 0x6F), static_cast<std::uint8_t>(0x84 | ((reg & 7) << 3)), 0x24});
    emitU32(code, offset);
}

class Harness {
public:
    Harness() : _memory(VirtualAlloc(nullptr, kSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE)) {
        if (_memory == nullptr) throw std::runtime_error("Cannot allocate executable memory");
    }

    ~Harness() {
        VirtualFree(_memory, 0, MEM_RELEASE);
    }

    void Run(const Bytes& body, const std::size_t returnBranchOffset, State& state) const {
        Bytes code;
        emit(code, {0x53, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x81, 0xEC, 0xA8, 0x00, 0x00, 0x00});
        for (std::uint8_t reg = 6; reg < 16; ++reg) movdquXmmRsp(code, reg, true, (reg - 6) * 16);
        emit(code, {0x48, 0x89, 0xCB, 0xFF, 0xB3});
        emitU32(code, kFlagsInOffset);
        emit(code, {0x9D, 0x48, 0xB8});
        for (std::size_t index = 0; index < 8; ++index) code.push_back(static_cast<std::uint8_t>(kCanary >> (index * 8)));
        for (int slot = 0; slot < 16; ++slot) emit(code, {0x48, 0x89, 0x44, 0x24, static_cast<std::uint8_t>(-128 + slot * 8)});
        emit(code, {0x48, 0x89, 0xA3});
        emitU32(code, kRspBeforeOffset);
        for (std::uint8_t reg = 0; reg < 16; ++reg) movdquXmmRbx(code, reg, false);
        code.push_back(0xE9);
        const auto jumpOffset = code.size();
        emitU32(code, 0);
        const auto continuation = code.size();
        for (int slot = 0; slot < 16; ++slot) {
            emit(code, {0x48, 0x8B, 0x44, 0x24, static_cast<std::uint8_t>(-128 + slot * 8), 0x48, 0x89, 0x83});
            emitU32(code, kRedZoneOffset + slot * 8);
        }
        emit(code, {0x48, 0x89, 0xA3});
        emitU32(code, kRspAfterOffset);
        emit(code, {0x9C, 0x8F, 0x83});
        emitU32(code, kFlagsOutOffset);
        for (std::uint8_t reg = 0; reg < 16; ++reg) movdquXmmRbx(code, reg, true);
        for (std::uint8_t reg = 6; reg < 16; ++reg) movdquXmmRsp(code, reg, false, (reg - 6) * 16);
        emit(code, {0x48, 0x81, 0xC4, 0xA8, 0x00, 0x00, 0x00, 0x41, 0x5F, 0x41, 0x5E, 0x41, 0x5D, 0x41, 0x5C, 0x5F, 0x5E, 0x5D, 0x5B, 0xC3});
        code.resize(Io::AlignUp(code.size(), std::size_t{16}), 0xCC);
        const auto bodyOffset = code.size();
        code.insert(code.end(), body.begin(), body.end());
        write<std::int32_t>(code, jumpOffset, static_cast<std::int32_t>(bodyOffset - (jumpOffset + 4)));
        write<std::int32_t>(code, bodyOffset + returnBranchOffset + 1, static_cast<std::int32_t>(continuation - (bodyOffset + returnBranchOffset + 5)));
        if (code.size() > kSize) throw std::runtime_error("Harness code exceeds its page");
        std::memcpy(_memory, code.data(), code.size());
        FlushInstructionCache(GetCurrentProcess(), _memory, code.size());
        reinterpret_cast<void (*)(State*)>(_memory)(&state);
    }

private:
    static constexpr std::size_t kSize = 4096;
    void* _memory;
};

std::uint64_t low(const std::uint8_t (&lane)[16]) {
    std::uint64_t value;
    std::memcpy(&value, lane, 8);
    return value;
}

std::uint64_t referenceLow(const Codegen::Sse4aOperands& operands, const State& input) {
    const auto mask = operands.Length >= 64 ? ~std::uint64_t{0} : ((std::uint64_t{1} << operands.Length) - 1);
    const auto destination = low(input.Xmm[operands.Destination]);
    if (!operands.Insertq) return (destination >> operands.Index) & mask;
    const auto hole = mask << operands.Index;
    return (destination & ~hole) | ((low(input.Xmm[operands.Source]) & mask) << operands.Index);
}

std::uint64_t libcReferenceLow(const Codegen::Sse4aOperands& operands, const State& input) {
    CONTEXT context{};
    for (unsigned reg = 0; reg < 16; ++reg) {
        std::memcpy(&context.FltSave.XmmRegisters[reg].Low, input.Xmm[reg], 8);
        std::memcpy(&context.FltSave.XmmRegisters[reg].High, input.Xmm[reg] + 8, 8);
    }
    sse4a::Instruction instruction;
    instruction.op = operands.Insertq ? sse4a::Op::Insertq : sse4a::Op::Extrq;
    instruction.registerForm = operands.RegisterForm;
    instruction.destination = operands.Destination;
    instruction.source = operands.Source;
    instruction.length = static_cast<std::uint8_t>(operands.Length == 64 ? 0 : operands.Length);
    instruction.index = operands.Index;
    sse4a::Execute(instruction, context);
    return static_cast<std::uint64_t>(context.FltSave.XmmRegisters[operands.Destination].Low);
}

struct Case {
    bool Insertq;
    std::uint8_t Destination;
    std::uint8_t Source;
    std::uint8_t Length;
    std::uint8_t Index;
    bool RegisterForm = false;
};

Bytes encode(const Case& item) {
    Bytes bytes = {static_cast<std::uint8_t>(item.Insertq ? 0xF2 : 0x66)};
    const std::uint8_t reg = item.Insertq || item.RegisterForm ? item.Destination : 0;
    const std::uint8_t rm = item.Insertq || item.RegisterForm ? item.Source : item.Destination;
    const auto rex = static_cast<std::uint8_t>(0x40 | (reg >= 8 ? 4 : 0) | (rm >= 8 ? 1 : 0));
    if (rex != 0x40) bytes.push_back(rex);
    bytes.insert(bytes.end(), {0x0F, static_cast<std::uint8_t>(item.RegisterForm ? 0x79 : 0x78), static_cast<std::uint8_t>(0xC0 | ((reg & 7) << 3) | (rm & 7))});
    if (!item.RegisterForm) bytes.insert(bytes.end(), {static_cast<std::uint8_t>(item.Length == 64 ? 0 : item.Length), item.Index});
    return bytes;
}

std::string describe(const Case& item) {
    const bool source = item.Insertq || item.RegisterForm;
    return std::string(item.Insertq ? "insertq xmm" : "extrq xmm") + std::to_string(item.Destination) + (source ? ", xmm" + std::to_string(item.Source) : "") + (item.RegisterForm ? " (register form)" : "") + ", " + std::to_string(item.Length) + ", " + std::to_string(item.Index);
}

std::size_t g_executions = 0;

void requireEnvironment(const State& input, const State& state, const std::uint16_t written, const std::string& description) {
    for (unsigned reg = 0; reg < 16; ++reg) {
        if (((written >> reg) & 1) != 0) continue;
        require(std::memcmp(state.Xmm[reg], input.Xmm[reg], 16) == 0, "Lowered sequence clobbered xmm" + std::to_string(reg) + ": " + description);
    }
    require(((state.FlagsOut ^ input.FlagsIn) & kStatusFlags) == 0, "Lowered sequence changed RFLAGS: " + description);
    require(state.RspAfter == state.RspBefore, "Lowered sequence did not restore rsp: " + description);
    for (int slot = 0; slot < 16; ++slot) {
        std::uint64_t value;
        std::memcpy(&value, state.RedZone + slot * 8, 8);
        require(value == kCanary, "Lowered sequence wrote into the red zone: " + description);
    }
}

void executeCase(const Harness& harness, const Codegen::IAmd64OnlyInstructionMatcher& matcher, const Case& item, std::mt19937_64& random, const int images) {
    const auto site = encode(item);
    const auto match = matcher.Match(site.data(), site.size());
    require(match.has_value() && match->Lowering != Codegen::Amd64OnlyLowering::Unsupported, "SSE4a instruction was not lowered: " + describe(item));
    Bytes body;
    std::size_t returnBranchOffset = 0;
    if (match->Lowering == Codegen::Amd64OnlyLowering::InPlace) {
        require(match->ReplacementBytes.size() == site.size(), "In-place lowering changed the length: " + describe(item));
        body = match->ReplacementBytes;
        returnBranchOffset = body.size();
        body.insert(body.end(), {0xE9, 0, 0, 0, 0});
    } else {
        body = match->StubBody;
        returnBranchOffset = match->ReturnBranchOffset;
        require(body.size() % 16 == 0 || body.size() == returnBranchOffset + 5, "Stub body with constants is not padded to 16 bytes: " + describe(item));
    }
    const auto operands = Codegen::DecodeSse4a(site.data(), site.size());
    auto field = operands;
    if (operands.RegisterForm) {
        field.Length = item.Length;
        field.Index = item.Index;
    }
    const std::size_t controlOffset = operands.Insertq ? 8 : 0;
    const std::array<std::uint64_t, 4> flags = {0x202, 0x203, 0x246, 0xAC7};
    for (int image = 0; image < images; ++image) {
        State input{};
        for (auto& lane : input.Xmm) for (auto& byte : lane) byte = static_cast<std::uint8_t>(random());
        if (operands.RegisterForm) {
            auto& control = input.Xmm[operands.Source];
            control[controlOffset] = static_cast<std::uint8_t>((control[controlOffset] & 0xC0) | (item.Length == 64 ? 0 : item.Length));
            control[controlOffset + 1] = static_cast<std::uint8_t>((control[controlOffset + 1] & 0xC0) | item.Index);
        }
        input.FlagsIn = flags[static_cast<std::size_t>(image) % flags.size()];
        State state = input;
        harness.Run(body, returnBranchOffset, state);
        ++g_executions;
        const auto expected = referenceLow(field, input);
        require(expected == libcReferenceLow(operands, input), "Transcribed reference disagrees with the libc emulation: " + describe(item));
        require(low(state.Xmm[operands.Destination]) == expected, "Lowered sequence computed the wrong field: " + describe(item));
        requireEnvironment(input, state, static_cast<std::uint16_t>(1u << operands.Destination), describe(item));
    }
}

struct Sha256Step {
    std::uint8_t Opcode;
    std::uint8_t Destination;
    std::uint8_t Source;
};

Bytes encodeSha256(const Sha256Step& step) {
    Bytes bytes;
    const auto rex = static_cast<std::uint8_t>(0x40 | (step.Destination >= 8 ? 4 : 0) | (step.Source >= 8 ? 1 : 0));
    if (rex != 0x40) bytes.push_back(rex);
    bytes.insert(bytes.end(), {0x0F, 0x38, step.Opcode, static_cast<std::uint8_t>(0xC0 | ((step.Destination & 7) << 3) | (step.Source & 7))});
    return bytes;
}

std::uint32_t rotr(const std::uint32_t value, const unsigned count) {
    return (value >> count) | (value << (32 - count));
}

void sha256Reference(const Sha256Step& step, std::uint8_t (&xmm)[16][16]) {
    std::uint32_t a[4];
    std::uint32_t b[4];
    std::uint32_t k[4];
    std::uint32_t r[4];
    std::memcpy(a, xmm[step.Destination], sizeof(a));
    std::memcpy(b, xmm[step.Source], sizeof(b));
    std::memcpy(k, xmm[0], sizeof(k));
    const auto sigma0 = [](const std::uint32_t w) { return rotr(w, 7) ^ rotr(w, 18) ^ (w >> 3); };
    const auto sigma1 = [](const std::uint32_t w) { return rotr(w, 17) ^ rotr(w, 19) ^ (w >> 10); };
    if (step.Opcode == 0xCC) {
        for (int lane = 0; lane < 3; ++lane) r[lane] = a[lane] + sigma0(a[lane + 1]);
        r[3] = a[3] + sigma0(b[0]);
    } else if (step.Opcode == 0xCD) {
        r[0] = a[0] + sigma1(b[2]);
        r[1] = a[1] + sigma1(b[3]);
        r[2] = a[2] + sigma1(r[0]);
        r[3] = a[3] + sigma1(r[1]);
    } else {
        std::uint32_t sa = b[3], sb = b[2], sc = a[3], sd = a[2], se = b[1], sf = b[0], sg = a[1], sh = a[0];
        for (int round = 0; round < 2; ++round) {
            const auto t1 = sh + (rotr(se, 6) ^ rotr(se, 11) ^ rotr(se, 25)) + ((se & sf) ^ (~se & sg)) + k[round];
            const auto t2 = (rotr(sa, 2) ^ rotr(sa, 13) ^ rotr(sa, 22)) + ((sa & sb) ^ (sa & sc) ^ (sb & sc));
            sh = sg; sg = sf; sf = se; se = sd + t1; sd = sc; sc = sb; sb = sa; sa = t1 + t2;
        }
        r[0] = sf;
        r[1] = se;
        r[2] = sb;
        r[3] = sa;
    }
    std::memcpy(xmm[step.Destination], r, sizeof(r));
}

std::string describeSha256(const std::vector<Sha256Step>& steps) {
    std::string text;
    for (const auto& step : steps)
        text += std::string(step.Opcode == 0xCB ? "sha256rnds2" : step.Opcode == 0xCC ? "sha256msg1" : "sha256msg2") + " xmm" + std::to_string(step.Destination) + ", xmm" + std::to_string(step.Source) + "; ";
    return text;
}

void executeSha256(const Harness& harness, const Codegen::IAmd64OnlyInstructionMatcher& matcher, const std::vector<Sha256Step>& steps, std::mt19937_64& random, const int images) {
    std::vector<Bytes> sites;
    for (const auto& step : steps) sites.push_back(encodeSha256(step));
    const std::vector<std::span<const std::uint8_t>> spans(sites.begin(), sites.end());
    const auto match = steps.size() == 1 ? matcher.Match(sites[0].data(), sites[0].size()) : matcher.MatchSequence(spans, {});
    require(match.has_value() && match->Lowering == Codegen::Amd64OnlyLowering::Trampoline, "SHA-256 was not lowered through a stub: " + describeSha256(steps));
    std::uint16_t written = 0;
    for (const auto& step : steps) written = static_cast<std::uint16_t>(written | (1u << step.Destination));
    const std::array<std::uint64_t, 4> flags = {0x202, 0x203, 0x246, 0xAC7};
    for (int image = 0; image < images; ++image) {
        State input{};
        for (auto& lane : input.Xmm) for (auto& byte : lane) byte = static_cast<std::uint8_t>(random());
        input.FlagsIn = flags[static_cast<std::size_t>(image) % flags.size()];
        State state = input;
        harness.Run(match->StubBody, match->ReturnBranchOffset, state);
        ++g_executions;
        State expected = input;
        for (const auto& step : steps) sha256Reference(step, expected.Xmm);
        for (unsigned reg = 0; reg < 16; ++reg)
            if (((written >> reg) & 1) != 0) require(std::memcmp(state.Xmm[reg], expected.Xmm[reg], 16) == 0, "Lowered SHA-256 computed the wrong xmm" + std::to_string(reg) + ": " + describeSha256(steps));
        requireEnvironment(input, state, written, describeSha256(steps));
    }
}

struct Sha1Memory {
    std::uint8_t ModRm;
    Bytes Tail;
    std::optional<std::uint8_t> Slot;
    const char* Name;
};

const std::array<Sha1Memory, 4> kSha1Memory = {{
    {0x03, {}, std::uint8_t{0}, "[rbx]"},
    {0x43, {0x70}, std::uint8_t{7}, "[rbx+0x70]"},
    {0x83, {0xF0, 0x00, 0x00, 0x00}, std::uint8_t{15}, "[rbx+0xf0]"},
    {0x44, {0x24, 0xF0}, std::nullopt, "[rsp-0x10]"}}};

struct Sha1Step {
    std::uint8_t Opcode;
    std::uint8_t Destination;
    std::uint8_t Source;
    std::uint8_t Immediate = 0;
    bool Memory = false;
};

Bytes encodeSha1(const Sha1Step& step) {
    Bytes bytes;
    const auto rex = static_cast<std::uint8_t>(0x40 | (step.Destination >= 8 ? 4 : 0) | (!step.Memory && step.Source >= 8 ? 1 : 0));
    if (rex != 0x40) bytes.push_back(rex);
    bytes.insert(bytes.end(), {0x0F, static_cast<std::uint8_t>(step.Opcode == 0xCC ? 0x3A : 0x38), step.Opcode});
    if (step.Memory) {
        const auto& memory = kSha1Memory[step.Source];
        bytes.push_back(static_cast<std::uint8_t>(memory.ModRm | ((step.Destination & 7) << 3)));
        bytes.insert(bytes.end(), memory.Tail.begin(), memory.Tail.end());
    } else {
        bytes.push_back(static_cast<std::uint8_t>(0xC0 | ((step.Destination & 7) << 3) | (step.Source & 7)));
    }
    if (step.Opcode == 0xCC) bytes.push_back(step.Immediate);
    return bytes;
}

std::uint32_t rotl(const std::uint32_t value, const unsigned count) {
    return (value << count) | (value >> (32 - count));
}

void sha1Reference(const Sha1Step& step, const State& input, std::uint8_t (&xmm)[16][16]) {
    std::uint32_t x[4];
    std::uint32_t y[4];
    std::uint32_t r[4];
    std::memcpy(x, xmm[step.Destination], sizeof(x));
    if (!step.Memory) {
        std::memcpy(y, xmm[step.Source], sizeof(y));
    } else if (const auto slot = kSha1Memory[step.Source].Slot) {
        std::memcpy(y, input.Xmm[*slot], sizeof(y));
    } else {
        const std::uint64_t canary[2] = {kCanary, kCanary};
        std::memcpy(y, canary, sizeof(y));
    }
    if (step.Opcode == 0xC8) {
        r[0] = y[0];
        r[1] = y[1];
        r[2] = y[2];
        r[3] = y[3] + rotl(x[3], 30);
    } else if (step.Opcode == 0xC9) {
        r[0] = x[0] ^ y[2];
        r[1] = x[1] ^ y[3];
        r[2] = x[2] ^ x[0];
        r[3] = x[3] ^ x[1];
    } else if (step.Opcode == 0xCA) {
        r[3] = rotl(x[3] ^ y[2], 1);
        r[2] = rotl(x[2] ^ y[1], 1);
        r[1] = rotl(x[1] ^ y[0], 1);
        r[0] = rotl(x[0] ^ r[3], 1);
    } else {
        constexpr std::uint32_t keys[4] = {0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6};
        const auto function = step.Immediate & 3;
        std::uint32_t a = x[3], b = x[2], c = x[1], d = x[0], e = 0;
        for (int round = 0; round < 4; ++round) {
            std::uint32_t f = b ^ c ^ d;
            if (function == 0) f = (b & c) ^ (~b & d);
            if (function == 2) f = (b & c) ^ (b & d) ^ (c & d);
            const auto next = f + rotl(a, 5) + y[3 - round] + e + keys[function];
            e = d; d = c; c = rotl(b, 30); b = a; a = next;
        }
        r[0] = d;
        r[1] = c;
        r[2] = b;
        r[3] = a;
    }
    std::memcpy(xmm[step.Destination], r, sizeof(r));
}

std::string describeSha1(const std::vector<Sha1Step>& steps) {
    std::string text;
    for (const auto& step : steps) {
        text += std::string(step.Opcode == 0xCC ? "sha1rnds4" : step.Opcode == 0xC8 ? "sha1nexte" : step.Opcode == 0xC9 ? "sha1msg1" : "sha1msg2") + " xmm" + std::to_string(step.Destination) + ", ";
        text += step.Memory ? std::string(kSha1Memory[step.Source].Name) : "xmm" + std::to_string(step.Source);
        if (step.Opcode == 0xCC) text += ", " + std::to_string(step.Immediate);
        text += "; ";
    }
    return text;
}

void executeSha1(const Harness& harness, const Codegen::IAmd64OnlyInstructionMatcher& matcher, const std::vector<Sha1Step>& steps, std::mt19937_64& random, const int images) {
    std::vector<Bytes> sites;
    for (const auto& step : steps) sites.push_back(encodeSha1(step));
    const std::vector<std::span<const std::uint8_t>> spans(sites.begin(), sites.end());
    const auto match = steps.size() == 1 ? matcher.Match(sites[0].data(), sites[0].size()) : matcher.MatchSequence(spans, {});
    require(match.has_value() && match->Lowering == Codegen::Amd64OnlyLowering::Trampoline, "SHA-1 was not lowered through a stub: " + describeSha1(steps));
    std::uint16_t written = 0;
    for (const auto& step : steps) written = static_cast<std::uint16_t>(written | (1u << step.Destination));
    const std::array<std::uint64_t, 4> flags = {0x202, 0x203, 0x246, 0xAC7};
    for (int image = 0; image < images; ++image) {
        State input{};
        for (auto& lane : input.Xmm) for (auto& byte : lane) byte = static_cast<std::uint8_t>(random());
        input.FlagsIn = flags[static_cast<std::size_t>(image) % flags.size()];
        State state = input;
        harness.Run(match->StubBody, match->ReturnBranchOffset, state);
        ++g_executions;
        State expected = input;
        for (const auto& step : steps) sha1Reference(step, input, expected.Xmm);
        for (unsigned reg = 0; reg < 16; ++reg)
            if (((written >> reg) & 1) != 0) require(std::memcmp(state.Xmm[reg], expected.Xmm[reg], 16) == 0, "Lowered SHA-1 computed the wrong xmm" + std::to_string(reg) + ": " + describeSha1(steps));
        requireEnvironment(input, state, written, describeSha1(steps));
    }
}

void sha1Execution(const Harness& harness, const Codegen::IAmd64OnlyInstructionMatcher& matcher, std::mt19937_64& random) {
    const std::array<std::uint8_t, 4> opcodes = {0xCC, 0xC8, 0xC9, 0xCA};
    for (const auto opcode : opcodes) {
        for (std::uint8_t dst = 0; dst < 16; ++dst) {
            for (std::uint8_t src = 0; src < 16; ++src)
                executeSha1(harness, matcher, {{opcode, dst, src, static_cast<std::uint8_t>(random())}}, random, 2);
            for (std::uint8_t memory = 0; memory < kSha1Memory.size(); ++memory)
                for (std::uint8_t function = 0; function < 4; ++function)
                    executeSha1(harness, matcher, {{opcode, dst, memory, static_cast<std::uint8_t>((random() & 0xFC) | function), true}}, random, 2);
        }
    }
    for (int sequence = 0; sequence < 256; ++sequence) {
        std::vector<Sha1Step> steps(2 + random() % 2);
        for (auto& step : steps) {
            const bool memory = random() % 2 == 0;
            step = {opcodes[random() % opcodes.size()], static_cast<std::uint8_t>(random() % 16), static_cast<std::uint8_t>(random() % (memory ? kSha1Memory.size() : 16)), static_cast<std::uint8_t>(random()), memory};
        }
        executeSha1(harness, matcher, steps, random, 2);
    }
}

void clzeroExecution(const Harness& harness, const Codegen::IAmd64OnlyInstructionMatcher& matcher, std::mt19937_64& random) {
    const Bytes plain = {0x0F, 0x01, 0xFC};
    const Bytes addressSize32 = {0x67, 0x0F, 0x01, 0xFC};
    const std::vector<std::span<const std::uint8_t>> pair = {plain, plain};
    constexpr std::size_t size = 3 * 4096;
    auto* high = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    std::uint8_t* low = nullptr;
    for (std::uintptr_t hint = 0x10000000; low == nullptr && hint < 0x80000000; hint += 0x10000000)
        low = static_cast<std::uint8_t*>(VirtualAlloc(reinterpret_cast<void*>(hint), size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    require(high != nullptr && low != nullptr, "Cannot allocate the CLZERO buffers");
    struct ClzeroCase {
        std::optional<Codegen::Amd64OnlyMatch> Match;
        std::uint8_t* Buffer;
        std::uint64_t Junk;
        std::string Name;
    };
    const std::vector<ClzeroCase> cases = {
        {matcher.Match(plain.data(), plain.size()), high, 0, "clzero"},
        {matcher.Match(addressSize32.data(), addressSize32.size()), low, 0x5A5A5A5A00000000ull, "67h clzero"},
        {matcher.MatchSequence(pair, {}), high, 0, "clzero; clzero"}};
    const std::array<std::uint64_t, 4> flags = {0x202, 0x203, 0x246, 0xAC7};
    for (const auto& item : cases) {
        require(item.Match.has_value() && item.Match->Lowering == Codegen::Amd64OnlyLowering::Trampoline, "CLZERO was not lowered through a stub: " + item.Name);
        for (int image = 0; image < 64; ++image) {
            const auto address = reinterpret_cast<std::uint64_t>(item.Buffer) + 64 + random() % (size - 128);
            Bytes body = {0x48, 0xB8};
            for (std::size_t index = 0; index < 8; ++index) body.push_back(static_cast<std::uint8_t>((address | item.Junk) >> (index * 8)));
            body.insert(body.end(), {0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00});
            const auto prefix = body.size();
            body.insert(body.end(), item.Match->StubBody.begin(), item.Match->StubBody.end());
            State input{};
            for (auto& lane : input.Xmm) for (auto& byte : lane) byte = static_cast<std::uint8_t>(random());
            input.FlagsIn = flags[static_cast<std::size_t>(image) % flags.size()];
            State state = input;
            std::memset(item.Buffer, 0xA5, size);
            harness.Run(body, prefix + item.Match->ReturnBranchOffset, state);
            ++g_executions;
            const auto line = address & ~std::uint64_t{63};
            for (std::size_t index = 0; index < size; ++index) {
                const auto at = reinterpret_cast<std::uint64_t>(item.Buffer + index);
                require(item.Buffer[index] == (at >= line && at < line + 64 ? 0x00 : 0xA5), "Lowered CLZERO did not clear exactly the addressed line: " + item.Name);
            }
            requireEnvironment(input, state, 0, item.Name);
        }
    }
    VirtualFree(high, 0, MEM_RELEASE);
    VirtualFree(low, 0, MEM_RELEASE);
}

void cpuExecution() {
    const Harness harness;
    const auto matcher = Codegen::MakeAmd64OnlyInstructionMatcher();
    std::mt19937_64 random(0x5EED);
    const std::vector<Case> sites = {{false, 3, 3, 8, 40}, {true, 3, 3, 8, 8}, {true, 1, 0, 8, 0}, {true, 9, 4, 16, 16}, {true, 3, 4, 16, 16}};
    for (const auto& site : sites) executeCase(harness, *matcher, site, random, 64);
    const std::vector<std::pair<std::uint8_t, std::uint8_t>> pairs = {{3, 3}, {1, 0}, {9, 4}, {3, 4}, {15, 15}, {0, 15}, {8, 8}};
    for (std::uint8_t length = 1; length <= 64; ++length) {
        for (std::uint8_t index = 0; index + length <= 64; ++index) {
            for (const auto& [dst, src] : pairs) executeCase(harness, *matcher, {true, dst, src, length, index}, random, 2);
            for (const std::uint8_t dst : std::array<std::uint8_t, 4>{0, 3, 9, 15}) executeCase(harness, *matcher, {false, dst, dst, length, index}, random, 2);
            const auto& [dst, src] = pairs[(length * 65u + index) % pairs.size()];
            executeCase(harness, *matcher, {true, dst, src, length, index, true}, random, 2);
        }
    }
    for (const std::uint8_t opcode : {std::uint8_t{0xCB}, std::uint8_t{0xCC}, std::uint8_t{0xCD}})
        for (std::uint8_t dst = 0; dst < 16; ++dst)
            for (std::uint8_t src = 0; src < 16; ++src)
                executeSha256(harness, *matcher, {{opcode, dst, src}}, random, 2);
    for (int sequence = 0; sequence < 256; ++sequence) {
        std::vector<Sha256Step> steps(2 + random() % 2);
        for (auto& step : steps) step = {static_cast<std::uint8_t>(0xCB + random() % 3), static_cast<std::uint8_t>(random() % 16), static_cast<std::uint8_t>(random() % 16)};
        executeSha256(harness, *matcher, steps, random, 2);
    }
    clzeroExecution(harness, *matcher, random);
    sha1Execution(harness, *matcher, random);
}

Bytes elfFixture(const Bytes& text) {
    Bytes bytes(0x400);
    bytes[0] = 0x7F;
    bytes[1] = 'E';
    bytes[2] = 'L';
    bytes[3] = 'F';
    bytes[4] = 2;
    bytes[5] = 1;
    bytes[6] = 1;
    write<std::uint16_t>(bytes, 16, 3);
    write<std::uint16_t>(bytes, 18, 62);
    write<std::uint64_t>(bytes, 24, 0x1000);
    write<std::uint64_t>(bytes, 32, 64);
    write<std::uint16_t>(bytes, 54, 56);
    write<std::uint16_t>(bytes, 56, 2);
    std::fill(bytes.begin() + 0x200, bytes.begin() + 0x300, 0xCC);
    std::copy(text.begin(), text.end(), bytes.begin() + 0x200);
    return bytes;
}

std::vector<Domain::ProgramHeader> elfHeaders() {
    return {{1, 5, 0x200, 0x1000, 0, 0x100, 0x100, 0x1000}, {1, 6, 0x300, 0x2000, 0, 0x100, 0x100, 0x1000}};
}

void peBuilder() {
    const Bytes site = {0xF2, 0x0F, 0x78, 0xDB, 0x08, 0x08};
    Bytes text = {0xEB, 0x06};
    text.insert(text.end(), site.begin(), site.end());
    text.push_back(0xC3);
    const auto source = elfFixture(text);
    const auto headers = elfHeaders();
    const auto converted = Codegen::MakeAmd64OnlyConverter()->Convert(source, {headers[0]});
    require(converted.Trampolines.size() == 1, "PE fixture conversion did not produce one stub");
    const WindowsLoadImage image(converted.Bytes, headers);
    auto sections = image.BuildSections();
    auto nextRva = image.GetEndRva();
    const auto stubSectionRva = nextRva;
    WindowsTrampolineBuilder().Build(converted.Trampolines, image, sections, nextRva);
    require(sections.size() == 3 && sections.back().Name == ".amdstub" && sections.back().Rva == stubSectionRva && (sections.back().Characteristics & SectionExecute) != 0 && nextRva == AlignRva(stubSectionRva + sections.back().Data.size()), "Stub section was not appended");
    const auto& text0 = sections[0];
    const auto siteRva = LoadRva + 2;
    require(text0.Data[2] == 0xE9 && text0.Data[7] == 0x90 && text0.Data[8] == 0xC3, "PE site was not replaced by a jump");
    const auto stubRva = static_cast<std::uint32_t>(static_cast<std::int64_t>(siteRva + 5) + read<std::int32_t>(text0.Data, 3));
    require(stubRva == stubSectionRva && stubRva % 16 == 0, "PE jump does not land on the 16-aligned stub");
    const auto& body = sections.back().Data;
    require(body.size() == 32 && body[0] == 0x66 && body[9] == 0xE9 && body[16] == 0x00 && body[17] == 0x00 && body[18] == 0x02, "PE stub body is wrong");
    require(static_cast<std::int64_t>(stubRva + 9 + 5) + read<std::int32_t>(body, 10) == siteRva + 6, "PE stub does not return past the site");
    require(read<std::int32_t>(body, 5) == 7 && (stubRva + 16) % 16 == 0, "PE mask constant is not 16-aligned");
    std::array<PeDirectory, 16> directories{};
    const auto file = WindowsPeWriter().Write(sections, LoadRva, directories);
    require(file.size() > 0x400, "PE writer rejected the stub section");
    auto altered = converted.Bytes;
    altered[0x205] = 0xDC;
    requireFailure([&] {
        const WindowsLoadImage alteredImage(altered, headers);
        auto alteredSections = alteredImage.BuildSections();
        auto rva = alteredImage.GetEndRva();
        WindowsTrampolineBuilder().Build(converted.Trampolines, alteredImage, alteredSections, rva);
    }, "Changed PE site bytes were accepted");
    requireFailure([&] {
        auto unmapped = converted.Trampolines;
        unmapped[0].Address = 0x5000;
        auto freshSections = image.BuildSections();
        auto rva = image.GetEndRva();
        WindowsTrampolineBuilder().Build(unmapped, image, freshSections, rva);
    }, "Unmapped PE site was accepted");
    auto untouched = image.BuildSections();
    auto untouchedRva = image.GetEndRva();
    WindowsTrampolineBuilder().Build({}, image, untouched, untouchedRva);
    require(untouched.size() == 2 && untouchedRva == image.GetEndRva(), "Empty site list changed the image");
}

}

int main() {
    try {
        cpuExecution();
        peBuilder();
        std::cout << "AMD64-only Windows tests passed (" << g_executions << " lowered sequences executed)\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
