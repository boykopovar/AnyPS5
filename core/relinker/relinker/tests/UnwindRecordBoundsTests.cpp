#include <relinker/analysis/UnusedNidFilter/EhFrameReader.hpp>
#include <io/BufferUtils.hpp>
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

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
        auto ulebOverLimit = Fixture();
        Io::WriteU64(ulebOverLimit, 4, 0xFFFFFFFFFFFF0040ULL);
        ulebOverLimit[3] = 0x11;
        ulebOverLimit[16] = 0xF0; ulebOverLimit[17] = 0xFF; ulebOverLimit[18] = 0x03;
        ulebOverLimit[19] = 0x4D;
        ulebOverLimit[104] = 0;
        ulebOverLimit[112] = 0;
        Check(Relinker::UnusedNidFilter::ReadExceptionFunctions(ulebOverLimit, {highSeg, highEh}, {}, {}).empty(), passedCases);
        for (int kind = 0; kind < 20; ++kind) {
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
                    bytes[3] = 0x3B;
                    Io::WriteU32(bytes, 96, 0x80000000);
                    Io::WriteU32(bytes, 100, 0x80000000);
                    break;
                }
                case 12: {
                    bytes[3] = 0x49;
                    bytes[96] = 0;
                    bytes[97] = 0x80;
                    bytes[98] = 0;
                    bytes[99] = 0x80;
                    break;
                }
                case 13: {
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
                case 14: Io::WriteU32(bytes, 68, 1); break;
                case 15: bytes[76] = 17; break;
                case 16: bytes[73] = 'z'; bytes[74] = 'R'; bytes[75] = 'L'; bytes[76] = 1; break;
                case 17: bytes[76] = 0; break;
                case 18: break;
                case 19: {
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
            if (kind == 10 || kind == 13) {
                Domain::ProgramHeader funcSeg{}; funcSeg.Type = 1; funcSeg.MappedAddress = 0x800; funcSeg.FileSize = 0x20;
                headers = {segment, ehHeader, funcSeg};
            }
            if (kind == 18) {
                Domain::ProgramHeader textSeg{}; textSeg.Type = 1; textSeg.Flags = 5; textSeg.MappedAddress = 0x3000; textSeg.FileSize = 0x100;
                headers = {segment, ehHeader, textSeg};
            }
            if (kind == 19) {
                Domain::ProgramHeader seg1{}; seg1.Type = 1; seg1.Flags = 5; seg1.MappedAddress = 0x2000; seg1.FileSize = 0x10;
                Domain::ProgramHeader seg2{}; seg2.Type = 1; seg2.Flags = 5; seg2.MappedAddress = 0x3000; seg2.FileSize = 0x10;
                headers = {segment, ehHeader, seg1, seg2};
            }
            try { Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {}); }
            catch (const Relinker::RelinkerException&) { rejected = true; }
            Check(rejected, passedCases);
        }
        auto tableOverflow = Fixture();
        tableOverflow[3] = 0x39;
        tableOverflow[16] = 0x80;
        tableOverflow[17] = 0x20;
        tableOverflow[18] = 0x80;
        tableOverflow[19] = 0x20;
        Domain::ProgramHeader highLoad{}; highLoad.Type = 1; highLoad.MappedAddress = 0xFFFFFFFFFFFFF000ULL; highLoad.FileSize = 128;
        Domain::ProgramHeader highEhHeader = highLoad; highEhHeader.Type = 0x6474E550; highEhHeader.FileSize = 32;
        bool overflowRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(tableOverflow, {highLoad, highEhHeader}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            overflowRejected = std::string(error.what()).find("unwind pointer address overflowed") != std::string::npos;
        }
        Check(overflowRejected, passedCases);
        auto udata4Overflow = Fixture();
        udata4Overflow[3] = 0x33;
        Io::WriteU32(udata4Overflow, 16, 0xFFFFFFFF);
        Io::WriteU32(udata4Overflow, 20, 0xFFFFFFFF);
        Domain::ProgramHeader udata4HighLoad{}; udata4HighLoad.Type = 1; udata4HighLoad.MappedAddress = 0xFFFFFFFFFFFFF000ULL; udata4HighLoad.FileSize = 128;
        Domain::ProgramHeader udata4HighEh = udata4HighLoad; udata4HighEh.Type = 0x6474E550; udata4HighEh.FileSize = 32;
        bool udata4Rejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(udata4Overflow, {udata4HighLoad, udata4HighEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            udata4Rejected = std::string(error.what()).find("unwind pointer address overflowed") != std::string::npos;
        }
        Check(udata4Rejected, passedCases);
        auto sdataOverflow = Fixture();
        sdataOverflow[3] = 0x3B;
        Io::WriteU32(sdataOverflow, 16, 0x1000);
        Io::WriteU32(sdataOverflow, 20, 0x1000);
        bool sdataRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(sdataOverflow, {highLoad, highEhHeader}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            sdataRejected = std::string(error.what()).find("unwind pointer address overflowed") != std::string::npos;
        }
        Check(sdataRejected, passedCases);
        auto sdata8Overflow = Fixture();
        sdata8Overflow[3] = 0x3C;
        Io::WriteU64(sdata8Overflow, 16, 0x1000);
        Io::WriteU64(sdata8Overflow, 24, 0x1000);
        bool sdata8Rejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(sdata8Overflow, {highLoad, highEhHeader}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            sdata8Rejected = std::string(error.what()).find("unwind pointer address overflowed") != std::string::npos;
        }
        Check(sdata8Rejected, passedCases);
        auto sdata2Overflow = Fixture();
        sdata2Overflow[3] = 0x3A;
        Io::WriteU16(sdata2Overflow, 16, 0x1000);
        Io::WriteU16(sdata2Overflow, 18, 0x1000);
        bool sdata2Rejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(sdata2Overflow, {highLoad, highEhHeader}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            sdata2Rejected = std::string(error.what()).find("unwind pointer address overflowed") != std::string::npos;
        }
        Check(sdata2Rejected, passedCases);
        std::vector<std::uint8_t> lsdaOverflowBytes(0x400, 0);
        Io::WriteU32(lsdaOverflowBytes, 0x200, 25);
        lsdaOverflowBytes[0x208] = 1;
        const std::uint8_t ciePrefix[] = {'z', 'P', 'L', 'R', 0, 1, 0, 16, 11, 0};
        std::copy(std::begin(ciePrefix), std::end(ciePrefix), lsdaOverflowBytes.begin() + 0x209);
        lsdaOverflowBytes[0x212] = 0;
        Io::WriteU64(lsdaOverflowBytes, 0x213, 0x1000);
        lsdaOverflowBytes[0x21B] = 0x4B;
        lsdaOverflowBytes[0x21C] = 0;
        Io::WriteU32(lsdaOverflowBytes, 0x220, 29);
        Io::WriteU32(lsdaOverflowBytes, 0x224, 0x24);
        Io::WriteU64(lsdaOverflowBytes, 0x228, 0x1000);
        Io::WriteU64(lsdaOverflowBytes, 0x230, 0x20);
        lsdaOverflowBytes[0x238] = 4;
        Io::WriteU32(lsdaOverflowBytes, 0x239, 0xFFFFEFFF);
        lsdaOverflowBytes[0x300] = 1;
        lsdaOverflowBytes[0x302] = 3;
        Io::WriteU64(lsdaOverflowBytes, 0x304, 0x200);
        Io::WriteU32(lsdaOverflowBytes, 0x30C, 1);
        Io::WriteU64(lsdaOverflowBytes, 0x310, 0x1000);
        Io::WriteU64(lsdaOverflowBytes, 0x318, 0x220);
        Domain::ProgramHeader lsdaLoad{}; lsdaLoad.Type = 1; lsdaLoad.Flags = 4; lsdaLoad.FileSize = lsdaOverflowBytes.size(); lsdaLoad.MemorySize = lsdaOverflowBytes.size(); lsdaLoad.Alignment = 8;
        Domain::ProgramHeader lsdaEh = lsdaLoad; lsdaEh.Type = 0x6474E550; lsdaEh.Offset = 0x300; lsdaEh.MappedAddress = 0x300; lsdaEh.FileSize = 32; lsdaEh.MemorySize = 32; lsdaEh.Alignment = 4;
        bool lsdaOverflowRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(lsdaOverflowBytes, {lsdaLoad, lsdaEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            lsdaOverflowRejected = std::string(error.what()).find("unwind pointer address overflowed") != std::string::npos;
        }
        Check(lsdaOverflowRejected, passedCases);
        lsdaOverflowBytes[0x21B] = 0x49;
        lsdaOverflowBytes[0x238] = 2;
        Io::WriteU32(lsdaOverflowBytes, 0x239, 0x5FFF);
        bool lsdaSlebRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(lsdaOverflowBytes, {lsdaLoad, lsdaEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            lsdaSlebRejected = std::string(error.what()).find("unwind pointer address overflowed") != std::string::npos;
        }
        Check(lsdaSlebRejected, passedCases);
        lsdaOverflowBytes[0x21B] = 0x4C;
        Io::WriteU32(lsdaOverflowBytes, 0x220, 33);
        lsdaOverflowBytes[0x238] = 8;
        Io::WriteU64(lsdaOverflowBytes, 0x239, 0xFFFFFFFFFFFFEFFFULL);
        bool lsda8OverflowRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(lsdaOverflowBytes, {lsdaLoad, lsdaEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            lsda8OverflowRejected = std::string(error.what()).find("unwind pointer address overflowed") != std::string::npos;
        }
        Check(lsda8OverflowRejected, passedCases);
        auto lsdaLeb = lsdaOverflowBytes;
        lsdaLeb[0x21B] = 0x41;
        lsdaLeb[0x21C] = 0;
        Io::WriteU32(lsdaLeb, 0x220, 35);
        Io::WriteU32(lsdaLeb, 0x224, 0x24);
        Io::WriteU64(lsdaLeb, 0x228, 0x1000);
        Io::WriteU64(lsdaLeb, 0x230, 0x20);
        lsdaLeb[0x238] = 10;
        for (int index = 0; index < 9; ++index) lsdaLeb[0x239 + index] = 0x80;
        lsdaLeb[0x242] = 0x82;
        bool lsdaLebRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(lsdaLeb, {lsdaLoad, lsdaEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            lsdaLebRejected = std::string(error.what()).find("overflowing LEB128 value") != std::string::npos;
        }
        Check(lsdaLebRejected, passedCases);
        auto lebOverflow = Fixture();
        lebOverflow[3] = 0x31;
        for (int index = 0; index < 9; ++index) lebOverflow[16 + index] = 0x80;
        lebOverflow[25] = 0x82;
        Domain::ProgramHeader lebLoad{}; lebLoad.Type = 1; lebLoad.MappedAddress = 0xFFFFFFFFFFFFF000ULL; lebLoad.FileSize = 128;
        Domain::ProgramHeader lebEh = lebLoad; lebEh.Type = 0x6474E550; lebEh.FileSize = 32;
        bool lebRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(lebOverflow, {lebLoad, lebEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            lebRejected = std::string(error.what()).find("overflowing LEB128 value") != std::string::npos;
        }
        Check(lebRejected, passedCases);
        std::vector<std::uint8_t> lsdaUdata4Bytes(0x400, 0);
        Io::WriteU32(lsdaUdata4Bytes, 0x200, 25);
        lsdaUdata4Bytes[0x208] = 1;
        std::copy(std::begin(ciePrefix), std::end(ciePrefix), lsdaUdata4Bytes.begin() + 0x209);
        lsdaUdata4Bytes[0x212] = 0;
        Io::WriteU64(lsdaUdata4Bytes, 0x213, 0x1000);
        lsdaUdata4Bytes[0x21B] = 0x43;
        lsdaUdata4Bytes[0x21C] = 0;
        Io::WriteU32(lsdaUdata4Bytes, 0x220, 29);
        Io::WriteU32(lsdaUdata4Bytes, 0x224, 0x24);
        Io::WriteU64(lsdaUdata4Bytes, 0x228, 0xFFFFFFFFFFFFF000ULL);
        Io::WriteU64(lsdaUdata4Bytes, 0x230, 0x20);
        lsdaUdata4Bytes[0x238] = 4;
        Io::WriteU32(lsdaUdata4Bytes, 0x239, 0x1000);
        lsdaUdata4Bytes[0x300] = 1;
        lsdaUdata4Bytes[0x302] = 3;
        Io::WriteU64(lsdaUdata4Bytes, 0x304, 0x200);
        Io::WriteU32(lsdaUdata4Bytes, 0x30C, 1);
        Io::WriteU64(lsdaUdata4Bytes, 0x310, 0xFFFFFFFFFFFFF000ULL);
        Io::WriteU64(lsdaUdata4Bytes, 0x318, 0x220);
        Domain::ProgramHeader lsdaUdata4Load{}; lsdaUdata4Load.Type = 1; lsdaUdata4Load.Flags = 4; lsdaUdata4Load.FileSize = lsdaUdata4Bytes.size(); lsdaUdata4Load.MemorySize = lsdaUdata4Bytes.size(); lsdaUdata4Load.Alignment = 8;
        Domain::ProgramHeader lsdaUdata4Eh = lsdaUdata4Load; lsdaUdata4Eh.Type = 0x6474E550; lsdaUdata4Eh.Offset = 0x300; lsdaUdata4Eh.MappedAddress = 0x300; lsdaUdata4Eh.FileSize = 32; lsdaUdata4Eh.MemorySize = 32; lsdaUdata4Eh.Alignment = 4;
        bool lsdaUdata4Rejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(lsdaUdata4Bytes, {lsdaUdata4Load, lsdaUdata4Eh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            lsdaUdata4Rejected = std::string(error.what()).find("unwind pointer address overflowed") != std::string::npos;
        }
        Check(lsdaUdata4Rejected, passedCases);
        std::vector<std::uint8_t> lsdaU2Bytes(0x400, 0);
        Io::WriteU32(lsdaU2Bytes, 0x200, 25);
        lsdaU2Bytes[0x208] = 1;
        std::copy(std::begin(ciePrefix), std::end(ciePrefix), lsdaU2Bytes.begin() + 0x209);
        lsdaU2Bytes[0x212] = 0;
        Io::WriteU64(lsdaU2Bytes, 0x213, 0x1000);
        lsdaU2Bytes[0x21B] = 0x42;
        lsdaU2Bytes[0x21C] = 0;
        Io::WriteU32(lsdaU2Bytes, 0x220, 29);
        Io::WriteU32(lsdaU2Bytes, 0x224, 0x24);
        Io::WriteU64(lsdaU2Bytes, 0x228, 0xFFFFFFFFFFFFF000ULL);
        Io::WriteU64(lsdaU2Bytes, 0x230, 0x20);
        lsdaU2Bytes[0x238] = 2;
        Io::WriteU16(lsdaU2Bytes, 0x239, 0xFFFF);
        lsdaU2Bytes[0x300] = 1;
        lsdaU2Bytes[0x302] = 3;
        Io::WriteU64(lsdaU2Bytes, 0x304, 0x200);
        Io::WriteU32(lsdaU2Bytes, 0x30C, 1);
        Io::WriteU64(lsdaU2Bytes, 0x310, 0xFFFFFFFFFFFFF000ULL);
        Io::WriteU64(lsdaU2Bytes, 0x318, 0x220);
        Domain::ProgramHeader lsdaU2Load{}; lsdaU2Load.Type = 1; lsdaU2Load.Flags = 4; lsdaU2Load.FileSize = lsdaU2Bytes.size(); lsdaU2Load.MemorySize = lsdaU2Bytes.size(); lsdaU2Load.Alignment = 8;
        Domain::ProgramHeader lsdaU2Eh = lsdaU2Load; lsdaU2Eh.Type = 0x6474E550; lsdaU2Eh.Offset = 0x300; lsdaU2Eh.MappedAddress = 0x300; lsdaU2Eh.FileSize = 32; lsdaU2Eh.MemorySize = 32; lsdaU2Eh.Alignment = 4;
        bool lsdaU2Rejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(lsdaU2Bytes, {lsdaU2Load, lsdaU2Eh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            lsdaU2Rejected = std::string(error.what()).find("unwind pointer address overflowed") != std::string::npos;
        }
        Check(lsdaU2Rejected, passedCases);
        std::vector<std::uint8_t> lsdaUdata8Bytes(0x400, 0);
        Io::WriteU32(lsdaUdata8Bytes, 0x200, 25);
        lsdaUdata8Bytes[0x208] = 1;
        std::copy(std::begin(ciePrefix), std::end(ciePrefix), lsdaUdata8Bytes.begin() + 0x209);
        lsdaUdata8Bytes[0x212] = 0;
        Io::WriteU64(lsdaUdata8Bytes, 0x213, 0x1000);
        lsdaUdata8Bytes[0x21B] = 0x44;
        lsdaUdata8Bytes[0x21C] = 0;
        Io::WriteU32(lsdaUdata8Bytes, 0x220, 33);
        Io::WriteU32(lsdaUdata8Bytes, 0x224, 0x24);
        Io::WriteU64(lsdaUdata8Bytes, 0x228, 0xFFFFFFFFFFFFF000ULL);
        Io::WriteU64(lsdaUdata8Bytes, 0x230, 0x20);
        lsdaUdata8Bytes[0x238] = 8;
        Io::WriteU64(lsdaUdata8Bytes, 0x239, 0x1000);
        lsdaUdata8Bytes[0x300] = 1;
        lsdaUdata8Bytes[0x302] = 3;
        Io::WriteU64(lsdaUdata8Bytes, 0x304, 0x200);
        Io::WriteU32(lsdaUdata8Bytes, 0x30C, 1);
        Io::WriteU64(lsdaUdata8Bytes, 0x310, 0xFFFFFFFFFFFFF000ULL);
        Io::WriteU64(lsdaUdata8Bytes, 0x318, 0x220);
        bool lsdaUdata8Rejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(lsdaUdata8Bytes, {lsdaLoad, lsdaEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            lsdaUdata8Rejected = std::string(error.what()).find("unwind pointer address overflowed") != std::string::npos;
        }
        Check(lsdaUdata8Rejected, passedCases);
        std::vector<std::uint8_t> lsdaS2Bytes(0x400, 0);
        Io::WriteU32(lsdaS2Bytes, 0x200, 25);
        lsdaS2Bytes[0x208] = 1;
        std::copy(std::begin(ciePrefix), std::end(ciePrefix), lsdaS2Bytes.begin() + 0x209);
        lsdaS2Bytes[0x212] = 0;
        Io::WriteU64(lsdaS2Bytes, 0x213, 0x1000);
        lsdaS2Bytes[0x21B] = 0x4A;
        lsdaS2Bytes[0x21C] = 0;
        Io::WriteU32(lsdaS2Bytes, 0x220, 29);
        Io::WriteU32(lsdaS2Bytes, 0x224, 0x24);
        Io::WriteU64(lsdaS2Bytes, 0x228, 0x1000);
        Io::WriteU64(lsdaS2Bytes, 0x230, 0x20);
        lsdaS2Bytes[0x238] = 2;
        Io::WriteU16(lsdaS2Bytes, 0x239, 0xEFFF);
        lsdaS2Bytes[0x300] = 1;
        lsdaS2Bytes[0x302] = 3;
        Io::WriteU64(lsdaS2Bytes, 0x304, 0x200);
        Io::WriteU32(lsdaS2Bytes, 0x30C, 1);
        Io::WriteU64(lsdaS2Bytes, 0x310, 0x1000);
        Io::WriteU64(lsdaS2Bytes, 0x318, 0x220);
        Domain::ProgramHeader lsdaS2Load{}; lsdaS2Load.Type = 1; lsdaS2Load.Flags = 4; lsdaS2Load.FileSize = lsdaS2Bytes.size(); lsdaS2Load.MemorySize = lsdaS2Bytes.size(); lsdaS2Load.Alignment = 8;
        Domain::ProgramHeader lsdaS2Eh = lsdaS2Load; lsdaS2Eh.Type = 0x6474E550; lsdaS2Eh.Offset = 0x300; lsdaS2Eh.MappedAddress = 0x300; lsdaS2Eh.FileSize = 32; lsdaS2Eh.MemorySize = 32; lsdaS2Eh.Alignment = 4;
        bool lsdaS2Rejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(lsdaS2Bytes, {lsdaS2Load, lsdaS2Eh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            lsdaS2Rejected = std::string(error.what()).find("unwind pointer address overflowed") != std::string::npos;
        }
        Check(lsdaS2Rejected, passedCases);
        Domain::ProgramHeader validText{}; validText.Type = 1; validText.Flags = 5; validText.MappedAddress = 0x2000; validText.FileSize = 0x100;
        const auto inText = Relinker::UnusedNidFilter::ReadExceptionFunctions(Fixture(), {load, eh, validText}, {}, {});
        Check(inText.size() == 1 && inText[0].Begin == 0x2000 && inText[0].End == 0x2010, passedCases);
        auto validLsdaBytes = Fixture();
        validLsdaBytes.resize(160, 0);
        Io::WriteU32(validLsdaBytes, 64, 25);
        validLsdaBytes[72] = 1;
        const std::vector<std::uint8_t> aug = {'z', 'P', 'L', 'R', 0, 1, 0x78, 16, 11, 0};
        std::copy(aug.begin(), aug.end(), validLsdaBytes.begin() + 73);
        Io::WriteU64(validLsdaBytes, 83, 0x2000);
        validLsdaBytes[91] = 0; validLsdaBytes[92] = 0;
        Io::WriteU32(validLsdaBytes, 96, 29);
        Io::WriteU32(validLsdaBytes, 100, 36);
        Io::WriteU64(validLsdaBytes, 104, 0x2000);
        Io::WriteU64(validLsdaBytes, 112, 0x10);
        validLsdaBytes[120] = 8;
        Io::WriteU64(validLsdaBytes, 121, 0x1088);
        const std::vector<std::uint8_t> lsdaTable = {0xFF, 0xFF, 1, 4, 0, 1, 0x40, 0};
        std::copy(lsdaTable.begin(), lsdaTable.end(), validLsdaBytes.begin() + 136);
        auto validLsdaLoad = load; validLsdaLoad.FileSize = 160;
        const auto withLsda = Relinker::UnusedNidFilter::ReadExceptionFunctions(validLsdaBytes, {validLsdaLoad, eh, validText}, {}, {});
        Check(withLsda.size() == 1 && withLsda[0].ExtraTargets.size() == 2 && withLsda[0].ExtraTargets[1] == 0x2040, passedCases);
        Domain::ProgramHeader smallText{}; smallText.Type = 1; smallText.Flags = 5; smallText.MappedAddress = 0x2000; smallText.FileSize = 0x20;
        bool validLsdaRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(validLsdaBytes, {validLsdaLoad, eh, smallText}, {}, {}); }
        catch (const Relinker::RelinkerException&) { validLsdaRejected = true; }
        Check(validLsdaRejected, passedCases);
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
        extraBytes[72] = 1;
        extraBytes[73] = 'z'; extraBytes[74] = 'R'; extraBytes[75] = 0;
        extraBytes[76] = 1; extraBytes[77] = 0x78; extraBytes[78] = 16;
        extraBytes[79] = 3;
        extraBytes[80] = 0;
        extraBytes[81] = 0xAA; extraBytes[82] = 0xBB;
        Io::WriteU32(extraBytes, 64, 15);
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
