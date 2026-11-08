#include <relinker/analysis/UnusedNidFilter/EhFrameReader.hpp>
#include <io/BufferUtils.hpp>
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

static void Require(bool ok) { if (!ok) throw std::runtime_error("EH frame personality validation failed"); }

static std::vector<std::uint8_t> PersonalityFixture() {
    std::vector<std::uint8_t> bytes(128, 0);
    bytes[0] = 1; bytes[1] = 0; bytes[2] = 3; bytes[3] = 0;
    Io::WriteU64(bytes, 4, 0x1040); Io::WriteU32(bytes, 12, 1);
    Io::WriteU64(bytes, 16, 0x2000); Io::WriteU64(bytes, 24, 0x1060);
    Io::WriteU32(bytes, 64, 11); bytes[72] = 1; bytes[73] = 0; bytes[74] = 'z'; bytes[75] = 'P'; bytes[76] = 2; bytes[77] = 0x13; Io::WriteU64(bytes, 78, 0x1080); bytes[86] = 16;
    Io::WriteU32(bytes, 96, 20); Io::WriteU32(bytes, 100, 36);
    Io::WriteU64(bytes, 104, 0x2000); Io::WriteU64(bytes, 112, 0x10);
    return bytes;
}

int main() {
    try {
        Domain::ProgramHeader load{}; load.Type = 1; load.MappedAddress = 0x1000; load.FileSize = 128;
        auto eh = load; eh.Type = 0x6474E550; eh.FileSize = 32;

        {
            auto bytes = PersonalityFixture();
            const auto regions = Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, {load, eh}, {}, {});
            Require(regions.size() == 1 && regions[0].Begin == 0x2000 && regions[0].ExtraTargets.size() == 1);
        }

        {
            auto bytes = PersonalityFixture();
            bytes[77] = 0x14;
            Io::WriteU64(bytes, 78, std::numeric_limits<std::uint64_t>::max() - 4);
            bool rejected = false;
            try { Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, {load, eh}, {}, {}); }
            catch (const Domain::RelinkerException& error) {
                rejected = std::string(error.what()).find("unwind pointer address overflowed") != std::string::npos;
            }
            Require(rejected);
        }

        {
            auto bytes = PersonalityFixture();
            bytes[77] = 0x9B;
            bool rejected = false;
            try { Relinker::UnusedNidFilter::ReadExceptionFunctions(bytes, {load, eh}, {}, {}); }
            catch (const Domain::RelinkerException& error) {
                rejected = std::string(error.what()).find("unsupported unwind pointer format") != std::string::npos;
            }
            Require(rejected);
        }

        std::cout << "EH frame personality validation tests passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
