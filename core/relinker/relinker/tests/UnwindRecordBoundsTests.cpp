#include <relinker/analysis/UnusedNidFilter/EhFrameReader.hpp>
#include <io/BufferUtils.hpp>
#include <iostream>
#include <limits>
#include <stdexcept>

static void Check(bool ok, std::size_t& count) {
    if (!ok) throw std::runtime_error("Unwind record validation failed");
    ++count;
}

static std::vector<std::uint8_t> Fixture() {
    std::vector<std::uint8_t> bytes(128, 0);
    bytes[0] = 1; bytes[1] = 0; bytes[2] = 3; bytes[3] = 0;
    Io::WriteU64(bytes, 4, 0x1040); Io::WriteU32(bytes, 12, 1);
    Io::WriteU64(bytes, 16, 0x2000); Io::WriteU64(bytes, 24, 0x1060);
    Io::WriteU32(bytes, 64, 9); bytes[72] = 1; bytes[73] = 0; bytes[74] = 1; bytes[75] = 0x78; bytes[76] = 16;
    Io::WriteU32(bytes, 96, 20); Io::WriteU32(bytes, 100, 36);
    Io::WriteU64(bytes, 104, 0x2000); Io::WriteU64(bytes, 112, 0x10);
    return bytes;
}

int main() {
    try {
        std::size_t passedCases = 0;
        Domain::ProgramHeader load{}; load.Type = 1; load.MappedAddress = 0x1000; load.FileSize = 128;
        auto eh = load; eh.Type = 0x6474E550; eh.FileSize = 32;
        const auto regions = Relinker::UnusedNidFilter::ReadExceptionFunctions(Fixture(), {load, eh}, {}, {});
        Check(regions.size() == 1 && regions[0].Begin == 0x2000 && regions[0].End == 0x2010, passedCases);
        auto second = load; second.MappedAddress = 0x1048; second.Offset = 72; second.FileSize = 56;
        auto first = load; first.FileSize = 72;
        Check(Relinker::UnusedNidFilter::ReadExceptionFunctions(Fixture(), {first, second, eh}, {}, {}).size() == 1, passedCases);
        auto belowBytes = Fixture();
        belowBytes[3] = 0x30;
        Io::WriteU64(belowBytes, 16, 0x800 - 0x1000);
        Io::WriteU64(belowBytes, 24, 0x1060 - 0x1000);
        Io::WriteU64(belowBytes, 104, 0x800);
        Domain::ProgramHeader funcSeg{}; funcSeg.Type = 1; funcSeg.MappedAddress = 0x800; funcSeg.FileSize = 0x20;
        const auto below = Relinker::UnusedNidFilter::ReadExceptionFunctions(belowBytes, {funcSeg, load, eh}, {}, {});
        Check(below.size() == 1 && below[0].Begin == 0x800 && below[0].End == 0x810, passedCases);
        auto belowDatarelUdata8 = Fixture();
        belowDatarelUdata8[3] = 0x34;
        Io::WriteU64(belowDatarelUdata8, 16, 0x800 - 0x1000);
        Io::WriteU64(belowDatarelUdata8, 24, 0x1060 - 0x1000);
        Io::WriteU64(belowDatarelUdata8, 104, 0x800);
        Check(Relinker::UnusedNidFilter::ReadExceptionFunctions(belowDatarelUdata8, {funcSeg, load, eh}, {}, {}).size() == 1, passedCases);
        auto belowPcrelUdata8 = Fixture();
        belowPcrelUdata8[3] = 0x14;
        Io::WriteU64(belowPcrelUdata8, 16, 0x800 - 0x1010);
        Io::WriteU64(belowPcrelUdata8, 24, 0x1060 - 0x1018);
        Io::WriteU64(belowPcrelUdata8, 104, 0x800);
        Check(Relinker::UnusedNidFilter::ReadExceptionFunctions(belowPcrelUdata8, {funcSeg, load, eh}, {}, {}).size() == 1, passedCases);
        auto empty = Fixture();
        Io::WriteU64(empty, 112, 0);
        Check(Relinker::UnusedNidFilter::ReadExceptionFunctions(empty, {load, eh}, {}, {}).empty(), passedCases);
        Domain::ProgramHeader highSeg{}; highSeg.Type = 1; highSeg.MappedAddress = 0xFFFFFFFFFFFF0000ULL; highSeg.FileSize = 128;
        auto highEh = highSeg; highEh.Type = 0x6474E550; highEh.FileSize = 32;
        auto ulebAtLimit = Fixture();
        Io::WriteU64(ulebAtLimit, 4, 0xFFFFFFFFFFFF0040ULL);
        ulebAtLimit[3] = 0x11;
        ulebAtLimit[16] = 0xEF; ulebAtLimit[17] = 0xFF; ulebAtLimit[18] = 0x03;
        ulebAtLimit[19] = 0x4D;
        Io::WriteU64(ulebAtLimit, 104, 0xFFFFFFFFFFFFFFFFULL);
        Io::WriteU64(ulebAtLimit, 112, 0);
        Check(Relinker::UnusedNidFilter::ReadExceptionFunctions(ulebAtLimit, {highSeg, highEh}, {}, {}).empty(), passedCases);
        auto udata8OverLimit = Fixture();
        Io::WriteU64(udata8OverLimit, 4, 0xFFFFFFFFFFFF0040ULL);
        udata8OverLimit[3] = 0x14;
        Io::WriteU64(udata8OverLimit, 16, 0xFFF0);
        Io::WriteU64(udata8OverLimit, 24, 0x48);
        Io::WriteU64(udata8OverLimit, 104, 0);
        Io::WriteU64(udata8OverLimit, 112, 0);
        Check(Relinker::UnusedNidFilter::ReadExceptionFunctions(udata8OverLimit, {highSeg, highEh}, {}, {}).empty(), passedCases);
        for (int kind = 0; kind < 18; ++kind) {
            auto bytes = Fixture(); auto segment = load; auto ehHeader = eh;
            switch (kind) {
                case 0: Io::WriteU32(bytes, 64, 0x1000); break;
                case 1: Io::WriteU32(bytes, 96, 0x1000); break;
                case 2: Io::WriteU32(bytes, 64, 100); break;
                case 3: Io::WriteU32(bytes, 96, 32); bytes.resize(120); break;
                case 4: segment.Offset = std::numeric_limits<std::uint64_t>::max() - 4095; segment.MappedAddress = 0; segment.FileSize = 0x1100; break;
                case 5: segment.FileSize = 119; break;
                case 6: Io::WriteU32(bytes, 64, 8); break;
                case 7: Io::WriteU32(bytes, 100, 0); break;
                case 8: {
                    bytes[3] = 0x1B;
                    Io::WriteU32(bytes, 16, 0xFFFFE000);
                    Io::WriteU32(bytes, 20, 0x1060 - 0x1014);
                    Io::WriteU64(bytes, 104, 0xFFFFFFFFFFFFF010ULL);
                    break;
                }
                case 9: bytes[3] = 0x1C; Io::WriteU64(bytes, 16, 0x8000000000000000ULL); break;
                case 10: {
                    segment.MappedAddress = 0xFFFFFFFFFFFF0000ULL;
                    ehHeader.MappedAddress = 0xFFFFFFFFFFFF0000ULL;
                    Io::WriteU64(bytes, 4, 0xFFFFFFFFFFFF0040ULL);
                    bytes[3] = 0x13;
                    Io::WriteU32(bytes, 16, 0x107F0);
                    Io::WriteU32(bytes, 20, 0x4C);
                    Io::WriteU64(bytes, 104, 0x800);
                    break;
                }
                case 11: {
                    segment.MappedAddress = 0xFFFFFFFFFFFF0000ULL;
                    ehHeader.MappedAddress = 0xFFFFFFFFFFFF0000ULL;
                    Io::WriteU64(bytes, 4, 0xFFFFFFFFFFFF0040ULL);
                    bytes[3] = 0x11;
                    bytes[16] = 0xF0;
                    bytes[17] = 0xFF;
                    bytes[18] = 0x03;
                    bytes[19] = 0x4D;
                    Io::WriteU64(bytes, 104, 0);
                    Io::WriteU64(bytes, 112, 0);
                    break;
                }
                case 12: Io::WriteU32(bytes, 68, 1); break;
                case 13: bytes[76] = 17; break;
                case 14: bytes[73] = 'z'; bytes[74] = 'R'; bytes[75] = 'L'; bytes[76] = 1; break;
                case 15: bytes[76] = 0; break;
                case 16: break;
                case 17: {
                    Io::WriteU64(bytes, 104, 0x2000);
                    Io::WriteU64(bytes, 112, 0x1010);
                    break;
                }
            }
            bool rejected = false;
            std::vector<Domain::ProgramHeader> headers = {segment, ehHeader};
            if (kind == 8) {
                Domain::ProgramHeader high{}; high.Type = 1; high.MappedAddress = 0xFFFFFFFFFFFFF000ULL; high.FileSize = 0x100;
                headers.push_back(high);
            }
            if (kind == 10 || kind == 11) {
                Domain::ProgramHeader funcSeg{}; funcSeg.Type = 1; funcSeg.MappedAddress = 0x800; funcSeg.FileSize = 0x20;
                headers = {segment, ehHeader, funcSeg};
            }
            if (kind == 16) {
                Domain::ProgramHeader textSeg{}; textSeg.Type = 1; textSeg.Flags = 5; textSeg.MappedAddress = 0x3000; textSeg.FileSize = 0x100;
                headers = {segment, ehHeader, textSeg};
            }
            if (kind == 17) {
                Domain::ProgramHeader seg1{}; seg1.Type = 1; seg1.Flags = 5; seg1.MappedAddress = 0x2000; seg1.FileSize = 0x10;
                Domain::ProgramHeader seg2{}; seg2.Type = 1; seg2.Flags = 5; seg2.MappedAddress = 0x3000; seg2.FileSize = 0x10;
                headers = {segment, ehHeader, seg1, seg2};
            }
            try { Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {}); }
            catch (const Relinker::RelinkerException&) { rejected = true; }
            Check(rejected, passedCases);
        }
        Domain::ProgramHeader validText{}; validText.Type = 1; validText.Flags = 5; validText.MappedAddress = 0x2000; validText.FileSize = 0x100;
        const auto inText = Relinker::UnusedNidFilter::ReadExceptionFunctions(Fixture(), {load, eh, validText}, {}, {});
        Check(inText.size() == 1 && inText[0].Begin == 0x2000 && inText[0].End == 0x2010, passedCases);
        auto lsdaBytes = Fixture();
        lsdaBytes.resize(160, 0);
        Io::WriteU32(lsdaBytes, 64, 25);
        lsdaBytes[72] = 1;
        const std::vector<std::uint8_t> aug = {'z', 'P', 'L', 'R', 0, 1, 0x78, 16, 11, 0};
        std::copy(aug.begin(), aug.end(), lsdaBytes.begin() + 73);
        Io::WriteU64(lsdaBytes, 83, 0x2000);
        lsdaBytes[91] = 0; lsdaBytes[92] = 0;
        Io::WriteU32(lsdaBytes, 96, 29);
        Io::WriteU32(lsdaBytes, 100, 36);
        Io::WriteU64(lsdaBytes, 104, 0x2000);
        Io::WriteU64(lsdaBytes, 112, 0x10);
        lsdaBytes[120] = 8;
        Io::WriteU64(lsdaBytes, 121, 0x1088);
        const std::vector<std::uint8_t> lsdaTable = {0xFF, 0xFF, 1, 4, 0, 1, 0x40, 0};
        std::copy(lsdaTable.begin(), lsdaTable.end(), lsdaBytes.begin() + 136);
        auto lsdaLoad = load; lsdaLoad.FileSize = 160;
        const auto withLsda = Relinker::UnusedNidFilter::ReadExceptionFunctions(lsdaBytes, {lsdaLoad, eh, validText}, {}, {});
        Check(withLsda.size() == 1 && withLsda[0].ExtraTargets.size() == 2 && withLsda[0].ExtraTargets[1] == 0x2040, passedCases);
        Domain::ProgramHeader smallText{}; smallText.Type = 1; smallText.Flags = 5; smallText.MappedAddress = 0x2000; smallText.FileSize = 0x20;
        bool lsdaRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(lsdaBytes, {lsdaLoad, eh, smallText}, {}, {}); }
        catch (const Relinker::RelinkerException&) { lsdaRejected = true; }
        Check(lsdaRejected, passedCases);
        auto zsBytes = Fixture();
        Io::WriteU32(zsBytes, 64, 12);
        zsBytes[72] = 1;
        zsBytes[73] = 'z'; zsBytes[74] = 'S'; zsBytes[75] = 0;
        zsBytes[76] = 1; zsBytes[77] = 0x78; zsBytes[78] = 16;
        zsBytes[79] = 0;
        Io::WriteU32(zsBytes, 96, 21);
        Io::WriteU32(zsBytes, 100, 36);
        Io::WriteU64(zsBytes, 104, 0x2000);
        Io::WriteU64(zsBytes, 112, 0x10);
        zsBytes[120] = 0;
        const auto zs = Relinker::UnusedNidFilter::ReadExceptionFunctions(zsBytes, {load, eh, validText}, {}, {});
        Check(zs.size() == 1 && zs[0].Begin == 0x2000 && zs[0].End == 0x2010, passedCases);
        auto zrsBytes = Fixture();
        Io::WriteU32(zrsBytes, 64, 14);
        zrsBytes[72] = 1;
        zrsBytes[73] = 'z'; zrsBytes[74] = 'R'; zrsBytes[75] = 'S'; zrsBytes[76] = 0;
        zrsBytes[77] = 1; zrsBytes[78] = 0x78; zrsBytes[79] = 16;
        zrsBytes[80] = 1;
        zrsBytes[81] = 0;
        Io::WriteU32(zrsBytes, 96, 21);
        Io::WriteU32(zrsBytes, 100, 36);
        Io::WriteU64(zrsBytes, 104, 0x2000);
        Io::WriteU64(zrsBytes, 112, 0x10);
        zrsBytes[120] = 0;
        const auto zrs = Relinker::UnusedNidFilter::ReadExceptionFunctions(zrsBytes, {load, eh, validText}, {}, {});
        Check(zrs.size() == 1 && zrs[0].Begin == 0x2000 && zrs[0].End == 0x2010, passedCases);
        auto extraBytes = Fixture();
        Io::WriteU32(extraBytes, 64, 15);
        extraBytes[72] = 1;
        extraBytes[73] = 'z'; extraBytes[74] = 'R'; extraBytes[75] = 0;
        extraBytes[76] = 1; extraBytes[77] = 0x78; extraBytes[78] = 16;
        extraBytes[79] = 3;
        extraBytes[80] = 0;
        extraBytes[81] = 0xAA; extraBytes[82] = 0xBB;
        Io::WriteU32(extraBytes, 96, 23);
        Io::WriteU32(extraBytes, 100, 36);
        Io::WriteU64(extraBytes, 104, 0x2000);
        Io::WriteU64(extraBytes, 112, 0x10);
        extraBytes[120] = 2;
        extraBytes[121] = 0xCC; extraBytes[122] = 0xDD;
        const auto extra = Relinker::UnusedNidFilter::ReadExceptionFunctions(extraBytes, {load, eh, validText}, {}, {});
        Check(extra.size() == 1 && extra[0].Begin == 0x2000 && extra[0].End == 0x2010, passedCases);
        auto removedBytes = Fixture();
        Io::WriteU64(removedBytes, 16, 0);
        Io::WriteU64(removedBytes, 104, 0);
        Io::WriteU64(removedBytes, 112, 0);
        const auto removed = Relinker::UnusedNidFilter::ReadExceptionFunctions(removedBytes, {load, eh, validText}, {}, {});
        Check(removed.empty(), passedCases);
        std::cout << passedCases << " unwind record cases passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
