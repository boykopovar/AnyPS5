#include "prx/libSceImageUtil/libSceImageUtil.hpp"

#include "Decoder/Jpeg.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct GuestString {
    union {
        char bytes[16];
        const char* pointer;
    };
    std::uint64_t size;
    std::uint64_t capacity;
};
static_assert(sizeof(GuestString) == 32);

constexpr auto ReadDimensions = libSceImageUtilUnknown_pjWv_plus__plus_MzaJY_nid_no_patch;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ImageUtil: %s\n", message);
    std::abort();
}

void WriteFile(const char* filename, std::span<const std::uint8_t> bytes) {
    std::ofstream file(filename, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    file.close();
    Require(file.good(), "write JPEG fixture");
}

void TestDimensions(const char* filename, std::uint32_t width, std::uint32_t height) {
    const std::string text = std::string("/") + filename;
    GuestString path{};
    path.size = text.size();
    if (text.size() < sizeof(path.bytes)) {
        path.capacity = sizeof(path.bytes) - 1;
        std::memcpy(path.bytes, text.c_str(), text.size() + 1);
    } else {
        path.capacity = text.capacity();
        path.pointer = text.c_str();
    }

    const auto image = Decoder::Jpeg::Encode(std::vector<std::uint8_t>(width * height * 3, 91), width, height, 3, 85);
    Require(!image.empty(), "encode JPEG fixture");
    WriteFile(filename, image);

    constexpr std::uint64_t marker = 0xABABABABABABABAB;
    std::array output{marker, marker, marker, marker};
    Require(ReadDimensions(&path, output.data() + 1) == 0, "read JPEG dimensions");
    Require(output == std::array{marker, std::uint64_t{width}, std::uint64_t{height}, marker},
            "write exactly the two zero-extended dimensions");

    output.fill(marker);
    const auto unchanged = output;
    WriteFile(filename, std::span(image).first(8));
    Require(ReadDimensions(&path, output.data() + 1) == static_cast<int>(0x809D4050u), "reject truncated JPEG");
    Require(output == unchanged, "truncated JPEG changed output");

    std::filesystem::remove(filename);
    Require(ReadDimensions(&path, output.data() + 1) == static_cast<int>(0x809D401Eu), "report missing file");
    Require(output == unchanged, "missing file changed output");
    Require(ReadDimensions(&path, nullptr) == static_cast<int>(0x809D4013u), "check null output before stat");
}

}

int main() {
    std::array<std::uint64_t, 2> output{19, 27};
    Require(ReadDimensions(nullptr, output.data()) == static_cast<int>(0x809D4013u), "reject null path");
    Require(output == std::array<std::uint64_t, 2>{19, 27}, "null path changed output");

    GuestString invalid{};
    invalid.capacity = 15;
    invalid.size = 16;
    bool rejected = false;
    try {
        ReadDimensions(&invalid, output.data());
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected && output == std::array<std::uint64_t, 2>{19, 27}, "reject inconsistent guest string");

    TestDimensions("imutil.jpg", 19, 7);
    TestDimensions("image_util_dimensions_test.jpg", 41, 13);
}
