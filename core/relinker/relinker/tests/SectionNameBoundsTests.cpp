#include <relinker/parsing/ElfReader.hpp>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<std::uint8_t>;

template<typename TValue>
void write(Bytes& bytes, std::size_t offset, TValue value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

Bytes fixture() {
    Bytes bytes(256);
    bytes[0] = 0x7f;
    bytes[1] = 'E';
    bytes[2] = 'L';
    bytes[3] = 'F';
    bytes[4] = 2;
    bytes[5] = 1;
    bytes[6] = 1;
    write<std::uint16_t>(bytes, 0x12, 62);
    write<std::uint32_t>(bytes, 0x14, 1);
    write<std::uint16_t>(bytes, 0x34, 64);
    write<std::uint64_t>(bytes, 0x28, 64);
    write<std::uint16_t>(bytes, 0x3a, 64);
    write<std::uint16_t>(bytes, 0x3c, 2);
    write<std::uint16_t>(bytes, 0x3e, 1);
    write<std::uint32_t>(bytes, 128, 1);
    write<std::uint32_t>(bytes, 132, 3);
    write<std::uint64_t>(bytes, 152, 192);
    write<std::uint64_t>(bytes, 160, 7);
    std::memcpy(bytes.data() + 192, "\0names\0", 7);
    return bytes;
}

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void rejected(const Bytes& bytes, const std::string& diagnostic) {
    try {
        Relinker::ElfReader(bytes).ReadSectionHeaders();
    } catch (const Relinker::RelinkerException& error) {
        require(error.what() == diagnostic, "Unexpected diagnostic: " + std::string(error.what()));
        return;
    }
    throw std::runtime_error("Accepted malformed section metadata: " + diagnostic);
}

}

int main() {
    try {
        auto bytes = fixture();
        auto headers = Relinker::ElfReader(bytes).ReadSectionHeaders();
        require(headers.size() == 2 && headers[0].Name.empty() && headers[1].Name == "names", "Valid names changed");
        write<std::uint32_t>(bytes, 128, 3);
        require(Relinker::ElfReader(bytes).ReadSectionHeaders()[1].Name == "mes", "Valid substring name rejected");
        bytes = fixture();
        write<std::uint64_t>(bytes, 152, 249);
        std::memcpy(bytes.data() + 249, "\0names\0", 7);
        require(Relinker::ElfReader(bytes).ReadSectionHeaders()[1].Name == "names", "Table ending at EOF rejected");
        bytes = fixture();
        write<std::uint16_t>(bytes, 0x3e, 0);
        require(Relinker::ElfReader(bytes).ReadSectionHeaders()[1].Name.empty(), "Absent name table rejected");
        write<std::uint16_t>(bytes, 0x3c, 0);
        write<std::uint16_t>(bytes, 0x3a, 0);
        write<std::uint64_t>(bytes, 0x28, std::numeric_limits<std::uint64_t>::max());
        require(Relinker::ElfReader(bytes).ReadSectionHeaders().empty(), "Absent section table rejected");
        bytes = fixture();
        write<std::uint64_t>(bytes, 160, 0);
        write<std::uint32_t>(bytes, 128, 0);
        require(Relinker::ElfReader(bytes).ReadSectionHeaders()[1].Name.empty(), "Empty name table rejected");
        for (const std::uint16_t size : {0, 56, 63, 65}) {
            bytes = fixture();
            write<std::uint16_t>(bytes, 0x3a, size);
            rejected(bytes, "Invalid ELF section header entry size: expected 64 bytes");
        }
        bytes = fixture();
        write<std::uint16_t>(bytes, 0x3e, 2);
        rejected(bytes, "Section name table index out of bounds");
        for (const std::uint64_t offset : {std::uint64_t{129}, std::numeric_limits<std::uint64_t>::max()}) {
            bytes = fixture();
            write<std::uint64_t>(bytes, 0x28, offset);
            rejected(bytes, "Section header table out of bounds");
        }
        for (const std::uint64_t size : {std::uint64_t{65}, std::numeric_limits<std::uint64_t>::max()}) {
            bytes = fixture();
            write<std::uint64_t>(bytes, 160, size);
            rejected(bytes, "Section name table out of bounds");
        }
        bytes = fixture();
        write<std::uint64_t>(bytes, 152, std::numeric_limits<std::uint64_t>::max());
        rejected(bytes, "Section name table out of bounds");
        for (const std::uint32_t offset : {7u, 8u, std::numeric_limits<std::uint32_t>::max()}) {
            bytes = fixture();
            write<std::uint32_t>(bytes, 128, offset);
            rejected(bytes, "Section name offset out of bounds");
        }
        bytes = fixture();
        write<std::uint64_t>(bytes, 160, 6);
        rejected(bytes, "Unterminated section name");
        std::cout << "Section name bounds tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
