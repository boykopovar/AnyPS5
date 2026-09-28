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
#include <random>
#include <stdexcept>
#include <string>

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
    instruction.destination = operands.Destination;
    instruction.source = operands.Source;
    instruction.length = static_cast<std::uint8_t>(operands.Length == 64 ? 0 : operands.Length);
    instruction.index = operands.Index;
    sse4a::Execute(instruction, context);
    return static_cast<std::uint64_t>(context.FltSave.XmmRegisters[operands.Destination].Low);
}

Bytes encode(const bool insertq, const std::uint8_t dst, const std::uint8_t src, const std::uint8_t length, const std::uint8_t index) {
    Bytes bytes = {static_cast<std::uint8_t>(insertq ? 0xF2 : 0x66)};
    const std::uint8_t reg = insertq ? dst : 0;
    const std::uint8_t rm = insertq ? src : dst;
    const auto rex = static_cast<std::uint8_t>(0x40 | (reg >= 8 ? 4 : 0) | (rm >= 8 ? 1 : 0));
    if (rex != 0x40) bytes.push_back(rex);
    bytes.insert(bytes.end(), {0x0F, 0x78, static_cast<std::uint8_t>(0xC0 | ((reg & 7) << 3) | (rm & 7)), static_cast<std::uint8_t>(length == 64 ? 0 : length), index});
    return bytes;
}

struct Case {
    bool Insertq;
    std::uint8_t Destination;
    std::uint8_t Source;
    std::uint8_t Length;
    std::uint8_t Index;
};

std::string describe(const Case& item) {
    return std::string(item.Insertq ? "insertq xmm" : "extrq xmm") + std::to_string(item.Destination) + (item.Insertq ? ", xmm" + std::to_string(item.Source) : "") + ", " + std::to_string(item.Length) + ", " + std::to_string(item.Index);
}

std::size_t g_executions = 0;

void executeCase(const Harness& harness, const Codegen::IAmd64OnlyInstructionMatcher& matcher, const Case& item, std::mt19937_64& random, const int images) {
    const auto site = encode(item.Insertq, item.Destination, item.Source, item.Length, item.Index);
    const auto match = matcher.Match(site.data(), site.size());
    require(match.has_value() && match->Lowering != Codegen::Amd64OnlyLowering::Unsupported, "Immediate-form SSE4a instruction was not lowered: " + describe(item));
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
    const std::array<std::uint64_t, 4> flags = {0x202, 0x203, 0x246, 0xAC7};
    for (int image = 0; image < images; ++image) {
        State input{};
        for (auto& lane : input.Xmm) for (auto& byte : lane) byte = static_cast<std::uint8_t>(random());
        input.FlagsIn = flags[static_cast<std::size_t>(image) % flags.size()];
        State state = input;
        harness.Run(body, returnBranchOffset, state);
        ++g_executions;
        const auto expected = referenceLow(operands, input);
        require(expected == libcReferenceLow(operands, input), "Transcribed reference disagrees with the libc emulation: " + describe(item));
        require(low(state.Xmm[operands.Destination]) == expected, "Lowered sequence computed the wrong field: " + describe(item));
        for (unsigned reg = 0; reg < 16; ++reg) {
            if (reg == operands.Destination) continue;
            require(std::memcmp(state.Xmm[reg], input.Xmm[reg], 16) == 0, "Lowered sequence clobbered xmm" + std::to_string(reg) + ": " + describe(item));
        }
        require(((state.FlagsOut ^ input.FlagsIn) & kStatusFlags) == 0, "Lowered sequence changed RFLAGS: " + describe(item));
        require(state.RspAfter == state.RspBefore, "Lowered sequence did not restore rsp: " + describe(item));
        for (int slot = 0; slot < 16; ++slot) {
            std::uint64_t value;
            std::memcpy(&value, state.RedZone + slot * 8, 8);
            require(value == kCanary, "Lowered sequence wrote into the red zone: " + describe(item));
        }
    }
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
        }
    }
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
