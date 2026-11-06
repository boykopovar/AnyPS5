#include <relinker/analysis/UnusedNidFilter/EhFrameReader.hpp>
#include <relinker/analysis/ExceptionTableParser.hpp>
#include <io/BufferUtils.hpp>
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

static void Require(bool ok) { if (!ok) throw std::runtime_error("Unwind record validation failed"); }

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
        Domain::ProgramHeader load{}; load.Type = 1; load.MappedAddress = 0x1000; load.FileSize = 128;
        auto eh = load; eh.Type = 0x6474E550; eh.FileSize = 32;
        const auto regions = Relinker::UnusedNidFilter::ReadExceptionFunctions(Fixture(), {load, eh}, {}, {});
        Require(regions.size() == 1 && regions[0].Begin == 0x2000 && regions[0].End == 0x2010);
        auto second = load; second.MappedAddress = 0x1048; second.Offset = 72; second.FileSize = 56;
        auto first = load; first.FileSize = 72;
        Require(Relinker::UnusedNidFilter::ReadExceptionFunctions(Fixture(), {first, second, eh}, {}, {}).size() == 1);
        auto belowBytes = Fixture();
        belowBytes[3] = 0x30;
        Io::WriteU64(belowBytes, 16, 0x800 - 0x1000);
        Io::WriteU64(belowBytes, 24, 0x1060 - 0x1000);
        Io::WriteU64(belowBytes, 104, 0x800);
        Domain::ProgramHeader funcSeg{}; funcSeg.Type = 1; funcSeg.MappedAddress = 0x800; funcSeg.FileSize = 0x20;
        const auto below = Relinker::UnusedNidFilter::ReadExceptionFunctions(belowBytes, {funcSeg, load, eh}, {}, {});
        Require(below.size() == 1 && below[0].Begin == 0x800 && below[0].End == 0x810);
        auto belowDatarelUdata8 = Fixture();
        belowDatarelUdata8[3] = 0x34;
        Io::WriteU64(belowDatarelUdata8, 16, 0x800 - 0x1000);
        Io::WriteU64(belowDatarelUdata8, 24, 0x1060 - 0x1000);
        Io::WriteU64(belowDatarelUdata8, 104, 0x800);
        Require(Relinker::UnusedNidFilter::ReadExceptionFunctions(belowDatarelUdata8, {funcSeg, load, eh}, {}, {}).size() == 1);
        auto belowPcrelUdata8 = Fixture();
        belowPcrelUdata8[3] = 0x14;
        Io::WriteU64(belowPcrelUdata8, 16, 0x800 - 0x1010);
        Io::WriteU64(belowPcrelUdata8, 24, 0x1060 - 0x1018);
        Io::WriteU64(belowPcrelUdata8, 104, 0x800);
        Require(Relinker::UnusedNidFilter::ReadExceptionFunctions(belowPcrelUdata8, {funcSeg, load, eh}, {}, {}).size() == 1);
        auto empty = Fixture();
        Io::WriteU64(empty, 112, 0);
        Require(Relinker::UnusedNidFilter::ReadExceptionFunctions(empty, {load, eh}, {}, {}).empty());
        for (int kind = 0; kind < 13; ++kind) {
            auto bytes = Fixture(); auto segment = load;
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
                    eh.MappedAddress = 0xFFFFFFFFFFFF0000ULL;
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
            }
            bool rejected = false;
            std::vector<Domain::ProgramHeader> headers = {segment, eh};
            if (kind == 8) {
                Domain::ProgramHeader high{}; high.Type = 1; high.MappedAddress = 0xFFFFFFFFFFFFF000ULL; high.FileSize = 0x100;
                headers.push_back(high);
            }
            if (kind == 10) {
                Domain::ProgramHeader funcSeg{}; funcSeg.Type = 1; funcSeg.MappedAddress = 0x800; funcSeg.FileSize = 0x20;
                headers = {segment, eh, funcSeg};
            }
            try { Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {}); }
            catch (const std::exception&) { rejected = true; }
            Require(rejected);
        }
        auto tableOverflow = Fixture();
        tableOverflow[3] = 0x39;
        tableOverflow[16] = 0x80;
        tableOverflow[17] = 0x20;
        tableOverflow[18] = 0x80;
        tableOverflow[19] = 0x20;
        Domain::ProgramHeader highLoad{}; highLoad.Type = 1; highLoad.MappedAddress = 0xFFFFFFFFFFFFF000ULL; highLoad.FileSize = 128;
        Domain::ProgramHeader highEh = highLoad; highEh.Type = 0x6474E550; highEh.FileSize = 32;
        bool overflowRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(tableOverflow, {highLoad, highEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            overflowRejected = std::string(error.what()).find("unwind pointer address overflowed") != std::string::npos;
        }
        Require(overflowRejected);
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
        Require(udata4Rejected);
        auto sdataOverflow = Fixture();
        sdataOverflow[3] = 0x3B;
        Io::WriteU32(sdataOverflow, 16, 0x1000);
        Io::WriteU32(sdataOverflow, 20, 0x1000);
        bool sdataRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(sdataOverflow, {highLoad, highEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            sdataRejected = std::string(error.what()).find("unwind pointer address overflowed") != std::string::npos;
        }
        Require(sdataRejected);
        auto sdata8Overflow = Fixture();
        sdata8Overflow[3] = 0x3C;
        Io::WriteU64(sdata8Overflow, 16, 0x1000);
        Io::WriteU64(sdata8Overflow, 24, 0x1000);
        bool sdata8Rejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(sdata8Overflow, {highLoad, highEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            sdata8Rejected = std::string(error.what()).find("unwind pointer address overflowed") != std::string::npos;
        }
        Require(sdata8Rejected);
        auto sdata2Overflow = Fixture();
        sdata2Overflow[3] = 0x3A;
        Io::WriteU16(sdata2Overflow, 16, 0x1000);
        Io::WriteU16(sdata2Overflow, 18, 0x1000);
        bool sdata2Rejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(sdata2Overflow, {highLoad, highEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            sdata2Rejected = std::string(error.what()).find("unwind pointer address overflowed") != std::string::npos;
        }
        Require(sdata2Rejected);
        std::vector<std::uint8_t> lsdaBytes(0x400, 0);
        Io::WriteU32(lsdaBytes, 0x200, 25);
        lsdaBytes[0x208] = 1;
        const std::uint8_t ciePrefix[] = {'z', 'P', 'L', 'R', 0, 1, 0, 0x78, 11, 0};
        std::copy(std::begin(ciePrefix), std::end(ciePrefix), lsdaBytes.begin() + 0x209);
        lsdaBytes[0x212] = 0;
        Io::WriteU64(lsdaBytes, 0x213, 0x1000);
        lsdaBytes[0x21B] = 0x4B;
        lsdaBytes[0x21C] = 0;
        Io::WriteU32(lsdaBytes, 0x220, 29);
        Io::WriteU32(lsdaBytes, 0x224, 0x24);
        Io::WriteU64(lsdaBytes, 0x228, 0x1000);
        Io::WriteU64(lsdaBytes, 0x230, 0x20);
        lsdaBytes[0x238] = 4;
        Io::WriteU32(lsdaBytes, 0x239, 0xFFFFEFFF);
        lsdaBytes[0x300] = 1;
        lsdaBytes[0x302] = 3;
        Io::WriteU64(lsdaBytes, 0x304, 0x200);
        Io::WriteU32(lsdaBytes, 0x30C, 1);
        Io::WriteU64(lsdaBytes, 0x310, 0x1000);
        Io::WriteU64(lsdaBytes, 0x318, 0x220);
        Domain::ProgramHeader lsdaLoad{}; lsdaLoad.Type = 1; lsdaLoad.Flags = 4; lsdaLoad.FileSize = lsdaBytes.size(); lsdaLoad.MemorySize = lsdaBytes.size(); lsdaLoad.Alignment = 8;
        Domain::ProgramHeader lsdaEh = lsdaLoad; lsdaEh.Type = 0x6474E550; lsdaEh.Offset = 0x300; lsdaEh.MappedAddress = 0x300; lsdaEh.FileSize = 32; lsdaEh.MemorySize = 32; lsdaEh.Alignment = 4;
        bool lsdaOverflowRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(lsdaBytes, {lsdaLoad, lsdaEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            lsdaOverflowRejected = std::string(error.what()).find("unwind pointer address overflowed") != std::string::npos;
        }
        Require(lsdaOverflowRejected);
        lsdaBytes[0x21B] = 0x49;
        lsdaBytes[0x238] = 2;
        Io::WriteU32(lsdaBytes, 0x239, 0x5FFF);
        bool lsdaSlebRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(lsdaBytes, {lsdaLoad, lsdaEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            lsdaSlebRejected = std::string(error.what()).find("unwind pointer address overflowed") != std::string::npos;
        }
        Require(lsdaSlebRejected);
        lsdaBytes[0x21B] = 0x4C;
        Io::WriteU32(lsdaBytes, 0x220, 33);
        lsdaBytes[0x238] = 8;
        Io::WriteU64(lsdaBytes, 0x239, 0xFFFFFFFFFFFFEFFFULL);
        bool lsda8OverflowRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(lsdaBytes, {lsdaLoad, lsdaEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            lsda8OverflowRejected = std::string(error.what()).find("unwind pointer address overflowed") != std::string::npos;
        }
        Require(lsda8OverflowRejected);
        auto lsdaLeb = lsdaBytes;
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
        Require(lsdaLebRejected);
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
        Require(lebRejected);
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
        Require(lsdaUdata4Rejected);
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
        Require(lsdaU2Rejected);
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
        Require(lsdaUdata8Rejected);
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
        Require(lsdaS2Rejected);
        Domain::ProgramHeader augLoad{}; augLoad.Type = 1; augLoad.MappedAddress = 0x1000; augLoad.FileSize = 128;
        Domain::ProgramHeader augEh = augLoad; augEh.Type = 0x6474E550; augEh.FileSize = 32;
        auto unterminatedAugBytes = Fixture();
        unterminatedAugBytes[73] = 'z';
        unterminatedAugBytes[74] = 'R';
        unterminatedAugBytes[75] = 'L';
        unterminatedAugBytes[76] = 'P';
        bool unterminatedAugRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(unterminatedAugBytes, {augLoad, augEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            unterminatedAugRejected = std::string(error.what()).find("unterminated CIE augmentation") != std::string::npos;
        }
        Require(unterminatedAugRejected);
        auto outOfBoundsAugBytes = Fixture();
        Io::WriteU32(outOfBoundsAugBytes, 64, 12);
        outOfBoundsAugBytes[73] = 'z';
        outOfBoundsAugBytes[74] = 0;
        outOfBoundsAugBytes[75] = 1;
        outOfBoundsAugBytes[76] = 0x78;
        outOfBoundsAugBytes[77] = 16;
        outOfBoundsAugBytes[78] = 10;
        bool outOfBoundsAugRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(outOfBoundsAugBytes, {augLoad, augEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            outOfBoundsAugRejected = std::string(error.what()).find("CIE augmentation exceeds record") != std::string::npos;
        }
        Require(outOfBoundsAugRejected);
        auto unterminatedRecordBytes = Fixture();
        Io::WriteU32(unterminatedRecordBytes, 64, 8);
        bool unterminatedRecordRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(unterminatedRecordBytes, {augLoad, augEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            unterminatedRecordRejected = std::string(error.what()).find("CIE fields exceed record") != std::string::npos;
        }
        Require(unterminatedRecordRejected);
        auto fdeZeroBytes = Fixture();
        Io::WriteU32(fdeZeroBytes, 96, 0);
        bool fdeZeroRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(fdeZeroBytes, {augLoad, augEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            fdeZeroRejected = std::string(error.what()).find("unsupported FDE record size") != std::string::npos;
        }
        Require(fdeZeroRejected);
        auto fdeOverflowBytes = Fixture();
        Io::WriteU32(fdeOverflowBytes, 96, 1000);
        bool fdeOverflowRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(fdeOverflowBytes, {augLoad, augEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            fdeOverflowRejected = std::string(error.what()).find("unwind range is not file-backed") != std::string::npos;
        }
        Require(fdeOverflowRejected);
        std::vector<std::uint8_t> landingPadBytes(0x400, 0);
        Io::WriteU32(landingPadBytes, 0x200, 25);
        landingPadBytes[0x208] = 1;
        std::copy(std::begin(ciePrefix), std::end(ciePrefix), landingPadBytes.begin() + 0x209);
        landingPadBytes[0x212] = 0;
        Io::WriteU64(landingPadBytes, 0x213, 0x1000);
        landingPadBytes[0x21B] = 0;
        landingPadBytes[0x21C] = 0;
        Io::WriteU32(landingPadBytes, 0x220, 29);
        Io::WriteU32(landingPadBytes, 0x224, 0x24);
        Io::WriteU64(landingPadBytes, 0x228, 0x1000);
        Io::WriteU64(landingPadBytes, 0x230, 0x20);
        landingPadBytes[0x238] = 8;
        Io::WriteU64(landingPadBytes, 0x239, 0x280);
        landingPadBytes[0x280] = 0;
        Io::WriteU64(landingPadBytes, 0x281, 0x1000);
        landingPadBytes[0x289] = 0xFF;
        landingPadBytes[0x28A] = 11;
        landingPadBytes[0x28B] = 13;
        Io::WriteU32(landingPadBytes, 0x28C, 0);
        Io::WriteU32(landingPadBytes, 0x290, 1);
        Io::WriteU32(landingPadBytes, 0x294, static_cast<std::uint32_t>(-0x1001));
        landingPadBytes[0x298] = 0;
        landingPadBytes[0x300] = 1;
        landingPadBytes[0x302] = 3;
        Io::WriteU64(landingPadBytes, 0x304, 0x200);
        Io::WriteU32(landingPadBytes, 0x30C, 1);
        Io::WriteU64(landingPadBytes, 0x310, 0x1000);
        Io::WriteU64(landingPadBytes, 0x318, 0x220);
        Domain::ProgramHeader landingPadLoad{}; landingPadLoad.Type = 1; landingPadLoad.Flags = 4; landingPadLoad.FileSize = landingPadBytes.size(); landingPadLoad.MemorySize = landingPadBytes.size(); landingPadLoad.Alignment = 8;
        Domain::ProgramHeader landingPadEh = landingPadLoad; landingPadEh.Type = 0x6474E550; landingPadEh.Offset = 0x300; landingPadEh.MappedAddress = 0x300; landingPadEh.FileSize = 32; landingPadEh.MemorySize = 32; landingPadEh.Alignment = 4;
        bool landingPadUnderflowRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(landingPadBytes, {landingPadLoad, landingPadEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            landingPadUnderflowRejected = std::string(error.what()).find("LSDA landing pad address overflowed") != std::string::npos;
        }
        Require(landingPadUnderflowRejected);
        auto landingPadPosBytes = landingPadBytes;
        Io::WriteU64(landingPadPosBytes, 0x281, std::numeric_limits<std::uint64_t>::max() - 16);
        Io::WriteU32(landingPadPosBytes, 0x294, 17);
        bool landingPadOverflowRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(landingPadPosBytes, {landingPadLoad, landingPadEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            landingPadOverflowRejected = std::string(error.what()).find("LSDA landing pad address overflowed") != std::string::npos;
        }
        Require(landingPadOverflowRejected);
        auto lsdaBaseFixture = []() {
            std::vector<std::uint8_t> bytes(0x400, 0);
            Io::WriteU32(bytes, 0x200, 25);
            bytes[0x208] = 1;
            const std::uint8_t prefix[] = {'z', 'P', 'L', 'R', 0, 1, 0, 0x78, 11, 0};
            std::copy(std::begin(prefix), std::end(prefix), bytes.begin() + 0x209);
            bytes[0x212] = 0;
            Io::WriteU64(bytes, 0x213, 0x1000);
            bytes[0x21B] = 0;
            bytes[0x21C] = 0;
            Io::WriteU32(bytes, 0x220, 33);
            Io::WriteU32(bytes, 0x224, 0x24);
            Io::WriteU64(bytes, 0x228, 0x2000);
            Io::WriteU64(bytes, 0x230, 0x20);
            bytes[0x238] = 8;
            Io::WriteU64(bytes, 0x239, 0x280);
            bytes[0x300] = 1;
            bytes[0x302] = 3;
            Io::WriteU64(bytes, 0x304, 0x200);
            Io::WriteU32(bytes, 0x30C, 1);
            Io::WriteU64(bytes, 0x310, 0x2000);
            Io::WriteU64(bytes, 0x318, 0x220);
            return bytes;
        };
        Domain::ProgramHeader lsdaHdrLoad{}; lsdaHdrLoad.Type = 1; lsdaHdrLoad.Flags = 4; lsdaHdrLoad.FileSize = 0x400; lsdaHdrLoad.MemorySize = 0x400; lsdaHdrLoad.Alignment = 8;
        Domain::ProgramHeader lsdaHdrEh = lsdaHdrLoad; lsdaHdrEh.Type = 0x6474E550; lsdaHdrEh.Offset = 0x300; lsdaHdrEh.MappedAddress = 0x300; lsdaHdrEh.FileSize = 32; lsdaHdrEh.MemorySize = 32; lsdaHdrEh.Alignment = 4;
        auto lsdaPosSdata4 = lsdaBaseFixture();
        lsdaPosSdata4[0x280] = 0xFF;
        lsdaPosSdata4[0x281] = 0xFF;
        lsdaPosSdata4[0x282] = 0x0B;
        lsdaPosSdata4[0x283] = 13;
        Io::WriteU32(lsdaPosSdata4, 0x284, 0);
        Io::WriteU32(lsdaPosSdata4, 0x288, 0x10);
        Io::WriteU32(lsdaPosSdata4, 0x28C, 0x40);
        lsdaPosSdata4[0x290] = 0;
        const auto resPosSdata4 = Relinker::UnusedNidFilter::ReadExceptionFunctions(lsdaPosSdata4, {lsdaHdrLoad, lsdaHdrEh}, {}, {});
        Require(resPosSdata4.size() == 1 && resPosSdata4[0].Begin == 0x2000 && resPosSdata4[0].End == 0x2020);
        const std::vector<Domain::VirtualAddress> expectedPosSdata4 = {0x1000, 0x2040};
        Require(resPosSdata4[0].ExtraTargets == expectedPosSdata4);
        auto lsdaNegSdata4 = lsdaBaseFixture();
        lsdaNegSdata4[0x280] = 0xFF;
        lsdaNegSdata4[0x281] = 0xFF;
        lsdaNegSdata4[0x282] = 0x0B;
        lsdaNegSdata4[0x283] = 13;
        Io::WriteU32(lsdaNegSdata4, 0x284, 0);
        Io::WriteU32(lsdaNegSdata4, 0x288, 0x10);
        Io::WriteU32(lsdaNegSdata4, 0x28C, static_cast<std::uint32_t>(-0x200));
        lsdaNegSdata4[0x290] = 0;
        const auto resNegSdata4 = Relinker::UnusedNidFilter::ReadExceptionFunctions(lsdaNegSdata4, {lsdaHdrLoad, lsdaHdrEh}, {}, {});
        Require(resNegSdata4.size() == 1);
        const std::vector<Domain::VirtualAddress> expectedNegSdata4 = {0x1000, 0x1E00};
        Require(resNegSdata4[0].ExtraTargets == expectedNegSdata4);
        auto lsdaNegSdata2 = lsdaBaseFixture();
        lsdaNegSdata2[0x280] = 0xFF;
        lsdaNegSdata2[0x281] = 0xFF;
        lsdaNegSdata2[0x282] = 0x0A;
        lsdaNegSdata2[0x283] = 7;
        Io::WriteU16(lsdaNegSdata2, 0x284, 0);
        Io::WriteU16(lsdaNegSdata2, 0x286, 0x10);
        Io::WriteU16(lsdaNegSdata2, 0x288, static_cast<std::uint16_t>(-0x80));
        lsdaNegSdata2[0x28A] = 0;
        const auto resNegSdata2 = Relinker::UnusedNidFilter::ReadExceptionFunctions(lsdaNegSdata2, {lsdaHdrLoad, lsdaHdrEh}, {}, {});
        Require(resNegSdata2.size() == 1);
        const std::vector<Domain::VirtualAddress> expectedNegSdata2 = {0x1000, 0x1F80};
        Require(resNegSdata2[0].ExtraTargets == expectedNegSdata2);
        auto lsdaNegSleb = lsdaBaseFixture();
        lsdaNegSleb[0x280] = 0xFF;
        lsdaNegSleb[0x281] = 0xFF;
        lsdaNegSleb[0x282] = 0x09;
        lsdaNegSleb[0x283] = 4;
        lsdaNegSleb[0x284] = 0;
        lsdaNegSleb[0x285] = 0x10;
        lsdaNegSleb[0x286] = 0x40;
        lsdaNegSleb[0x287] = 0;
        const auto resNegSleb = Relinker::UnusedNidFilter::ReadExceptionFunctions(lsdaNegSleb, {lsdaHdrLoad, lsdaHdrEh}, {}, {});
        Require(resNegSleb.size() == 1);
        const std::vector<Domain::VirtualAddress> expectedNegSleb = {0x1000, 0x1FC0};
        Require(resNegSleb[0].ExtraTargets == expectedNegSleb);
        auto lsdaNegSdata8 = lsdaBaseFixture();
        lsdaNegSdata8[0x280] = 0xFF;
        lsdaNegSdata8[0x281] = 0xFF;
        lsdaNegSdata8[0x282] = 0x0C;
        lsdaNegSdata8[0x283] = 25;
        Io::WriteU64(lsdaNegSdata8, 0x284, 0);
        Io::WriteU64(lsdaNegSdata8, 0x28C, 0x10);
        Io::WriteU64(lsdaNegSdata8, 0x294, static_cast<std::uint64_t>(-0x300LL));
        lsdaNegSdata8[0x29C] = 0;
        const auto resNegSdata8 = Relinker::UnusedNidFilter::ReadExceptionFunctions(lsdaNegSdata8, {lsdaHdrLoad, lsdaHdrEh}, {}, {});
        Require(resNegSdata8.size() == 1);
        const std::vector<Domain::VirtualAddress> expectedNegSdata8 = {0x1000, 0x1D00};
        Require(resNegSdata8[0].ExtraTargets == expectedNegSdata8);
        auto lsdaZeroLanding = lsdaBaseFixture();
        lsdaZeroLanding[0x280] = 0xFF;
        lsdaZeroLanding[0x281] = 0xFF;
        lsdaZeroLanding[0x282] = 0x0B;
        lsdaZeroLanding[0x283] = 13;
        Io::WriteU32(lsdaZeroLanding, 0x284, 0);
        Io::WriteU32(lsdaZeroLanding, 0x288, 0x10);
        Io::WriteU32(lsdaZeroLanding, 0x28C, 0);
        lsdaZeroLanding[0x290] = 0;
        const auto resZeroLanding = Relinker::UnusedNidFilter::ReadExceptionFunctions(lsdaZeroLanding, {lsdaHdrLoad, lsdaHdrEh}, {}, {});
        Require(resZeroLanding.size() == 1);
        const std::vector<Domain::VirtualAddress> expectedZeroLanding = {0x1000};
        Require(resZeroLanding[0].ExtraTargets == expectedZeroLanding);
        auto lsdaUnderflowSdata4 = lsdaBaseFixture();
        lsdaUnderflowSdata4[0x280] = 0xFF;
        lsdaUnderflowSdata4[0x281] = 0xFF;
        lsdaUnderflowSdata4[0x282] = 0x0B;
        lsdaUnderflowSdata4[0x283] = 13;
        Io::WriteU32(lsdaUnderflowSdata4, 0x284, 0);
        Io::WriteU32(lsdaUnderflowSdata4, 0x288, 0x10);
        Io::WriteU32(lsdaUnderflowSdata4, 0x28C, static_cast<std::uint32_t>(-0x2001));
        lsdaUnderflowSdata4[0x290] = 0;
        bool lsdaUnderflowRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(lsdaUnderflowSdata4, {lsdaHdrLoad, lsdaHdrEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            lsdaUnderflowRejected = std::string(error.what()).find("LSDA landing pad address overflowed") != std::string::npos;
        }
        Require(lsdaUnderflowRejected);
        auto lsdaOverflowSdata4 = lsdaBaseFixture();
        lsdaOverflowSdata4[0x280] = 0;
        Io::WriteU64(lsdaOverflowSdata4, 0x281, std::numeric_limits<std::uint64_t>::max() - 15);
        lsdaOverflowSdata4[0x289] = 0xFF;
        lsdaOverflowSdata4[0x28A] = 0x0B;
        lsdaOverflowSdata4[0x28B] = 13;
        Io::WriteU32(lsdaOverflowSdata4, 0x28C, 0);
        Io::WriteU32(lsdaOverflowSdata4, 0x290, 0x10);
        Io::WriteU32(lsdaOverflowSdata4, 0x294, 16);
        lsdaOverflowSdata4[0x298] = 0;
        bool lsdaPosOverflowRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(lsdaOverflowSdata4, {lsdaHdrLoad, lsdaHdrEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            lsdaPosOverflowRejected = std::string(error.what()).find("LSDA landing pad address overflowed") != std::string::npos;
        }
        Require(lsdaPosOverflowRejected);
        auto lsdaUnderflowSdata2 = lsdaBaseFixture();
        lsdaUnderflowSdata2[0x280] = 0;
        Io::WriteU64(lsdaUnderflowSdata2, 0x281, 0x50);
        lsdaUnderflowSdata2[0x289] = 0xFF;
        lsdaUnderflowSdata2[0x28A] = 0x0A;
        lsdaUnderflowSdata2[0x28B] = 7;
        Io::WriteU16(lsdaUnderflowSdata2, 0x28C, 0);
        Io::WriteU16(lsdaUnderflowSdata2, 0x28E, 0x10);
        Io::WriteU16(lsdaUnderflowSdata2, 0x290, static_cast<std::uint16_t>(-0x51));
        lsdaUnderflowSdata2[0x292] = 0;
        bool lsdaUnderflowSdata2Rejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(lsdaUnderflowSdata2, {lsdaHdrLoad, lsdaHdrEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            lsdaUnderflowSdata2Rejected = std::string(error.what()).find("LSDA landing pad address overflowed") != std::string::npos;
        }
        Require(lsdaUnderflowSdata2Rejected);
        auto lsdaTruncatedTable = lsdaBaseFixture();
        lsdaTruncatedTable[0x280] = 0xFF;
        lsdaTruncatedTable[0x281] = 0xFF;
        lsdaTruncatedTable[0x282] = 0x0B;
        lsdaTruncatedTable[0x283] = 10;
        Io::WriteU32(lsdaTruncatedTable, 0x284, 0);
        Io::WriteU32(lsdaTruncatedTable, 0x288, 0x10);
        Io::WriteU32(lsdaTruncatedTable, 0x28C, 0x40);
        lsdaTruncatedTable[0x290] = 0;
        bool lsdaTruncatedRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(lsdaTruncatedTable, {lsdaHdrLoad, lsdaHdrEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            lsdaTruncatedRejected = std::string(error.what()).find("LSDA call-site table size mismatch") != std::string::npos;
        }
        Require(lsdaTruncatedRejected);
        auto lsdaTrailingTable = lsdaBaseFixture();
        lsdaTrailingTable[0x280] = 0xFF;
        lsdaTrailingTable[0x281] = 0xFF;
        lsdaTrailingTable[0x282] = 0x0B;
        lsdaTrailingTable[0x283] = 14;
        Io::WriteU32(lsdaTrailingTable, 0x284, 0);
        Io::WriteU32(lsdaTrailingTable, 0x288, 0x10);
        Io::WriteU32(lsdaTrailingTable, 0x28C, 0x40);
        lsdaTrailingTable[0x290] = 0;
        lsdaTrailingTable[0x291] = 0;
        bool lsdaTrailingRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(lsdaTrailingTable, {lsdaHdrLoad, lsdaHdrEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            lsdaTrailingRejected = std::string(error.what()).find("LSDA call-site table size mismatch") != std::string::npos;
        }
        Require(lsdaTrailingRejected);
        Relinker::ExceptionTableParser parser;
        auto multiLevelLsda = []() {
            std::vector<std::uint8_t> bytes(0x200, 0);
            bytes[0x100] = 0xFF;
            bytes[0x101] = 0xFF;
            bytes[0x102] = 0x0B;
            bytes[0x103] = 39;
            Io::WriteU32(bytes, 0x104, 0);
            Io::WriteU32(bytes, 0x108, 0x10);
            Io::WriteU32(bytes, 0x10C, 0x50);
            bytes[0x110] = 1;
            Io::WriteU32(bytes, 0x111, 0x10);
            Io::WriteU32(bytes, 0x115, 0x10);
            Io::WriteU32(bytes, 0x119, 0x70);
            bytes[0x11D] = 3;
            Io::WriteU32(bytes, 0x11E, 0x20);
            Io::WriteU32(bytes, 0x122, 0x10);
            Io::WriteU32(bytes, 0x126, 0x90);
            bytes[0x12A] = 5;
            bytes[0x12B] = 1;
            bytes[0x12C] = 1;
            bytes[0x12D] = 2;
            bytes[0x12E] = 1;
            bytes[0x12F] = 0;
            bytes[0x130] = 0;
            return bytes;
        };
        const auto nestedParsed = parser.Parse(multiLevelLsda(), 0x100, 0x2000);
        Require(nestedParsed.LandingBase == 0x2000 && nestedParsed.CallSites.size() == 3);
        Require(nestedParsed.CallSites[0].Start == 0 && nestedParsed.CallSites[0].Length == 0x10 && nestedParsed.CallSites[0].LandingPad == 0x2050);
        Require(nestedParsed.CallSites[0].ActionChain.size() == 3);
        Require(nestedParsed.CallSites[0].ActionChain[0].Filter == 1 && nestedParsed.CallSites[0].ActionChain[0].RecordAddress == 0x12B && nestedParsed.CallSites[0].ActionChain[0].NextRecordAddress == 0x12D);
        Require(nestedParsed.CallSites[0].ActionChain[1].Filter == 2 && nestedParsed.CallSites[0].ActionChain[1].RecordAddress == 0x12D && nestedParsed.CallSites[0].ActionChain[1].NextRecordAddress == 0x12F);
        Require(nestedParsed.CallSites[0].ActionChain[2].Filter == 0 && nestedParsed.CallSites[0].ActionChain[2].RecordAddress == 0x12F && nestedParsed.CallSites[0].ActionChain[2].NextRecordAddress == 0);
        Require(nestedParsed.CallSites[0].LinksToCatchAll);
        Require(nestedParsed.CallSites[1].Start == 0x10 && nestedParsed.CallSites[1].Length == 0x10 && nestedParsed.CallSites[1].LandingPad == 0x2070);
        Require(nestedParsed.CallSites[1].ActionChain.size() == 2);
        Require(nestedParsed.CallSites[1].ActionChain[0].Filter == 2 && nestedParsed.CallSites[1].ActionChain[0].NextRecordAddress == 0x12F);
        Require(nestedParsed.CallSites[1].ActionChain[1].Filter == 0 && nestedParsed.CallSites[1].ActionChain[1].NextRecordAddress == 0);
        Require(nestedParsed.CallSites[1].LinksToCatchAll);
        Require(nestedParsed.CallSites[2].Start == 0x20 && nestedParsed.CallSites[2].Length == 0x10 && nestedParsed.CallSites[2].LandingPad == 0x2090);
        Require(nestedParsed.CallSites[2].ActionChain.size() == 1);
        Require(nestedParsed.CallSites[2].ActionChain[0].Filter == 0 && nestedParsed.CallSites[2].ActionChain[0].NextRecordAddress == 0);
        Require(nestedParsed.CallSites[2].LinksToCatchAll);
        auto noCatchAllLsda = []() {
            std::vector<std::uint8_t> bytes(0x200, 0);
            bytes[0x100] = 0xFF;
            bytes[0x101] = 0xFF;
            bytes[0x102] = 0x0B;
            bytes[0x103] = 13;
            Io::WriteU32(bytes, 0x104, 0);
            Io::WriteU32(bytes, 0x108, 0x10);
            Io::WriteU32(bytes, 0x10C, 0x50);
            bytes[0x110] = 1;
            bytes[0x111] = 1;
            bytes[0x112] = 1;
            bytes[0x113] = 2;
            bytes[0x114] = 0;
            return bytes;
        };
        const auto noCatchAllParsed = parser.Parse(noCatchAllLsda(), 0x100, 0x2000);
        Require(noCatchAllParsed.CallSites.size() == 1);
        Require(noCatchAllParsed.CallSites[0].ActionChain.size() == 2);
        Require(noCatchAllParsed.CallSites[0].ActionChain[0].Filter == 1 && noCatchAllParsed.CallSites[0].ActionChain[0].NextRecordAddress == 0x113);
        Require(noCatchAllParsed.CallSites[0].ActionChain[1].Filter == 2 && noCatchAllParsed.CallSites[0].ActionChain[1].NextRecordAddress == 0);
        Require(!noCatchAllParsed.CallSites[0].LinksToCatchAll);
        auto cleanupLsda = []() {
            std::vector<std::uint8_t> bytes(0x200, 0);
            bytes[0x100] = 0xFF;
            bytes[0x101] = 0xFF;
            bytes[0x102] = 0x0B;
            bytes[0x103] = 13;
            Io::WriteU32(bytes, 0x104, 0);
            Io::WriteU32(bytes, 0x108, 0x10);
            Io::WriteU32(bytes, 0x10C, 0x30);
            bytes[0x110] = 0;
            return bytes;
        };
        const auto cleanupParsed = parser.Parse(cleanupLsda(), 0x100, 0x2000);
        Require(cleanupParsed.CallSites.size() == 1);
        Require(cleanupParsed.CallSites[0].LandingPad == 0x2030 && cleanupParsed.CallSites[0].ActionOffset == 0);
        Require(cleanupParsed.CallSites[0].ActionChain.empty() && !cleanupParsed.CallSites[0].LinksToCatchAll);
        auto typeTableCatchAllLsda = []() {
            std::vector<std::uint8_t> bytes(0x200, 0);
            bytes[0x100] = 0xFF;
            bytes[0x101] = 0x03;
            bytes[0x102] = 21;
            bytes[0x103] = 0x0B;
            bytes[0x104] = 13;
            Io::WriteU32(bytes, 0x105, 0);
            Io::WriteU32(bytes, 0x109, 0x10);
            Io::WriteU32(bytes, 0x10D, 0x60);
            bytes[0x111] = 1;
            bytes[0x112] = 1;
            bytes[0x113] = 0;
            Io::WriteU32(bytes, 0x114, 0);
            return bytes;
        };
        const auto typeTableCatchAllParsed = parser.Parse(typeTableCatchAllLsda(), 0x100, 0x2000);
        Require(typeTableCatchAllParsed.CallSites.size() == 1);
        Require(typeTableCatchAllParsed.CallSites[0].ActionChain.size() == 1);
        Require(typeTableCatchAllParsed.CallSites[0].ActionChain[0].Filter == 1 && typeTableCatchAllParsed.CallSites[0].ActionChain[0].NextRecordAddress == 0);
        Require(typeTableCatchAllParsed.CallSites[0].LinksToCatchAll);
        auto backwardLsda = []() {
            std::vector<std::uint8_t> bytes(0x200, 0);
            bytes[0x100] = 0xFF;
            bytes[0x101] = 0xFF;
            bytes[0x102] = 0x0B;
            bytes[0x103] = 13;
            Io::WriteU32(bytes, 0x104, 0);
            Io::WriteU32(bytes, 0x108, 0x10);
            Io::WriteU32(bytes, 0x10C, 0x80);
            bytes[0x110] = 3;
            bytes[0x111] = 0;
            bytes[0x112] = 0;
            bytes[0x113] = 1;
            bytes[0x114] = 0x7D;
            return bytes;
        };
        const auto backwardParsed = parser.Parse(backwardLsda(), 0x100, 0x2000);
        Require(backwardParsed.CallSites.size() == 1);
        Require(backwardParsed.CallSites[0].ActionChain.size() == 2);
        Require(backwardParsed.CallSites[0].ActionChain[0].Filter == 1 && backwardParsed.CallSites[0].ActionChain[0].RecordAddress == 0x113 && backwardParsed.CallSites[0].ActionChain[0].NextRecordAddress == 0x111);
        Require(backwardParsed.CallSites[0].ActionChain[1].Filter == 0 && backwardParsed.CallSites[0].ActionChain[1].RecordAddress == 0x111 && backwardParsed.CallSites[0].ActionChain[1].NextRecordAddress == 0);
        Require(backwardParsed.CallSites[0].LinksToCatchAll);
        auto cyclicLsda = []() {
            std::vector<std::uint8_t> bytes(0x200, 0);
            bytes[0x100] = 0xFF;
            bytes[0x101] = 0xFF;
            bytes[0x102] = 0x0B;
            bytes[0x103] = 13;
            Io::WriteU32(bytes, 0x104, 0);
            Io::WriteU32(bytes, 0x108, 0x10);
            Io::WriteU32(bytes, 0x10C, 0x40);
            bytes[0x110] = 1;
            bytes[0x111] = 1;
            bytes[0x112] = 1;
            bytes[0x113] = 2;
            bytes[0x114] = 0x7D;
            return bytes;
        };
        bool cyclicRejected = false;
        try { parser.Parse(cyclicLsda(), 0x100, 0x2000); }
        catch (const Domain::RelinkerException& error) {
            cyclicRejected = std::string(error.what()).find("cyclic LSDA action record chain") != std::string::npos;
        }
        Require(cyclicRejected);
        auto underflowDispLsda = []() {
            std::vector<std::uint8_t> bytes(0x200, 0);
            bytes[0x100] = 0xFF;
            bytes[0x101] = 0xFF;
            bytes[0x102] = 0x0B;
            bytes[0x103] = 13;
            Io::WriteU32(bytes, 0x104, 0);
            Io::WriteU32(bytes, 0x108, 0x10);
            Io::WriteU32(bytes, 0x10C, 0x40);
            bytes[0x110] = 1;
            bytes[0x111] = 1;
            bytes[0x112] = 0x7A;
            return bytes;
        };
        bool underflowDispRejected = false;
        try { parser.Parse(underflowDispLsda(), 0x100, 0x2000); }
        catch (const Domain::RelinkerException& error) {
            underflowDispRejected = std::string(error.what()).find("LSDA action record address underflow") != std::string::npos;
        }
        Require(underflowDispRejected);
        auto actionExceedsLsda = []() {
            std::vector<std::uint8_t> bytes(0x115, 0);
            bytes[0x100] = 0xFF;
            bytes[0x101] = 0xFF;
            bytes[0x102] = 0x0B;
            bytes[0x103] = 13;
            Io::WriteU32(bytes, 0x104, 0);
            Io::WriteU32(bytes, 0x108, 0x10);
            Io::WriteU32(bytes, 0x10C, 0x40);
            bytes[0x110] = 0x7F;
            return bytes;
        };
        bool actionExceedsRejected = false;
        try { parser.Parse(actionExceedsLsda(), 0x100, 0x2000); }
        catch (const Domain::RelinkerException& error) {
            actionExceedsRejected = std::string(error.what()).find("LSDA action record exceeds table") != std::string::npos;
        }
        Require(actionExceedsRejected);
        Domain::ProgramHeader segLoad{}; segLoad.Type = 1; segLoad.MappedAddress = 0x1000; segLoad.FileSize = 0x80;
        bool nonFileBackedLsdaRejected = false;
        try { parser.Parse(std::vector<std::uint8_t>(0x80, 0), {segLoad}, 0x1090, 0x1000); }
        catch (const Domain::RelinkerException& error) {
            nonFileBackedLsdaRejected = std::string(error.what()).find("unwind address is not file-backed") != std::string::npos;
        }
        Require(nonFileBackedLsdaRejected);
        std::cout << "56 unwind record cases passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
