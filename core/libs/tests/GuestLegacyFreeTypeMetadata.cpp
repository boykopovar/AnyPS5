#include "prx/libfreetype/libfreetype.hpp"
#include "prx/libSceFreeType/libSceFreeType.hpp"
#include "FontFixture.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_BDF_H
#include FT_SFNT_NAMES_H
#include FT_WINFONTS_H

#include <algorithm>
#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <source_location>
#include <string>

#ifdef _WIN32
#include "prx/libkernel/libkernel.hpp"
#include <windows.h>
#else
#include <dlfcn.h>
#endif

using namespace LegacyFreeType;

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Legacy FreeType metadata check failed at line %u\n", location.line());
        std::abort();
    }
}

static auto Load(const char* path) {
#ifdef _WIN32
    auto module = LoadLibraryW(std::filesystem::path(path).c_str());
    if (!module) std::fprintf(stderr, "LoadLibraryW failed: %lu\n", GetLastError());
#else
    auto module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!module) std::fprintf(stderr, "%s\n", dlerror());
#endif
    Require(module != nullptr);
    return module;
}

template <typename TFunction, typename TModule>
static TFunction Import(TModule module, const char* name) {
#ifdef _WIN32
    const auto address = GetProcAddress(module, name);
#else
    const auto address = dlsym(module, name);
#endif
    if (!address) std::fprintf(stderr, "Missing symbol: %s\n", name);
    Require(address != nullptr);
    return std::bit_cast<TFunction>(address);
}

struct Api {
    decltype(&FT_Init_FreeType_nid_postfix) initialize;
    decltype(&FT_Done_FreeType_nid_postfix) doneLibrary;
    decltype(&FT_New_Memory_Face_nid_postfix) newFace;
    decltype(&FT_Done_Face_nid_postfix) doneFace;
    decltype(&freetypeLegacyGetBDFProperty_nid_no_patch) property;
    decltype(&freetypeLegacyGetBDFCharsetID_nid_no_patch) charset;
    decltype(&freetypeLegacyGetSfntLangTag_nid_no_patch) tag;
    decltype(&freetypeLegacyGetWinFNTHeader_nid_no_patch) header;
};

static void CheckText(const char* actual, const char* expected) {
    Require((actual == nullptr) == (expected == nullptr));
    if (actual) Require(std::strcmp(actual, expected) == 0);
}

#ifdef _WIN32
struct KernelApi {
    decltype(&sceKernelAllocateDirectMemory) allocate;
    decltype(&sceKernelMapDirectMemory) map;
    decltype(&sceKernelMprotect) protect;
    decltype(&sceKernelMunmap) unmap;
    decltype(&sceKernelReleaseDirectMemory) release;
};

class GuestOutput {
public:
    static constexpr std::size_t Page = 0x4000;

    explicit GuestOutput(const KernelApi& api) : api(api) {
        Require(api.allocate(0, 0x7fffffffffll, Page * 2, 0, 0, &physical) == 0);
        Require(api.map(&mapping, Page * 2, 3, 0, physical, 0) == 0);
        std::memset(mapping, 0xA5, Page * 2);
    }

    ~GuestOutput() {
        Require(api.unmap(reinterpret_cast<std::uint64_t>(mapping), Page * 2) == 0);
        Require(api.release(physical, Page * 2) == 0);
    }

    template <typename TValue>
    TValue* At(unsigned page = 0) const {
        return reinterpret_cast<TValue*>(static_cast<unsigned char*>(mapping) + Page * page + 16);
    }

    void Protect(unsigned page, int protection) const {
        Require(api.protect(static_cast<unsigned char*>(mapping) + Page * page, Page, protection) == 0);
        if (protection == 1) {
            MEMORY_BASIC_INFORMATION information{};
            Require(VirtualQuery(At<unsigned char>(page), &information, sizeof(information)) == sizeof(information));
            Require(information.Protect == PAGE_READONLY);
        }
    }

    template <typename TCall>
    void Reject(TCall call, int error = FT_Err_Invalid_Argument) const {
        const auto* bytes = static_cast<const unsigned char*>(mapping);
        const std::vector<unsigned char> before(bytes, bytes + Page * 2);
        Require(call() == error);
        Require(std::memcmp(mapping, before.data(), before.size()) == 0);
    }

private:
    const KernelApi& api;
    std::int64_t physical = 0;
    void* mapping = nullptr;
};

static void CheckGuestOutputs(const Api& api, const KernelApi& kernel, FreeType::GuestFaceRec* face, unsigned format) {
    GuestOutput output(kernel);
    output.Protect(0, 1);
    output.Protect(1, 1);
    if (format == 0) {
        auto* property = output.At<BdfProperty>();
        output.Reject([&] { return api.property(face, "FONT_NAME", property); });
        output.Reject([&] { return api.property(face, "MISSING_PROPERTY", property); });
        output.Reject([&] { return api.property(nullptr, "FONT_NAME", property); }, FT_Err_Invalid_Face_Handle);
        output.Protect(0, 3);
        Require(api.property(face, "FONT_NAME", property) == 0 && property->type == BDF_PROPERTY_TYPE_ATOM);
        CheckText(property->value.atom, "metadata fixture");
        auto** encoding = output.At<const char*>();
        auto** registry = output.At<const char*>(1);
        output.Reject([&] { return api.charset(face, encoding, registry); });
        output.Reject([&] { return api.charset(face, nullptr, registry); });
        output.Protect(0, 1);
        output.Protect(1, 3);
        output.Reject([&] { return api.charset(face, encoding, registry); });
        output.Reject([&] { return api.charset(face, encoding, nullptr); });
        output.Protect(1, 1);
        output.Reject([&] { return api.charset(face, encoding, registry); });
        output.Reject([&] { return api.charset(nullptr, encoding, registry); }, FT_Err_Invalid_Face_Handle);
        output.Protect(0, 3);
        output.Protect(1, 3);
        Require(api.charset(face, encoding, registry) == 0);
        CheckText(*encoding, "1");
        CheckText(*registry, "ISO10646");
    } else if (format == 1) {
        auto* tag = output.At<SfntLangTag>();
        output.Reject([&] { return api.tag(face, 0x8001, tag); });
        output.Reject([&] { return api.tag(face, 0xFFFFFFFFu, tag); });
        output.Reject([&] { return api.tag(nullptr, 0x8001, tag); });
        output.Protect(0, 3);
        Require(api.tag(face, 0x8001, tag) == 0 && tag->string_len == 4);
    } else {
        auto* header = output.At<WinFntHeader>();
        output.Reject([&] { return api.header(face, header); });
        output.Reject([&] { return api.header(nullptr, header); }, FT_Err_Invalid_Face_Handle);
        output.Protect(0, 3);
        Require(api.header(face, header) == 0 && header->version == 0x300 && header->file_size == 196);
    }
}
#endif

template <typename TValue>
struct Guarded {
    std::array<unsigned char, 8> before;
    TValue value;
    std::array<unsigned char, 8> after;

    Guarded() { std::memset(this, 0xA5, sizeof(*this)); }
    void Check() const {
        for (auto byte : before) Require(byte == 0xA5);
        for (auto byte : after) Require(byte == 0xA5);
    }
};


static std::vector<unsigned char> BdfFont() {
    auto bytes = FontFixture::BitmapFont();
    std::string text(bytes.begin(), bytes.end());
    text.replace(text.find("STARTPROPERTIES 4"), 17, "STARTPROPERTIES 7");
    text.insert(text.find("ENDPROPERTIES"), "FONT_NAME \"metadata fixture\"\nUNDERLINE_POSITION -17\nDEFAULT_CHAR 4294967295\n");
    return {text.begin(), text.end()};
}

static std::vector<unsigned char> TaggedFont(int format) {
    auto name = FontFixture::Words({format, 1, format == 1 ? 28 : 18, 3, 1, 0x8001, 1, 2, 0});
    if (format == 1) {
        for (int word : {2, 4, 2, 4, 6}) FontFixture::Put16(name, word);
    }
    for (unsigned char byte : {0, 65, 0, 101, 0, 110, 0, 106, 0, 97}) name.push_back(byte);
    return FontFixture::BuildFont(2, {{"name", name}});
}

static std::vector<unsigned char> FntFont(std::uint16_t version) {
    std::vector<unsigned char> bytes(196);
    auto put = [&](std::size_t offset, std::uint32_t value, unsigned count) {
        for (unsigned index = 0; index < count; ++index) bytes[offset + index] = static_cast<unsigned char>(value >> (8 * index));
    };
    put(0, version, 2);
    put(2, static_cast<std::uint32_t>(bytes.size()), 4);
    std::memcpy(bytes.data() + 6, "AnyPS5 generated metadata fixture", 32);
    for (auto [offset, value] : std::array<std::pair<unsigned, unsigned>, 15>{{{68, 12}, {70, 96}, {72, 120}, {74, 7}, {76, 1}, {78, 2}, {83, 700}, {86, 8}, {88, 8}, {91, 8}, {93, 9}, {99, 2}, {122, 3}, {124, 4}, {126, 5}}}) put(offset, value, 2);
    bytes[80] = 1;
    bytes[81] = 1;
    bytes[85] = 204;
    bytes[90] = 0x31;
    bytes[95] = 65;
    bytes[96] = 66;
    put(105, 180, 4);
    put(109, 0xFEDCBA98, 4);
    put(113, 166, 4);
    if (version == 0x300) {
        put(118, 0x10, 4);
        put(128, 0x1234, 4);
    }
    std::memcpy(bytes.data() + 180, "Fixture FNT", 12);
    return bytes;
}

static std::int32_t NativePropertyType(const BDF_PropertyRec& property) {
    static_assert(sizeof(property.type) == sizeof(std::int32_t));
    std::int32_t type;
    std::memcpy(&type, &property.type, sizeof type);
    return type;
}

static void CheckProperty(const Api& api, FreeType::GuestFaceRec* face, FT_Face native, const char* name) {
    Guarded<BdfProperty> actual;
    BDF_PropertyRec expected;
    std::memset(&expected, 0xA5, sizeof(expected));
    const int error = FT_Get_BDF_Property(native, name, &expected);
    Require(api.property(face, name, &actual.value) == error);
    actual.Check();
    Require(actual.value.type == NativePropertyType(expected));
    if (!error) {
        switch (expected.type) {
            case BDF_PROPERTY_TYPE_ATOM: CheckText(actual.value.value.atom, expected.u.atom); break;
            case BDF_PROPERTY_TYPE_INTEGER: Require(actual.value.value.integer == expected.u.integer); break;
            case BDF_PROPERTY_TYPE_CARDINAL: Require(actual.value.value.cardinal == expected.u.cardinal); break;
            default: Require(false);
        }
    } else {
        const auto* bytes = reinterpret_cast<const unsigned char*>(&actual.value.value);
        Require(std::all_of(bytes, bytes + sizeof(actual.value.value), [](auto byte) { return byte == 0xA5; }));
    }
    Require(api.property(face, name, nullptr) == FT_Get_BDF_Property(native, name, nullptr));
}

static void CheckCharset(const Api& api, FreeType::GuestFaceRec* face, FT_Face native) {
    for (unsigned mask = 0; mask < 4; ++mask) {
        const char* encoding = "untouched encoding";
        const char* registry = "untouched registry";
        const char* expectedEncoding = encoding;
        const char* expectedRegistry = registry;
        const int error = FT_Get_BDF_Charset_ID(native, mask & 1 ? &expectedEncoding : nullptr, mask & 2 ? &expectedRegistry : nullptr);
        Require(api.charset(face, mask & 1 ? &encoding : nullptr, mask & 2 ? &registry : nullptr) == error);
        CheckText(encoding, expectedEncoding);
        CheckText(registry, expectedRegistry);
    }
}

static void CheckTag(const Api& api, FreeType::GuestFaceRec* face, FT_Face native, std::uint32_t language) {
    Guarded<SfntLangTag> actual;
    const auto before = actual;
    FT_SfntLangTag expected{};
    const int error = FT_Get_Sfnt_LangTag(native, language, &expected);
    Require(api.tag(face, language, &actual.value) == error);
    actual.Check();
    if (error) Require(std::memcmp(&actual, &before, sizeof(actual)) == 0);
    else {
        Require(actual.value.string_len == expected.string_len);
        if (expected.string_len) Require(std::memcmp(actual.value.string, expected.string, expected.string_len) == 0);
        SfntLangTag again{};
        Require(api.tag(face, language, &again) == 0);
        Require(again.string == actual.value.string && again.string_len == actual.value.string_len);
    }
    Require(api.tag(face, language, nullptr) == FT_Get_Sfnt_LangTag(native, language, nullptr));
}

static void CheckHeader(const Api& api, FreeType::GuestFaceRec* face, FT_Face native) {
    Guarded<WinFntHeader> actual;
    const auto before = actual;
    FT_WinFNT_HeaderRec expected{};
    const int error = FT_Get_WinFNT_Header(native, &expected);
    Require(api.header(face, &actual.value) == error);
    actual.Check();
    if (error) Require(std::memcmp(&actual, &before, sizeof(actual)) == 0);
    else {
        const auto& header = actual.value;
        Require(header.version == expected.version && header.file_size == expected.file_size);
        Require(std::memcmp(header.copyright, expected.copyright, 60) == 0);
        Require(header.file_type == expected.file_type && header.nominal_point_size == expected.nominal_point_size);
        Require(header.vertical_resolution == expected.vertical_resolution && header.horizontal_resolution == expected.horizontal_resolution);
        Require(header.ascent == expected.ascent && header.internal_leading == expected.internal_leading && header.external_leading == expected.external_leading);
        Require(header.italic == expected.italic && header.underline == expected.underline && header.strike_out == expected.strike_out);
        Require(header.weight == expected.weight && header.charset == expected.charset);
        Require(header.pixel_width == expected.pixel_width && header.pixel_height == expected.pixel_height && header.pitch_and_family == expected.pitch_and_family);
        Require(header.avg_width == expected.avg_width && header.max_width == expected.max_width);
        Require(header.first_char == expected.first_char && header.last_char == expected.last_char && header.default_char == expected.default_char && header.break_char == expected.break_char);
        Require(header.bytes_per_row == expected.bytes_per_row && header.device_offset == expected.device_offset && header.face_name_offset == expected.face_name_offset);
        Require(header.bits_pointer == expected.bits_pointer && header.bits_offset == expected.bits_offset && header.reserved == expected.reserved);
        Require(header.flags == expected.flags && header.A_space == expected.A_space && header.B_space == expected.B_space && header.C_space == expected.C_space);
        Require(header.color_table_offset == expected.color_table_offset);
        for (unsigned index = 0; index < 4; ++index) Require(header.reserved1[index] == expected.reserved1[index]);
        Require(header.bits_pointer == 0xFEDCBA98 && header.file_size == 196 && header.charset == 204);
    }
    Require(api.header(face, nullptr) == FT_Get_WinFNT_Header(native, nullptr));
}

int main(int argc, char** argv) {
    Require(argc == 4);
    const bool patched = std::strcmp(argv[3], "patched") == 0;
    Require(patched || std::strcmp(argv[3], "unpatched") == 0);
    auto module = Load(argv[1]);
    auto base = Load(argv[2]);
#ifdef _WIN32
    auto kernel = Load((std::filesystem::path(argv[2]).parent_path() / "libkernel.prx").string().c_str());
    const KernelApi kernelApi{
        Import<decltype(KernelApi::allocate)>(kernel, patched ? "rTXw65xmLIA" : "sceKernelAllocateDirectMemory"),
        Import<decltype(KernelApi::map)>(kernel, patched ? "L-Q3LEjIbgA" : "sceKernelMapDirectMemory"),
        Import<decltype(KernelApi::protect)>(kernel, patched ? "vSMAm3cxYTY" : "sceKernelMprotect"),
        Import<decltype(KernelApi::unmap)>(kernel, patched ? "cQke9UuBQOk" : "sceKernelMunmap"),
        Import<decltype(KernelApi::release)>(kernel, patched ? "MBuItvba6z8" : "sceKernelReleaseDirectMemory")
    };
#endif
    Api api{};
    api.initialize = Import<decltype(api.initialize)>(base, patched ? "GNsiLzm4SH8" : "FT_Init_FreeType_nid_postfix");
    api.doneLibrary = Import<decltype(api.doneLibrary)>(base, patched ? "dH02CpEEcYk" : "FT_Done_FreeType_nid_postfix");
    api.newFace = Import<decltype(api.newFace)>(base, patched ? "40Wa2neSLM0" : "FT_New_Memory_Face_nid_postfix");
    api.doneFace = Import<decltype(api.doneFace)>(base, patched ? "OgFczHko0w8" : "FT_Done_Face_nid_postfix");
    api.property = Import<decltype(api.property)>(module, patched ? "4kVFLShKGkQ" : "4kVFLShKGkQ_nid_no_patch_cut");
    api.charset = Import<decltype(api.charset)>(module, patched ? "KVRW5HPnbgU" : "KVRW5HPnbgU_nid_no_patch_cut");
    api.tag = Import<decltype(api.tag)>(module, patched ? "MDsMjUGulyY" : "MDsMjUGulyY_nid_no_patch_cut");
    api.header = Import<decltype(api.header)>(module, patched ? "suWwpZzQl00" : "suWwpZzQl00_nid_no_patch_cut");
    CheckProperty(api, nullptr, nullptr, "FONT_ASCENT");
    CheckCharset(api, nullptr, nullptr);
    CheckTag(api, nullptr, nullptr, 0x8001);
    CheckHeader(api, nullptr, nullptr);
    FreeType::GuestLibrary* library = nullptr;
    FT_Library nativeLibrary = nullptr;
    Require(api.initialize(&library) == 0 && FT_Init_FreeType(&nativeLibrary) == 0);
    const std::array fonts = {BdfFont(), TaggedFont(1), TaggedFont(0), FontFixture::BuildFont(2, {}), FntFont(0x200), FntFont(0x300)};
    for (const auto& bytes : fonts) {
        FreeType::GuestFaceRec* face = nullptr;
        FT_Face native = nullptr;
        Require(api.newFace(library, bytes.data(), static_cast<std::int64_t>(bytes.size()), 0, &face) == 0);
        Require(FT_New_Memory_Face(nativeLibrary, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &native) == 0);
        for (const char* property : {"FONT_NAME", "UNDERLINE_POSITION", "DEFAULT_CHAR", "FONT_ASCENT", "CHARSET_REGISTRY", "MISSING_PROPERTY", "", static_cast<const char*>(nullptr)}) CheckProperty(api, face, native, property);
        if (&bytes == &fonts[0]) {
            BdfProperty property{};
            Require(api.property(face, "FONT_NAME", &property) == 0 && property.type == BDF_PROPERTY_TYPE_ATOM);
            CheckText(property.value.atom, "metadata fixture");
            const char* retained = property.value.atom;
            Require(api.property(face, "UNDERLINE_POSITION", &property) == 0 && property.type == BDF_PROPERTY_TYPE_INTEGER && property.value.integer == -17);
            Require(api.property(face, "DEFAULT_CHAR", &property) == 0 && property.type == BDF_PROPERTY_TYPE_CARDINAL && property.value.cardinal == 0xFFFFFFFFu);
            CheckText(retained, "metadata fixture");
        }
        if (&bytes == &fonts[1]) {
            SfntLangTag tag{};
            Require(api.tag(face, 0x8001, &tag) == 0 && tag.string_len == 4);
            const std::array<unsigned char, 4> japanese = {0, 'j', 0, 'a'};
            Require(std::memcmp(tag.string, japanese.data(), japanese.size()) == 0);
        }
        CheckCharset(api, face, native);
        for (std::uint32_t language : {0u, 0x7FFFu, 0x8000u, 0x8001u, 0x8002u, 0xFFFFFFFFu}) CheckTag(api, face, native, language);
        CheckHeader(api, face, native);
#ifdef _WIN32
        if (&bytes == &fonts[0] || &bytes == &fonts[1] || &bytes == &fonts[5])
            CheckGuestOutputs(api, kernelApi, face, static_cast<unsigned>(&bytes - fonts.data()));
#endif
        Require(api.doneFace(face) == 0 && FT_Done_Face(native) == 0);
    }
    Require(api.doneLibrary(library) == 0 && FT_Done_FreeType(nativeLibrary) == 0);
#ifdef _WIN32
    Require(FreeLibrary(base) != 0 && FreeLibrary(module) != 0 && FreeLibrary(kernel) != 0);
#else
    Require(dlclose(base) == 0 && dlclose(module) == 0);
#endif
}
