#include <relinker/analysis/CodeInstructionCollector.hpp>
#include <io/BufferUtils.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

void Require(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

Domain::ProgramHeader MakeLoadHeader(const Domain::VirtualAddress mappedAddress, const Domain::FileByteOffset offset, const Domain::ByteCount size, const std::uint32_t flags = 5) {
    Domain::ProgramHeader header{};
    header.Type = 1;
    header.Flags = flags;
    header.Offset = offset;
    header.MappedAddress = mappedAddress;
    header.PhysicalAddress = mappedAddress;
    header.FileSize = size;
    header.MemorySize = size;
    header.Alignment = 0x1000;
    return header;
}

Domain::ProgramHeader MakeEhFrameHeader(const Domain::VirtualAddress mappedAddress, const Domain::FileByteOffset offset, const Domain::ByteCount size) {
    Domain::ProgramHeader header{};
    header.Type = 0x6474e550;
    header.Flags = 4;
    header.Offset = offset;
    header.MappedAddress = mappedAddress;
    header.PhysicalAddress = mappedAddress;
    header.FileSize = size;
    header.MemorySize = size;
    header.Alignment = 4;
    return header;
}

void WriteBytes(std::vector<std::uint8_t>& image, const std::size_t offset, const std::vector<std::uint8_t>& data) {
    Require(offset <= image.size() && data.size() <= image.size() - offset, "Test fixture write out of range");
    std::copy(data.begin(), data.end(), image.begin() + static_cast<std::ptrdiff_t>(offset));
}

void WriteDisp32(std::vector<std::uint8_t>& image, const std::size_t dispOffset, const Domain::VirtualAddress nextAddress, const Domain::VirtualAddress targetAddress) {
    const auto delta = static_cast<std::int64_t>(targetAddress - nextAddress);
    Require(delta >= std::numeric_limits<std::int32_t>::min() && delta <= std::numeric_limits<std::int32_t>::max(), "Test displacement out of signed 32-bit range");
    Io::WriteU32(image, dispOffset, static_cast<std::uint32_t>(static_cast<std::int32_t>(delta)));
}

bool ContainsAddress(const std::vector<Domain::VirtualAddress>& addresses, const Domain::VirtualAddress target) {
    return std::binary_search(addresses.begin(), addresses.end(), target);
}

void BuildEhFrame(std::vector<std::uint8_t>& image, const std::size_t hdrOffset, const Domain::VirtualAddress hdrVaddr, const std::size_t frameOffset, const Domain::VirtualAddress frameVaddr, const std::vector<std::pair<Domain::VirtualAddress, Domain::ByteCount>>& functions) {
    constexpr std::size_t cieSize = 20;
    constexpr std::size_t fdeSize = 28;
    const std::array<std::uint8_t, cieSize> cie{
        16, 0, 0, 0,
        0, 0, 0, 0,
        1,
        'z', 'R', 0,
        1,
        0x78,
        16,
        1,
        0x04,
        0, 0, 0
    };
    Require(frameOffset + cieSize + functions.size() * fdeSize <= image.size(), "EH frame fixture exceeds image");
    std::copy(cie.begin(), cie.end(), image.begin() + static_cast<std::ptrdiff_t>(frameOffset));

    const std::size_t hdrSize = 16 + functions.size() * 16;
    Require(hdrOffset + hdrSize <= image.size(), "EH frame header fixture exceeds image");
    image[hdrOffset] = 1;
    image[hdrOffset + 1] = 0x04;
    image[hdrOffset + 2] = 0x03;
    image[hdrOffset + 3] = 0x04;
    Io::WriteU64(image, hdrOffset + 4, frameVaddr);
    Io::WriteU32(image, hdrOffset + 12, static_cast<std::uint32_t>(functions.size()));

    for (std::size_t index = 0; index < functions.size(); ++index) {
        const auto [begin, size] = functions[index];
        const std::size_t fdeFileOffset = frameOffset + cieSize + index * fdeSize;
        const Domain::VirtualAddress fdeVaddr = frameVaddr + cieSize + index * fdeSize;
        Io::WriteU32(image, fdeFileOffset, static_cast<std::uint32_t>(fdeSize - 4));
        Io::WriteU32(image, fdeFileOffset + 4, static_cast<std::uint32_t>(fdeVaddr + 4 - frameVaddr));
        Io::WriteU64(image, fdeFileOffset + 8, begin);
        Io::WriteU64(image, fdeFileOffset + 16, size);
        image[fdeFileOffset + 24] = 0;
        Io::WriteU64(image, hdrOffset + 16 + index * 16, begin);
        Io::WriteU64(image, hdrOffset + 16 + index * 16 + 8, fdeVaddr);
    }
    (void)hdrVaddr;
}

void CheckAddressSize32WrapsHighInstructionToLowCallback() {
    std::vector<std::uint8_t> image(0x400, 0xcc);
    std::fill_n(image.begin(), 64, std::uint8_t{0});
    constexpr Domain::VirtualAddress highCaller = 0x100001000ull;
    constexpr Domain::VirtualAddress lowCallback = 0x00002000ull;
    constexpr Domain::VirtualAddress highDecoy = 0x100002000ull;
    Io::WriteU64(image, 24, highCaller);

    WriteBytes(image, 0x100, {0x67, 0x48, 0x8d, 0x05, 0, 0, 0, 0, 0xc3});
    WriteDisp32(image, 0x104, highCaller + 8, highDecoy);
    WriteBytes(image, 0x200, {0xb8, 0x2a, 0x00, 0x00, 0x00, 0xc3});
    WriteBytes(image, 0x300, {0x90, 0xc3});

    const std::vector<Domain::ProgramHeader> headers{
        MakeLoadHeader(lowCallback, 0x200, 0x20),
        MakeLoadHeader(highCaller, 0x100, 0x20),
        MakeLoadHeader(highDecoy, 0x300, 0x20),
    };

    std::set<Domain::VirtualAddress> entryRoots;
    const auto instructions = Relinker::CodeInstructionCollector().Collect(image, headers, {}, &entryRoots);
    const std::vector<Domain::VirtualAddress> expected{lowCallback, lowCallback + 5, highCaller, highCaller + 8};
    Require(instructions == expected, "67h LEA failed to wrap effective address to 32-bit low callback");
    Require(entryRoots.contains(lowCallback), "67h LEA low callback root missing from entryRoots");
    Require(!entryRoots.contains(highDecoy), "67h LEA falsely added unmasked 64-bit decoy to entryRoots");
}

void CheckOrdinary64BitLeaPreservesHighBits() {
    std::vector<std::uint8_t> image(0x400, 0xcc);
    std::fill_n(image.begin(), 64, std::uint8_t{0});
    constexpr Domain::VirtualAddress highCaller = 0x100001000ull;
    constexpr Domain::VirtualAddress lowDecoy = 0x00002000ull;
    constexpr Domain::VirtualAddress highCallback = 0x100002000ull;
    Io::WriteU64(image, 24, highCaller);

    WriteBytes(image, 0x100, {0x48, 0x8d, 0x05, 0, 0, 0, 0, 0xc3});
    WriteDisp32(image, 0x103, highCaller + 7, highCallback);
    WriteBytes(image, 0x200, {0x90, 0xc3});
    WriteBytes(image, 0x300, {0xb8, 0x07, 0x00, 0x00, 0x00, 0xc3});

    const std::vector<Domain::ProgramHeader> headers{
        MakeLoadHeader(lowDecoy, 0x200, 0x20),
        MakeLoadHeader(highCaller, 0x100, 0x20),
        MakeLoadHeader(highCallback, 0x300, 0x20),
    };

    std::set<Domain::VirtualAddress> entryRoots;
    const auto instructions = Relinker::CodeInstructionCollector().Collect(image, headers, {}, &entryRoots);
    const std::vector<Domain::VirtualAddress> expected{highCaller, highCaller + 7, highCallback, highCallback + 5};
    Require(instructions == expected, "64-bit LEA failed to preserve upper 32 bits");
    Require(entryRoots.contains(highCallback), "64-bit LEA high callback root missing from entryRoots");
    Require(!entryRoots.contains(lowDecoy), "64-bit LEA falsely wrapped to low decoy");
}

void CheckNegativeDisplacementAnd32BitWrap() {
    {
        std::vector<std::uint8_t> image(0x400, 0xcc);
        std::fill_n(image.begin(), 64, std::uint8_t{0});
        constexpr Domain::VirtualAddress highCaller = 0x100003000ull;
        constexpr Domain::VirtualAddress lowCallback = 0x00002000ull;
        constexpr Domain::VirtualAddress highDecoy = 0x100002000ull;
        Io::WriteU64(image, 24, highCaller);

        WriteBytes(image, 0x100, {0x67, 0x48, 0x8d, 0x05, 0, 0, 0, 0, 0xc3});
        WriteDisp32(image, 0x104, highCaller + 8, highDecoy);
        WriteBytes(image, 0x200, {0xb8, 0x0b, 0x00, 0x00, 0x00, 0xc3});
        WriteBytes(image, 0x300, {0x90, 0xc3});

        const std::vector<Domain::ProgramHeader> headers{
            MakeLoadHeader(lowCallback, 0x200, 0x20),
            MakeLoadHeader(highDecoy, 0x300, 0x20),
            MakeLoadHeader(highCaller, 0x100, 0x20),
        };

        const auto instructions = Relinker::CodeInstructionCollector().Collect(image, headers);
        const std::vector<Domain::VirtualAddress> expected{lowCallback, lowCallback + 5, highCaller, highCaller + 8};
        Require(instructions == expected, "67h LEA with negative displacement failed to wrap to low callback");
    }

    {
        std::vector<std::uint8_t> image(0x400, 0xcc);
        std::fill_n(image.begin(), 64, std::uint8_t{0});
        constexpr Domain::VirtualAddress nearBoundaryCaller = 0x0fffffff0ull;
        constexpr Domain::VirtualAddress wrappedLowCallback = 0x00002000ull;
        constexpr Domain::VirtualAddress unmaskedHighDecoy = 0x100002000ull;
        Io::WriteU64(image, 24, nearBoundaryCaller);

        WriteBytes(image, 0x100, {0x67, 0x48, 0x8d, 0x05, 0, 0, 0, 0, 0xc3});
        WriteDisp32(image, 0x104, nearBoundaryCaller + 8, unmaskedHighDecoy);
        WriteBytes(image, 0x200, {0xb8, 0x0d, 0x00, 0x00, 0x00, 0xc3});
        WriteBytes(image, 0x300, {0x90, 0xc3});

        const std::vector<Domain::ProgramHeader> headers{
            MakeLoadHeader(wrappedLowCallback, 0x200, 0x20),
            MakeLoadHeader(nearBoundaryCaller, 0x100, 0x20),
            MakeLoadHeader(unmaskedHighDecoy, 0x300, 0x20),
        };

        const auto instructions = Relinker::CodeInstructionCollector().Collect(image, headers);
        const std::vector<Domain::VirtualAddress> expected{wrappedLowCallback, wrappedLowCallback + 5, nearBoundaryCaller, nearBoundaryCaller + 8};
        Require(instructions == expected, "67h LEA crossing 4 GiB upward failed to wrap modulo 2^32");
    }

    {
        std::vector<std::uint8_t> image(0x400, 0xcc);
        std::fill_n(image.begin(), 64, std::uint8_t{0});
        constexpr Domain::VirtualAddress lowCaller = 0x00002000ull;
        constexpr Domain::VirtualAddress zeroExtendedCallback = 0x00000000fffff000ull;
        constexpr Domain::VirtualAddress signExtendedDecoy = 0xfffffffffffff000ull;
        Io::WriteU64(image, 24, lowCaller);

        WriteBytes(image, 0x100, {0x67, 0x48, 0x8d, 0x05, 0, 0, 0, 0, 0xc3});
        WriteDisp32(image, 0x104, lowCaller + 8, signExtendedDecoy);
        WriteBytes(image, 0x200, {0xb8, 0x13, 0x00, 0x00, 0x00, 0xc3});
        WriteBytes(image, 0x300, {0x90, 0xc3});

        const std::vector<Domain::ProgramHeader> headers{
            MakeLoadHeader(lowCaller, 0x100, 0x20),
            MakeLoadHeader(zeroExtendedCallback, 0x200, 0x20),
            MakeLoadHeader(signExtendedDecoy, 0x300, 0x20),
        };

        const auto instructions = Relinker::CodeInstructionCollector().Collect(image, headers);
        const std::vector<Domain::VirtualAddress> expected{lowCaller, lowCaller + 8, zeroExtendedCallback, zeroExtendedCallback + 5};
        Require(instructions == expected, "67h LEA wrapping below 0 did not zero-extend 32-bit effective address");
    }
}

void CheckEhFrameAndSceBranchesHonorAddressSize32() {
    constexpr Domain::VirtualAddress highCaller = 0x100001000ull;
    constexpr Domain::VirtualAddress lowTarget = 0x00002000ull;
    constexpr Domain::VirtualAddress highDecoy = 0x100002000ull;
    constexpr Domain::VirtualAddress unwindVaddr = 0x100004000ull;

    {
        std::vector<std::uint8_t> image(0x600, 0xcc);
        std::fill_n(image.begin(), 64, std::uint8_t{0});
        Io::WriteU64(image, 24, highCaller);
        WriteBytes(image, 0x100, {0x67, 0x4c, 0x8d, 0x3d, 0, 0, 0, 0, 0x41, 0xff, 0xd7, 0xc3});
        WriteDisp32(image, 0x104, highCaller + 8, highDecoy);
        WriteBytes(image, 0x200, {0xb8, 0x2a, 0x00, 0x00, 0x00, 0xc3});
        WriteBytes(image, 0x300, {0x90, 0xc3});
        BuildEhFrame(image, 0x400, unwindVaddr, 0x480, unwindVaddr + 0x80, {{highCaller, 12}});

        const std::vector<Domain::ProgramHeader> headers{
            MakeLoadHeader(lowTarget, 0x200, 0x20),
            MakeLoadHeader(highCaller, 0x100, 0x20),
            MakeLoadHeader(highDecoy, 0x300, 0x20),
            MakeLoadHeader(unwindVaddr, 0x400, 0x100, 4),
            MakeEhFrameHeader(unwindVaddr, 0x400, 32),
        };

        std::set<Domain::VirtualAddress> entryRoots;
        const auto instructions = Relinker::CodeInstructionCollector().Collect(image, headers, {}, &entryRoots);
        const std::vector<Domain::VirtualAddress> expected{lowTarget, lowTarget + 5, highCaller, highCaller + 8, highCaller + 11};
        Require(instructions == expected, "67h LEA in unwind function failed to follow wrapped callback");
        Require(entryRoots.contains(lowTarget) && !entryRoots.contains(highDecoy), "67h LEA in unwind function recorded wrong root");
    }

    {
        std::vector<std::uint8_t> image(0x600, 0xcc);
        std::fill_n(image.begin(), 64, std::uint8_t{0});
        Io::WriteU64(image, 24, highCaller);
        WriteBytes(image, 0x100, {0x67, 0x48, 0x8d, 0x05, 0, 0, 0, 0, 0x8b, 0x00, 0xc3});
        WriteDisp32(image, 0x104, highCaller + 8, highDecoy);
        WriteBytes(image, 0x200, {0x0f, 0x78, 0x00, 0x11});
        WriteBytes(image, 0x300, {0x90, 0xc3});
        BuildEhFrame(image, 0x400, unwindVaddr, 0x480, unwindVaddr + 0x80, {{highCaller, 11}});

        const std::vector<Domain::ProgramHeader> headers{
            MakeLoadHeader(lowTarget, 0x200, 0x20),
            MakeLoadHeader(highCaller, 0x100, 0x20),
            MakeLoadHeader(highDecoy, 0x300, 0x20),
            MakeLoadHeader(unwindVaddr, 0x400, 0x100, 4),
            MakeEhFrameHeader(unwindVaddr, 0x400, 32),
        };

        std::set<Domain::VirtualAddress> entryRoots;
        const auto instructions = Relinker::CodeInstructionCollector().Collect(image, headers, {}, &entryRoots);
        const std::vector<Domain::VirtualAddress> expected{highCaller, highCaller + 8, highCaller + 10};
        Require(instructions == expected, "67h LEA data target was wrongly decoded as code");
        Require(entryRoots.contains(lowTarget) && !entryRoots.contains(highDecoy), "67h LEA data target failed to protect wrapped address");
    }

    {
        std::vector<std::uint8_t> image(0x600, 0xcc);
        std::fill_n(image.begin(), 64, std::uint8_t{0});
        Io::WriteU64(image, 24, highCaller);
        WriteBytes(image, 0x100, {0x67, 0x48, 0x8d, 0x05, 0, 0, 0, 0, 0x8b, 0x00, 0xc3});
        WriteDisp32(image, 0x104, highCaller + 8, highDecoy + 3);
        WriteBytes(image, 0x200, {0xeb, 0x01, 0x90, 0xc3});
        WriteBytes(image, 0x300, {0x90, 0xc3});
        BuildEhFrame(image, 0x400, unwindVaddr, 0x480, unwindVaddr + 0x80, {{lowTarget, 4}, {highCaller, 11}});

        const std::vector<Domain::ProgramHeader> headers{
            MakeLoadHeader(lowTarget, 0x200, 0x20),
            MakeLoadHeader(highCaller, 0x100, 0x20),
            MakeLoadHeader(highDecoy, 0x300, 0x20),
            MakeLoadHeader(unwindVaddr, 0x400, 0x100, 4),
            MakeEhFrameHeader(unwindVaddr, 0x400, 48),
        };

        std::string messages;
        std::set<Domain::VirtualAddress> entryRoots;
        const auto instructions = Relinker::CodeInstructionCollector().Collect(image, headers, {}, &entryRoots, [&](std::string_view text) { messages.append(text); });
        Require(ContainsAddress(instructions, lowTarget + 3), "Low unwind function instruction missing");
        Require(entryRoots.contains(lowTarget + 3), "67h LEA into known low function failed to record code root");
        Require(messages.empty(), "67h LEA into known low function was misclassified as data");
    }

    {
        std::vector<std::uint8_t> image(0x600, 0xcc);
        std::fill_n(image.begin(), 64, std::uint8_t{0});
        Io::WriteU64(image, 24, highCaller);
        WriteBytes(image, 0x100, {0x67, 0x48, 0x8d, 0x05, 0, 0, 0, 0, 0xc3});
        WriteDisp32(image, 0x104, highCaller + 8, highDecoy);
        WriteBytes(image, 0x200, {0x64, 0x48, 0x8b, 0x04, 0x25, 0x28, 0x00, 0x00, 0x00, 0xc3});
        WriteBytes(image, 0x300, {0x90, 0xc3});
        BuildEhFrame(image, 0x400, unwindVaddr, 0x480, unwindVaddr + 0x80, {{highCaller, 9}});

        Domain::ProgramHeader sceHeader{};
        sceHeader.Type = 0x61000000;
        sceHeader.Flags = 4;
        sceHeader.Offset = 0x580;

        const std::vector<Domain::ProgramHeader> headers{
            MakeLoadHeader(lowTarget, 0x200, 0x20),
            MakeLoadHeader(highCaller, 0x100, 0x20),
            MakeLoadHeader(highDecoy, 0x300, 0x20),
            MakeLoadHeader(unwindVaddr, 0x400, 0x100, 4),
            MakeEhFrameHeader(unwindVaddr, 0x400, 32),
            sceHeader,
        };

        std::set<Domain::VirtualAddress> entryRoots;
        const auto instructions = Relinker::CodeInstructionCollector().Collect(image, headers, {}, &entryRoots);
        const std::vector<Domain::VirtualAddress> expected{lowTarget, lowTarget + 9, highCaller, highCaller + 8};
        Require(instructions == expected, "67h LEA in SCE image failed to follow wrapped gap TLS stub");
        Require(entryRoots.contains(lowTarget) && !entryRoots.contains(highDecoy), "67h LEA in SCE image recorded wrong root");
    }
}

}

int main() {
    try {
        CheckAddressSize32WrapsHighInstructionToLowCallback();
        CheckOrdinary64BitLeaPreservesHighBits();
        CheckNegativeDisplacementAnd32BitWrap();
        CheckEhFrameAndSceBranchesHonorAddressSize32();
        std::cout << "CodeInstructionCollector address-size override tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
