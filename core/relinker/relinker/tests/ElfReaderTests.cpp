#include <relinker/parsing/ElfReader.hpp>
#include <relinker/domain/Types.hpp>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

void require(const bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<typename TValue>
void write(std::vector<std::uint8_t>& bytes, const std::size_t offset, const TValue value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

std::vector<std::uint8_t> image(const std::uint16_t sectionHeaderEntrySize, const std::uint16_t sectionHeaderCount) {
    std::vector<std::uint8_t> bytes(0x200);
    const std::uint8_t ident[16] = {0x7F, 'E', 'L', 'F', 2, 1, 1};
    std::memcpy(bytes.data(), ident, sizeof(ident));
    write<std::uint16_t>(bytes, 0x10, 3);
    write<std::uint16_t>(bytes, 0x12, 62);
    write<std::uint64_t>(bytes, 0x28, 0x40);
    write<std::uint16_t>(bytes, 0x3a, sectionHeaderEntrySize);
    write<std::uint16_t>(bytes, 0x3c, sectionHeaderCount);
    return bytes;
}

bool rejects(const std::vector<std::uint8_t>& bytes) {
    try {
        Relinker::ElfReader(bytes).ReadSectionHeaders();
    } catch (const Relinker::RelinkerException&) {
        return true;
    }
    return false;
}

void zeroSectionHeaderEntrySize() {
    require(rejects(image(0, 0xFFFF)), "A zero section header entry size with sections present was accepted");
    require(!rejects(image(0, 0)), "A zero section header entry size without sections was rejected");
    require(!rejects(image(64, 1)), "A well-formed section header table was rejected");
}

}

int main() {
    try {
        zeroSectionHeaderEntrySize();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "ELF reader tests passed\n";
    return 0;
}
