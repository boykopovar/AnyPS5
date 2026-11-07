#include <relinker/parsing/RelrRelocations.hpp>
#include <io/BufferUtils.hpp>
#include <functional>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

void Require(const bool condition) {
    if (!condition) throw std::runtime_error("RELR validation failed");
}

struct Fixture {
    std::vector<std::uint8_t> Bytes = std::vector<std::uint8_t>(0x2000);
    std::vector<Domain::ProgramHeader> Headers{{1, 7, 0, 0, 0, 0x2000, 0x3000, 0x1000}};
    std::vector<Domain::DynamicTag> Tags{{36, 0x1000}, {35, 8}, {37, 8}};

    Fixture() {
        Bytes[0] = 0x7f; Bytes[1] = 'E'; Bytes[2] = 'L'; Bytes[3] = 'F';
        Bytes[4] = 2; Bytes[5] = 1; Bytes[6] = 1;
        Io::WriteU16(Bytes, 16, 3); Io::WriteU16(Bytes, 18, 62); Io::WriteU32(Bytes, 20, 1);
        Io::WriteU64(Bytes, 32, 64); Io::WriteU16(Bytes, 52, 64);
        Io::WriteU16(Bytes, 54, 56); Io::WriteU16(Bytes, 56, 2);
        Io::WriteU32(Bytes, 64, 1); Io::WriteU32(Bytes, 68, 7);
        Io::WriteU64(Bytes, 96, 0x2000); Io::WriteU64(Bytes, 104, 0x3000); Io::WriteU64(Bytes, 112, 0x1000);
        Io::WriteU32(Bytes, 120, 2); Io::WriteU64(Bytes, 128, 0x100); Io::WriteU64(Bytes, 136, 0x100);
        Io::WriteU64(Bytes, 152, 64); Io::WriteU64(Bytes, 160, 64);
        for (std::size_t index = 0; index < Tags.size(); ++index) {
            Io::WriteU64(Bytes, 0x100 + index * 16, Tags[index].Tag);
            Io::WriteU64(Bytes, 0x108 + index * 16, Tags[index].Value);
        }
        for (std::size_t offset = 0x200; offset < 0x900; offset += 8) Io::WriteU64(Bytes, offset, offset + 0x900);
        SetWords({0x200});
    }

    void SetWords(const std::initializer_list<std::uint64_t> words) {
        Tags[1].Value = words.size() * 8;
        Io::WriteU64(Bytes, 0x118, words.size() * 8);
        std::size_t offset = 0x1000;
        for (const auto word : words) { Io::WriteU64(Bytes, offset, word); offset += 8; }
    }

    auto Read() const { return Relinker::ReadRelrRelocations(Bytes, Headers, Tags); }
};

void Reject(const std::function<void(Fixture&)>& change, const std::string_view diagnostic) {
    Fixture fixture;
    change(fixture);
    try { fixture.Read(); }
    catch (const Domain::RelinkerException& error) {
        Require(std::string_view(error.what()).find(diagnostic) != std::string_view::npos);
        return;
    }
    throw std::runtime_error("Malformed RELR input accepted");
}

}

int main() try {
    Reject([](auto& input) { input.SetWords({0x202}); }, "Misaligned RELR target address");
    Reject([](auto& input) { input.SetWords({0x204}); }, "Misaligned RELR target address");
    Reject([](auto& input) { input.SetWords({0x206}); }, "Misaligned RELR target address");
    return 0;
} catch (const std::exception& error) {
    return 1;
}
