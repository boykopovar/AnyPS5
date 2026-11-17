#include "FontFixture.hpp"
#include "Type1Fixture.hpp"
#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libSceFreeTypeOl/libSceFreeTypeOl.hpp"
#include "prx/libSceFreeTypeOt/libSceFreeTypeOt.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#include FT_RENDER_H
#include <freetype/internal/ftobjs.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

using namespace FreeType;

namespace {

void Check(bool value, int line) {
    if (value) return;
    std::fprintf(stderr, "FreeType library object check failed at line %d\n", line);
    std::abort();
}
#define Require(value) Check((value), __LINE__)

template <typename TFunction>
bool Throws(TFunction function) {
    try {
        function();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

struct Recorder {
    GuestMemoryRec memory{};
    int selfFrees = 0;
    int otherFrees = 0;
};

void* APS5_VABI RecordAlloc(GuestMemoryRec*, std::int64_t size) {
    return std::malloc(static_cast<std::size_t>(size));
}

void APS5_VABI RecordFree(GuestMemoryRec* memory, void* block) {
    auto* recorder = static_cast<Recorder*>(memory->user);
    if (block == memory) {
        Require(recorder->selfFrees == 0);
        ++recorder->selfFrees;
        return;
    }
    ++recorder->otherFrees;
    std::free(block);
}

void* APS5_VABI RecordRealloc(GuestMemoryRec*, std::int64_t, std::int64_t size, void* block) {
    return std::realloc(block, static_cast<std::size_t>(size));
}

void InitRecorder(Recorder& recorder) {
    recorder.memory = {&recorder, RecordAlloc, RecordFree, RecordRealloc};
}

void* HostAlloc(FT_Memory, long size) {
    return std::malloc(static_cast<std::size_t>(size));
}

void HostFree(FT_Memory, void* block) {
    std::free(block);
}

void* HostRealloc(FT_Memory, long, long size, void* block) {
    return std::realloc(block, static_cast<std::size_t>(size));
}

void NewLibrary() {
    Recorder recorder;
    InitRecorder(recorder);
    FT_MemoryRec_ hostMemory{nullptr, HostAlloc, HostFree, HostRealloc};
    GuestLibrary* library = nullptr;
    FT_Library oracle = nullptr;
    Require(FT_New_Library_nid_postfix(nullptr, &library) == FT_New_Library(nullptr, &oracle));
    Require(FT_New_Library_nid_postfix(&recorder.memory, nullptr) == FT_New_Library(&hostMemory, nullptr));
    Require(FT_New_Library_nid_postfix(nullptr, &library) == FT_Err_Invalid_Argument && !library);
    Require(FT_New_Library_nid_postfix(&recorder.memory, &library) == 0 && library);
    Require(FT_New_Library(&hostMemory, &oracle) == 0);
    Require(!FT_Get_Renderer_nid_postfix(library, FT_GLYPH_FORMAT_OUTLINE) && !FT_Get_Renderer(oracle, FT_GLYPH_FORMAT_OUTLINE));
    const auto font = FontFixture::BuildFont(3, {});
    GuestFaceRec* face = nullptr;
    FT_Face oracleFace = nullptr;
    Require(FT_New_Memory_Face_nid_postfix(library, font.data(), static_cast<std::int64_t>(font.size()), 0, &face) ==
            FT_New_Memory_Face(oracle, font.data(), static_cast<FT_Long>(font.size()), 0, &oracleFace));
    Require(!face);
    FT_Add_Default_Modules_nid_postfix(library);
    FT_Add_Default_Modules(oracle);
    Require(FT_Get_Renderer_nid_postfix(library, FT_GLYPH_FORMAT_OUTLINE) && FT_Get_Renderer(oracle, FT_GLYPH_FORMAT_OUTLINE));
    Require(FT_New_Memory_Face_nid_postfix(library, font.data(), static_cast<std::int64_t>(font.size()), 0, &face) == 0 && face);
    Require(FT_New_Memory_Face(oracle, font.data(), static_cast<FT_Long>(font.size()), 0, &oracleFace) == 0);
    Require(face->num_glyphs == oracleFace->num_glyphs);
    Require(FT_Reference_Library_nid_postfix(library) == 0 && FT_Reference_Library(oracle) == 0);
    Require(FT_Reference_Library_nid_postfix(library) == 0 && FT_Reference_Library(oracle) == 0);
    Require(FT_Done_FreeType_nid_postfix(library) == 0 && recorder.selfFrees == 0);
    Require(FT_Done_Library(oracle) == 0);
    Require(FT_Done_FreeType_nid_postfix(library) == 0 && recorder.selfFrees == 0);
    Require(FT_Done_Library(oracle) == 0);
    Require(FT_Get_Char_Index_nid_postfix(face, 'A') == FT_Get_Char_Index(oracleFace, 'A'));
    Require(FT_Load_Glyph_nid_postfix(face, 1, FT_LOAD_NO_SCALE) == FT_Load_Glyph(oracleFace, 1, FT_LOAD_NO_SCALE));
    Require(FT_Get_Renderer_nid_postfix(library, FT_GLYPH_FORMAT_OUTLINE));
    Require(library->references == 1);
    Require(Throws([&] { FT_Done_FreeType_nid_postfix(library); }));
    Require(Throws([&] { FT_Done_FreeType_nid_postfix(library); }));
    Require(library->references == 1 && library->host && recorder.selfFrees == 0 && recorder.otherFrees == 0);
    Require(FT_Get_Char_Index_nid_postfix(face, 'A') == FT_Get_Char_Index(oracleFace, 'A'));
    Require(FT_Done_Library(library->host) == 0);
    delete library;
    Require(FT_Done_Library(oracle) == 0);
    Require(recorder.selfFrees == 0 && recorder.otherFrees == 0);
}

void ReferenceLibrary() {
    Require(FT_Reference_Library_nid_postfix(nullptr) == FT_Reference_Library(nullptr));
    Require(FT_Reference_Library_nid_postfix(nullptr) == FT_Err_Invalid_Library_Handle);
    GuestLibrary* library = nullptr;
    Require(FT_Init_FreeType_nid_postfix(&library) == 0);
    const auto font = FontFixture::BuildFont(3, {});
    GuestFaceRec* face = nullptr;
    Require(FT_New_Memory_Face_nid_postfix(library, font.data(), static_cast<std::int64_t>(font.size()), 0, &face) == 0);
    const std::uint32_t glyph = FT_Get_Char_Index_nid_postfix(face, 'A');
    Require(FT_Reference_Library_nid_postfix(library) == 0);
    Require(FT_Reference_Library_nid_postfix(library) == 0);
    Require(FT_Done_FreeType_nid_postfix(library) == 0);
    Require(FT_Done_FreeType_nid_postfix(library) == 0);
    Require(FT_Get_Char_Index_nid_postfix(face, 'A') == glyph);
    Require(FT_Load_Glyph_nid_postfix(face, 1, FT_LOAD_NO_SCALE) == 0);
    GuestFaceRec* second = nullptr;
    Require(FT_New_Memory_Face_nid_postfix(library, font.data(), static_cast<std::int64_t>(font.size()), 0, &second) == 0);
    Require(FT_Done_Face_nid_postfix(second) == 0);
    Require(FT_Done_FreeType_nid_postfix(library) == 0);
    Require(FT_Done_FreeType_nid_postfix(nullptr) == FT_Done_FreeType(nullptr));
}

std::vector<void*> Renderers(GuestLibrary* library, std::uint32_t format) {
    std::vector<void*> result;
    void* node = nullptr;
    for (;;) {
        void* renderer = FT_Lookup_Renderer_nid_postfix(library, format, &node);
        Require((renderer == nullptr) == (node == nullptr));
        if (!renderer) return result;
        result.push_back(renderer);
    }
}

std::size_t OracleCount(FT_Library library, FT_Glyph_Format format) {
    std::size_t count = 0;
    FT_ListNode node = nullptr;
    while (FT_Lookup_Renderer(library, format, &node)) ++count;
    Require(!node);
    return count;
}

void LookupRenderer() {
    void* sentinel = &sentinel;
    void* node = sentinel;
    Require(!FT_Lookup_Renderer_nid_postfix(nullptr, FT_GLYPH_FORMAT_OUTLINE, &node) && node == sentinel);
    Require(!FT_Lookup_Renderer_nid_postfix(nullptr, FT_GLYPH_FORMAT_OUTLINE, nullptr));
    GuestLibrary* library = nullptr;
    FT_Library oracle = nullptr;
    Require(FT_Init_FreeType_nid_postfix(&library) == 0 && FT_Init_FreeType(&oracle) == 0);
    for (const FT_Glyph_Format format : {FT_GLYPH_FORMAT_OUTLINE, FT_GLYPH_FORMAT_BITMAP, FT_GLYPH_FORMAT_COMPOSITE, FT_GLYPH_FORMAT_SVG}) {
        const auto renderers = Renderers(library, static_cast<std::uint32_t>(format));
        Require(renderers.size() == OracleCount(oracle, format));
        const void* first = renderers.empty() ? nullptr : renderers.front();
        Require(FT_Lookup_Renderer_nid_postfix(library, static_cast<std::uint32_t>(format), nullptr) == first);
        Require(FT_Get_Renderer_nid_postfix(library, static_cast<std::uint32_t>(format)) == first);
        for (std::size_t index = 0; index < renderers.size(); ++index) {
            for (std::size_t other = index + 1; other < renderers.size(); ++other) Require(renderers[index] != renderers[other]);
        }
    }
    Require(OracleCount(oracle, FT_GLYPH_FORMAT_OUTLINE) > 1);
    const auto outline = Renderers(library, FT_GLYPH_FORMAT_OUTLINE);
    Require(FT_Set_Renderer_nid_postfix(library, outline.back(), 0, nullptr) == 0);
    Require(FT_Lookup_Renderer_nid_postfix(library, FT_GLYPH_FORMAT_OUTLINE, nullptr) == outline.back());
    node = sentinel;
    Require(Throws([&] { FT_Lookup_Renderer_nid_postfix(library, FT_GLYPH_FORMAT_OUTLINE, &node); }));
    Require(FT_Done_FreeType_nid_postfix(library) == 0 && FT_Done_FreeType(oracle) == 0);
}

struct AttachPair {
    std::vector<unsigned char> bytes;
    GuestLibrary* library = nullptr;
    GuestFaceRec* face = nullptr;
    FT_Library native = nullptr;
    FT_Face oracle = nullptr;

    explicit AttachPair(std::vector<unsigned char> font) : bytes(std::move(font)) {
        Require(FT_Init_FreeType_nid_postfix(&library) == 0 && FT_Init_FreeType(&native) == 0);
        Require(FT_New_Memory_Face_nid_postfix(library, bytes.data(), static_cast<std::int64_t>(bytes.size()), 0, &face) == 0);
        Require(FT_New_Memory_Face(native, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &oracle) == 0);
    }

    ~AttachPair() {
        Require(FT_Done_Face_nid_postfix(face) == 0 && FT_Done_Face(oracle) == 0);
        Require(FT_Done_FreeType_nid_postfix(library) == 0 && FT_Done_FreeType(native) == 0);
    }

    void Matches() const {
        Require(face->face_flags == oracle->face_flags && face->style_flags == oracle->style_flags);
        Require(face->bbox.xMin == oracle->bbox.xMin && face->bbox.yMin == oracle->bbox.yMin);
        Require(face->bbox.xMax == oracle->bbox.xMax && face->bbox.yMax == oracle->bbox.yMax);
        Require(face->ascender == oracle->ascender && face->descender == oracle->descender && face->height == oracle->height);
        const std::uint32_t glyph = FT_Get_Char_Index(oracle, 'A');
        GuestVector actual{7, 7};
        FT_Vector reference{7, 7};
        Require(FT_Get_Kerning_nid_postfix(face, glyph, glyph, FT_KERNING_UNSCALED, &actual) ==
                FT_Get_Kerning(oracle, glyph, glyph, FT_KERNING_UNSCALED, &reference));
        Require(actual.x == reference.x && actual.y == reference.y);
    }
};

int closes = 0;
int nativeCloses = 0;

std::uint64_t APS5_VABI GuestStreamRead(GuestStreamRec* stream, std::uint64_t offset, std::uint8_t* buffer, std::uint64_t count) {
    if (offset > stream->size) return 0;
    const std::uint64_t bytes = std::min(count, stream->size - offset);
    if (bytes) std::memcpy(buffer, static_cast<const std::uint8_t*>(stream->descriptor) + offset, bytes);
    return bytes;
}

void APS5_VABI GuestStreamClose(GuestStreamRec*) {
    ++closes;
}

unsigned long NativeStreamRead(FT_Stream stream, unsigned long offset, unsigned char* buffer, unsigned long count) {
    if (offset > stream->size) return 0;
    const unsigned long bytes = std::min(count, stream->size - offset);
    if (bytes) std::memcpy(buffer, static_cast<const unsigned char*>(stream->descriptor.pointer) + offset, bytes);
    return bytes;
}

void NativeStreamClose(FT_Stream) {
    ++nativeCloses;
}

void AttachStream(const std::filesystem::path& root) {
    const std::string afm = Type1Fixture::Afm();
    const std::string afmPath = (root / "app0" / "fixture.afm").string();
    const std::string emptyPath = (root / "app0" / "empty.afm").string();
    const std::string missingPath = (root / "app0" / "missing.afm").string();
    {
        AttachPair pair(Type1Fixture::Font());
        pair.Matches();
        Require(!(pair.face->face_flags & FT_FACE_FLAG_KERNING));
        GuestOpenArgs guest{};
        FT_Open_Args native{};
        guest.flags = native.flags = FT_OPEN_MEMORY;
        guest.memory_base = native.memory_base = reinterpret_cast<const unsigned char*>(afm.data());
        guest.memory_size = native.memory_size = static_cast<FT_Long>(afm.size());
        Require(FT_Attach_Stream_nid_postfix(pair.face, &guest) == 0 && FT_Attach_Stream(pair.oracle, &native) == 0);
        Require(pair.face->face_flags & FT_FACE_FLAG_KERNING);
        pair.Matches();
        guest.flags = native.flags = FT_OPEN_MEMORY | FT_OPEN_PATHNAME;
        Require(FT_Attach_Stream_nid_postfix(pair.face, &guest) == FT_Attach_Stream(pair.oracle, &native));
        Require(FT_Attach_Stream_nid_postfix(pair.face, &guest) == FT_Err_Invalid_Argument);
        guest.flags = native.flags = 0;
        Require(FT_Attach_Stream_nid_postfix(pair.face, &guest) == FT_Attach_Stream(pair.oracle, &native));
        guest.flags = native.flags = FT_OPEN_STREAM;
        Require(FT_Attach_Stream_nid_postfix(pair.face, &guest) == FT_Attach_Stream(pair.oracle, &native));
        GuestStreamRec stream{};
        FT_StreamRec nativeStream{};
        guest.stream = &stream;
        native.stream = &nativeStream;
        guest.flags = native.flags = FT_OPEN_MEMORY | FT_OPEN_STREAM;
        Require(FT_Attach_Stream_nid_postfix(pair.face, &guest) == FT_Attach_Stream(pair.oracle, &native));
        Require(FT_Attach_Stream_nid_postfix(pair.face, &guest) == FT_Err_Invalid_Argument && closes == 0);
        guest = {};
        guest.flags = FT_OPEN_PATHNAME;
        Require(Throws([&] { FT_Attach_Stream_nid_postfix(pair.face, &guest); }));
        Require(FT_Attach_Stream_nid_postfix(pair.face, nullptr) == FT_Attach_Stream(pair.oracle, nullptr));
        pair.Matches();
    }
    {
        AttachPair pair(Type1Fixture::Font());
        GuestOpenArgs guest{};
        FT_Open_Args native{};
        guest.flags = native.flags = FT_OPEN_PATHNAME;
        guest.pathname = const_cast<char*>("/app0/fixture.afm");
        native.pathname = const_cast<char*>(afmPath.c_str());
        Require(FT_Attach_Stream_nid_postfix(pair.face, &guest) == 0 && FT_Attach_Stream(pair.oracle, &native) == 0);
        pair.Matches();
        guest.pathname = const_cast<char*>("/app0/missing.afm");
        native.pathname = const_cast<char*>(missingPath.c_str());
        Require(FT_Attach_Stream_nid_postfix(pair.face, &guest) == FT_Attach_Stream(pair.oracle, &native));
        Require(FT_Attach_Stream_nid_postfix(pair.face, &guest) == FT_Err_Cannot_Open_Resource);
        guest.pathname = const_cast<char*>("/app0/empty.afm");
        native.pathname = const_cast<char*>(emptyPath.c_str());
        Require(FT_Attach_Stream_nid_postfix(pair.face, &guest) == FT_Attach_Stream(pair.oracle, &native));
        Require(FT_Attach_Stream_nid_postfix(pair.face, &guest) == FT_Err_Cannot_Open_Stream);
        pair.Matches();
    }
    for (const bool reader : {false, true}) {
        AttachPair pair(Type1Fixture::Font());
        auto* data = reinterpret_cast<std::uint8_t*>(const_cast<char*>(afm.data()));
        GuestStreamRec stream{};
        stream.base = reader ? nullptr : data;
        stream.size = afm.size();
        stream.descriptor = data;
        stream.read = reader ? GuestStreamRead : nullptr;
        stream.close = GuestStreamClose;
        FT_StreamRec nativeStream{};
        nativeStream.base = reader ? nullptr : data;
        nativeStream.size = afm.size();
        nativeStream.descriptor.pointer = data;
        nativeStream.read = reader ? NativeStreamRead : nullptr;
        nativeStream.close = NativeStreamClose;
        GuestOpenArgs guest{};
        FT_Open_Args native{};
        guest.flags = native.flags = FT_OPEN_STREAM;
        guest.stream = &stream;
        native.stream = &nativeStream;
        closes = nativeCloses = 0;
        Require(FT_Attach_Stream_nid_postfix(pair.face, &guest) == 0 && FT_Attach_Stream(pair.oracle, &native) == 0);
        Require(closes == 1 && nativeCloses == 1 && stream.pos == nativeStream.pos);
        Require(pair.face->face_flags & FT_FACE_FLAG_KERNING);
        pair.Matches();
    }
    {
        AttachPair pair(FontFixture::BuildFont(3, {}));
        GuestOpenArgs guest{};
        FT_Open_Args native{};
        guest.flags = native.flags = FT_OPEN_MEMORY;
        guest.memory_base = native.memory_base = reinterpret_cast<const unsigned char*>(afm.data());
        guest.memory_size = native.memory_size = static_cast<FT_Long>(afm.size());
        Require(FT_Attach_Stream_nid_postfix(pair.face, &guest) == FT_Attach_Stream(pair.oracle, &native));
        Require(FT_Attach_Stream_nid_postfix(pair.face, &guest) == FT_Err_Unimplemented_Feature);
        Require(FT_Attach_Stream_nid_postfix(nullptr, &guest) == FT_Attach_Stream(nullptr, &native));
        GuestFaceRec empty{};
        Require(FT_Attach_Stream_nid_postfix(&empty, &guest) == FT_Err_Invalid_Face_Handle);
    }
}

}

int main() {
    NewLibrary();
    ReferenceLibrary();
    LookupRenderer();
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-freetype-library-object-" + std::to_string(std::random_device{}()));
    Require(std::filesystem::create_directory(root));
    std::filesystem::create_directory(root / "app0");
    const std::string afm = Type1Fixture::Afm();
    {
        std::ofstream file(root / "app0" / "fixture.afm", std::ios::binary);
        file.write(afm.data(), static_cast<std::streamsize>(afm.size()));
        Require(static_cast<bool>(file));
        std::ofstream empty(root / "app0" / "empty.afm", std::ios::binary);
        Require(static_cast<bool>(empty));
    }
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(root);
    AttachStream(root);
    std::filesystem::current_path(previous);
    std::filesystem::remove_all(root);
    std::puts("FreeType library object tests passed");
}
