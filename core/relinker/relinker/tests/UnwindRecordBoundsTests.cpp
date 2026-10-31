#include <relinker/analysis/UnusedNidFilter/EhFrameReader.hpp>
#include <io/BufferUtils.hpp>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

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
        for (int kind = 0; kind < 12; ++kind) {
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
        std::cout << "20 unwind record cases passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
