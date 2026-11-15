#include "prx/libSceFreeTypeOl/libSceFreeTypeOl.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "FreeTypeFontFixture.hpp"
#include "Type1Fixture.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

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
#include <utility>
#include <vector>

using namespace FreeType;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeTypeOl attach stream check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

struct StreamState {
    const std::string* data = nullptr;
    std::uint64_t shortBy = 0;
    int reads = 0;
    int closes = 0;
};

static std::uint64_t ReadBytes(StreamState& state, std::uint64_t size, std::uint64_t offset, unsigned char* output, std::uint64_t count) {
    ++state.reads;
    if (count == 0) return offset > size ? 1 : 0;
    if (offset >= size) return 0;
    std::uint64_t length = std::min(count, size - offset);
    length -= std::min(length, state.shortBy);
    std::memcpy(output, state.data->data() + offset, static_cast<std::size_t>(length));
    return length;
}

static std::uint64_t APS5_VABI GuestRead(GuestStreamRec* stream, std::uint64_t offset, std::uint8_t* output, std::uint64_t count) {
    return ReadBytes(*static_cast<StreamState*>(stream->descriptor), stream->size, offset, output, count);
}

static void APS5_VABI GuestClose(GuestStreamRec* stream) {
    ++static_cast<StreamState*>(stream->descriptor)->closes;
}

static unsigned long NativeRead(FT_Stream stream, unsigned long offset, unsigned char* output, unsigned long count) {
    return static_cast<unsigned long>(ReadBytes(*static_cast<StreamState*>(stream->descriptor.pointer), stream->size, offset, output, count));
}

static void NativeClose(FT_Stream stream) {
    ++static_cast<StreamState*>(stream->descriptor.pointer)->closes;
}

template <typename TCall>
static bool Throws(TCall call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

struct FacePair {
    std::vector<unsigned char> bytes;
    GuestLibrary* library = nullptr;
    FT_Library native = nullptr;
    GuestFaceRec* guest = nullptr;
    FT_Face oracle = nullptr;

    explicit FacePair(std::vector<unsigned char> font) : bytes(std::move(font)) {
        Require(FT_Init_FreeType_nid_postfix(&library) == 0 && FT_Init_FreeType(&native) == 0);
        Require(FT_New_Memory_Face_nid_postfix(library, bytes.data(), static_cast<std::int64_t>(bytes.size()), 0, &guest) == 0);
        Require(FT_New_Memory_Face(native, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &oracle) == 0);
        Matches();
    }

    ~FacePair() {
        Require(FT_Done_Face_nid_postfix(guest) == 0 && FT_Done_Face(oracle) == 0);
        Require(FT_Done_FreeType_nid_postfix(library) == 0 && FT_Done_FreeType(native) == 0);
    }

    FT_Face Host() const {
        return static_cast<GuestFace*>(guest->internal)->host;
    }

    void Matches() const {
        Require(guest->face_flags == oracle->face_flags);
        Require(guest->bbox.xMin == oracle->bbox.xMin && guest->bbox.yMin == oracle->bbox.yMin);
        Require(guest->bbox.xMax == oracle->bbox.xMax && guest->bbox.yMax == oracle->bbox.yMax);
        Require(guest->ascender == oracle->ascender && guest->descender == oracle->descender);
    }

    FT_Pos Kerning() const {
        const FT_UInt glyph = FT_Get_Char_Index(oracle, 'A');
        Require(glyph != 0 && glyph == FT_Get_Char_Index(Host(), 'A'));
        FT_Vector actual{7, 7};
        FT_Vector reference{7, 7};
        Require(FT_Get_Kerning(Host(), glyph, glyph, FT_KERNING_UNSCALED, &actual) == FT_Get_Kerning(oracle, glyph, glyph, FT_KERNING_UNSCALED, &reference));
        Require(actual.x == reference.x && actual.y == reference.y);
        return reference.x;
    }
};

static std::string ReplaceOnce(std::string text, const std::string& from, const std::string& to) {
    const auto position = text.find(from);
    Require(position != std::string::npos);
    return text.replace(position, from.size(), to);
}

static void CheckStream(const std::vector<unsigned char>& font, const std::string& data, std::uint64_t shortBy, bool callbacks, bool type1, FT_Pos kerning) {
    FacePair pair(font);
    StreamState guestState{&data, shortBy};
    StreamState nativeState{&data, shortBy};
    GuestStreamRec guest{};
    guest.size = data.size();
    guest.descriptor = &guestState;
    guest.close = GuestClose;
    FT_StreamRec nativeStream{};
    nativeStream.size = static_cast<unsigned long>(data.size());
    nativeStream.descriptor.pointer = &nativeState;
    nativeStream.close = NativeClose;
    if (callbacks) {
        guest.read = GuestRead;
        nativeStream.read = NativeRead;
    } else {
        guest.base = reinterpret_cast<std::uint8_t*>(const_cast<char*>(data.data()));
        nativeStream.base = reinterpret_cast<unsigned char*>(const_cast<char*>(data.data()));
    }
    GuestOpenArgs args{};
    args.flags = FT_OPEN_STREAM;
    args.stream = &guest;
    FT_Open_Args nativeArgs{};
    nativeArgs.flags = FT_OPEN_STREAM;
    nativeArgs.stream = &nativeStream;
    Require(sceFreeTypeOlAttachStream_nid_postfix(pair.guest, &args) == FT_Attach_Stream(pair.oracle, &nativeArgs));
    Require(guestState.closes == 1 && nativeState.closes == 1);
    Require(guestState.reads == nativeState.reads);
    Require(guest.pos == nativeStream.pos && guest.cursor == nullptr && guest.limit == nullptr);
    pair.Matches();
    if (type1) Require(pair.Kerning() == kerning);
}

int main() {
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-freetype-ol-attach-stream-" + std::to_string(std::random_device{}()));
    Require(std::filesystem::create_directory(root));
    std::filesystem::create_directory(root / "app0");
    std::filesystem::create_directory(root / "app0" / "folder");
    const std::string afm = ReplaceOnce(ReplaceOnce(Type1Fixture::Afm(), "FontBBox 0 0 600 700", "FontBBox -10 -20 610 720"), "Ascender 700\nDescender 0", "Ascender 690\nDescender -15");
    {
        std::ofstream file(root / "app0" / "fixture.afm", std::ios::binary);
        file.write(afm.data(), static_cast<std::streamsize>(afm.size()));
        Require(static_cast<bool>(file));
        std::ofstream empty(root / "app0" / "empty.afm", std::ios::binary);
        Require(static_cast<bool>(empty));
    }
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(root);
    const auto nativePath = [&](const char* name) { return (root / "app0" / name).string(); };
    const std::vector<unsigned char> type1 = Type1Fixture::Font();
    const std::vector<unsigned char> truetype = SquareGlyphFont('A', 700, true);

    {
        FacePair pair(type1);
        Require(pair.Kerning() == 0 && !(pair.guest->face_flags & FT_FACE_FLAG_KERNING));
        GuestOpenArgs args{};
        args.flags = FT_OPEN_MEMORY;
        args.memory_base = reinterpret_cast<const unsigned char*>(afm.data());
        args.memory_size = static_cast<std::int64_t>(afm.size());
        FT_Open_Args nativeArgs{};
        nativeArgs.flags = FT_OPEN_MEMORY;
        nativeArgs.memory_base = reinterpret_cast<const FT_Byte*>(afm.data());
        nativeArgs.memory_size = static_cast<FT_Long>(afm.size());
        Require(sceFreeTypeOlAttachStream_nid_postfix(pair.guest, &args) == 0);
        Require(FT_Attach_Stream(pair.oracle, &nativeArgs) == 0);
        pair.Matches();
        Require(pair.guest->face_flags & FT_FACE_FLAG_KERNING);
        Require(pair.guest->bbox.xMin == -10 && pair.guest->bbox.yMin == -20 && pair.guest->bbox.xMax == 610 && pair.guest->bbox.yMax == 720);
        Require(pair.guest->ascender == 690 && pair.guest->descender == -15);
        Require(pair.Kerning() == -50);
    }
    CheckStream(type1, afm, 0, true, true, -50);
    CheckStream(type1, afm, 1, true, true, 0);
    CheckStream(type1, afm, 0, false, true, -50);
    CheckStream(truetype, afm, 0, true, false, 0);
    {
        FacePair pair(type1);
        GuestOpenArgs args{};
        args.flags = FT_OPEN_PATHNAME;
        args.pathname = const_cast<char*>("/app0/fixture.afm");
        const std::string path = nativePath("fixture.afm");
        FT_Open_Args nativeArgs{};
        nativeArgs.flags = FT_OPEN_PATHNAME;
        nativeArgs.pathname = const_cast<char*>(path.c_str());
        Require(sceFreeTypeOlAttachStream_nid_postfix(pair.guest, &args) == 0);
        Require(FT_Attach_Stream(pair.oracle, &nativeArgs) == 0);
        pair.Matches();
        Require(pair.Kerning() == -50);
    }
    {
        FacePair pair(type1);
        GuestOpenArgs args{};
        args.flags = FT_OPEN_PATHNAME;
        FT_Open_Args nativeArgs{};
        nativeArgs.flags = FT_OPEN_PATHNAME;
        for (const char* name : {"missing.afm", "empty.afm"}) {
            const std::string guestPath = std::string("/app0/") + name;
            const std::string path = nativePath(name);
            args.pathname = const_cast<char*>(guestPath.c_str());
            nativeArgs.pathname = const_cast<char*>(path.c_str());
            const int error = sceFreeTypeOlAttachStream_nid_postfix(pair.guest, &args);
            Require(error == FT_Attach_Stream(pair.oracle, &nativeArgs));
            Require(error == (std::strcmp(name, "empty.afm") == 0 ? FT_Err_Cannot_Open_Stream : FT_Err_Cannot_Open_Resource));
        }
        pair.Matches();
        Require(pair.Kerning() == 0);
        args.pathname = const_cast<char*>("/app0/folder");
        Require(Throws([&] { sceFreeTypeOlAttachStream_nid_postfix(pair.guest, &args); }));
        args.pathname = nullptr;
        Require(Throws([&] { sceFreeTypeOlAttachStream_nid_postfix(pair.guest, &args); }));
    }
    {
        FacePair pair(truetype);
        GuestOpenArgs args{};
        args.flags = FT_OPEN_MEMORY;
        args.memory_base = reinterpret_cast<const unsigned char*>(afm.data());
        args.memory_size = static_cast<std::int64_t>(afm.size());
        FT_Open_Args nativeArgs{};
        nativeArgs.flags = FT_OPEN_MEMORY;
        nativeArgs.memory_base = reinterpret_cast<const FT_Byte*>(afm.data());
        nativeArgs.memory_size = static_cast<FT_Long>(afm.size());
        Require(sceFreeTypeOlAttachStream_nid_postfix(pair.guest, &args) == FT_Attach_Stream(pair.oracle, &nativeArgs));
        Require(FT_Attach_Stream(pair.oracle, &nativeArgs) == FT_Err_Unimplemented_Feature);
        pair.Matches();
    }
    {
        FacePair pair(type1);
        GuestOpenArgs args{};
        args.flags = FT_OPEN_MEMORY;
        args.memory_base = reinterpret_cast<const unsigned char*>(afm.data());
        args.memory_size = static_cast<std::int64_t>(afm.size());
        FT_Open_Args nativeArgs{};
        nativeArgs.flags = FT_OPEN_MEMORY;
        nativeArgs.memory_base = reinterpret_cast<const FT_Byte*>(afm.data());
        nativeArgs.memory_size = static_cast<FT_Long>(afm.size());
        Require(sceFreeTypeOlAttachStream_nid_postfix(nullptr, &args) == FT_Attach_Stream(nullptr, &nativeArgs));
        Require(FT_Attach_Stream(nullptr, &nativeArgs) == FT_Err_Invalid_Face_Handle);
        GuestFaceRec unregistered{};
        Require(sceFreeTypeOlAttachStream_nid_postfix(&unregistered, &args) == FT_Err_Invalid_Face_Handle);
        Require(sceFreeTypeOlAttachStream_nid_postfix(nullptr, nullptr) == FT_Err_Invalid_Face_Handle);
        Require(sceFreeTypeOlAttachStream_nid_postfix(pair.guest, nullptr) == FT_Attach_Stream(pair.oracle, nullptr));
        Require(FT_Attach_Stream(pair.oracle, nullptr) == FT_Err_Invalid_Argument);
        GuestOpenArgs none{};
        FT_Open_Args nativeNone{};
        Require(sceFreeTypeOlAttachStream_nid_postfix(pair.guest, &none) == FT_Attach_Stream(pair.oracle, &nativeNone));
        none.flags = FT_OPEN_STREAM;
        nativeNone.flags = FT_OPEN_STREAM;
        Require(sceFreeTypeOlAttachStream_nid_postfix(pair.guest, &none) == FT_Attach_Stream(pair.oracle, &nativeNone));
        Require(FT_Attach_Stream(pair.oracle, &nativeNone) == FT_Err_Invalid_Argument);
        none.flags = FT_OPEN_DRIVER | FT_OPEN_PARAMS;
        nativeNone.flags = FT_OPEN_DRIVER | FT_OPEN_PARAMS;
        Require(sceFreeTypeOlAttachStream_nid_postfix(pair.guest, &none) == FT_Attach_Stream(pair.oracle, &nativeNone));
        args.flags = FT_OPEN_MEMORY | FT_OPEN_DRIVER | FT_OPEN_PARAMS;
        nativeArgs.flags = FT_OPEN_MEMORY | FT_OPEN_DRIVER | FT_OPEN_PARAMS;
        Require(sceFreeTypeOlAttachStream_nid_postfix(pair.guest, &args) == 0);
        Require(FT_Attach_Stream(pair.oracle, &nativeArgs) == 0);
        pair.Matches();
        Require(pair.Kerning() == -50);
        StreamState state{&afm};
        GuestStreamRec stream{};
        stream.size = afm.size();
        stream.descriptor = &state;
        stream.read = GuestRead;
        stream.close = GuestClose;
        args.flags = FT_OPEN_MEMORY | FT_OPEN_STREAM;
        args.stream = &stream;
        Require(Throws([&] { sceFreeTypeOlAttachStream_nid_postfix(pair.guest, &args); }));
        args.flags = FT_OPEN_MEMORY | FT_OPEN_PATHNAME;
        Require(Throws([&] { sceFreeTypeOlAttachStream_nid_postfix(pair.guest, &args); }));
        Require(state.reads == 0 && state.closes == 0);
    }
    std::filesystem::current_path(previous);
    std::filesystem::remove_all(root);
}
