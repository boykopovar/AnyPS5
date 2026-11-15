#include "FontFixture.hpp"
#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

using namespace FreeType;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType open face check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static void SameFace(const GuestFaceRec* guest, FT_Face native, bool externalStream = false) {
    Require(guest->num_faces == native->num_faces && guest->face_index == native->face_index);
    const auto expectedFlags = native->face_flags | (externalStream ? FT_FACE_FLAG_EXTERNAL_STREAM : 0);
    Require(guest->face_flags == expectedFlags && guest->num_glyphs == native->num_glyphs);
    Require(guest->units_per_EM == native->units_per_EM && guest->ascender == native->ascender && guest->descender == native->descender);
}

static int streamReads = 0;
static int streamCloses = 0;

static std::uint64_t APS5_VABI ReadStream(GuestStreamRec* stream, std::uint64_t offset, unsigned char* output, std::uint64_t count) {
    ++streamReads;
    if (!count || offset >= stream->size) return 0;
    const auto length = std::min(count, stream->size - offset);
    std::memcpy(output, stream->base + offset, static_cast<std::size_t>(length));
    stream->pos = offset + length;
    return length;
}

static void APS5_VABI CloseStream(GuestStreamRec*) {
    ++streamCloses;
}

int main() {
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-freetype-open-face-" + std::to_string(std::random_device{}()));
    Require(std::filesystem::create_directory(root));
    std::filesystem::create_directory(root / "app0");
    const std::vector<unsigned char> font = FontFixture::BuildFont(3, {});
    {
        std::ofstream file(root / "app0" / "open.ttf", std::ios::binary);
        file.write(reinterpret_cast<const char*>(font.data()), static_cast<std::streamsize>(font.size()));
        Require(static_cast<bool>(file));
    }
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(root);

    GuestLibrary library{};
    FT_Library native = nullptr;
    Require(FT_Init_FreeType(&library.host) == 0 && FT_Init_FreeType(&native) == 0);

    GuestOpenArgs memory{FT_OPEN_MEMORY, font.data(), static_cast<std::int64_t>(font.size()), nullptr, nullptr, nullptr, 0, nullptr};
    FT_Open_Args nativeMemory{FT_OPEN_MEMORY, font.data(), static_cast<FT_Long>(font.size()), nullptr, nullptr, nullptr, 0, nullptr};
    GuestFaceRec* face = nullptr;
    FT_Face expected = nullptr;
    Require(FT_Open_Face_nid_postfix(&library, &memory, 0, &face) == FT_Open_Face(native, &nativeMemory, 0, &expected));
    Require(face && expected);
    SameFace(face, expected);
    Require(FT_Done_Face_nid_postfix(face) == 0 && FT_Done_Face(expected) == 0);

    GuestStreamRec memoryStream{};
    memoryStream.base = const_cast<unsigned char*>(font.data());
    memoryStream.size = font.size();
    memoryStream.close = CloseStream;
    GuestOpenArgs memoryStreamArgs{FT_OPEN_STREAM, nullptr, 0, nullptr, &memoryStream, nullptr, 0, nullptr};
    face = nullptr;
    expected = nullptr;
    Require(FT_Open_Face_nid_postfix(&library, &memoryStreamArgs, 0, &face) == FT_Open_Face(native, &nativeMemory, 0, &expected));
    Require(face && expected && streamCloses == 0);
    SameFace(face, expected, true);
    Require(FT_Done_Face_nid_postfix(face) == 0 && FT_Done_Face(expected) == 0);
    Require(streamCloses == 1);

    GuestStreamRec callbackStream{};
    callbackStream.base = const_cast<unsigned char*>(font.data());
    callbackStream.size = font.size();
    callbackStream.read = ReadStream;
    callbackStream.close = CloseStream;
    GuestOpenArgs callbackArgs{FT_OPEN_STREAM, nullptr, 0, nullptr, &callbackStream, nullptr, 0, nullptr};
    face = nullptr;
    expected = nullptr;
    Require(FT_Open_Face_nid_postfix(&library, &callbackArgs, 0, &face) == FT_Open_Face(native, &nativeMemory, 0, &expected));
    Require(face && expected && streamReads > 0 && streamCloses == 1);
    SameFace(face, expected, true);
    Require(FT_Done_Face_nid_postfix(face) == 0 && FT_Done_Face(expected) == 0);
    Require(streamCloses == 2);

    unsigned char invalidBytes[4]{};
    GuestStreamRec invalidStream{};
    invalidStream.base = invalidBytes;
    invalidStream.size = sizeof(invalidBytes);
    invalidStream.close = CloseStream;
    GuestOpenArgs invalidStreamArgs{FT_OPEN_STREAM, nullptr, 0, nullptr, &invalidStream, nullptr, 0, nullptr};
    face = nullptr;
    Require(FT_Open_Face_nid_postfix(&library, &invalidStreamArgs, 0, &face) != 0);
    Require(face == nullptr && streamCloses == 3);

    char path[] = "/app0/open.ttf";
    GuestOpenArgs byPath{FT_OPEN_PATHNAME, nullptr, 0, path, nullptr, nullptr, 0, nullptr};
    FT_Open_Args nativePath{FT_OPEN_MEMORY, font.data(), static_cast<FT_Long>(font.size()), nullptr, nullptr, nullptr, 0, nullptr};
    face = nullptr;
    expected = nullptr;
    Require(FT_Open_Face_nid_postfix(&library, &byPath, 0, &face) == FT_Open_Face(native, &nativePath, 0, &expected));
    Require(face && expected);
    SameFace(face, expected);
    Require(FT_Done_Face_nid_postfix(face) == 0 && FT_Done_Face(expected) == 0);

    char missing[] = "/app0/missing.ttf";
    GuestOpenArgs byMissingPath{FT_OPEN_PATHNAME, nullptr, 0, missing, nullptr, nullptr, 0, nullptr};
    face = reinterpret_cast<GuestFaceRec*>(1);
    Require(FT_Open_Face_nid_postfix(&library, &byMissingPath, 0, &face) == FT_Err_Cannot_Open_Resource);
    GuestOpenArgs nullPath{FT_OPEN_PATHNAME, nullptr, 0, nullptr, nullptr, nullptr, 0, nullptr};
    Require(FT_Open_Face_nid_postfix(&library, &nullPath, 0, &face) == FT_Err_Invalid_Argument);

    GuestOpenArgs both{FT_OPEN_MEMORY | FT_OPEN_PATHNAME, font.data(), static_cast<std::int64_t>(font.size()), path, nullptr, nullptr, 0, nullptr};
    FT_Open_Args nativeBoth{FT_OPEN_MEMORY | FT_OPEN_PATHNAME, font.data(), static_cast<FT_Long>(font.size()), path, nullptr, nullptr, 0, nullptr};
    face = reinterpret_cast<GuestFaceRec*>(1);
    expected = nullptr;
    Require(FT_Open_Face_nid_postfix(&library, &both, 0, &face) == FT_Err_Invalid_Argument);
    Require(FT_Open_Face(native, &nativeBoth, 0, &expected) == FT_Err_Invalid_Argument);
    Require(face == reinterpret_cast<GuestFaceRec*>(1));

    FT_Open_Args nativeNullMemory{FT_OPEN_MEMORY, nullptr, 0, nullptr, nullptr, nullptr, 0, nullptr};
    GuestOpenArgs nullMemory{FT_OPEN_MEMORY, nullptr, 0, nullptr, nullptr, nullptr, 0, nullptr};
    expected = nullptr;
    Require(FT_Open_Face_nid_postfix(&library, &nullMemory, 0, &face) == FT_Open_Face(native, &nativeNullMemory, 0, &expected));
    GuestOpenArgs noSource{0, nullptr, 0, nullptr, nullptr, nullptr, 0, nullptr};
    FT_Open_Args nativeNoSource{};
    Require(FT_Open_Face_nid_postfix(&library, &noSource, 0, &face) == FT_Open_Face(native, &nativeNoSource, 0, &expected));

    face = nullptr;
    expected = nullptr;
    Require(FT_Open_Face_nid_postfix(&library, &memory, -1, nullptr) == FT_Open_Face(native, &nativeMemory, -1, nullptr));
    Require(FT_Open_Face_nid_postfix(&library, &memory, 0, nullptr) == FT_Open_Face(native, &nativeMemory, 0, nullptr));
    Require(FT_Open_Face_nid_postfix(&library, &memory, 1, &face) == FT_Open_Face(native, &nativeMemory, 1, &expected));
    Require(face == nullptr && expected == nullptr);
    Require(FT_Open_Face_nid_postfix(&library, nullptr, 0, &face) == FT_Open_Face(native, nullptr, 0, &expected));
    Require(FT_Open_Face_nid_postfix(nullptr, &memory, 0, &face) == FT_Open_Face(nullptr, &nativeMemory, 0, &expected));
    const FT_Long maskedIndex = static_cast<FT_Long>(0x80000000ll);
    GuestFaceRec* masked = nullptr;
    FT_Face nativeMasked = nullptr;
    Require(FT_Open_Face_nid_postfix(&library, &memory, 0x180000000ll, &masked) == FT_Open_Face(native, &nativeMemory, maskedIndex, &nativeMasked));
    Require(masked && nativeMasked);
    Require(FT_Done_Face_nid_postfix(masked) == 0 && FT_Done_Face(nativeMasked) == 0);

    Require(FT_Done_FreeType(library.host) == 0 && FT_Done_FreeType(native) == 0);
    std::filesystem::current_path(previous);
    std::filesystem::remove_all(root);
}
