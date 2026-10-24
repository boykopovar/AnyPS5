#include "FontFixture.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_SFNT_NAMES_H

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

using namespace FreeType;

extern "C" {
std::uint32_t APS5_VABI FT_Get_Sfnt_Name_Count_nid_postfix(GuestFaceRec*);
int APS5_VABI FT_Get_Sfnt_Name_nid_postfix(GuestFaceRec*, std::uint32_t, GuestSfntName*);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType SFNT names check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

struct NameRecord {
    std::uint16_t platform;
    std::uint16_t encoding;
    std::uint16_t language;
    std::uint16_t name;
    std::vector<unsigned char> bytes;
};

static std::vector<unsigned char> NamedFont(const std::vector<NameRecord>& records) {
    auto table = FontFixture::Words({0, static_cast<int>(records.size()), 6 + 12 * static_cast<int>(records.size())});
    int offset = 0;
    for (const auto& record : records) {
        for (const int value : {static_cast<int>(record.platform), static_cast<int>(record.encoding), static_cast<int>(record.language),
                               static_cast<int>(record.name), static_cast<int>(record.bytes.size()), offset}) FontFixture::Put16(table, value);
        offset += static_cast<int>(record.bytes.size());
    }
    for (const auto& record : records) table.insert(table.end(), record.bytes.begin(), record.bytes.end());
    return FontFixture::BuildFont(3, {{"name", table}});
}

struct FacePair {
    std::vector<unsigned char> bytes;
    GuestFace guest;
    FT_Face oracle = nullptr;

    FacePair(GuestLibrary& library, FT_Library nativeLibrary, std::vector<unsigned char> font) : bytes(std::move(font)) {
        guest.library = &library;
        Require(FT_New_Memory_Face(library.host, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &guest.host) == 0);
        Require(FT_New_Memory_Face(nativeLibrary, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &oracle) == 0);
        SyncFace(guest);
    }

    ~FacePair() {
        Require(FT_Done_Face(guest.host) == 0);
        Require(FT_Done_Face(oracle) == 0);
    }
};

static GuestSfntName Query(FacePair& face, std::uint32_t index, const NameRecord& record) {
    struct GuardedName {
        std::array<unsigned char, 8> before;
        GuestSfntName value;
        std::array<unsigned char, 8> after;
    } actual;
    std::memset(&actual, 0xA5, sizeof(actual));
    FT_SfntName expected{};
    Require(FT_Get_Sfnt_Name(face.oracle, index, &expected) == 0);
    Require(FT_Get_Sfnt_Name_nid_postfix(&face.guest.rec, index, &actual.value) == 0);
    for (const auto byte : actual.before) Require(byte == 0xA5);
    for (const auto byte : actual.after) Require(byte == 0xA5);
    const auto& name = actual.value;
    Require(name.platform_id == expected.platform_id && name.platform_id == record.platform);
    Require(name.encoding_id == expected.encoding_id && name.encoding_id == record.encoding);
    Require(name.language_id == expected.language_id && name.language_id == record.language);
    Require(name.name_id == expected.name_id && name.name_id == record.name);
    Require(name.string_len == expected.string_len && name.string_len == record.bytes.size());
    Require(std::memcmp(name.string, expected.string, name.string_len) == 0);
    Require(std::memcmp(name.string, record.bytes.data(), name.string_len) == 0);
    FT_SfntName native{};
    Require(FT_Get_Sfnt_Name(face.guest.host, index, &native) == 0 && name.string == native.string);
    return name;
}

static void InvalidQuery(GuestFaceRec* guest, FT_Face native, std::uint32_t index) {
    GuestSfntName actual;
    std::memset(&actual, 0xA5, sizeof(actual));
    std::array<unsigned char, sizeof(actual)> before;
    std::memcpy(before.data(), &actual, sizeof(actual));
    FT_SfntName expected{};
    const FT_Error error = FT_Get_Sfnt_Name(native, index, &expected);
    Require(error != 0);
    Require(FT_Get_Sfnt_Name_nid_postfix(guest, index, &actual) == error);
    Require(std::memcmp(&actual, before.data(), sizeof(actual)) == 0);
    Require(FT_Get_Sfnt_Name_nid_postfix(guest, index, nullptr) == FT_Get_Sfnt_Name(native, index, nullptr));
}

int main() {
    GuestLibrary library{};
    FT_Library nativeLibrary = nullptr;
    Require(FT_Init_FreeType(&library.host) == 0);
    Require(FT_Init_FreeType(&nativeLibrary) == 0);
    {
        const std::vector<NameRecord> records = {
            {1, 0, 0, 1, {'F', 'a', 'm', 'i', 'l', 'y'}},
            {3, 1, 0x0409, 6, {0, 'S', 0, 'f', 0, 'n', 0, 't'}},
            {3, 10, 0x0411, 256, {0, 'A', 0, 0, 0, 'B'}}
        };
        FacePair first(library, nativeLibrary, NamedFont(records));
        FacePair empty(library, nativeLibrary, FontFixture::BuildFont(3, {}));
        Require(FT_Get_Sfnt_Name_Count_nid_postfix(&first.guest.rec) == records.size());
        Require(FT_Get_Sfnt_Name_Count_nid_postfix(&first.guest.rec) == FT_Get_Sfnt_Name_Count(first.oracle));
        std::vector<GuestSfntName> retained;
        for (std::uint32_t index = 0; index < records.size(); ++index) retained.push_back(Query(first, index, records[index]));
        {
            const std::vector<NameRecord> other = {{3, 1, 0x0409, 6, {0, 'O', 0, 't', 0, 'h', 0, 'e', 0, 'r'}}};
            FacePair second(library, nativeLibrary, NamedFont(other));
            Require(FT_Get_Sfnt_Name_Count_nid_postfix(&second.guest.rec) == other.size());
            Query(second, 0, other[0]);
        }
        for (std::uint32_t index = 0; index < records.size(); ++index) {
            Require(Query(first, index, records[index]).string == retained[index].string);
            Require(std::memcmp(retained[index].string, records[index].bytes.data(), retained[index].string_len) == 0);
            Require(FT_Get_Sfnt_Name_nid_postfix(&first.guest.rec, index, nullptr) == FT_Get_Sfnt_Name(first.oracle, index, nullptr));
        }
        Require(FT_Get_Sfnt_Name_Count_nid_postfix(&empty.guest.rec) == 0);
        Require(FT_Get_Sfnt_Name_Count_nid_postfix(nullptr) == 0);
        for (const std::uint32_t index : {0u, 3u, 0x80000000u, 0xFFFFFFFFu}) {
            if (index >= records.size()) InvalidQuery(&first.guest.rec, first.oracle, index);
            InvalidQuery(&empty.guest.rec, empty.oracle, index);
            InvalidQuery(nullptr, nullptr, index);
        }
    }
    Require(FT_Done_FreeType(library.host) == 0);
    Require(FT_Done_FreeType(nativeLibrary) == 0);
}
