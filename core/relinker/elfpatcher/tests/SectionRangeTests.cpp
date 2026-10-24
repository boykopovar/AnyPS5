#include <elfpatcher/general/SectionHeaderTableBuilder.hpp>
#include <io/ByteWriter.hpp>
#include <io/BufferUtils.hpp>
#include <iostream>
#include <limits>
#include <stdexcept>

static void Require(bool ok) { if (!ok) throw std::runtime_error("Section range validation failed"); }

static std::vector<std::uint8_t> Fixture() {
    std::vector<std::uint8_t> bytes(512, 0);
    Io::WriteU64(bytes, 32, 64); Io::WriteU16(bytes, 54, 56); Io::WriteU16(bytes, 56, 1);
    Io::WriteU32(bytes, 64, 1); Io::WriteU64(bytes, 72, 128);
    Io::WriteU64(bytes, 80, 0x1000); Io::WriteU64(bytes, 96, 256);
    return bytes;
}

int main() {
    try {
        Elfpatcher::SectionHeaderTableBuilder builder(std::make_shared<Io::ByteWriter>());
        const Elfpatcher::SectionHeaderTableRequest valid{128, 8, 136, 24, 160, 16, 176, 16};
        auto bytes = Fixture(); builder.WriteTable(bytes, valid);
        const auto table = Io::ReadU64(bytes, 40);
        Require(Io::ReadU64(bytes, table + 64 + 8) == 0);
        Require(Io::ReadU64(bytes, table + 64 + 16) == 0);
        Require(Io::ReadU64(bytes, table + 64 + 24) == 512);
        Require(Io::ReadU64(bytes, table + 64 + 32) != 0);
        Require(Io::ReadU64(bytes, table + 2 * 64 + 16) == 0x1000);
        auto empty = valid; empty.StubOffset = 384; empty.StubSize = 0;
        bytes = Fixture(); builder.WriteTable(bytes, empty);
        Require(Io::ReadU64(bytes, Io::ReadU64(bytes, 40) + 5 * 64 + 16) == 0x1100);
        for (int kind = 0; kind < 11; ++kind) {
            bytes = Fixture(); auto request = valid;
            switch (kind) {
                case 0: request.DynStrSize = 257; break;
                case 1: request.DynSymSize = std::numeric_limits<std::uint64_t>::max(); break;
                case 2: request.DynamicSegmentOffset = 383; request.DynamicSegmentSize = 2; break;
                case 3: request.StubOffset = 384; request.StubSize = 1; break;
                case 4: Io::WriteU16(bytes, 54, 1); break;
                case 5: Io::WriteU64(bytes, 32, std::numeric_limits<std::uint64_t>::max()); break;
                case 6: Io::WriteU64(bytes, 80, std::numeric_limits<std::uint64_t>::max() - 4); break;
                case 7: Io::WriteU16(bytes, 56, 20); break;
                case 8: request.DynStrSize = 16; break;
                case 9: bytes.resize(40); break;
                case 10: Io::WriteU64(bytes, 32, 48); Io::WriteU32(bytes, 48, 1); Io::WriteU64(bytes, 56, 128); break;
            }
            const auto before = bytes;
            bool rejected = false;
            try { builder.WriteTable(bytes, request); } catch (const std::runtime_error&) { rejected = true; }
            Require(rejected && bytes == before);
        }
        std::cout << "13 section range cases passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
