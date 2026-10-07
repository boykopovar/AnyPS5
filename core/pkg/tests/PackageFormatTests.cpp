#include <pkg/InnerFileSystem.hpp>
#include <pkg/KrakenStream.hpp>
#include <pkg/SelfImage.hpp>
#include <pkg/PackageError.hpp>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using Bytes = std::vector<std::uint8_t>;

void require(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

template<typename TOperation>
void requireFailure(const TOperation& operation, const std::string& expected) {
    try {
        operation();
    } catch (const Pkg::PackageError& error) {
        require(std::string(error.what()).find(expected) != std::string::npos, "unexpected diagnostic: " + std::string(error.what()));
        return;
    }
    throw std::runtime_error("operation did not fail: " + expected);
}

Bytes concat(std::initializer_list<Bytes> parts) {
    Bytes result;
    for (const auto& part : parts) result.insert(result.end(), part.begin(), part.end());
    return result;
}

void krakenStreams() {
    const Bytes payload{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    require(Pkg::BuildKrakenStream(payload.data(), payload.size(), 0x1000, 0, 0x02) == concat({{0x8c, 0x06, 0x00, 0x00, 0x0c, 0x88, 0x00, 0x0a}, payload}), "raw literal LZ half");
    require(Pkg::BuildKrakenStream(payload.data(), payload.size(), 0x1000, 0, 0x03) == concat({{0x8c, 0x06, 0x00, 0x00, 0x0c, 0x80, 0x00, 0x0a}, payload}), "delta literal LZ half");
    require(Pkg::BuildKrakenStream(payload.data(), payload.size(), 0x1000, 0, 0x00) == concat({{0x8c, 0x06, 0x00, 0x00, 0x09}, payload}), "entropy-only half");

    Bytes twoHalves(0x20000, 0xab);
    twoHalves.insert(twoHalves.end(), payload.begin(), payload.begin() + 5);
    const auto stream = Pkg::BuildKrakenStream(twoHalves.data(), twoHalves.size(), 0x30000, 0x20000, 0x30);
    require(stream.size() == 5 + 3 + 0x20000 + 3 + 5, "two-half stream size");
    require(Bytes(stream.begin(), stream.begin() + 8) == Bytes{0x8c, 0x06, 0x02, 0x00, 0x0a, 0x82, 0x00, 0x00}, "stored even half header");
    require(Bytes(stream.begin() + 8 + 0x20000, stream.begin() + 8 + 0x20000 + 3) == Bytes{0x80, 0x00, 0x05}, "odd half header");

    requireFailure([&] { Pkg::BuildKrakenStream(payload.data(), payload.size(), 10, 0, 0x02); }, "Invalid Kraken block geometry");
    requireFailure([&] { Pkg::BuildKrakenStream(twoHalves.data(), twoHalves.size(), 0x30000, static_cast<std::uint32_t>(twoHalves.size()), 0x22); }, "Invalid Kraken block geometry");
    Bytes oversized(0x20000 + 0x1fffe, 0x11);
    requireFailure([&] { Pkg::BuildKrakenStream(oversized.data(), oversized.size(), 0x40000, 0x20000, 0x22); }, "single quantum");
}

Bytes selfFixture(const std::uint64_t properties, const std::size_t trailer) {
    Bytes elf(0x40 + 2 * 0x38);
    std::memcpy(elf.data(), "\x7f" "ELF\x02\x01\x01", 7);
    const auto put = [](Bytes& bytes, const std::size_t offset, const std::uint64_t value, const std::size_t size) {
        for (std::size_t index = 0; index < size; ++index) bytes[offset + index] = static_cast<std::uint8_t>(value >> (8 * index));
    };
    put(elf, 0x20, 0x40, 8);
    put(elf, 0x36, 0x38, 2);
    put(elf, 0x38, 2, 2);
    put(elf, 0x40, 1, 4);
    put(elf, 0x48, 0x1000, 8);
    put(elf, 0x60, 0x10, 8);
    put(elf, 0x78, 0x6fffff01, 4);
    put(elf, 0x80, 0x1010, 8);
    put(elf, 0x98, 4, 8);
    Bytes self(0x40);
    std::memcpy(self.data(), "\x54\x14\xf5\xee", 4);
    put(self, 0x18, 1, 2);
    put(self, 0x20, properties, 8);
    put(self, 0x28, 0x100, 8);
    put(self, 0x30, 0x10, 8);
    self.insert(self.end(), elf.begin(), elf.end());
    self.resize(0x100, 0);
    for (std::uint8_t value = 0; value < 0x10; ++value) self.push_back(static_cast<std::uint8_t>(0xa0 + value));
    self.resize(self.size() + trailer - 4, 0);
    for (const std::uint8_t value : {0x56, 0x45, 0x52, 0x53}) self.push_back(value);
    return self;
}

void selfImages() {
    const auto elf = Pkg::UnwrapSelfImage(selfFixture(0x800 | 0x4, 12));
    require(elf.size() == 0x1014, "unwrapped ELF size");
    require(std::memcmp(elf.data(), "\x7f" "ELF", 4) == 0, "unwrapped ELF header");
    require(elf[0x1000] == 0xa0 && elf[0x100f] == 0xaf, "data segment placement");
    require(std::memcmp(elf.data() + 0x1010, "VERS", 4) == 0, "version segment comes from the file tail");
    require(Pkg::IsSelfImage(selfFixture(0x800, 4).data(), 4) && !Pkg::IsSelfImage(elf.data(), elf.size()), "SELF magic detection");
    requireFailure([] { Pkg::UnwrapSelfImage(selfFixture(0x800 | 0x2, 12)); }, "Encrypted or compressed");
    requireFailure([] { Pkg::UnwrapSelfImage(selfFixture(0x800 | 0x8, 12)); }, "Encrypted or compressed");
    auto truncated = selfFixture(0x800, 4);
    truncated.resize(truncated.size() - 2);
    requireFailure([&] { Pkg::UnwrapSelfImage(truncated); }, "version segment");
}

void entryNames() {
    for (const std::string name : {"eboot.bin", "sce_module", "con.txt", "a b"}) require(Pkg::IsSafeEntryName(name), "rejected " + name);
    const std::vector<std::string> unsafe{"", ".", "..", "a/b", "a\\b", "c:x", "x.", "x ", std::string("a\0b", 3), "a\nb"};
    for (const auto& name : unsafe) require(!Pkg::IsSafeEntryName(name), "accepted unsafe name");
}

}

int main() {
    try {
        krakenStreams();
        selfImages();
        entryNames();
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Package format tests passed\n";
    return 0;
}
