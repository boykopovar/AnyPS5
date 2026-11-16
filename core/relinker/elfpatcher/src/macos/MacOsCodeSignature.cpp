#include <elfpatcher/macos/MacOsCodeSignature.hpp>
#include <domain/Types.hpp>
#include <algorithm>
#include <bit>
#include <limits>

namespace Elfpatcher::MacOs {

namespace {

constexpr std::uint32_t CsMagicEmbeddedSignature = 0xfade0cc0;
constexpr std::uint32_t CsMagicCodeDirectory = 0xfade0c02;
constexpr std::uint32_t CsMagicRequirements = 0xfade0c01;
constexpr std::uint32_t CsMagicBlobWrapper = 0xfade0b01;
constexpr std::uint32_t CsSlotCodeDirectory = 0;
constexpr std::uint32_t CsSlotRequirements = 2;
constexpr std::uint32_t CsSlotSignature = 0x10000;
constexpr std::uint32_t CsSupportsExecSeg = 0x20400;
constexpr std::uint32_t CsAdhoc = 0x2;
constexpr std::uint8_t CsHashTypeSha256 = 2;
constexpr std::uint64_t CsExecSegMainBinary = 0x1;

constexpr std::size_t HashSize = 32;
constexpr std::size_t CodePageShift = 12;
constexpr std::uint64_t CodePageSize = std::uint64_t{1} << CodePageShift;
constexpr std::uint32_t SpecialSlots = CsSlotRequirements;
constexpr std::size_t SuperBlobHeaderSize = 12 + 3 * 8;
constexpr std::size_t CodeDirectoryHeaderSize = 88;
constexpr std::size_t RequirementsSize = 12;
constexpr std::size_t BlobWrapperSize = 8;

constexpr std::array<std::uint32_t, 64> RoundConstants = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

void CompressBlock(std::array<std::uint32_t, 8>& state, const std::uint8_t* block) {
    std::array<std::uint32_t, 64> schedule{};
    for (std::size_t index = 0; index < 16; ++index)
        schedule[index] = static_cast<std::uint32_t>(block[index * 4]) << 24 | static_cast<std::uint32_t>(block[index * 4 + 1]) << 16 |
                          static_cast<std::uint32_t>(block[index * 4 + 2]) << 8 | block[index * 4 + 3];
    for (std::size_t index = 16; index < 64; ++index) {
        const auto low = std::rotr(schedule[index - 15], 7) ^ std::rotr(schedule[index - 15], 18) ^ (schedule[index - 15] >> 3);
        const auto high = std::rotr(schedule[index - 2], 17) ^ std::rotr(schedule[index - 2], 19) ^ (schedule[index - 2] >> 10);
        schedule[index] = schedule[index - 16] + low + schedule[index - 7] + high;
    }
    auto [a, b, c, d, e, f, g, h] = state;
    for (std::size_t index = 0; index < 64; ++index) {
        const auto first = h + (std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25)) + ((e & f) ^ (~e & g)) + RoundConstants[index] + schedule[index];
        const auto second = (std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g;
        g = f;
        f = e;
        e = d + first;
        d = c;
        c = b;
        b = a;
        a = first + second;
    }
    const std::array<std::uint32_t, 8> result = {a, b, c, d, e, f, g, h};
    for (std::size_t index = 0; index < 8; ++index) state[index] += result[index];
}

void AppendU32Be(std::vector<std::uint8_t>& out, const std::uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<std::uint8_t>(value >> shift));
}

void AppendU64Be(std::vector<std::uint8_t>& out, const std::uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) out.push_back(static_cast<std::uint8_t>(value >> shift));
}

std::uint64_t CodeSlots(const std::uint64_t codeLimit) {
    return (codeLimit + CodePageSize - 1) / CodePageSize;
}

std::size_t CodeDirectorySize(const std::uint64_t codeLimit, const std::string& identifier) {
    return CodeDirectoryHeaderSize + identifier.size() + 1 + (SpecialSlots + CodeSlots(codeLimit)) * HashSize;
}

std::size_t SuperBlobSize(const std::uint64_t codeLimit, const std::string& identifier) {
    return SuperBlobHeaderSize + CodeDirectorySize(codeLimit, identifier) + RequirementsSize + BlobWrapperSize;
}

}

std::array<std::uint8_t, 32> Sha256(const std::uint8_t* data, const std::size_t size) {
    std::array<std::uint32_t, 8> state = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    std::size_t offset = 0;
    for (; size - offset >= 64; offset += 64) CompressBlock(state, data + offset);
    std::array<std::uint8_t, 128> tail{};
    const auto rest = size - offset;
    std::copy_n(data + offset, rest, tail.begin());
    tail[rest] = 0x80;
    const auto tailSize = rest + 9 <= 64 ? std::size_t{64} : std::size_t{128};
    const auto bits = static_cast<std::uint64_t>(size) * 8;
    for (std::size_t index = 0; index < 8; ++index) tail[tailSize - 1 - index] = static_cast<std::uint8_t>(bits >> (index * 8));
    for (std::size_t block = 0; block < tailSize; block += 64) CompressBlock(state, tail.data() + block);
    std::array<std::uint8_t, 32> digest{};
    for (std::size_t index = 0; index < 8; ++index)
        for (std::size_t byte = 0; byte < 4; ++byte) digest[index * 4 + byte] = static_cast<std::uint8_t>(state[index] >> (24 - byte * 8));
    return digest;
}

std::size_t CodeSignatureSize(const std::uint64_t codeLimit, const std::string& identifier) {
    return (SuperBlobSize(codeLimit, identifier) + 15) & ~std::size_t{15};
}

void AppendCodeSignature(std::vector<std::uint8_t>& file, const std::string& identifier, const std::uint64_t executableSegmentOffset, const std::uint64_t executableSegmentSize, const bool mainBinary) {
    if (identifier.empty() || identifier.find('\0') != std::string::npos)
        throw Domain::RelinkerException("Mach-O code signature identifier must be a non-empty name without NUL characters");
    const std::uint64_t codeLimit = file.size();
    if (codeLimit % 16 != 0) throw Domain::RelinkerException("Mach-O code signature must start 16-byte aligned", codeLimit);
    if (codeLimit + CodeSignatureSize(codeLimit, identifier) > std::numeric_limits<std::uint32_t>::max())
        throw Domain::RelinkerException("Mach-O output with its code signature exceeds 4 GiB");
    if (executableSegmentOffset > codeLimit || executableSegmentSize > codeLimit - executableSegmentOffset)
        throw Domain::RelinkerException("Mach-O executable segment is outside the signed code", executableSegmentOffset);

    const auto codeDirectorySize = CodeDirectorySize(codeLimit, identifier);
    const auto superBlobSize = SuperBlobSize(codeLimit, identifier);
    const auto codeDirectoryOffset = SuperBlobHeaderSize;
    const auto requirementsOffset = codeDirectoryOffset + codeDirectorySize;
    const auto wrapperOffset = requirementsOffset + RequirementsSize;
    const auto identifierOffset = CodeDirectoryHeaderSize;
    const auto hashOffset = identifierOffset + identifier.size() + 1 + SpecialSlots * HashSize;

    std::vector<std::uint8_t> requirements;
    AppendU32Be(requirements, CsMagicRequirements);
    AppendU32Be(requirements, static_cast<std::uint32_t>(RequirementsSize));
    AppendU32Be(requirements, 0);

    std::vector<std::uint8_t> signature;
    signature.reserve(CodeSignatureSize(codeLimit, identifier));
    AppendU32Be(signature, CsMagicEmbeddedSignature);
    AppendU32Be(signature, static_cast<std::uint32_t>(superBlobSize));
    AppendU32Be(signature, 3);
    AppendU32Be(signature, CsSlotCodeDirectory);
    AppendU32Be(signature, static_cast<std::uint32_t>(codeDirectoryOffset));
    AppendU32Be(signature, CsSlotRequirements);
    AppendU32Be(signature, static_cast<std::uint32_t>(requirementsOffset));
    AppendU32Be(signature, CsSlotSignature);
    AppendU32Be(signature, static_cast<std::uint32_t>(wrapperOffset));

    AppendU32Be(signature, CsMagicCodeDirectory);
    AppendU32Be(signature, static_cast<std::uint32_t>(codeDirectorySize));
    AppendU32Be(signature, CsSupportsExecSeg);
    AppendU32Be(signature, CsAdhoc);
    AppendU32Be(signature, static_cast<std::uint32_t>(hashOffset));
    AppendU32Be(signature, static_cast<std::uint32_t>(identifierOffset));
    AppendU32Be(signature, SpecialSlots);
    AppendU32Be(signature, static_cast<std::uint32_t>(CodeSlots(codeLimit)));
    AppendU32Be(signature, static_cast<std::uint32_t>(codeLimit));
    signature.push_back(static_cast<std::uint8_t>(HashSize));
    signature.push_back(CsHashTypeSha256);
    signature.push_back(0);
    signature.push_back(static_cast<std::uint8_t>(CodePageShift));
    AppendU32Be(signature, 0);
    AppendU32Be(signature, 0);
    AppendU32Be(signature, 0);
    AppendU32Be(signature, 0);
    AppendU64Be(signature, 0);
    AppendU64Be(signature, executableSegmentOffset);
    AppendU64Be(signature, executableSegmentSize);
    AppendU64Be(signature, mainBinary ? CsExecSegMainBinary : 0);
    signature.insert(signature.end(), identifier.begin(), identifier.end());
    signature.push_back(0);

    const auto requirementsHash = Sha256(requirements.data(), requirements.size());
    signature.insert(signature.end(), requirementsHash.begin(), requirementsHash.end());
    signature.insert(signature.end(), HashSize, 0);
    for (std::uint64_t page = 0; page < codeLimit; page += CodePageSize) {
        const auto hash = Sha256(file.data() + page, static_cast<std::size_t>(std::min(CodePageSize, codeLimit - page)));
        signature.insert(signature.end(), hash.begin(), hash.end());
    }
    if (signature.size() != codeDirectoryOffset + codeDirectorySize)
        throw Domain::RelinkerException("Mach-O code directory size changed while it was written");

    signature.insert(signature.end(), requirements.begin(), requirements.end());
    AppendU32Be(signature, CsMagicBlobWrapper);
    AppendU32Be(signature, static_cast<std::uint32_t>(BlobWrapperSize));
    if (signature.size() != superBlobSize) throw Domain::RelinkerException("Mach-O code signature size changed while it was written");
    signature.resize(CodeSignatureSize(codeLimit, identifier), 0);
    file.insert(file.end(), signature.begin(), signature.end());
}

}
