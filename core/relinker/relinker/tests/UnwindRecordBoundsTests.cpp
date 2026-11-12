#include <relinker/analysis/UnusedNidFilter/EhFrameReader.hpp>
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
        auto belowPcrelSdata4 = Fixture();
        belowPcrelSdata4[3] = 0x1B;
        Io::WriteU32(belowPcrelSdata4, 16, static_cast<std::uint32_t>(static_cast<std::int32_t>(0x800 - 0x1010)));
        Io::WriteU32(belowPcrelSdata4, 20, 0x1060 - 0x1014);
        Io::WriteU64(belowPcrelSdata4, 104, 0x800);
        Require(Relinker::UnusedNidFilter::ReadExceptionFunctions(belowPcrelSdata4, {funcSeg, load, eh}, {}, {}).size() == 1);
        auto belowDatarelSdata4 = Fixture();
        belowDatarelSdata4[3] = 0x3B;
        Io::WriteU32(belowDatarelSdata4, 16, static_cast<std::uint32_t>(static_cast<std::int32_t>(0x800 - 0x1000)));
        Io::WriteU32(belowDatarelSdata4, 20, 0x1060 - 0x1000);
        Io::WriteU64(belowDatarelSdata4, 104, 0x800);
        Require(Relinker::UnusedNidFilter::ReadExceptionFunctions(belowDatarelSdata4, {funcSeg, load, eh}, {}, {}).size() == 1);
        auto pcrelUnderflow = Fixture();
        pcrelUnderflow[3] = 0x1B;
        Io::WriteU32(pcrelUnderflow, 16, 0xFFFE0000);
        Io::WriteU32(pcrelUnderflow, 20, 0x1060 - 0x1014);
        bool pcrelUnderflowRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(pcrelUnderflow, {funcSeg, load, eh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            pcrelUnderflowRejected = std::string(error.what()).find("unwind pointer address overflowed") != std::string::npos;
        }
        Require(pcrelUnderflowRejected);
        auto empty = Fixture();
        Io::WriteU64(empty, 112, 0);
        Require(Relinker::UnusedNidFilter::ReadExceptionFunctions(empty, {load, eh}, {}, {}).empty());
        for (int kind = 0; kind < 14; ++kind) {
            auto bytes = Fixture(); auto segment = load; auto ehSeg = eh;
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
                    ehSeg.MappedAddress = 0xFFFFFFFFFFFF0000ULL;
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
                case 13: bytes[3] = 0x05; break;
            }
            bool rejected = false;
            std::vector<Domain::ProgramHeader> headers = {segment, ehSeg};
            if (kind == 8) {
                Domain::ProgramHeader high{}; high.Type = 1; high.MappedAddress = 0xFFFFFFFFFFFFF000ULL; high.FileSize = 0x100;
                headers.push_back(high);
            }
            if (kind == 10) {
                Domain::ProgramHeader funcSeg{}; funcSeg.Type = 1; funcSeg.MappedAddress = 0x800; funcSeg.FileSize = 0x20;
                headers = {segment, ehSeg, funcSeg};
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
        auto unsupportedAugBytes = Fixture();
        Io::WriteU32(unsupportedAugBytes, 64, 11);
        unsupportedAugBytes[73] = 'e';
        unsupportedAugBytes[74] = 'h';
        unsupportedAugBytes[75] = 0;
        unsupportedAugBytes[76] = 1;
        unsupportedAugBytes[77] = 0x78;
        unsupportedAugBytes[78] = 16;
        bool unsupportedAugRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(unsupportedAugBytes, {augLoad, augEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            unsupportedAugRejected = std::string(error.what()).find("unsupported CIE augmentation") != std::string::npos;
        }
        Require(unsupportedAugRejected);
        auto unknownAugFieldBytes = Fixture();
        Io::WriteU32(unknownAugFieldBytes, 64, 13);
        unknownAugFieldBytes[73] = 'z';
        unknownAugFieldBytes[74] = 'X';
        unknownAugFieldBytes[75] = 0;
        unknownAugFieldBytes[76] = 1;
        unknownAugFieldBytes[77] = 0x78;
        unknownAugFieldBytes[78] = 16;
        unknownAugFieldBytes[79] = 1;
        unknownAugFieldBytes[80] = 0;
        bool unknownAugFieldRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(unknownAugFieldBytes, {augLoad, augEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            unknownAugFieldRejected = std::string(error.what()).find("unknown CIE augmentation field") != std::string::npos;
        }
        Require(unknownAugFieldRejected);
        auto cieAugMismatchBytes = Fixture();
        Io::WriteU32(cieAugMismatchBytes, 64, 14);
        cieAugMismatchBytes[73] = 'z';
        cieAugMismatchBytes[74] = 'R';
        cieAugMismatchBytes[75] = 0;
        cieAugMismatchBytes[76] = 1;
        cieAugMismatchBytes[77] = 0x78;
        cieAugMismatchBytes[78] = 16;
        cieAugMismatchBytes[79] = 2;
        cieAugMismatchBytes[80] = 0;
        cieAugMismatchBytes[81] = 0;
        bool cieAugMismatchRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(cieAugMismatchBytes, {augLoad, augEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            cieAugMismatchRejected = std::string(error.what()).find("CIE augmentation size mismatch") != std::string::npos;
        }
        Require(cieAugMismatchRejected);
        auto fdeAugExceedsBytes = Fixture();
        fdeAugExceedsBytes.resize(160, 0);
        Domain::ProgramHeader fdeAugLoad = augLoad;
        fdeAugLoad.FileSize = 160;
        Io::WriteU32(fdeAugExceedsBytes, 64, 13);
        fdeAugExceedsBytes[73] = 'z';
        fdeAugExceedsBytes[74] = 'R';
        fdeAugExceedsBytes[75] = 0;
        fdeAugExceedsBytes[76] = 1;
        fdeAugExceedsBytes[77] = 0x78;
        fdeAugExceedsBytes[78] = 16;
        fdeAugExceedsBytes[79] = 1;
        fdeAugExceedsBytes[80] = 0;
        Io::WriteU32(fdeAugExceedsBytes, 96, 28);
        Io::WriteU32(fdeAugExceedsBytes, 100, 36);
        Io::WriteU64(fdeAugExceedsBytes, 104, 0x2000);
        Io::WriteU64(fdeAugExceedsBytes, 112, 0x10);
        fdeAugExceedsBytes[120] = 30;
        bool fdeAugExceedsRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(fdeAugExceedsBytes, {fdeAugLoad, augEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            fdeAugExceedsRejected = std::string(error.what()).find("FDE augmentation exceeds record") != std::string::npos;
        }
        Require(fdeAugExceedsRejected);
        auto fdeAugMismatchBytes = Fixture();
        fdeAugMismatchBytes.resize(160, 0);
        Io::WriteU32(fdeAugMismatchBytes, 64, 13);
        fdeAugMismatchBytes[73] = 'z';
        fdeAugMismatchBytes[74] = 'L';
        fdeAugMismatchBytes[75] = 0;
        fdeAugMismatchBytes[76] = 1;
        fdeAugMismatchBytes[77] = 0x78;
        fdeAugMismatchBytes[78] = 16;
        fdeAugMismatchBytes[79] = 1;
        fdeAugMismatchBytes[80] = 3;
        Io::WriteU32(fdeAugMismatchBytes, 96, 25);
        Io::WriteU32(fdeAugMismatchBytes, 100, 36);
        Io::WriteU64(fdeAugMismatchBytes, 104, 0x2000);
        Io::WriteU64(fdeAugMismatchBytes, 112, 0x10);
        fdeAugMismatchBytes[120] = 2;
        Io::WriteU32(fdeAugMismatchBytes, 121, 0);
        bool fdeAugMismatchRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(fdeAugMismatchBytes, {fdeAugLoad, augEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            fdeAugMismatchRejected = std::string(error.what()).find("FDE augmentation size mismatch") != std::string::npos;
        }
        Require(fdeAugMismatchRejected);
        auto fdeFieldsExceedBytes = Fixture();
        Io::WriteU32(fdeFieldsExceedBytes, 96, 16);
        bool fdeFieldsExceedRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(fdeFieldsExceedBytes, {augLoad, augEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            fdeFieldsExceedRejected = std::string(error.what()).find("FDE fields exceed record") != std::string::npos;
        }
        Require(fdeFieldsExceedRejected);
        std::vector<std::uint8_t> stdHdr(256, 0);
        stdHdr[0] = 1;
        stdHdr[1] = 0x1B;
        stdHdr[2] = 3;
        stdHdr[3] = 0x3B;
        Io::WriteU32(stdHdr, 4, 0x1040 - 0x1004);
        Io::WriteU32(stdHdr, 8, 2);
        Io::WriteU32(stdHdr, 12, 0x2000 - 0x1000);
        Io::WriteU32(stdHdr, 16, 0x1060 - 0x1000);
        Io::WriteU32(stdHdr, 20, 0x2100 - 0x1000);
        Io::WriteU32(stdHdr, 24, 0x1080 - 0x1000);
        Io::WriteU32(stdHdr, 64, 9);
        stdHdr[72] = 1; stdHdr[73] = 0; stdHdr[74] = 1; stdHdr[75] = 0x78; stdHdr[76] = 16;
        Io::WriteU32(stdHdr, 96, 20);
        Io::WriteU32(stdHdr, 100, 36);
        Io::WriteU64(stdHdr, 104, 0x2000);
        Io::WriteU64(stdHdr, 112, 0x10);
        Io::WriteU32(stdHdr, 128, 20);
        Io::WriteU32(stdHdr, 132, 68);
        Io::WriteU64(stdHdr, 136, 0x2100);
        Io::WriteU64(stdHdr, 144, 0x20);
        Domain::ProgramHeader stdLoad{}; stdLoad.Type = 1; stdLoad.MappedAddress = 0x1000; stdLoad.FileSize = 256;
        Domain::ProgramHeader stdEh = stdLoad; stdEh.Type = 0x6474E550; stdEh.FileSize = 32;
        const auto stdRegions = Relinker::UnusedNidFilter::ReadExceptionFunctions(stdHdr, {stdLoad, stdEh}, {}, {});
        Require(stdRegions.size() == 2 && stdRegions[0].Begin == 0x2000 && stdRegions[0].End == 0x2010 && stdRegions[1].Begin == 0x2100 && stdRegions[1].End == 0x2120);
        auto dupHdr = stdHdr;
        Io::WriteU32(dupHdr, 20, 0x2000 - 0x1000);
        Io::WriteU32(dupHdr, 24, 0x1060 - 0x1000);
        const auto dupRegions = Relinker::UnusedNidFilter::ReadExceptionFunctions(dupHdr, {stdLoad, stdEh}, {}, {});
        Require(dupRegions.size() == 1 && dupRegions[0].Begin == 0x2000 && dupRegions[0].End == 0x2010);
        auto unsuppHdr = stdHdr;
        unsuppHdr[3] = 0xFF;
        bool unsuppRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(unsuppHdr, {stdLoad, stdEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            unsuppRejected = std::string(error.what()).find("unsupported EH frame header") != std::string::npos;
        }
        Require(unsuppRejected);
        auto invalidCountHdr = stdHdr;
        Io::WriteU32(invalidCountHdr, 8, 200);
        bool invalidCountRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(invalidCountHdr, {stdLoad, stdEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            invalidCountRejected = std::string(error.what()).find("invalid EH frame count") != std::string::npos;
        }
        Require(invalidCountRejected);
        auto cieUnderflowSize = Fixture();
        Io::WriteU32(cieUnderflowSize, 64, 4);
        bool cieUnderflowSizeRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(cieUnderflowSize, {augLoad, augEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            cieUnderflowSizeRejected = std::string(error.what()).find("unsupported CIE record size") != std::string::npos;
        }
        Require(cieUnderflowSizeRejected);
        auto cieDwarf64Size = Fixture();
        Io::WriteU32(cieDwarf64Size, 64, 0xFFFFFFFF);
        bool cieDwarf64SizeRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(cieDwarf64Size, {augLoad, augEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            cieDwarf64SizeRejected = std::string(error.what()).find("unsupported CIE record size") != std::string::npos;
        }
        Require(cieDwarf64SizeRejected);
        auto fdeUnderflowSize = Fixture();
        Io::WriteU32(fdeUnderflowSize, 96, 4);
        bool fdeUnderflowSizeRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(fdeUnderflowSize, {augLoad, augEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            fdeUnderflowSizeRejected = std::string(error.what()).find("unsupported FDE record size") != std::string::npos;
        }
        Require(fdeUnderflowSizeRejected);
        auto fdeDwarf64Size = Fixture();
        Io::WriteU32(fdeDwarf64Size, 96, 0xFFFFFFFF);
        bool fdeDwarf64SizeRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(fdeDwarf64Size, {augLoad, augEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            fdeDwarf64SizeRejected = std::string(error.what()).find("unsupported FDE record size") != std::string::npos;
        }
        Require(fdeDwarf64SizeRejected);
        auto invalidCieId = Fixture();
        Io::WriteU32(invalidCieId, 68, 0xFFFFFFFF);
        bool invalidCieIdRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(invalidCieId, {augLoad, augEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            invalidCieIdRejected = std::string(error.what()).find("invalid CIE identifier") != std::string::npos;
        }
        Require(invalidCieIdRejected);
        auto unsupportedCieVersion = Fixture();
        unsupportedCieVersion[72] = 2;
        bool unsupportedCieVersionRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(unsupportedCieVersion, {augLoad, augEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            unsupportedCieVersionRejected = std::string(error.what()).find("unsupported CIE version") != std::string::npos;
        }
        Require(unsupportedCieVersionRejected);
        auto cieVersion3 = Fixture();
        cieVersion3[72] = 3;
        const auto v3Regions = Relinker::UnusedNidFilter::ReadExceptionFunctions(cieVersion3, {augLoad, augEh}, {}, {});
        Require(v3Regions.size() == 1 && v3Regions[0].Begin == 0x2000 && v3Regions[0].End == 0x2010);
        auto cieVersion4 = Fixture();
        cieVersion4.resize(160, 0);
        Domain::ProgramHeader v4Load = augLoad;
        v4Load.FileSize = 160;
        Io::WriteU32(cieVersion4, 64, 11);
        cieVersion4[72] = 4;
        cieVersion4[73] = 0;
        cieVersion4[74] = 8;
        cieVersion4[75] = 0;
        cieVersion4[76] = 1;
        cieVersion4[77] = 0x78;
        cieVersion4[78] = 16;
        Io::WriteU32(cieVersion4, 96, 20);
        Io::WriteU32(cieVersion4, 100, 36);
        Io::WriteU64(cieVersion4, 104, 0x2000);
        Io::WriteU64(cieVersion4, 112, 0x10);
        const auto v4Regions = Relinker::UnusedNidFilter::ReadExceptionFunctions(cieVersion4, {v4Load, augEh}, {}, {});
        Require(v4Regions.size() == 1 && v4Regions[0].Begin == 0x2000 && v4Regions[0].End == 0x2010);
        auto cieVersion4BadAddr = cieVersion4;
        cieVersion4BadAddr[74] = 4;
        bool v4BadAddrRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(cieVersion4BadAddr, {v4Load, augEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            v4BadAddrRejected = std::string(error.what()).find("unsupported CIE address size") != std::string::npos;
        }
        Require(v4BadAddrRejected);
        auto cieVersion4BadSeg = cieVersion4;
        cieVersion4BadSeg[75] = 1;
        bool v4BadSegRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(cieVersion4BadSeg, {v4Load, augEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            v4BadSegRejected = std::string(error.what()).find("unsupported CIE address size") != std::string::npos;
        }
        Require(v4BadSegRejected);
        auto ciePadded = Fixture();
        Io::WriteU32(ciePadded, 64, 16);
        const auto paddedRegions = Relinker::UnusedNidFilter::ReadExceptionFunctions(ciePadded, {augLoad, augEh}, {}, {});
        Require(paddedRegions.size() == 1 && paddedRegions[0].Begin == 0x2000 && paddedRegions[0].End == 0x2010);
        auto fdePadded = Fixture();
        fdePadded.resize(160, 0);
        Domain::ProgramHeader fdePaddedLoad = augLoad;
        fdePaddedLoad.FileSize = 160;
        Io::WriteU32(fdePadded, 96, 28);
        const auto fdePaddedRegions = Relinker::UnusedNidFilter::ReadExceptionFunctions(fdePadded, {fdePaddedLoad, augEh}, {}, {});
        Require(fdePaddedRegions.size() == 1 && fdePaddedRegions[0].Begin == 0x2000 && fdePaddedRegions[0].End == 0x2010);
        auto fdeCieOffsetExceeds = Fixture();
        Io::WriteU32(fdeCieOffsetExceeds, 100, 0x1070);
        bool fdeCieOffsetExceedsRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(fdeCieOffsetExceeds, {augLoad, augEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            fdeCieOffsetExceedsRejected = std::string(error.what()).find("invalid FDE CIE offset") != std::string::npos;
        }
        Require(fdeCieOffsetExceedsRejected);
        auto fdeRangeMismatch = Fixture();
        Io::WriteU64(fdeRangeMismatch, 104, 0x2020);
        bool fdeRangeMismatchRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(fdeRangeMismatch, {augLoad, augEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            fdeRangeMismatchRejected = std::string(error.what()).find("inconsistent FDE function range") != std::string::npos;
        }
        Require(fdeRangeMismatchRejected);
        auto fdeRangeOverflow = Fixture();
        Io::WriteU64(fdeRangeOverflow, 112, 0xFFFFFFFFFFFFFFFFULL);
        bool fdeRangeOverflowRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(fdeRangeOverflow, {augLoad, augEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            fdeRangeOverflowRejected = std::string(error.what()).find("inconsistent FDE function range") != std::string::npos;
        }
        Require(fdeRangeOverflowRejected);
        auto augSignalFrame = Fixture();
        augSignalFrame.resize(160, 0);
        Domain::ProgramHeader sigLoad = augLoad;
        sigLoad.FileSize = 160;
        Io::WriteU32(augSignalFrame, 64, 12);
        augSignalFrame[72] = 1;
        augSignalFrame[73] = 'z';
        augSignalFrame[74] = 'S';
        augSignalFrame[75] = 0;
        augSignalFrame[76] = 1;
        augSignalFrame[77] = 0x78;
        augSignalFrame[78] = 16;
        augSignalFrame[79] = 0;
        Io::WriteU32(augSignalFrame, 96, 21);
        Io::WriteU32(augSignalFrame, 100, 36);
        Io::WriteU64(augSignalFrame, 104, 0x2000);
        Io::WriteU64(augSignalFrame, 112, 0x10);
        augSignalFrame[120] = 0;
        const auto sigRegions = Relinker::UnusedNidFilter::ReadExceptionFunctions(augSignalFrame, {sigLoad, augEh}, {}, {});
        Require(sigRegions.size() == 1 && sigRegions[0].Begin == 0x2000 && sigRegions[0].End == 0x2010);
        auto augPersonality = Fixture();
        augPersonality.resize(160, 0);
        Io::WriteU32(augPersonality, 64, 21);
        augPersonality[72] = 1;
        augPersonality[73] = 'z';
        augPersonality[74] = 'P';
        augPersonality[75] = 0;
        augPersonality[76] = 1;
        augPersonality[77] = 0x78;
        augPersonality[78] = 16;
        augPersonality[79] = 9;
        augPersonality[80] = 0;
        Io::WriteU64(augPersonality, 81, 0x4000);
        Io::WriteU32(augPersonality, 96, 21);
        Io::WriteU32(augPersonality, 100, 36);
        Io::WriteU64(augPersonality, 104, 0x2000);
        Io::WriteU64(augPersonality, 112, 0x10);
        augPersonality[120] = 0;
        const auto persRegions = Relinker::UnusedNidFilter::ReadExceptionFunctions(augPersonality, {sigLoad, augEh}, {}, {});
        Require(persRegions.size() == 1 && persRegions[0].ExtraTargets.size() == 1 && persRegions[0].ExtraTargets[0] == 0x4000);
        auto fdeAugExceedsRecord = Fixture();
        fdeAugExceedsRecord.resize(160, 0);
        Io::WriteU32(fdeAugExceedsRecord, 64, 13);
        fdeAugExceedsRecord[73] = 'z';
        fdeAugExceedsRecord[74] = 'R';
        fdeAugExceedsRecord[75] = 0;
        fdeAugExceedsRecord[76] = 1;
        fdeAugExceedsRecord[77] = 0x78;
        fdeAugExceedsRecord[78] = 16;
        fdeAugExceedsRecord[79] = 1;
        fdeAugExceedsRecord[80] = 0;
        Io::WriteU32(fdeAugExceedsRecord, 96, 22);
        Io::WriteU32(fdeAugExceedsRecord, 100, 36);
        Io::WriteU64(fdeAugExceedsRecord, 104, 0x2000);
        Io::WriteU64(fdeAugExceedsRecord, 112, 0x10);
        fdeAugExceedsRecord[120] = 10;
        bool fdeAugExceedsRecordRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(fdeAugExceedsRecord, {sigLoad, augEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            fdeAugExceedsRecordRejected = std::string(error.what()).find("FDE augmentation exceeds record") != std::string::npos;
        }
        Require(fdeAugExceedsRecordRejected);
        auto truncatedZrAug = Fixture();
        Io::WriteU32(truncatedZrAug, 64, 13);
        truncatedZrAug[73] = 'z';
        truncatedZrAug[74] = 'R';
        truncatedZrAug[75] = 0;
        truncatedZrAug[76] = 1;
        truncatedZrAug[77] = 0x78;
        truncatedZrAug[78] = 16;
        truncatedZrAug[79] = 0;
        bool truncatedZrRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(truncatedZrAug, {augLoad, augEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            truncatedZrRejected = std::string(error.what()).find("CIE augmentation size mismatch") != std::string::npos;
        }
        Require(truncatedZrRejected);
        auto truncatedZlAug = Fixture();
        Io::WriteU32(truncatedZlAug, 64, 13);
        truncatedZlAug[73] = 'z';
        truncatedZlAug[74] = 'L';
        truncatedZlAug[75] = 0;
        truncatedZlAug[76] = 1;
        truncatedZlAug[77] = 0x78;
        truncatedZlAug[78] = 16;
        truncatedZlAug[79] = 0;
        bool truncatedZlRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(truncatedZlAug, {augLoad, augEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            truncatedZlRejected = std::string(error.what()).find("CIE augmentation size mismatch") != std::string::npos;
        }
        Require(truncatedZlRejected);
        auto truncatedZrlAug = Fixture();
        Io::WriteU32(truncatedZrlAug, 64, 15);
        truncatedZrlAug[73] = 'z';
        truncatedZrlAug[74] = 'R';
        truncatedZrlAug[75] = 'L';
        truncatedZrlAug[76] = 0;
        truncatedZrlAug[77] = 1;
        truncatedZrlAug[78] = 0x78;
        truncatedZrlAug[79] = 16;
        truncatedZrlAug[80] = 1;
        truncatedZrlAug[81] = 0;
        bool truncatedZrlRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(truncatedZrlAug, {augLoad, augEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            truncatedZrlRejected = std::string(error.what()).find("CIE augmentation size mismatch") != std::string::npos;
        }
        Require(truncatedZrlRejected);
        auto truncatedZlrAug = Fixture();
        Io::WriteU32(truncatedZlrAug, 64, 15);
        truncatedZlrAug[73] = 'z';
        truncatedZlrAug[74] = 'L';
        truncatedZlrAug[75] = 'R';
        truncatedZlrAug[76] = 0;
        truncatedZlrAug[77] = 1;
        truncatedZlrAug[78] = 0x78;
        truncatedZlrAug[79] = 16;
        truncatedZlrAug[80] = 1;
        truncatedZlrAug[81] = 0;
        bool truncatedZlrRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(truncatedZlrAug, {augLoad, augEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            truncatedZlrRejected = std::string(error.what()).find("CIE augmentation size mismatch") != std::string::npos;
        }
        Require(truncatedZlrRejected);
        auto truncatedZpAug = Fixture();
        Io::WriteU32(truncatedZpAug, 64, 21);
        truncatedZpAug[73] = 'z';
        truncatedZpAug[74] = 'P';
        truncatedZpAug[75] = 0;
        truncatedZpAug[76] = 1;
        truncatedZpAug[77] = 0x78;
        truncatedZpAug[78] = 16;
        truncatedZpAug[79] = 5;
        truncatedZpAug[80] = 0;
        Io::WriteU64(truncatedZpAug, 81, 0x4000);
        bool truncatedZpRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(truncatedZpAug, {augLoad, augEh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            truncatedZpRejected = std::string(error.what()).find("CIE augmentation size mismatch") != std::string::npos;
        }
        Require(truncatedZpRejected);
        auto validZrlAug = Fixture();
        validZrlAug.resize(160, 0);
        Io::WriteU32(validZrlAug, 64, 15);
        validZrlAug[73] = 'z';
        validZrlAug[74] = 'R';
        validZrlAug[75] = 'L';
        validZrlAug[76] = 0;
        validZrlAug[77] = 1;
        validZrlAug[78] = 0x78;
        validZrlAug[79] = 16;
        validZrlAug[80] = 2;
        validZrlAug[81] = 0;
        validZrlAug[82] = 0xFF;
        Io::WriteU32(validZrlAug, 96, 21);
        Io::WriteU32(validZrlAug, 100, 36);
        Io::WriteU64(validZrlAug, 104, 0x2000);
        Io::WriteU64(validZrlAug, 112, 0x10);
        validZrlAug[120] = 0;
        const auto zrlRegions = Relinker::UnusedNidFilter::ReadExceptionFunctions(validZrlAug, {sigLoad, augEh}, {}, {});
        Require(zrlRegions.size() == 1 && zrlRegions[0].Begin == 0x2000 && zrlRegions[0].End == 0x2010);
        auto validZlrAug = Fixture();
        validZlrAug.resize(160, 0);
        Io::WriteU32(validZlrAug, 64, 15);
        validZlrAug[73] = 'z';
        validZlrAug[74] = 'L';
        validZlrAug[75] = 'R';
        validZlrAug[76] = 0;
        validZlrAug[77] = 1;
        validZlrAug[78] = 0x78;
        validZlrAug[79] = 16;
        validZlrAug[80] = 2;
        validZlrAug[81] = 0xFF;
        validZlrAug[82] = 0;
        Io::WriteU32(validZlrAug, 96, 21);
        Io::WriteU32(validZlrAug, 100, 36);
        Io::WriteU64(validZlrAug, 104, 0x2000);
        Io::WriteU64(validZlrAug, 112, 0x10);
        validZlrAug[120] = 0;
        const auto zlrRegions = Relinker::UnusedNidFilter::ReadExceptionFunctions(validZlrAug, {sigLoad, augEh}, {}, {});
        Require(zlrRegions.size() == 1 && zlrRegions[0].Begin == 0x2000 && zlrRegions[0].End == 0x2010);
        auto slebMinUnderflow = Fixture();
        slebMinUnderflow[3] = 0x39;
        for (int i = 0; i < 9; ++i) slebMinUnderflow[16 + i] = 0x80;
        slebMinUnderflow[25] = 0x7F;
        bool slebMinUnderflowRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(slebMinUnderflow, {load, eh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            slebMinUnderflowRejected = std::string(error.what()).find("unwind pointer address overflowed") != std::string::npos;
        }
        Require(slebMinUnderflowRejected);
        auto slebMinValid = Fixture();
        slebMinValid[3] = 0x39;
        Io::WriteU64(slebMinValid, 4, 0x8000000000002040ULL);
        for (int i = 0; i < 9; ++i) slebMinValid[16 + i] = 0x80;
        slebMinValid[25] = 0x7F;
        slebMinValid[26] = 0xE0;
        slebMinValid[27] = 0x00;
        Domain::ProgramHeader slebMinLoad{}; slebMinLoad.Type = 1; slebMinLoad.MappedAddress = 0x8000000000002000ULL; slebMinLoad.FileSize = 128;
        Domain::ProgramHeader slebMinEh = slebMinLoad; slebMinEh.Type = 0x6474E550; slebMinEh.FileSize = 32;
        const auto slebMinRegions = Relinker::UnusedNidFilter::ReadExceptionFunctions(slebMinValid, {slebMinLoad, slebMinEh}, {}, {});
        Require(slebMinRegions.size() == 1 && slebMinRegions[0].Begin == 0x2000 && slebMinRegions[0].End == 0x2010);
        auto unterminatedLeb = Fixture();
        unterminatedLeb[3] = 0x31;
        for (int i = 0; i < 11; ++i) unterminatedLeb[16 + i] = 0x80;
        bool unterminatedLebRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(unterminatedLeb, {load, eh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            unterminatedLebRejected = std::string(error.what()).find("unterminated LEB128 value") != std::string::npos;
        }
        Require(unterminatedLebRejected);
        auto unterminatedSleb = Fixture();
        unterminatedSleb[3] = 0x39;
        for (int i = 0; i < 9; ++i) unterminatedSleb[16 + i] = 0x80;
        unterminatedSleb[25] = 0xFF;
        bool unterminatedSlebRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(unterminatedSleb, {load, eh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            unterminatedSlebRejected = std::string(error.what()).find("unterminated LEB128 value") != std::string::npos;
        }
        Require(unterminatedSlebRejected);
        for (std::uint32_t terminal = 1; terminal < 0x7F; ++terminal) {
            auto slebOverflowFuzz = Fixture();
            slebOverflowFuzz[3] = 0x39;
            for (int i = 0; i < 9; ++i) slebOverflowFuzz[16 + i] = 0x80;
            slebOverflowFuzz[25] = static_cast<std::uint8_t>(terminal);
            bool slebFuzzRejected = false;
            try { Relinker::UnusedNidFilter::ReadExceptionFunctions(slebOverflowFuzz, {load, eh}, {}, {}); }
            catch (const Domain::RelinkerException& error) {
                slebFuzzRejected = std::string(error.what()).find("overflowing LEB128 value") != std::string::npos;
            }
            Require(slebFuzzRejected);
        }
        for (std::uint32_t terminal = 2; terminal <= 0x7F; ++terminal) {
            auto ulebOverflowFuzz = Fixture();
            ulebOverflowFuzz[3] = 0x31;
            for (int i = 0; i < 9; ++i) ulebOverflowFuzz[16 + i] = 0x80;
            ulebOverflowFuzz[25] = static_cast<std::uint8_t>(terminal);
            bool ulebFuzzRejected = false;
            try { Relinker::UnusedNidFilter::ReadExceptionFunctions(ulebOverflowFuzz, {load, eh}, {}, {}); }
            catch (const Domain::RelinkerException& error) {
                ulebFuzzRejected = std::string(error.what()).find("overflowing LEB128 value") != std::string::npos;
            }
            Require(ulebFuzzRejected);
        }
        auto cieMinBytes = Fixture();
        Io::WriteU32(cieMinBytes, 64, 18);
        cieMinBytes[72] = 1;
        cieMinBytes[73] = 0;
        cieMinBytes[74] = 1;
        for (int i = 0; i < 9; ++i) cieMinBytes[75 + i] = 0x80;
        cieMinBytes[84] = 0x7F;
        cieMinBytes[85] = 16;
        const auto cieMinRegions = Relinker::UnusedNidFilter::ReadExceptionFunctions(cieMinBytes, {load, eh}, {}, {});
        Require(cieMinRegions.size() == 1 && cieMinRegions[0].Begin == 0x2000 && cieMinRegions[0].End == 0x2010);
        auto cieUnterminatedBytes = Fixture();
        Io::WriteU32(cieUnterminatedBytes, 64, 18);
        cieUnterminatedBytes[72] = 1;
        cieUnterminatedBytes[73] = 0;
        cieUnterminatedBytes[74] = 1;
        for (int i = 0; i < 10; ++i) cieUnterminatedBytes[75 + i] = 0x80;
        cieUnterminatedBytes[85] = 16;
        bool cieUnterminatedRejected = false;
        try { Relinker::UnusedNidFilter::ReadExceptionFunctions(cieUnterminatedBytes, {load, eh}, {}, {}); }
        catch (const Domain::RelinkerException& error) {
            cieUnterminatedRejected = std::string(error.what()).find("unterminated LEB128 value") != std::string::npos;
        }
        Require(cieUnterminatedRejected);
        std::cout << "83 unwind record cases passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
