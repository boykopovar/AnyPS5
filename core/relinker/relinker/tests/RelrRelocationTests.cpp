#include <relinker/parsing/RelrRelocations.hpp>
#include <relinker/analysis/UnusedNidFilter/IRelativeRelocationIndex.hpp>
#include <io/BufferUtils.hpp>
#include <functional>
#include <iostream>
#include <limits>
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
    Fixture fixture;
    Require(fixture.Read() == std::map<Domain::VirtualAddress, std::uint64_t>{{0x200, 0xb00}});
    for (unsigned bit = 1; bit < 64; ++bit) {
        fixture.SetWords({0x200, 1ull | (1ull << bit), 1ull | (1ull << 63)});
        const auto result = fixture.Read();
        const auto target = 0x208 + (bit - 1) * 8;
        Require(result.size() == 3 && result.at(0x200) == 0xb00 && result.at(target) == target + 0x900);
        Require(result.at(0x5f0) == 0xef0);
        const auto index = Relinker::UnusedNidFilter::BuildRelativeRelocationIndex(fixture.Bytes);
        Require(index->TargetOfSlot(target) == target + 0x900 && index->TargetOfSlot(0x5f0) == 0xef0);
    }
    fixture.SetWords({0x200, std::numeric_limits<std::uint64_t>::max()});
    Require(fixture.Read().size() == 64);
    fixture.SetWords({0x200, 1, 3});
    Require(fixture.Read().size() == 2 && fixture.Read().at(0x400) == 0xd00);
    fixture.SetWords({0x800, 0x202});
    Require(fixture.Read().size() == 2 && fixture.Read().at(0x202) == 0x0b08000000000000ull);
    fixture.SetWords({0x2000});
    Require(fixture.Read().at(0x2000) == 0);
    fixture.SetWords({0x1ffc});
    Io::WriteU32(fixture.Bytes, 0x1ffc, 0x12345678);
    Require(fixture.Read().at(0x1ffc) == 0x12345678);
    fixture.SetWords({});
    Require(fixture.Read().empty());
    fixture.Tags.clear();
    Require(fixture.Read().empty());

    for (unsigned mask = 1; mask < 7; ++mask) {
        Reject([&](auto& input) {
            const auto tags = input.Tags;
            input.Tags.clear();
            for (unsigned index = 0; index < 3; ++index) if (mask & (1u << index)) input.Tags.push_back(tags[index]);
        }, "Incomplete DT_RELR metadata");
    }
    Reject([](auto& input) { input.Tags[2].Value = 4; }, "DT_RELRENT");
    Reject([](auto& input) { input.Tags[1].Value = 9; }, "DT_RELRSZ");
    for (std::size_t index = 0; index < 3; ++index) {
        Reject([&](auto& input) { auto tag = input.Tags[index]; ++tag.Value; input.Tags.push_back(tag); }, "Conflicting duplicate");
    }
    Reject([](auto& input) { input.SetWords({3}); }, "bitmap precedes address");
    Reject([](auto& input) { input.SetWords({0x200, 0x200}); }, "Overlapping RELR targets");
    Reject([](auto& input) { input.SetWords({0x200, 0x206}); }, "Overlapping RELR targets");
    Reject([](auto& input) { input.SetWords({0x3000}); }, "target exceeds load");
    Reject([](auto& input) { input.SetWords({0x2ffc}); }, "target exceeds load");
    Reject([](auto& input) { input.Tags[0].Value = 0x2000; }, "table exceeds file-backed");
    Reject([](auto& input) { input.Tags[0].Value = 0x1ffc; }, "table exceeds file-backed");
    Reject([](auto& input) { input.Tags[1].Value = 0xfffffffffffffff8ull; }, "table exceeds file-backed");
    Reject([](auto& input) { input.Headers[0].Offset = 0xffffffffffffffffull; }, "Invalid RELR load");
    Reject([](auto& input) { input.Headers[0].MemorySize = 0x1000; }, "Invalid RELR load");
    Reject([](auto& input) { input.Headers.push_back(input.Headers.front()); }, "Ambiguous RELR load");
    Reject([](auto& input) {
        const auto address = std::numeric_limits<std::uint64_t>::max() - 0x3f;
        input.Headers.push_back({1, 6, 0, address, 0, 0, 0x38, 1});
        input.SetWords({address, 1});
    }, "bitmap address overflow");
    std::cout << "RELR address, bitmap, bounds, addend and reachability tests passed\n";
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
