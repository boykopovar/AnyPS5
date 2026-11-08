#include <relinker/analysis/UnusedNidFilter/EhFrameReader.hpp>
#include <relinker/domain/Types.hpp>
#include <elfpatcher/general/ElfConstants.hpp>
#include <io/BufferUtils.hpp>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<std::uint8_t>;

void Require(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error("Validation failure: " + message);
}

template<typename T>
void Write(Bytes& bytes, const std::size_t offset, const T value) {
    if (offset > bytes.size() || sizeof(value) > bytes.size() - offset) {
        throw std::runtime_error("Buffer offset exceeds fixture capacity");
    }
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

std::uint64_t DecodeLeb128(const Bytes& bytes, std::size_t& position, const bool signedValue = false) {
    std::uint64_t value = 0;
    unsigned shift = 0;
    for (;;) {
        if (position >= bytes.size()) {
            throw Domain::RelinkerException("Strict filter: unterminated LEB128 value", position);
        }
        const auto byte = bytes[position++];
        if (shift == 63 && (byte & 0x7F) != 0 && (byte & 0x7F) != (signedValue ? 0x7F : 1)) {
            throw Domain::RelinkerException("Strict filter: overflowing LEB128 value", position);
        }
        value |= static_cast<std::uint64_t>(byte & 0x7F) << shift;
        shift += 7;
        if ((byte & 0x80) == 0) {
            if (signedValue && (byte & 0x40) != 0 && shift < 64) {
                value |= (~std::uint64_t{0}) << shift;
            }
            return value;
        }
        if (shift >= 64) {
            throw Domain::RelinkerException("Strict filter: unterminated LEB128 value", position);
        }
    }
}

Bytes EncodeUleb128(std::uint64_t value) {
    Bytes out;
    do {
        std::uint8_t byte = value & 0x7F;
        value >>= 7;
        if (value != 0) byte |= 0x80;
        out.push_back(byte);
    } while (value != 0);
    return out;
}

Bytes EncodeSleb128(std::int64_t value) {
    Bytes out;
    for (bool more = true; more;) {
        std::uint8_t byte = value & 0x7F;
        value >>= 7;
        more = !((value == 0 && (byte & 0x40) == 0) || (value == -1 && (byte & 0x40) != 0));
        if (more) byte |= 0x80;
        out.push_back(byte);
    }
    return out;
}

Domain::VirtualAddress ApplySignedDisplacement(const Domain::VirtualAddress base, const std::int64_t displacement) {
    const auto value = static_cast<std::uint64_t>(displacement);
    const bool negative = displacement < 0;
    if (negative ? base < std::uint64_t{0} - value : value > std::numeric_limits<Domain::VirtualAddress>::max() - base) {
        throw Domain::RelinkerException("Strict filter: unwind pointer address overflowed", base);
    }
    return base + value;
}

Bytes CreateEhFixture() {
    Bytes bytes(128, 0);
    bytes[0] = 1;
    bytes[1] = 0;
    bytes[2] = 3;
    bytes[3] = 0;
    Io::WriteU64(bytes, 4, 0x1040);
    Io::WriteU32(bytes, 12, 1);
    Io::WriteU64(bytes, 16, 0x2000);
    Io::WriteU64(bytes, 24, 0x1060);
    Io::WriteU32(bytes, 64, 9);
    bytes[72] = 1;
    bytes[73] = 0;
    bytes[74] = 1;
    bytes[75] = 0x78;
    bytes[76] = 16;
    Io::WriteU32(bytes, 96, 20);
    Io::WriteU32(bytes, 100, 36);
    Io::WriteU64(bytes, 104, 0x2000);
    Io::WriteU64(bytes, 112, 0x10);
    return bytes;
}

void TestUleb128Decoding() {
    struct TestCase {
        std::uint64_t Expected;
        Bytes Encoded;
    };

    const std::vector<TestCase> cases = {
        {0, {0x00}},
        {1, {0x01}},
        {63, {0x3F}},
        {64, {0x40}},
        {127, {0x7F}},
        {128, {0x80, 0x01}},
        {129, {0x81, 0x01}},
        {16383, {0xFF, 0x7F}},
        {16384, {0x80, 0x80, 0x01}},
        {624485, {0xE5, 0x8E, 0x26}},
        {0xFFFFFFFFULL, {0xFF, 0xFF, 0xFF, 0xFF, 0x0F}},
        {0x8000000000000000ULL, {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01}},
        {0xFFFFFFFFFFFFFFFFULL, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01}}
    };

    for (const auto& item : cases) {
        std::size_t pos = 0;
        const auto decoded = DecodeLeb128(item.Encoded, pos, false);
        Require(decoded == item.Expected, "uleb128 decoding mismatch");
        Require(pos == item.Encoded.size(), "uleb128 decoder must consume entire byte sequence");
        const auto reencoded = EncodeUleb128(item.Expected);
        Require(reencoded == item.Encoded, "uleb128 roundtrip encoding mismatch");
    }

    for (std::uint64_t val = 0; val < 1000; ++val) {
        const auto enc = EncodeUleb128(val);
        std::size_t pos = 0;
        const auto dec = DecodeLeb128(enc, pos, false);
        Require(dec == val, "sequential uleb128 roundtrip mismatch");
        Require(pos == enc.size(), "sequential uleb128 position mismatch");
    }
}

void TestSleb128Decoding() {
    struct TestCase {
        std::int64_t Expected;
        Bytes Encoded;
    };

    const std::vector<TestCase> cases = {
        {0, {0x00}},
        {1, {0x01}},
        {-1, {0x7F}},
        {2, {0x02}},
        {-2, {0x7E}},
        {63, {0x3F}},
        {-64, {0x40}},
        {64, {0xC0, 0x00}},
        {-65, {0xBF, 0x7F}},
        {127, {0xFF, 0x00}},
        {-128, {0x80, 0x7F}},
        {128, {0x80, 0x01}},
        {-129, {0xFF, 0x7E}},
        {624485, {0xE5, 0x8E, 0x26}},
        {-624485, {0x9B, 0xF1, 0x59}},
        {std::numeric_limits<std::int64_t>::max(), {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00}},
        {std::numeric_limits<std::int64_t>::min(), {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7F}}
    };

    for (const auto& item : cases) {
        std::size_t pos = 0;
        const auto decoded = static_cast<std::int64_t>(DecodeLeb128(item.Encoded, pos, true));
        Require(decoded == item.Expected, "sleb128 decoding mismatch");
        Require(pos == item.Encoded.size(), "sleb128 decoder must consume entire byte sequence");
        const auto reencoded = EncodeSleb128(item.Expected);
        Require(reencoded == item.Encoded, "sleb128 roundtrip encoding mismatch");
    }

    for (std::int64_t val = -1000; val <= 1000; ++val) {
        const auto enc = EncodeSleb128(val);
        std::size_t pos = 0;
        const auto dec = static_cast<std::int64_t>(DecodeLeb128(enc, pos, true));
        Require(dec == val, "sequential sleb128 roundtrip mismatch");
        Require(pos == enc.size(), "sequential sleb128 position mismatch");
    }

    {
        Bytes tenByteMinusOne(10, 0xFF);
        tenByteMinusOne.back() = 0x7F;
        std::size_t pos = 0;
        const auto dec = static_cast<std::int64_t>(DecodeLeb128(tenByteMinusOne, pos, true));
        Require(dec == -1, "ten byte sleb128 minus one must decode to -1");
        Require(pos == 10, "ten byte sleb128 minus one must consume all 10 bytes");
    }
}

void TestMaxMultiByteEdgeCases() {
    {
        Bytes validTenByteUleb(10, 0x80);
        validTenByteUleb.back() = 0x01;
        std::size_t pos = 0;
        const auto val = DecodeLeb128(validTenByteUleb, pos, false);
        Require(val == 0x8000000000000000ULL, "ten byte uleb with terminal 0x01 must yield high bit set");
        Require(pos == 10, "ten byte uleb must consume 10 bytes");
    }

    {
        Bytes zeroTenByteUleb(10, 0x80);
        zeroTenByteUleb.back() = 0x00;
        std::size_t pos = 0;
        const auto val = DecodeLeb128(zeroTenByteUleb, pos, false);
        Require(val == 0, "ten byte uleb with terminal 0x00 must yield 0");
        Require(pos == 10, "ten byte uleb must consume 10 bytes");
    }

    for (const std::uint8_t invalidTerminal : {0x02, 0x03, 0x04, 0x10, 0x40, 0x7F}) {
        Bytes overflowUleb(10, 0x80);
        overflowUleb.back() = invalidTerminal;
        std::size_t pos = 0;
        bool rejected = false;
        try {
            DecodeLeb128(overflowUleb, pos, false);
        } catch (const Domain::RelinkerException& ex) {
            rejected = std::string(ex.what()).find("overflowing LEB128 value") != std::string::npos;
        }
        Require(rejected, "uleb with invalid tenth byte bits must throw overflowing LEB128 value");
    }

    for (const std::uint8_t invalidTerminal : {0x01, 0x02, 0x3F, 0x40, 0x7E}) {
        Bytes overflowSleb(10, 0x80);
        overflowSleb.back() = invalidTerminal;
        std::size_t pos = 0;
        bool rejected = false;
        try {
            DecodeLeb128(overflowSleb, pos, true);
        } catch (const Domain::RelinkerException& ex) {
            rejected = std::string(ex.what()).find("overflowing LEB128 value") != std::string::npos;
        }
        Require(rejected, "sleb with invalid tenth byte bits must throw overflowing LEB128 value");
    }

    {
        Bytes elevenByteUleb(11, 0x80);
        elevenByteUleb.back() = 0x01;
        std::size_t pos = 0;
        bool rejected = false;
        try {
            DecodeLeb128(elevenByteUleb, pos, false);
        } catch (const Domain::RelinkerException& ex) {
            rejected = std::string(ex.what()).find("unterminated LEB128 value") != std::string::npos;
        }
        Require(rejected, "shifts >= 64 bits must throw unterminated LEB128 value");
    }
}

void TestOverflowAndUnterminatedBounds() {
    {
        Bytes empty;
        std::size_t pos = 0;
        bool rejected = false;
        try {
            DecodeLeb128(empty, pos, false);
        } catch (const Domain::RelinkerException& ex) {
            rejected = std::string(ex.what()).find("unterminated LEB128 value") != std::string::npos;
        }
        Require(rejected, "empty buffer must throw unterminated LEB128 value");
    }

    for (std::size_t len = 1; len <= 10; ++len) {
        Bytes unterminated(len, 0x80);
        std::size_t pos = 0;
        bool rejected = false;
        try {
            DecodeLeb128(unterminated, pos, false);
        } catch (const Domain::RelinkerException& ex) {
            rejected = std::string(ex.what()).find("unterminated LEB128 value") != std::string::npos;
        }
        Require(rejected, "buffer ending with MSB set must throw unterminated LEB128 value");
    }

    {
        Bytes tenByteAllMsb(10, 0xFF);
        std::size_t pos = 0;
        bool rejected = false;
        try {
            DecodeLeb128(tenByteAllMsb, pos, false);
        } catch (const Domain::RelinkerException& ex) {
            rejected = std::string(ex.what()).find("overflowing LEB128 value") != std::string::npos ||
                       std::string(ex.what()).find("unterminated LEB128 value") != std::string::npos;
        }
        Require(rejected, "ten byte sequence with all MSB set must be rejected");
    }
}

void TestSignedDisplacementArithmetic() {
    {
        const std::int32_t disp32 = -256;
        const auto signExtended = static_cast<std::int64_t>(disp32);
        const auto target = ApplySignedDisplacement(0x2000, signExtended);
        Require(target == 0x1F00, "sign extended negative displacement must subtract from base");

        const auto rawUnsigned = static_cast<std::uint32_t>(disp32);
        Require(rawUnsigned == 0xFFFFFF00U, "raw unsigned cast verification");
        const auto incorrectAddition = 0x2000ULL + rawUnsigned;
        Require(incorrectAddition != target, "unextended unsigned addition must not equal correct signed target");
    }

    {
        const auto res = ApplySignedDisplacement(0x1000, 0x500);
        Require(res == 0x1500, "positive displacement addition mismatch");
    }

    {
        const auto res = ApplySignedDisplacement(0x2000, -0x500);
        Require(res == 0x1B00, "negative displacement subtraction mismatch");
    }

    {
        const auto res = ApplySignedDisplacement(0x1000, 0);
        Require(res == 0x1000, "zero displacement must leave base unchanged");
    }

    {
        const auto res = ApplySignedDisplacement(0x1000, -0x1000);
        Require(res == 0, "displacement exactly reaching zero must succeed");
    }

    {
        const auto res = ApplySignedDisplacement(0xFFFFFFFFFFFFFF00ULL, 0xFF);
        Require(res == 0xFFFFFFFFFFFFFFFFULL, "displacement reaching maximum address must succeed");
    }

    {
        bool rejected = false;
        try {
            ApplySignedDisplacement(0xFFFFFFFFFFFFFF00ULL, 0x101);
        } catch (const Domain::RelinkerException& ex) {
            rejected = std::string(ex.what()).find("unwind pointer address overflowed") != std::string::npos;
        }
        Require(rejected, "positive displacement exceeding max address must throw overflow exception");
    }

    {
        bool rejected = false;
        try {
            ApplySignedDisplacement(0x1000, -0x1001);
        } catch (const Domain::RelinkerException& ex) {
            rejected = std::string(ex.what()).find("unwind pointer address overflowed") != std::string::npos;
        }
        Require(rejected, "negative displacement underflowing zero must throw overflow exception");
    }

    {
        bool rejected = false;
        try {
            ApplySignedDisplacement(0, -1);
        } catch (const Domain::RelinkerException& ex) {
            rejected = std::string(ex.what()).find("unwind pointer address overflowed") != std::string::npos;
        }
        Require(rejected, "negative displacement from zero address must throw overflow exception");
    }
}

void TestRelinkerExceptionStructure() {
    const std::string message = "Strict filter: overflowing LEB128 value";
    const Domain::FileByteOffset failureOffset = 0x12345678;
    const Domain::RelinkerException ex(message, failureOffset);

    Require(std::string(ex.what()) == message, "RelinkerException message must match constructor argument");
    Require(ex.FailureOffset == failureOffset, "RelinkerException failure offset must match constructor argument");

    const std::exception& baseRef = ex;
    Require(std::string(baseRef.what()) == message, "polymorphic catch as std::exception must preserve message");
}

void TestDwarfEhFrameIntegration() {
    Domain::ProgramHeader load{};
    load.Type = Elfpatcher::PT_LOAD;
    load.MappedAddress = 0x1000;
    load.FileSize = 128;
    Domain::ProgramHeader eh = load;
    eh.Type = Elfpatcher::PT_GNU_EH_FRAME;
    eh.FileSize = 32;

    {
        const auto regions = Relinker::UnusedNidFilter::ReadExceptionFunctions(CreateEhFixture(), {load, eh}, {}, {});
        Require(regions.size() == 1, "expected 1 function region from valid fixture");
        Require(regions[0].Begin == 0x2000 && regions[0].End == 0x2010, "function region range mismatch");
    }

    {
        auto belowBytes = CreateEhFixture();
        belowBytes[3] = 0x30;
        Io::WriteU64(belowBytes, 16, 0x800 - 0x1000);
        Io::WriteU64(belowBytes, 24, 0x1060 - 0x1000);
        Io::WriteU64(belowBytes, 104, 0x800);
        Domain::ProgramHeader funcSeg{};
        funcSeg.Type = Elfpatcher::PT_LOAD;
        funcSeg.MappedAddress = 0x800;
        funcSeg.FileSize = 0x20;
        const auto below = Relinker::UnusedNidFilter::ReadExceptionFunctions(belowBytes, {funcSeg, load, eh}, {}, {});
        Require(below.size() == 1 && below[0].Begin == 0x800 && below[0].End == 0x810, "negative displacement function region mismatch");
    }

    {
        auto lebOverflow = CreateEhFixture();
        lebOverflow[3] = 0x31;
        for (int index = 0; index < 9; ++index) lebOverflow[16 + index] = 0x80;
        lebOverflow[25] = 0x82;
        Domain::ProgramHeader lebLoad{};
        lebLoad.Type = Elfpatcher::PT_LOAD;
        lebLoad.MappedAddress = 0xFFFFFFFFFFFFF000ULL;
        lebLoad.FileSize = 128;
        Domain::ProgramHeader lebEh = lebLoad;
        lebEh.Type = Elfpatcher::PT_GNU_EH_FRAME;
        lebEh.FileSize = 32;
        bool lebRejected = false;
        try {
            Relinker::UnusedNidFilter::ReadExceptionFunctions(lebOverflow, {lebLoad, lebEh}, {}, {});
        } catch (const Domain::RelinkerException& error) {
            lebRejected = std::string(error.what()).find("overflowing LEB128 value") != std::string::npos;
        }
        Require(lebRejected, "overflowing LEB128 value in eh_frame must be rejected");
    }

    {
        auto unterminated = CreateEhFixture();
        unterminated[3] = 0x31;
        for (int index = 0; index < 10; ++index) unterminated[16 + index] = 0x80;
        Domain::ProgramHeader untermLoad{};
        untermLoad.Type = Elfpatcher::PT_LOAD;
        untermLoad.MappedAddress = 0x1000;
        untermLoad.FileSize = 128;
        Domain::ProgramHeader untermEh = untermLoad;
        untermEh.Type = Elfpatcher::PT_GNU_EH_FRAME;
        untermEh.FileSize = 32;
        bool rejected = false;
        try {
            Relinker::UnusedNidFilter::ReadExceptionFunctions(unterminated, {untermLoad, untermEh}, {}, {});
        } catch (const Domain::RelinkerException& error) {
            rejected = std::string(error.what()).find("overflowing LEB128 value") != std::string::npos ||
                       std::string(error.what()).find("unterminated LEB128 value") != std::string::npos;
        }
        Require(rejected, "unterminated LEB128 in eh_frame must throw RelinkerException");
    }

    {
        auto dispOverflow = CreateEhFixture();
        dispOverflow[3] = 0x1B;
        Io::WriteU32(dispOverflow, 16, 0xFFFFE000);
        Io::WriteU32(dispOverflow, 20, 0x1060 - 0x1014);
        Io::WriteU64(dispOverflow, 104, 0xFFFFFFFFFFFFF010ULL);
        bool rejected = false;
        try {
            Relinker::UnusedNidFilter::ReadExceptionFunctions(dispOverflow, {load, eh}, {}, {});
        } catch (const Domain::RelinkerException& error) {
            rejected = std::string(error.what()).find("unwind pointer address overflowed") != std::string::npos;
        }
        Require(rejected, "displacement address overflow in eh_frame must throw RelinkerException");
    }
}

} // namespace

int main() {
    try {
        TestUleb128Decoding();
        TestSleb128Decoding();
        TestMaxMultiByteEdgeCases();
        TestOverflowAndUnterminatedBounds();
        TestSignedDisplacementArithmetic();
        TestRelinkerExceptionStructure();
        TestDwarfEhFrameIntegration();
        std::cout << "All LEB128 arithmetic and displacement overflow tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "GuestRelinkerLeb128 failed: " << error.what() << '\n';
        return 1;
    }
}
