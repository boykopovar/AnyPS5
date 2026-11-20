#include <relinker/analysis/UnusedNidFilter/EhFrameReader.hpp>
#include <io/BufferUtils.hpp>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

static void Require(bool condition) {
    if (!condition) {
        throw std::runtime_error("Assertion failed");
    }
}

static std::vector<std::uint8_t> EncodeSleb128(std::int64_t value) {
    std::vector<std::uint8_t> result;
    bool more = true;
    while (more) {
        auto byte = static_cast<std::uint8_t>(value & 0x7F);
        value >>= 7;
        const bool signBit = (byte & 0x40) != 0;
        if ((value == 0 && !signBit) || (value == -1 && signBit)) {
            more = false;
        } else {
            byte |= 0x80;
        }
        result.push_back(byte);
    }
    return result;
}

static std::vector<std::uint8_t> EncodeUleb128(std::uint64_t value) {
    std::vector<std::uint8_t> result;
    do {
        auto byte = static_cast<std::uint8_t>(value & 0x7F);
        value >>= 7;
        if (value != 0) {
            byte |= 0x80;
        }
        result.push_back(byte);
    } while (value != 0);
    return result;
}

static std::vector<std::uint8_t> BuildBaseFixture() {
    std::vector<std::uint8_t> bytes(256, 0);
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

static void TestDisplacementSignExtension() {
    const std::uint64_t baseAddress = 0x400000;
    const std::uint32_t rawNegative = 0xFFFFF000;
    const auto signedDisp = static_cast<std::int64_t>(static_cast<std::int32_t>(rawNegative));
    Require(signedDisp == -4096);
    const std::uint64_t correctTarget = baseAddress + signedDisp;
    Require(correctTarget == 0x3FF000);
    const std::uint64_t incorrectTarget = baseAddress + static_cast<std::uint64_t>(rawNegative);
    Require(incorrectTarget == 0x1003FF000ULL);
    Require(correctTarget != incorrectTarget);

    const std::uint64_t smallBase = 0x1000;
    const std::int64_t underflowDisp = -0x2000;
    const bool wouldUnderflow = (underflowDisp < 0) && (static_cast<std::uint64_t>(-underflowDisp) > smallBase);
    Require(wouldUnderflow);

    const std::uint64_t nearMaxBase = std::numeric_limits<std::uint64_t>::max() - 0x100;
    const std::int64_t overflowDisp = 0x200;
    const bool wouldOverflow = (overflowDisp > 0) && (static_cast<std::uint64_t>(overflowDisp) > std::numeric_limits<std::uint64_t>::max() - nearMaxBase);
    Require(wouldOverflow);

    const std::int32_t minInt32 = std::numeric_limits<std::int32_t>::min();
    const auto extMinInt32 = static_cast<std::int64_t>(minInt32);
    Require(extMinInt32 == -2147483648LL);

    const std::int32_t maxInt32 = std::numeric_limits<std::int32_t>::max();
    const auto extMaxInt32 = static_cast<std::int64_t>(maxInt32);
    Require(extMaxInt32 == 2147483647LL);
}

static void TestSleb128EncodingParity() {
    const auto zero = EncodeSleb128(0);
    Require(zero == std::vector<std::uint8_t>{0x00});

    const auto posOne = EncodeSleb128(1);
    Require(posOne == std::vector<std::uint8_t>{0x01});

    const auto negOne = EncodeSleb128(-1);
    Require(negOne == std::vector<std::uint8_t>{0x7F});

    const auto negTwo = EncodeSleb128(-2);
    Require(negTwo == std::vector<std::uint8_t>{0x7E});

    const auto neg64 = EncodeSleb128(-64);
    Require(neg64 == std::vector<std::uint8_t>{0x40});

    const auto neg65 = EncodeSleb128(-65);
    Require(neg65 == std::vector<std::uint8_t>{0xBF, 0x7F});

    const auto neg128 = EncodeSleb128(-128);
    Require(neg128 == std::vector<std::uint8_t>{0x80, 0x7F});

    const auto neg4096 = EncodeSleb128(-4096);
    Require(neg4096 == std::vector<std::uint8_t>{0x80, 0x60});

    const auto pos64 = EncodeSleb128(64);
    Require(pos64 == std::vector<std::uint8_t>{0xC0, 0x00});
}

static void TestPcrelSignedLeb128() {
    Domain::ProgramHeader load{};
    load.Type = 1;
    load.MappedAddress = 0x1000;
    load.FileSize = 256;

    auto eh = load;
    eh.Type = 0x6474E550;
    eh.FileSize = 32;

    Domain::ProgramHeader funcSeg{};
    funcSeg.Type = 1;
    funcSeg.MappedAddress = 0x800;
    funcSeg.FileSize = 0x20;

    auto validPcrel = BuildBaseFixture();
    validPcrel[3] = 0x19;
    const auto dispBegin = EncodeSleb128(static_cast<std::int64_t>(0x800) - 0x1010);
    const auto dispFde = EncodeSleb128(static_cast<std::int64_t>(0x1060) - (0x1010 + dispBegin.size()));
    std::copy(dispBegin.begin(), dispBegin.end(), validPcrel.begin() + 16);
    std::copy(dispFde.begin(), dispFde.end(), validPcrel.begin() + 16 + dispBegin.size());
    Io::WriteU64(validPcrel, 104, 0x800);

    const auto regions = Relinker::UnusedNidFilter::ReadExceptionFunctions(validPcrel, {funcSeg, load, eh}, {}, {});
    Require(regions.size() == 1);
    Require(regions[0].Begin == 0x800);
    Require(regions[0].End == 0x810);

    auto underflowPcrel = BuildBaseFixture();
    underflowPcrel[3] = 0x19;
    const auto underflowDisp = EncodeSleb128(-0x2000);
    std::copy(underflowDisp.begin(), underflowDisp.end(), underflowPcrel.begin() + 16);
    std::copy(dispFde.begin(), dispFde.end(), underflowPcrel.begin() + 16 + underflowDisp.size());

    bool underflowRejected = false;
    try {
        Relinker::UnusedNidFilter::ReadExceptionFunctions(underflowPcrel, {funcSeg, load, eh}, {}, {});
    } catch (const Domain::RelinkerException& error) {
        underflowRejected = std::string(error.what()).find("unwind pointer address overflowed") != std::string::npos;
    }
    Require(underflowRejected);

    auto overflowPcrel = BuildBaseFixture();
    overflowPcrel[3] = 0x19;
    const auto overflowDisp = EncodeSleb128(0x2000);
    std::copy(overflowDisp.begin(), overflowDisp.end(), overflowPcrel.begin() + 16);
    std::copy(dispFde.begin(), dispFde.end(), overflowPcrel.begin() + 16 + overflowDisp.size());

    auto highLoad = load;
    highLoad.MappedAddress = 0xFFFFFFFFFFFFF000ULL;
    auto highEh = eh;
    highEh.MappedAddress = 0xFFFFFFFFFFFFF000ULL;
    Io::WriteU64(overflowPcrel, 4, 0xFFFFFFFFFFFFF040ULL);

    bool overflowRejected = false;
    try {
        Relinker::UnusedNidFilter::ReadExceptionFunctions(overflowPcrel, {highLoad, highEh}, {}, {});
    } catch (const Domain::RelinkerException& error) {
        overflowRejected = std::string(error.what()).find("unwind pointer address overflowed") != std::string::npos;
    }
    Require(overflowRejected);
}

static void TestDatarelSignedLeb128() {
    Domain::ProgramHeader load{};
    load.Type = 1;
    load.MappedAddress = 0x1000;
    load.FileSize = 256;

    auto eh = load;
    eh.Type = 0x6474E550;
    eh.FileSize = 32;

    Domain::ProgramHeader funcSeg{};
    funcSeg.Type = 1;
    funcSeg.MappedAddress = 0x800;
    funcSeg.FileSize = 0x20;

    auto validDatarel = BuildBaseFixture();
    validDatarel[3] = 0x39;
    const auto dispBegin = EncodeSleb128(static_cast<std::int64_t>(0x800) - 0x1000);
    const auto dispFde = EncodeSleb128(static_cast<std::int64_t>(0x1060) - 0x1000);
    std::copy(dispBegin.begin(), dispBegin.end(), validDatarel.begin() + 16);
    std::copy(dispFde.begin(), dispFde.end(), validDatarel.begin() + 16 + dispBegin.size());
    Io::WriteU64(validDatarel, 104, 0x800);

    const auto regions = Relinker::UnusedNidFilter::ReadExceptionFunctions(validDatarel, {funcSeg, load, eh}, {}, {});
    Require(regions.size() == 1);
    Require(regions[0].Begin == 0x800);
    Require(regions[0].End == 0x810);

    auto underflowDatarel = BuildBaseFixture();
    underflowDatarel[3] = 0x39;
    const auto underflowDisp = EncodeSleb128(-0x1800);
    std::copy(underflowDisp.begin(), underflowDisp.end(), underflowDatarel.begin() + 16);
    std::copy(dispFde.begin(), dispFde.end(), underflowDatarel.begin() + 16 + underflowDisp.size());

    bool underflowRejected = false;
    try {
        Relinker::UnusedNidFilter::ReadExceptionFunctions(underflowDatarel, {funcSeg, load, eh}, {}, {});
    } catch (const Domain::RelinkerException& error) {
        underflowRejected = std::string(error.what()).find("unwind pointer address overflowed") != std::string::npos;
    }
    Require(underflowRejected);

    auto overflowDatarel = BuildBaseFixture();
    overflowDatarel[3] = 0x39;
    const auto overflowDisp = EncodeSleb128(0x2000);
    std::copy(overflowDisp.begin(), overflowDisp.end(), overflowDatarel.begin() + 16);
    std::copy(dispFde.begin(), dispFde.end(), overflowDatarel.begin() + 16 + overflowDisp.size());

    auto highLoad = load;
    highLoad.MappedAddress = 0xFFFFFFFFFFFFF000ULL;
    auto highEh = eh;
    highEh.MappedAddress = 0xFFFFFFFFFFFFF000ULL;
    Io::WriteU64(overflowDatarel, 4, 0xFFFFFFFFFFFFF040ULL);

    bool overflowRejected = false;
    try {
        Relinker::UnusedNidFilter::ReadExceptionFunctions(overflowDatarel, {highLoad, highEh}, {}, {});
    } catch (const Domain::RelinkerException& error) {
        overflowRejected = std::string(error.what()).find("unwind pointer address overflowed") != std::string::npos;
    }
    Require(overflowRejected);
}

static void TestCieAugmentationSignedLeb128() {
    std::vector<std::uint8_t> bytes(256, 0);
    bytes[0] = 1;
    bytes[1] = 0;
    bytes[2] = 3;
    bytes[3] = 0;
    Io::WriteU64(bytes, 4, 0x1040);
    Io::WriteU32(bytes, 12, 1);
    Io::WriteU64(bytes, 16, 0x2000);
    Io::WriteU64(bytes, 24, 0x1060);

    Io::WriteU32(bytes, 64, 13);
    bytes[72] = 1;
    bytes[73] = 'z';
    bytes[74] = 'R';
    bytes[75] = 0;
    bytes[76] = 1;
    bytes[77] = 0x78;
    bytes[78] = 16;
    bytes[79] = 1;
    bytes[80] = 0x19;

    Io::WriteU32(bytes, 96, 25);
    Io::WriteU32(bytes, 100, 36);
    const auto fdeBeginDisp = EncodeSleb128(static_cast<std::int64_t>(0x2000) - 0x1068);
    const auto fdeSize = EncodeSleb128(0x20);
    std::copy(fdeBeginDisp.begin(), fdeBeginDisp.end(), bytes.begin() + 104);
    std::copy(fdeSize.begin(), fdeSize.end(), bytes.begin() + 104 + fdeBeginDisp.size());
    bytes[104 + fdeBeginDisp.size() + fdeSize.size()] = 0;

    Domain::ProgramHeader load{};
    load.Type = 1;
    load.MappedAddress = 0x1000;
    load.FileSize = 256;

    auto eh = load;
    eh.Type = 0x6474E550;
    eh.FileSize = 32;

    const auto regions = Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, {load, eh}, {}, {});
    Require(regions.size() == 1);
    Require(regions[0].Begin == 0x2000);
    Require(regions[0].End == 0x2020);

    auto underflowBytes = bytes;
    const auto underflowFdeDisp = EncodeSleb128(-0x2000);
    std::copy(underflowFdeDisp.begin(), underflowFdeDisp.end(), underflowBytes.begin() + 104);

    bool underflowRejected = false;
    try {
        Relinker::UnusedNidFilter::ReadExceptionFunctions(underflowBytes, {load, eh}, {}, {});
    } catch (const Domain::RelinkerException& error) {
        underflowRejected = std::string(error.what()).find("unwind pointer address overflowed") != std::string::npos;
    }
    Require(underflowRejected);
}

static void TestLsdaLandingPadSignedLeb128() {
    std::vector<std::uint8_t> lsdaBytes(0x400, 0);
    Io::WriteU32(lsdaBytes, 0x200, 25);
    lsdaBytes[0x208] = 1;
    const std::uint8_t ciePrefix[] = {'z', 'P', 'L', 'R', 0, 1, 0, 0x78, 11, 0};
    std::copy(std::begin(ciePrefix), std::end(ciePrefix), lsdaBytes.begin() + 0x209);
    lsdaBytes[0x212] = 0;
    Io::WriteU64(lsdaBytes, 0x213, 0x1000);
    lsdaBytes[0x21B] = 0x49;
    lsdaBytes[0x21C] = 0;
    Io::WriteU32(lsdaBytes, 0x220, 29);
    Io::WriteU32(lsdaBytes, 0x224, 0x24);
    Io::WriteU64(lsdaBytes, 0x228, 0x1000);
    Io::WriteU64(lsdaBytes, 0x230, 0x20);

    lsdaBytes[0x238] = 2;
    const auto slebTarget = EncodeSleb128(-0x2000);
    std::copy(slebTarget.begin(), slebTarget.end(), lsdaBytes.begin() + 0x239);

    lsdaBytes[0x300] = 1;
    lsdaBytes[0x302] = 3;
    Io::WriteU64(lsdaBytes, 0x304, 0x200);
    Io::WriteU32(lsdaBytes, 0x30C, 1);
    Io::WriteU64(lsdaBytes, 0x310, 0x1000);
    Io::WriteU64(lsdaBytes, 0x318, 0x220);

    Domain::ProgramHeader lsdaLoad{};
    lsdaLoad.Type = 1;
    lsdaLoad.Flags = 4;
    lsdaLoad.FileSize = lsdaBytes.size();
    lsdaLoad.MemorySize = lsdaBytes.size();
    lsdaLoad.Alignment = 8;

    Domain::ProgramHeader lsdaEh = lsdaLoad;
    lsdaEh.Type = 0x6474E550;
    lsdaEh.Offset = 0x300;
    lsdaEh.MappedAddress = 0x300;
    lsdaEh.FileSize = 32;
    lsdaEh.MemorySize = 32;
    lsdaEh.Alignment = 4;

    bool lsdaSlebUnderflow = false;
    try {
        Relinker::UnusedNidFilter::ReadExceptionFunctions(lsdaBytes, {lsdaLoad, lsdaEh}, {}, {});
    } catch (const Domain::RelinkerException& error) {
        lsdaSlebUnderflow = std::string(error.what()).find("unwind pointer address overflowed") != std::string::npos;
    }
    Require(lsdaSlebUnderflow);
}

static void TestDataAlignmentFactorBounds() {
    auto baseBytes = BuildBaseFixture();
    Domain::ProgramHeader load{};
    load.Type = 1;
    load.MappedAddress = 0x1000;
    load.FileSize = 256;

    auto eh = load;
    eh.Type = 0x6474E550;
    eh.FileSize = 32;

    const std::vector<std::int64_t> validFactors = {-128, -65, -64, -16, -8, -4, -1, 1, 4, 8, 128};
    for (const auto factor : validFactors) {
        auto testBytes = baseBytes;
        const auto encodedFactor = EncodeSleb128(factor);
        Io::WriteU32(testBytes, 64, static_cast<std::uint32_t>(8 + encodedFactor.size()));
        std::copy(encodedFactor.begin(), encodedFactor.end(), testBytes.begin() + 75);
        testBytes[75 + encodedFactor.size()] = 16;
        const auto regions = Relinker::UnusedNidFilter::ReadExceptionFunctions(testBytes, {load, eh}, {}, {});
        Require(regions.size() == 1);
    }

    auto tenByteValidNeg = baseBytes;
    Io::WriteU32(tenByteValidNeg, 64, 18);
    for (int i = 0; i < 9; ++i) {
        tenByteValidNeg[75 + i] = 0x80;
    }
    tenByteValidNeg[84] = 0x7F;
    tenByteValidNeg[85] = 16;
    const auto validNegRegions = Relinker::UnusedNidFilter::ReadExceptionFunctions(tenByteValidNeg, {load, eh}, {}, {});
    Require(validNegRegions.size() == 1);

    auto tenByteValidPos = baseBytes;
    Io::WriteU32(tenByteValidPos, 64, 18);
    for (int i = 0; i < 9; ++i) {
        tenByteValidPos[75 + i] = 0x80;
    }
    tenByteValidPos[84] = 0x00;
    tenByteValidPos[85] = 16;
    const auto validPosRegions = Relinker::UnusedNidFilter::ReadExceptionFunctions(tenByteValidPos, {load, eh}, {}, {});
    Require(validPosRegions.size() == 1);

    const std::uint8_t invalidTerminals[] = {0x01, 0x02, 0x10, 0x40, 0x7E};
    for (const auto terminal : invalidTerminals) {
        auto overflowBytes = baseBytes;
        Io::WriteU32(overflowBytes, 64, 18);
        for (int i = 0; i < 9; ++i) {
            overflowBytes[75 + i] = 0x80;
        }
        overflowBytes[84] = terminal;
        overflowBytes[85] = 16;

        bool overflowRejected = false;
        try {
            Relinker::UnusedNidFilter::ReadExceptionFunctions(overflowBytes, {load, eh}, {}, {});
        } catch (const Domain::RelinkerException& error) {
            overflowRejected = std::string(error.what()).find("overflowing LEB128 value") != std::string::npos;
        }
        Require(overflowRejected);
    }

    auto unterminatedBytes = baseBytes;
    Io::WriteU32(unterminatedBytes, 64, 20);
    for (int i = 0; i < 11; ++i) {
        unterminatedBytes[75 + i] = 0x80;
    }
    unterminatedBytes[86] = 16;

    bool unterminatedRejected = false;
    try {
        Relinker::UnusedNidFilter::ReadExceptionFunctions(unterminatedBytes, {load, eh}, {}, {});
    } catch (const Domain::RelinkerException& error) {
        unterminatedRejected = std::string(error.what()).find("unterminated LEB128 value") != std::string::npos;
    }
    Require(unterminatedRejected);
}

static void TestLsdaCallSiteSignedLeb128() {
    std::vector<std::uint8_t> lsdaBytes(0x400, 0);
    Io::WriteU32(lsdaBytes, 0x200, 25);
    lsdaBytes[0x208] = 1;
    const std::uint8_t ciePrefix[] = {'z', 'P', 'L', 'R', 0, 1, 0, 0x78, 11, 0};
    std::copy(std::begin(ciePrefix), std::end(ciePrefix), lsdaBytes.begin() + 0x209);
    lsdaBytes[0x212] = 0;
    Io::WriteU64(lsdaBytes, 0x213, 0x1000);
    lsdaBytes[0x21B] = 0;
    lsdaBytes[0x21C] = 0;
    Io::WriteU32(lsdaBytes, 0x220, 37);
    Io::WriteU32(lsdaBytes, 0x224, 0x24);
    Io::WriteU64(lsdaBytes, 0x228, 0x1000);
    Io::WriteU64(lsdaBytes, 0x230, 0x20);

    lsdaBytes[0x238] = 10;
    Io::WriteU64(lsdaBytes, 0x239, 0x250);
    lsdaBytes[0x241] = 0;
    lsdaBytes[0x242] = 0;

    lsdaBytes[0x250] = 0xFF;
    lsdaBytes[0x251] = 0xFF;
    lsdaBytes[0x252] = 0x09;
    lsdaBytes[0x253] = 4;
    lsdaBytes[0x254] = 0;
    lsdaBytes[0x255] = 0x10;
    lsdaBytes[0x256] = 0x20;
    lsdaBytes[0x257] = 0;

    lsdaBytes[0x300] = 1;
    lsdaBytes[0x302] = 3;
    Io::WriteU64(lsdaBytes, 0x304, 0x200);
    Io::WriteU32(lsdaBytes, 0x30C, 1);
    Io::WriteU64(lsdaBytes, 0x310, 0x1000);
    Io::WriteU64(lsdaBytes, 0x318, 0x220);

    Domain::ProgramHeader lsdaLoad{};
    lsdaLoad.Type = 1;
    lsdaLoad.Flags = 4;
    lsdaLoad.FileSize = lsdaBytes.size();
    lsdaLoad.MemorySize = lsdaBytes.size();
    lsdaLoad.Alignment = 8;

    Domain::ProgramHeader lsdaEh = lsdaLoad;
    lsdaEh.Type = 0x6474E550;
    lsdaEh.Offset = 0x300;
    lsdaEh.MappedAddress = 0x300;
    lsdaEh.FileSize = 32;
    lsdaEh.MemorySize = 32;
    lsdaEh.Alignment = 4;

    const auto validRegions = Relinker::UnusedNidFilter::ReadExceptionFunctions(lsdaBytes, {lsdaLoad, lsdaEh}, {}, {});
    Require(validRegions.size() == 1);
    Require(validRegions[0].ExtraTargets.size() == 2);
    Require(validRegions[0].ExtraTargets[0] == 0x1000);
    Require(validRegions[0].ExtraTargets[1] == 0x1020);

    auto underflowLsda = lsdaBytes;
    underflowLsda[0x253] = 5;
    underflowLsda[0x254] = 0;
    underflowLsda[0x255] = 0x10;
    underflowLsda[0x256] = 0x80;
    underflowLsda[0x257] = 0x40;
    underflowLsda[0x258] = 0;

    bool csUnderflowRejected = false;
    try {
        Relinker::UnusedNidFilter::ReadExceptionFunctions(underflowLsda, {lsdaLoad, lsdaEh}, {}, {});
    } catch (const Domain::RelinkerException& error) {
        csUnderflowRejected = std::string(error.what()).find("LSDA landing pad address overflowed") != std::string::npos;
    }
    Require(csUnderflowRejected);
}

int main() {
    try {
        TestDisplacementSignExtension();
        TestSleb128EncodingParity();
        TestPcrelSignedLeb128();
        TestDatarelSignedLeb128();
        TestCieAugmentationSignedLeb128();
        TestLsdaLandingPadSignedLeb128();
        TestDataAlignmentFactorBounds();
        TestLsdaCallSiteSignedLeb128();

        std::cout << "All signed LEB128 displacement tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
