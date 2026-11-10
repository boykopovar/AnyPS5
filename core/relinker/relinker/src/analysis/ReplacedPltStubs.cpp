#include <relinker/analysis/ReplacedPltStubs.hpp>
#include <codegen/CodegenException.hpp>
#include <codegen/x86/X64InstructionDecoder.hpp>
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <sstream>

namespace Relinker {

static constexpr std::size_t PLT_ENTRY_SIZE = 16;
static constexpr std::size_t PLT_JUMP_SIZE = 6;
static constexpr std::size_t RELA_ENTRY_SIZE = 24;
static constexpr VirtualAddress GOT_RESERVED_SIZE = 24;

static VirtualAddress RelativeTarget(std::span<const std::uint8_t> segment, const VirtualAddress segmentVAddr, const std::size_t at, const std::size_t length) {
    std::int32_t displacement = 0;
    std::memcpy(&displacement, segment.data() + at + length - sizeof(displacement), sizeof(displacement));
    return segmentVAddr + at + length + static_cast<VirtualAddress>(static_cast<std::int64_t>(displacement));
}

static std::optional<std::int32_t> Displacement(const VirtualAddress target, const VirtualAddress next) {
    const auto distance = static_cast<std::int64_t>(target - next);
    if (distance < std::numeric_limits<std::int32_t>::min() || distance > std::numeric_limits<std::int32_t>::max())
        return std::nullopt;
    return static_cast<std::int32_t>(distance);
}

static std::optional<std::size_t> FindPltHeader(std::span<const std::uint8_t> segment, const VirtualAddress segmentVAddr, const VirtualAddress pltGot) {
    if (segment.size() < PLT_ENTRY_SIZE)
        return std::nullopt;
    for (std::size_t at = segment.size() - PLT_ENTRY_SIZE + 1; at-- > 0;)
        if (segment[at] == 0xff && segment[at + 1] == 0x35 && segment[at + 6] == 0xff && segment[at + 7] == 0x25
            && RelativeTarget(segment, segmentVAddr, at, PLT_JUMP_SIZE) == pltGot + 8
            && RelativeTarget(segment, segmentVAddr, at + PLT_JUMP_SIZE, PLT_JUMP_SIZE) == pltGot + 16)
            return at;
    return std::nullopt;
}

std::vector<ReplacedPltStub> FindReplacedPltStubs(std::span<const std::uint8_t> segment, const VirtualAddress segmentVAddr, const VirtualAddress pltGot, const ByteCount pltRelocationsSize) {
    std::vector<ReplacedPltStub> replaced;
    const ByteCount count = pltRelocationsSize / RELA_ENTRY_SIZE;
    if (count == 0 || count > std::numeric_limits<std::uint32_t>::max())
        return replaced;
    const auto header = FindPltHeader(segment, segmentVAddr, pltGot);
    if (!header || count > (segment.size() - *header) / PLT_ENTRY_SIZE - 1)
        return replaced;
    bool linkedStubSeen = false;
    for (ByteCount index = 0; index < count; ++index) {
        const std::size_t at = *header + PLT_ENTRY_SIZE * static_cast<std::size_t>(index + 1);
        const VirtualAddress address = segmentVAddr + at;
        const VirtualAddress gotEntry = pltGot + GOT_RESERVED_SIZE + 8 * index;
        if (segment[at] == 0xff && segment[at + 1] == 0x25 && RelativeTarget(segment, segmentVAddr, at, PLT_JUMP_SIZE) == gotEntry) {
            linkedStubSeen = true;
            continue;
        }
        const auto gotDisplacement = Displacement(gotEntry, address + PLT_JUMP_SIZE);
        const auto headerDisplacement = Displacement(segmentVAddr + *header, address + PLT_ENTRY_SIZE);
        if (!gotDisplacement || !headerDisplacement)
            return {};
        std::array<std::uint8_t, PLT_ENTRY_SIZE> linked{0xff, 0x25, 0, 0, 0, 0, 0x68, 0, 0, 0, 0, 0xe9};
        const auto slot = static_cast<std::uint32_t>(index);
        std::memcpy(linked.data() + 2, &*gotDisplacement, sizeof(std::int32_t));
        std::memcpy(linked.data() + 7, &slot, sizeof(slot));
        std::memcpy(linked.data() + 12, &*headerDisplacement, sizeof(std::int32_t));
        const auto found = segment.subspan(at, PLT_ENTRY_SIZE);
        const std::size_t length = std::equal(found.begin() + PLT_JUMP_SIZE, found.end(), linked.begin() + PLT_JUMP_SIZE) ? PLT_JUMP_SIZE : PLT_ENTRY_SIZE;
        ReplacedPltStub stub{address, gotEntry, {found.begin(), found.begin() + static_cast<std::ptrdiff_t>(length)}, {linked.begin(), linked.begin() + static_cast<std::ptrdiff_t>(length)}, std::nullopt};
        if (found[0] == 0xe9)
            stub.JumpTarget = RelativeTarget(segment, segmentVAddr, at, 5);
        replaced.push_back(std::move(stub));
    }
    if (!linkedStubSeen)
        return {};
    return replaced;
}

std::string DescribeReplacedPltStub(const ReplacedPltStub& stub, const std::string_view target, const std::string_view import) {
    const auto hex = [](const std::vector<std::uint8_t>& bytes) {
        static constexpr char digits[] = "0123456789abcdef";
        std::string text;
        for (const auto byte : bytes) {
            text.push_back(digits[byte >> 4]);
            text.push_back(digits[byte & 15]);
        }
        return text;
    };
    std::ostringstream message;
    message << "Replaced PLT stub:";
    if (!target.empty())
        message << ' ' << target;
    message << " va=0x" << std::hex << stub.Address << ' ' << hex(stub.Linked) << " -> " << hex(stub.Found);
    if (stub.JumpTarget)
        message << " (jump to 0x" << *stub.JumpTarget << ')';
    if (!import.empty())
        message << "; import " << import << " is not called through it";
    message << "; a --patches entry with expected = \"" << hex(stub.Found) << "\" and value = \"" << hex(stub.Linked) << "\" restores the linked stub\n";
    return message.str();
}

std::string DescribeReplacedPltStubReaching(const std::vector<ReplacedPltStub>& stubs, std::span<const std::uint8_t> segment, const VirtualAddress segmentVAddr, const VirtualAddress address) {
    const Codegen::X64InstructionDecoder decoder;
    for (const auto& stub : stubs) {
        if (!stub.JumpTarget || *stub.JumpTarget < segmentVAddr || *stub.JumpTarget > address || address - segmentVAddr >= segment.size())
            continue;
        std::size_t at = static_cast<std::size_t>(*stub.JumpTarget - segmentVAddr);
        const std::size_t end = static_cast<std::size_t>(address - segmentVAddr);
        try {
            while (at < end) {
                const auto instruction = decoder.DecodeInstruction(segment.data() + at, segment.size() - at);
                if (instruction.FlowKind == Codegen::ControlFlowKind::UnconditionalJump || instruction.FlowKind == Codegen::ControlFlowKind::IndirectJump
                    || instruction.FlowKind == Codegen::ControlFlowKind::Return || instruction.FlowKind == Codegen::ControlFlowKind::Trap)
                    break;
                at += instruction.Length;
            }
        } catch (const Codegen::CodegenException&) {
            continue;
        }
        if (at != end)
            continue;
        std::ostringstream message;
        message << "; it is in the code at 0x" << std::hex << *stub.JumpTarget << " that the replaced PLT stub at 0x" << stub.Address << " jumps to";
        return message.str();
    }
    return {};
}

}
