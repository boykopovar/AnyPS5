#include <nid/NidPatcherUtils.hpp>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

void Require(bool condition) {
    if (!condition) throw std::runtime_error("NID buffer validation failed");
}

template<typename TAction>
void RequireRejected(TAction action) {
    try { action(); }
    catch (const std::runtime_error&) { return; }
    throw std::runtime_error("Malformed NID buffer was accepted");
}

}

int main() {
    try {
        using namespace Nid::Internal;
        const std::vector<std::uint8_t> unterminated{'n', 'a', 'm', 'e'};
        RequireRejected([&] { ReadCStr(unterminated, 0); });
        RequireRejected([&] { ReadCStr(unterminated, 3); });
        const std::vector<std::uint8_t> strings{'a', 0, 'b', 'c', 0};
        Require(ReadCStr(strings, 0) == "a");
        Require(ReadCStr(strings, 1).empty());
        Require(ReadCStr(strings, 2) == "bc");
        Require(ReadCStr(strings, 4).empty());
        RequireRejected([&] { ReadCStr(strings, strings.size()); });
        RequireRejected([&] { ReadCStr({}, 0); });

        std::vector<std::uint8_t> bytes(8, 0xa5);
        const std::uint64_t value = 0x1020304050607080;
        Write(bytes, 0, value);
        Require(Read<std::uint64_t>(bytes, 0) == value);
        Write<std::uint32_t>(bytes, 4, 0x12345678);
        Require(Read<std::uint32_t>(bytes, 4) == 0x12345678);
        const auto before = bytes;
        for (const auto offset : {std::size_t{5}, std::size_t{8}, std::size_t{9},
                std::numeric_limits<std::size_t>::max(), std::numeric_limits<std::size_t>::max() - 1,
                std::numeric_limits<std::size_t>::max() - 3}) {
            RequireRejected([&] { Read<std::uint32_t>(bytes, offset); });
            RequireRejected([&] { Write<std::uint32_t>(bytes, offset, 0); });
            Require(bytes == before);
        }
        RequireRejected([&] { Read<std::uint64_t>(bytes, 1); });
        std::vector<std::uint8_t> empty;
        RequireRejected([&] { Read<std::uint8_t>(empty, 0); });
        RequireRejected([&] { Write<std::uint8_t>(empty, 0, 0); });
        std::cout << "NID buffer validation passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
