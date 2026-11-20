#include "prx/libSceJpegParser/libSceJpegParser.hpp"

#include "Decoder/Jpeg.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "JpegParser: %s\n", message);
    std::abort();
}

using Bytes = std::vector<std::uint8_t>;

struct GuestString {
    union {
        char bytes[16];
        const char* pointer;
    };
    std::uint64_t size;
    std::uint64_t capacity;
};

std::vector<std::size_t> allocations;
unsigned releases = 0;
bool refuse = false;

void* APS5_VABI Allocate(std::size_t size) {
    allocations.push_back(size);
    return refuse ? nullptr : std::malloc(size);
}

void APS5_VABI Release(void* address) {
    ++releases;
    std::free(address);
}

void Append(Bytes& bytes, std::uint32_t value, unsigned count, bool little) {
    for (unsigned i = 0; i < count; ++i)
        bytes.push_back(static_cast<std::uint8_t>(value >> (8 * (little ? i : count - 1 - i))));
}

Bytes Image(const Bytes& exif = {}) {
    Bytes result{0xff, 0xd8};
    if (!exif.empty()) {
        result.insert(result.end(), {0xff, 0xe1});
        Append(result, static_cast<std::uint32_t>(exif.size() + 2), 2, false);
        result.insert(result.end(), exif.begin(), exif.end());
    }
    result.insert(result.end(), {0xff, 0xc0, 0, 11, 8, 0, 7, 0, 13, 1, 1, 0x11, 0,
                                 0xff, 0xda, 0, 8, 1, 1, 0, 0, 63, 0, 0, 0xff, 0xd9});
    return result;
}

Bytes Exif(const Bytes& image, bool little, std::uint32_t offset, std::uint32_t length, int compression = -1) {
    Bytes result{'E', 'x', 'i', 'f', 0, 0};
    Append(result, little ? 0x4949 : 0x4d4d, 2, little);
    Append(result, 42, 2, little);
    Append(result, 8, 4, little);
    Append(result, 0, 2, little);
    Append(result, 14, 4, little);
    Append(result, compression < 0 ? 2 : 3, 2, little);
    if (compression >= 0) {
        Append(result, 0x103, 2, little);
        Append(result, 3, 2, little);
        Append(result, 1, 4, little);
        Append(result, compression, 2, little);
        Append(result, 0, 2, little);
    }
    for (auto [tag, value] : {std::pair{0x201u, offset}, std::pair{0x202u, length}}) {
        Append(result, tag, 2, little);
        Append(result, 4, 2, little);
        Append(result, 1, 4, little);
        Append(result, value, 4, little);
    }
    Append(result, 0, 4, little);
    result.insert(result.end(), image.begin(), image.end());
    return result;
}

void TestExtract() {
    std::array<std::uintptr_t, 11> config{};
    config[0] = reinterpret_cast<std::uintptr_t>(Allocate);
    config[3] = reinterpret_cast<std::uintptr_t>(Release);
    refuse = true;
    Require(libSceJpegParserUnknown_v3aOHSEXfjo_nid_no_patch(config.data()) == static_cast<int>(0x809D4029u), "context allocation failure");
    refuse = false;
    GuestString empty{};
    empty.capacity = 15;
    Require(libSceJpegParserUnknown_minus_0oLMyS3AcA_nid_no_patch(&empty, nullptr) == static_cast<int>(0x809D4014u), "no context after failed allocation");
    Require(libSceJpegParserUnknown_v3aOHSEXfjo_nid_no_patch(config.data()) == 0, "initialize");
    Require(allocations.back() == 0x168, "context allocation size");
    Require(libSceJpegParserUnknown_v3aOHSEXfjo_nid_no_patch(nullptr) == static_cast<int>(0x809D4014u), "double initialize");
    const std::string longPath = "/jpeg_thumbnail_test.jpg";
    const std::string shortPath = "/thumb.jpg";
    for (bool little : {true, false}) {
        const std::string& name = little ? shortPath : longPath;
        const auto host = std::filesystem::path(name.substr(1));
        GuestString path{};
        path.size = name.size();
        path.capacity = std::max<std::size_t>(15, name.size());
        if (path.capacity == 15) std::memcpy(path.bytes, name.c_str(), name.size() + 1);
        else path.pointer = name.c_str();
        JpegParserThumbnail output;
        const auto check = [&](const Bytes& bytes, std::uint32_t expected, const Bytes& pixels = {}) {
            if (bytes.empty()) std::filesystem::remove(host);
            else {
                std::ofstream stream(host, std::ios::binary | std::ios::trunc);
                stream.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
                stream.close();
                Require(stream.good(), "write JPEG fixture");
            }
            std::memset(&output, 0xab, sizeof(output));
            const auto before = output;
            Require(libSceJpegParserUnknown_minus_0oLMyS3AcA_nid_no_patch(&path, &output) == static_cast<int>(expected), "extract result");
            if (expected) {
                Require(std::memcmp(&before, &output, sizeof(output)) == 0, "failed extract changed output");
                return;
            }
            Require(output.type == 1 && output.format == 0x20001 && output.width == 9 && output.height == 5, "thumbnail fields");
            Require(output.reserved04 == 0xabababab && output.reserved24 == 0xabababab, "padding preservation");
            Require(output.mappingSize == (pixels.size() + 0x3fff) / 0x4000 * 0x4000 && output.size == pixels.size(), "mapping size");
            Require(output.data && std::memcmp(output.data, pixels.data(), pixels.size()) == 0, "mapped JPEG bytes");
            VirtualQueryInfo info{};
            Require(sceKernelVirtualQuery(output.data, 0, &info, sizeof(info)) == 0 && info.is_flexible &&
                    std::strcmp(info.name, "SceMdrwFMem") == 0 && info.end - info.start == output.mappingSize, "named flexible map");
            Require(sceKernelMunmap(reinterpret_cast<std::uintptr_t>(output.data), output.mappingSize) == 0, "guest unmap");
        };
        auto thumb = Image();
        thumb[8] = 5;
        thumb[10] = 9;
        thumb.resize(thumb.size() - 2);
        if (!little) {
            thumb = Decoder::Jpeg::Encode(Bytes(9 * 5 * 3, 147), 9, 5, 3, 80);
            Require(!thumb.empty(), "encode real JPEG");
            thumb.resize(61475, 0x41);
        }
        const auto length = static_cast<std::uint32_t>(thumb.size());
        check(Image(Exif(thumb, little, 44, length)), 0, thumb);
        check(Image(Exif(thumb, little, 56, length, little ? 6 : 1)), 0, thumb);
        auto missingTag = Exif(thumb, little, 44, length);
        missingTag[6 + 16] = missingTag[6 + 17] = 0;
        check(Image(missingTag), 0x809D4038);
        auto noIfd = Exif(thumb, little, 44, length);
        std::fill_n(noIfd.begin() + 16, 4, 0);
        check(Image(noIfd), 0x809D4038);
        check(Image(Exif(thumb, little, 44, 0)), 0x809D4038);
        check(Image(Exif(thumb, little, 0xfffffffe, length)), 0x809D4038);
        check(Image(Exif(thumb, little, 44, length + 1)), 0x809D4038);
        thumb[0] = 0;
        check(Image(Exif(thumb, little, 44, length)), 0x809D4038);
        check(Image(), 0x809D4038);
        check({'n', 'o', 't', 'j', 'p', 'e', 'g'}, 0x809D4052);
        check({}, 0x809D401e);
    }
    Require(_ZN23sceMetadataReaderWriter10jpegParser18jpegParserFinalizeEv() == 0, "active finalize");
    Require(allocations.size() == releases + 1, "callback allocation/release balance");
    Require(libSceJpegParserUnknown_v3aOHSEXfjo_nid_no_patch(config.data()) == 0, "reinitialize");
    Require(_ZN23sceMetadataReaderWriter10jpegParser18jpegParserFinalizeEv() == 0, "finalize again");
}

}

int main() {
    errno = 71;
    Require(_ZN23sceMetadataReaderWriter10jpegParser18jpegParserFinalizeEv() == 0, "Finalize without Initialize did not return 0");
    Require(_ZN23sceMetadataReaderWriter10jpegParser18jpegParserFinalizeEv() == 0, "a second Finalize did not return 0");
    Require(errno == 71, "Finalize changed errno");
    TestExtract();
    std::puts("JpegParser tests passed");
    return 0;
}
