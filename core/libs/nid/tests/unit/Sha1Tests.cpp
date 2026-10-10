#include <Testing/Test.hpp>
#include <nid/Sha1.hpp>

#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace {

using Testing::Case;
using Testing::RequireEqual;

std::vector<std::uint8_t> Bytes(const std::string& text) {
    return {text.begin(), text.end()};
}

std::string Hex(const std::array<std::uint8_t, 20>& digest) {
    std::ostringstream stream;
    for (const auto byte : digest) stream << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    return stream.str();
}

const Case emptyInput{"Sha1_EmptyInput_MatchesFips180Vector", [] {
    RequireEqual(Hex(Nid::Sha1({})), std::string("da39a3ee5e6b4b0d3255bfef95601890afd80709"), "digest of empty input");
}};

const Case singleBlock{"Sha1_Abc_MatchesFips180Vector", [] {
    RequireEqual(Hex(Nid::Sha1(Bytes("abc"))), std::string("a9993e364706816aba3e25717850c26c9cd0d89d"), "digest of abc");
}};

const Case twoBlocks{"Sha1_FiftySixByteMessage_MatchesFips180Vector", [] {
    const auto message = Bytes("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq");
    RequireEqual(Hex(Nid::Sha1(message)), std::string("84983e441c3bd26ebaae4aa1f95129e5e54670f1"), "digest of two-block message");
}};

const Case millionBytes{"Sha1_OneMillionLetterA_MatchesFips180Vector", [] {
    const std::vector<std::uint8_t> message(1000000, 'a');
    RequireEqual(Hex(Nid::Sha1(message)), std::string("34aa973cd4c4daa4f61eeb2bdbad27316534016f"), "digest of a million letters");
}};

const Case paddingBoundary{"Sha1_FiftyFiveAndSixtyFourBytes_MatchPaddingBoundaryVectors", [] {
    RequireEqual(Hex(Nid::Sha1(std::vector<std::uint8_t>(55, 'a'))),
        std::string("c1c8bbdc22796e28c0e15163d20899b65621d65a"), "digest of 55 bytes");
    RequireEqual(Hex(Nid::Sha1(std::vector<std::uint8_t>(64, 'a'))),
        std::string("0098ba824b5c16427bd7a1122a5a442a25ec644d"), "digest of 64 bytes");
}};

} // namespace
