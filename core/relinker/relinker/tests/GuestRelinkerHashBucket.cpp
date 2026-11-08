#include "ElfFixture.hpp"
#include <relinker/guest/GuestImage.hpp>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace RelinkerTests;

static Bytes Fixture(const std::uint32_t bucketCount, const std::uint32_t chainCount, const std::uint32_t bucketValue, const std::uint32_t chainValue) {
    const std::uint8_t code[] = {0xc3};
    auto bytes = MakeExecutable(code);
    const auto stringOffset = 0x2200;
    const auto symbolOffset = 0x2400;
    const auto hashOffset = 0x2600;
    const auto hashSize = 8 + (bucketCount + chainCount) * 4;
    Write<std::uint32_t>(bytes, hashOffset, bucketCount);
    Write<std::uint32_t>(bytes, hashOffset + 4, chainCount);
    for (std::uint32_t i = 0; i < bucketCount; ++i) {
        Write<std::uint32_t>(bytes, hashOffset + 8 + i * 4, bucketValue);
    }
    for (std::uint32_t i = 0; i < chainCount; ++i) {
        Write<std::uint32_t>(bytes, hashOffset + 8 + (bucketCount + i) * 4, chainValue);
    }
    const std::pair<std::uint64_t, std::uint64_t> tags[] = {
        {5, stringOffset},
        {10, 16},
        {6, symbolOffset},
        {11, 24},
        {4, hashOffset},
        {7, 0x2800},
        {8, 24},
        {0, 0}
    };
    WriteProgramHeader(bytes, 2, 2, 6, 0x2000, 0x2000, sizeof(tags), 8);
    for (std::size_t i = 0; i < std::size(tags); ++i) {
        Write<std::uint64_t>(bytes, 0x2000 + i * 16, tags[i].first);
        Write<std::uint64_t>(bytes, 0x2000 + i * 16 + 8, tags[i].second);
    }
    Write<std::uint32_t>(bytes, symbolOffset + 24, 1);
    Write<std::uint8_t>(bytes, symbolOffset + 28, 0x12);
    Write<std::uint8_t>(bytes, symbolOffset + 29, 3);
    Write<std::uint16_t>(bytes, symbolOffset + 30, 1);
    Write<std::uint64_t>(bytes, symbolOffset + 32, 0x1000);
    Write<std::uint64_t>(bytes, symbolOffset + 40, 1);
    Write<std::uint64_t>(bytes, 0x2800, 0x2900);
    Write<std::uint64_t>(bytes, 0x2808, (1ull << 32) | 1);
    return bytes;
}

static void Reject(const Bytes& bytes, const std::string& expected) {
    try {
        Relinker::GuestImageReader().Read("generic.prx", bytes, true);
    } catch (const Domain::RelinkerException& error) {
        if (std::string(error.what()).find(expected) != std::string::npos) return;
        throw std::runtime_error("Expected " + expected + ", got " + error.what());
    }
    throw std::runtime_error("Accepted invalid input: " + expected);
}

int main() {
    try {
        Reject(Fixture(0, 1, 0, 0), "Invalid SysV hash header");
        Reject(Fixture(1, 0, 0, 0), "Invalid SysV hash header");
        Reject(Fixture(1, 1, 2, 0), "Invalid SysV hash bucket index");
        Reject(Fixture(1, 1, 1, 2), "Invalid SysV hash chain index");
        Reject(Fixture(2, 1, 0, 0), "Invalid SysV hash header");
        Reject(Fixture(1, 1, 0, 0x100000), "Unmapped ELF address");

        const auto good = Fixture(1, 1, 1, 0);
        const auto image = Relinker::GuestImageReader().Read("generic.prx", good, true);
        if (image.Symbols.size() != 2) throw std::runtime_error("Valid SysV hash rejected or symbol count incorrect");

        std::cout << "Guest relinker hash bucket bounds tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "GuestRelinkerHashBucket: " << error.what() << '\n';
        return 1;
    }
}
