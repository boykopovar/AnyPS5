#include <elfpatcher/general/EntryStubBuilder.hpp>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

bool bytesAt(const std::vector<std::uint8_t>& stub, std::size_t& offset, const std::vector<std::uint8_t>& expected) {
    if (offset + expected.size() > stub.size() || std::memcmp(stub.data() + offset, expected.data(), expected.size()) != 0) return false;
    offset += expected.size();
    return true;
}

void stubTerminatesTheFrameChain() {
    constexpr std::uint64_t stubVaddr = 0xb32db98;
    constexpr std::uint64_t entryVaddr = 0x80;
    const auto stub = Elfpatcher::EntryStubBuilder().BuildEntryStub(stubVaddr, entryVaddr);
    std::size_t offset = 0;
    require(bytesAt(stub, offset, {0x48, 0x89, 0xe7}), "the stub does not pass the initial stack pointer in rdi");
    require(bytesAt(stub, offset, {0x48, 0x83, 0xe4, 0xf0}), "the stub does not align the stack to 16 bytes");
    require(bytesAt(stub, offset, {0x6a, 0x00, 0x6a, 0x00}), "the stub does not push a zero frame record");
    require(bytesAt(stub, offset, {0x48, 0x89, 0xe5}), "the stub does not point rbp at the zero frame record");
    require(bytesAt(stub, offset, {0x48, 0x31, 0xf6}), "the stub does not clear rsi");
    require(offset < stub.size() && stub[offset] == 0xe8, "the stub does not call the entry point");
    std::int32_t rel32 = 0;
    std::memcpy(&rel32, stub.data() + offset + 1, sizeof(rel32));
    offset += 5;
    require(stubVaddr + offset + static_cast<std::int64_t>(rel32) == entryVaddr, "the stub calls the wrong address");
    require(bytesAt(stub, offset, {0x0f, 0x0b}) && offset == stub.size(), "the stub does not end with ud2 after the call");
}

}

int main() {
    try {
        stubTerminatesTheFrameChain();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
