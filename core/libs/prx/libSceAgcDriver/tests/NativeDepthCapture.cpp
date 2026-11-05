#include "prx/libSceAgcDriver/Execution/include/NativeDepthCapture.hpp"
#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace {

void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

template<typename F>
void Refuses(F&& function, const char* message) {
    bool refused = false;
    try { function(); } catch (const std::runtime_error&) { refused = true; }
    Require(refused, message);
}

struct Directory {
    std::filesystem::path path = std::filesystem::temp_directory_path() / ("aps5-native-depth-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Directory() { Require(std::filesystem::create_directory(path), "cannot create depth codec test directory"); }
    ~Directory() { std::error_code error; std::filesystem::remove_all(path, error); }
};

using Snapshot = AgcDriver::NativeDepthSnapshot;

std::vector<unsigned char> Bytes(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    Require(file.is_open(), "cannot read codec fixture");
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void Save(const std::filesystem::path& path, const std::vector<unsigned char>& bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    Require(file.good(), "cannot write codec fixture");
}

void Word(std::vector<unsigned char>& bytes, std::size_t at, std::uint32_t word) {
    for (unsigned byte = 0; byte != 4; ++byte) bytes.at(at + byte) = static_cast<unsigned char>(word >> (byte * 8u));
}

void Run() {
    Directory directory;
    const auto path = directory.path / "depth.bin";
    Require(AgcDriver::ReadNativeDepthCapture(path).empty(), "an older capture requires a depth file");
    AgcDriver::WriteNativeDepthCapture(path, {});
    Require(AgcDriver::ReadNativeDepthCapture(path).empty(), "an empty depth capture changed");
    Snapshot first;
    first.target = {0x123456780000ull, 0u, {2u, 2u}, VK_FORMAT_D16_UNORM, 0.375f, 0u, 0x123456790000ull, true, 1u, 24u, 3u, 5u, 5u, true, true, 2u, true, true};
    first.hostFormat = VK_FORMAT_D16_UNORM;
    first.depth = {std::byte{0}, std::byte{0}, std::byte{255}, std::byte{255}, std::byte{0}, std::byte{128}, std::byte{0}, std::byte{64}};
    Snapshot second;
    second.target = {0x223456780000ull, 0x223456790000ull, {2u, 1u}, VK_FORMAT_D32_SFLOAT_S8_UINT, 1.0f, 37u};
    second.hostFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
    second.depth = {std::byte{0}, std::byte{0}, std::byte{0}, std::byte{63}, std::byte{0}, std::byte{0}, std::byte{128}, std::byte{63}};
    second.stencil = {std::byte{7}, std::byte{233}};
    Snapshot third;
    third.target = {0u, 0x323456790000ull, {1u, 1u}, VK_FORMAT_D16_UNORM_S8_UINT, 1.0f, 19u};
    third.hostFormat = VK_FORMAT_D24_UNORM_S8_UINT;
    third.stencil = {std::byte{19}};
    const std::array snapshots{first, second, third};
    Require(AgcDriver::ValidateNativeDepthSnapshots(snapshots) == 19u, "decoded native depth size is wrong");
    AgcDriver::WriteNativeDepthCapture(path, snapshots);
    const auto decoded = AgcDriver::ReadNativeDepthCapture(path);
    Require(decoded.size() == 3u, "native owner records were lost");
    const auto& target = decoded[0].target;
    Require(target.address == 0x123456780000ull && target.stencilAddress == 0u && target.htileAddress == 0x123456790000ull && target.extent.width == 2u && target.extent.height == 2u && target.format == VK_FORMAT_D16_UNORM && target.clearDepth == 0.375f && target.samples == 1u, "depth target geometry or clear state was lost");
    Require(target.htileStencil && target.iterate256 && target.htilePipeAligned && target.htileDepthOnly && target.gcn && target.depthSwizzle == 24u && target.pipeBankXor == 3u && target.firstSlice == 5u && target.lastSlice == 5u && target.htileVrsEncoding == 2u, "depth target layout metadata was lost");
    Require(decoded[0].hostFormat == VK_FORMAT_D16_UNORM && decoded[0].depth == first.depth && decoded[0].stencil.empty(), "D16 texels changed");
    Require(decoded[1].depth == second.depth && decoded[1].stencil == second.stencil && decoded[1].target.clearStencil == 37u, "depth or stencil texels changed");
    Require(decoded[2].target.address == 0u && decoded[2].depth.empty() && decoded[2].stencil == third.stencil && decoded[2].hostFormat == VK_FORMAT_D24_UNORM_S8_UINT, "a stencil-only owner was lost");
    auto invalid = first;
    invalid.depth.pop_back();
    Refuses([&] { AgcDriver::WriteNativeDepthCapture(path, std::span(&invalid, 1)); }, "a short D16 plane was accepted");
    invalid = first;
    invalid.target.samples = 4u;
    Refuses([&] { AgcDriver::WriteNativeDepthCapture(path, std::span(&invalid, 1)); }, "a multisampled native snapshot was accepted");
    const std::array duplicates{first, first};
    Refuses([&] { AgcDriver::WriteNativeDepthCapture(path, duplicates); }, "two current owners of one depth plane were accepted");
    const auto valid = Bytes(path);
    Require(valid.size() > 128u, "the valid codec fixture is empty");
    for (const auto length : {0u, 19u, 23u, 24u, 127u, static_cast<unsigned>(valid.size() - 1u)}) {
        auto broken = valid;
        broken.resize(length);
        Save(path, broken);
        Refuses([&] { static_cast<void>(AgcDriver::ReadNativeDepthCapture(path)); }, "a truncated depth file was accepted");
    }
    for (const auto [at, value] : std::array<std::array<std::uint32_t, 2>, 7>{{{20u, 0xffffffffu}, {48u, 0u}, {48u, 0xffffffffu}, {60u, 0xffffffffu}, {72u, 4u}, {96u, 0x80000000u}, {120u, 0xffffffffu}}}) {
        auto broken = valid;
        Word(broken, at, value);
        Save(path, broken);
        Refuses([&] { static_cast<void>(AgcDriver::ReadNativeDepthCapture(path)); }, "invalid depth metadata or sizes were accepted");
    }
    for (const auto at : {0u, 128u}) {
        auto broken = valid;
        broken[at] ^= 0xffu;
        Save(path, broken);
        Refuses([&] { static_cast<void>(AgcDriver::ReadNativeDepthCapture(path)); }, "a bad depth header or compressed payload was accepted");
    }
    auto trailing = valid;
    trailing.push_back(0u);
    Save(path, trailing);
    Refuses([&] { static_cast<void>(AgcDriver::ReadNativeDepthCapture(path)); }, "trailing depth data was accepted");
}

}

int main() {
    try { Run(); } catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
    std::puts("native depth capture codec tests passed");
}
