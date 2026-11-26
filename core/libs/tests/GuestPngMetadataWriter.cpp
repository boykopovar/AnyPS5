#include "prx/libScePngParser/libScePngParser.hpp"
#include "prx/libSceSysmodule/include/Api.hpp"
#include "Decoder/Png.hpp"
#include <zlib.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using Bytes = std::vector<std::uint8_t>;

void Require(bool condition, const char* reason) {
    if (!condition) {
        std::fprintf(stderr, "PNG writer: %s\n", reason);
        std::abort();
    }
}

struct String {
    union {
        char bytes[16];
        const char* pointer;
    };
    std::uint64_t size;
    std::uint64_t capacity;
};

struct Value {
    std::int32_t type;
    std::int64_t size;
    const char* data;
};

struct Metadata {
    const std::uintptr_t* table;
    std::string title;
    std::string comment;
    std::vector<std::string> keys;
};

void* APS5_VABI Allocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI Release(void* memory) { std::free(memory); }

int APS5_VABI Get(Metadata* metadata, const String* key, Value* value) {
    Require(sceSysmoduleIsLoadedInternal(0x80000020u) == 0, "RTC must be loaded around the callback");
    Require(value->type == 2 && key->capacity == 15, "text key layout");
    metadata->keys.emplace_back(key->bytes, key->size);
    const auto& text = metadata->keys.back() == "title" ? metadata->title : metadata->comment;
    if (text == "<absent>") return 1;
    value->size = static_cast<std::int64_t>(text.size());
    value->data = text.data();
    return 0;
}

void Append32(Bytes& result, std::uint32_t value) {
    for (int shift : {24, 16, 8, 0}) result.push_back(static_cast<std::uint8_t>(value >> shift));
}

Bytes Chunk(std::string_view type, const Bytes& data) {
    Bytes bytes;
    Append32(bytes, static_cast<std::uint32_t>(data.size()));
    bytes.insert(bytes.end(), type.begin(), type.end());
    bytes.insert(bytes.end(), data.begin(), data.end());
    Append32(bytes, crc32(0, bytes.data() + 4, static_cast<uInt>(bytes.size() - 4)));
    return bytes;
}

Bytes Text(std::string_view type, std::string_view key, std::string_view text) {
    Bytes data(key.begin(), key.end());
    data.insert(data.end(), type == "iTXt" ? 5 : 1, 0);
    data.insert(data.end(), text.begin(), text.end());
    return Chunk(type, data);
}

Bytes WithChunks(const Bytes& png, const std::vector<Bytes>& chunks) {
    Bytes bytes(png.begin(), png.begin() + 33);
    for (const auto& part : chunks) bytes.insert(bytes.end(), part.begin(), part.end());
    bytes.insert(bytes.end(), png.begin() + 33, png.end());
    return bytes;
}

void Save(const std::filesystem::path& path, const Bytes& bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    file.close();
    Require(file.good(), "fixture write");
}

Bytes Read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

}

int main() {
    std::array<std::uintptr_t, 11> config{};
    config[0] = reinterpret_cast<std::uintptr_t>(Allocate);
    config[3] = reinterpret_cast<std::uintptr_t>(Release);
    Require(libScePngParserUnknown_vJpgNFIBlCk_nid_no_patch(config.data()) == 0, "initialize");
    const std::filesystem::path temp("user/av_contents/content_tmp/D9C31768-CC44-4E0A-B369-78E2B97F0A8E.png");
    std::filesystem::create_directories(temp.parent_path());
    const std::string pathName = "/metadata-writer.png";
    String path{};
    path.pointer = pathName.c_str();
    path.size = path.capacity = pathName.size();
    const std::filesystem::path image = pathName.substr(1);
    const std::array<std::uintptr_t, 3> table{0, 0, reinterpret_cast<std::uintptr_t>(Get)};
    Metadata metadata{table.data(), {}, {}, {}};
    const auto write = [&](std::string title, std::string comment) {
        metadata.keys.clear();
        metadata.title = std::move(title);
        metadata.comment = std::move(comment);
        const int result = libScePngParserWriteMetadata_nid_no_patch(&path, &metadata);
        Require(metadata.keys == std::vector<std::string>{"title", "comment"}, "lookup order");
        return result;
    };
    const Bytes pixels{255, 0, 0, 255, 0, 210, 0, 255};
    const auto png = Decoder::Png::Encode(pixels, 2, 1, 4);
    const auto oldTitle = Text("iTXt", "Title", "old");
    const auto oldComment = Text("iTXt", "Comment", "old comment");
    const auto other = Text("iTXt", "Game Title", "keep");
    const auto similar = Text("iTXt", "Title ID", "PPSA00000");
    const auto plain = Text("tEXt", "Title", "unchanged");
    const auto custom = Chunk("vpAg", {2, 3, 5, 7});
    const auto original = WithChunks(png, {oldComment, oldTitle, similar, other, plain, custom, oldTitle});
    Save(image, original);
    Require(write("Δ", "<absent>") == 0, "title update");
    const auto title = Text("iTXt", "Title", "Δ");
    Require(Read(image) == WithChunks(png, {title, oldComment, similar, other, plain, custom}), "title-only bytes");
    Require(write("", "text") == 0, "comment update");
    const auto comment = Text("iTXt", "Comment", "text");
    Require(Read(image) == WithChunks(png, {comment, title, similar, other, plain, custom}), "comment-only bytes");
    Require(write("Δ", "text") == 0, "combined update");
    const auto combined = WithChunks(png, {title, comment, similar, other, plain, custom});
    const auto written = Read(image);
    const auto decoded = Decoder::Png::Decode(written);
    Require(written == combined && decoded && decoded->pixels == pixels, "pixel-preserving combined rewrite");
    Save(temp, {19, 23});
    Require(write("<absent>", "") == 0 && Read(image) == combined && Read(temp) == Bytes({19, 23}), "empty update");
    bool occupied = false;
    try { static_cast<void>(write("other", "")); } catch (const std::runtime_error&) { occupied = true; }
    Require(occupied && Read(image) == combined && Read(temp) == Bytes({19, 23}), "occupied temp preserves both files");
    std::filesystem::remove(temp);
    auto truncated = combined;
    truncated.resize(truncated.size() - 3);
    Save(image, truncated);
    Require(write("more", "") == static_cast<int>(0x809D4038u) && Read(image) == truncated && !std::filesystem::exists(temp),
            "incomplete source changed");
    Save(image, {'x', 'y'});
    Require(write("more", "") == static_cast<int>(0x809D4038u), "other format");
    const auto large = Chunk("vpAg", Bytes((4 << 20) + 19, 0x6e));
    Save(image, WithChunks(png, {large, oldTitle}));
    Require(write("wide", "") == 0 && Read(image) == WithChunks(png, {Text("iTXt", "Title", "wide"), large}),
            "large ancillary chunk");
    std::filesystem::remove(image);
    Require(write("more", "") == static_cast<int>(0x809D4038u), "missing file");
    Require(_ZN23sceMetadataReaderWriter9pngParser17pngParserFinalizeEv() == 0, "finalize");
}
