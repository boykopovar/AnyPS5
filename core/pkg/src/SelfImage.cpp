#include <pkg/SelfImage.hpp>
#include <pkg/ByteOrder.hpp>
#include <pkg/PackageError.hpp>
#include <algorithm>
#include <cstring>

namespace Pkg {

namespace {

constexpr std::uint64_t EncryptedSegment = 0x2;
constexpr std::uint64_t CompressedSegment = 0x8;
constexpr std::uint64_t DataSegment = 0x800;
constexpr std::uint32_t VersionSegment = 0x6fffff01;
constexpr std::uint64_t ProgramHeaderSize = 0x38;
constexpr std::uint64_t MaximumElfSize = 0x100000000;

struct Segment {
    std::uint32_t Type;
    std::uint64_t Offset;
    std::uint64_t FileSize;
};

bool fits(const std::uint64_t offset, const std::uint64_t size, const std::uint64_t limit) {
    return offset <= limit && size <= limit - offset;
}

}

bool IsSelfImage(const std::uint8_t* bytes, const std::size_t size) {
    return size >= 4 && bytes[0] == 0x54 && bytes[1] == 0x14 && bytes[2] == 0xf5 && bytes[3] == 0xee;
}

std::vector<std::uint8_t> UnwrapSelfImage(const std::vector<std::uint8_t>& self) {
    if (self.size() < 0x20 || !IsSelfImage(self.data(), self.size())) throw PackageError("Not a PS5 SELF image");
    const std::uint64_t entryCount = LoadLittle16(self.data() + 0x18);
    const std::uint64_t elfOffset = 0x20 + entryCount * 0x20;
    if (!fits(elfOffset, 0x40, self.size())) throw PackageError("SELF image is truncated");
    const std::uint8_t* elf = self.data() + elfOffset;
    if (elf[0] != 0x7f || elf[1] != 'E' || elf[2] != 'L' || elf[3] != 'F' || elf[4] != 2 || elf[5] != 1) throw PackageError("SELF image does not embed a 64-bit ELF header");
    const std::uint64_t programHeaders = LoadLittle64(elf + 0x20);
    const std::uint64_t programHeaderCount = LoadLittle16(elf + 0x38);
    if (LoadLittle16(elf + 0x36) != ProgramHeaderSize || !fits(programHeaders, programHeaderCount * ProgramHeaderSize, self.size() - elfOffset))
        throw PackageError("SELF image has invalid ELF program headers");
    const std::uint64_t headerSize = programHeaders + programHeaderCount * ProgramHeaderSize;

    std::vector<Segment> segments;
    std::uint64_t elfSize = headerSize;
    for (std::uint64_t index = 0; index < programHeaderCount; ++index) {
        const std::uint8_t* header = elf + programHeaders + index * ProgramHeaderSize;
        Segment segment{LoadLittle32(header), LoadLittle64(header + 8), LoadLittle64(header + 0x20)};
        if (!fits(segment.Offset, segment.FileSize, MaximumElfSize)) throw PackageError("SELF image ELF segment is too large", segment.Offset);
        elfSize = std::max(elfSize, segment.Offset + segment.FileSize);
        segments.push_back(segment);
    }

    std::vector<std::uint8_t> output(static_cast<std::size_t>(elfSize));
    std::memcpy(output.data(), elf, static_cast<std::size_t>(headerSize));
    std::vector<bool> filled(segments.size());
    std::uint64_t storedEnd = elfOffset + headerSize;
    for (std::uint64_t index = 0; index < entryCount; ++index) {
        const std::uint8_t* entry = self.data() + 0x20 + index * 0x20;
        const std::uint64_t properties = LoadLittle64(entry);
        const std::uint64_t offset = LoadLittle64(entry + 8);
        const std::uint64_t size = LoadLittle64(entry + 0x10);
        if ((properties & (EncryptedSegment | CompressedSegment)) != 0) throw PackageError("Encrypted or compressed SELF segments are not supported");
        if (!fits(offset, size, self.size())) throw PackageError("SELF segment lies outside the image", offset);
        storedEnd = std::max(storedEnd, offset + size);
        if ((properties & DataSegment) == 0) continue;
        const std::uint64_t segmentIndex = (properties >> 20) & 0xfff;
        if (segmentIndex >= segments.size() || segments[segmentIndex].FileSize != size) throw PackageError("SELF segment does not match its ELF program header", offset);
        std::memcpy(output.data() + segments[segmentIndex].Offset, self.data() + offset, static_cast<std::size_t>(size));
        filled[segmentIndex] = true;
    }
    for (std::size_t index = 0; index < segments.size(); ++index) {
        const auto& segment = segments[index];
        if (segment.Type != VersionSegment || segment.FileSize == 0 || filled[index]) continue;
        if (self.size() - storedEnd < segment.FileSize) throw PackageError("SELF image is missing its version segment");
        std::memcpy(output.data() + segment.Offset, self.data() + self.size() - segment.FileSize, static_cast<std::size_t>(segment.FileSize));
    }
    return output;
}

}
