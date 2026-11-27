#include "prx/libScePngParser/libScePngParser.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libSceSysmodule/include/Api.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void ClobberErrno(std::uintptr_t, std::size_t) {
    errno = 99;
}

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "png parser check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

struct GuestString {
    union {
        char inline_[16];
        const char* data;
    };
    std::uint64_t size;
    std::uint64_t capacity;
};
static_assert(sizeof(GuestString) == 0x20);

struct Value {
    std::int32_t type;
    std::int64_t size;
    std::int64_t payload;
};
static_assert(sizeof(Value) == 0x18);

struct Field {
    std::string key;
    std::int32_t type;
    std::int64_t size;
    std::int64_t number;
    std::string text;
};

std::vector<Field> g_fields;
int g_fieldCount = 0;
int g_errnoInCallback = 0;

std::string KeyText(const GuestString* key) {
    if (key->capacity < 16) {
        Require(key->size <= key->capacity);
        return std::string(key->inline_, key->size);
    }
    return std::string(key->data, key->size);
}

int APS5_VABI SetField(void*, const GuestString* key, const Value* value) {
    Field field{KeyText(key), value->type, value->size, 0, {}};
    if (value->type == 2)
        field.text.assign(reinterpret_cast<const char*>(static_cast<std::uintptr_t>(value->payload)), static_cast<std::size_t>(value->size));
    else
        field.number = value->payload;
    g_fields.push_back(field);
    g_errnoInCallback = errno;
    return 0;
}

int APS5_VABI FieldCount(void*) {
    return g_fieldCount;
}

std::array<std::uintptr_t, 11> g_table{};

struct alignas(8) FakeMetadata {
    std::array<unsigned char, 0x30> bytes{};

    FakeMetadata() {
        const std::uintptr_t table = reinterpret_cast<std::uintptr_t>(g_table.data());
        std::memcpy(bytes.data(), &table, sizeof(table));
        std::memset(bytes.data() + 0x0c, 0xA5, 4);
    }
    std::uint32_t Status() const {
        std::uint32_t status = 0;
        std::memcpy(&status, bytes.data() + 0x0c, sizeof(status));
        return status;
    }
};

constexpr std::uint32_t kUntouched = 0xA5A5A5A5u;
constexpr int kNotFound = static_cast<int>(0x809D401Eu);
constexpr int kBadFile = static_cast<int>(0x809D4038u);
const std::string kDir = "guest_png_parser_files";

std::uint32_t Crc(const std::string& bytes) {
    std::uint32_t crc = 0xFFFFFFFFu;
    for (const char byte : bytes) {
        crc ^= static_cast<std::uint8_t>(byte);
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return crc ^ 0xFFFFFFFFu;
}

std::string BigEndian(std::uint32_t value) {
    std::string bytes(4, '\0');
    for (int i = 0; i < 4; ++i)
        bytes[i] = static_cast<char>(value >> (24 - 8 * i));
    return bytes;
}

std::string Chunk(const std::string& type, const std::string& data, bool goodCrc = true) {
    return BigEndian(static_cast<std::uint32_t>(data.size())) + type + data + BigEndian(goodCrc ? Crc(type + data) : 0);
}

std::string Png(std::uint32_t width, std::uint32_t height, std::uint8_t colour, std::initializer_list<std::string> chunks, bool goodHeaderCrc = true) {
    std::string header = BigEndian(width) + BigEndian(height);
    header += static_cast<char>(8);
    header += static_cast<char>(colour);
    header += std::string(3, '\0');
    std::string file("\x89PNG\r\n\x1a\n", 8);
    file += Chunk("IHDR", header, goodHeaderCrc);
    for (const std::string& chunk : chunks) file += chunk;
    file += Chunk("IDAT", std::string("\x78\x9c\x63\x00\x00\x00\x01\x00\x01", 9));
    file += Chunk("IEND", "");
    return file;
}

std::string Text(const std::string& keyword, const std::string& text) {
    return Chunk("tEXt", keyword + std::string(1, '\0') + text);
}

std::string International(const std::string& keyword, const std::string& text) {
    return Chunk("iTXt", keyword + std::string(5, '\0') + text);
}

std::string Time(std::uint16_t year, std::uint8_t month, std::uint8_t day, std::uint8_t hour, std::uint8_t minute, std::uint8_t second) {
    std::string data;
    data += static_cast<char>(year >> 8);
    data += static_cast<char>(year & 0xFF);
    for (const std::uint8_t part : {month, day, hour, minute, second}) data += static_cast<char>(part);
    return Chunk("tIME", data);
}

std::string Write(const std::string& path, const std::string& bytes) {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path().empty() ? std::filesystem::path(".") : std::filesystem::path(path).parent_path());
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    Require(file.good());
    return path;
}

GuestString MakePath(const std::string& path) {
    GuestString result{};
    result.size = path.size();
    if (path.size() < 16) {
        std::memcpy(result.inline_, path.data(), path.size());
        result.capacity = 15;
    } else {
        result.data = path.data();
        result.capacity = path.size();
    }
    return result;
}

int Read(const std::string& path, FakeMetadata& output) {
    g_fields.clear();
    const GuestString guestPath = MakePath(path);
    FakeMetadata request;
    return libScePngParserUnknown_MBQfjtzRyh0_nid_no_patch(&guestPath, request.bytes.data(), output.bytes.data());
}

const Field* Find(const std::string& key) {
    for (const Field& field : g_fields)
        if (field.key == key) return &field;
    return nullptr;
}

std::optional<std::string> TextOf(const std::string& key) {
    const Field* field = Find(key);
    if (field == nullptr) return std::nullopt;
    Require(field->type == 2 && field->size == static_cast<std::int64_t>(field->text.size()));
    return field->text;
}

std::optional<std::int64_t> NumberOf(const std::string& key) {
    const Field* field = Find(key);
    if (field == nullptr) return std::nullopt;
    Require(field->type == 0 && field->size == 8);
    return field->number;
}

FileStat Stat(const std::string& path) {
    FileStat status{};
    Require(sceKernelStat(path.c_str(), &status) == 0);
    return status;
}

void ReadOk(const std::string& path) {
    FakeMetadata output;
    Require(Read(path, output) == 0);
    Require(output.Status() == 0x2000Bu);
    const FileStat status = Stat(path);
    Require(NumberOf("birth_time") == status.st_birthtim.tv_sec);
    Require(NumberOf("imported_time") == status.st_birthtim.tv_sec);
    Require(NumberOf("size") == status.st_size);
    Require(TextOf("content_path") == path);
    Require(NumberOf("orientation_type") == 1);
    Require(NumberOf("duration") == 0);
    Require(NumberOf("duration_ms") == 0);
    Require(Find("copyright") == nullptr);
}

void ReadFails(const std::string& path, int expected) {
    FakeMetadata output;
    Require(Read(path, output) == expected);
    Require(g_fields.empty());
    Require(output.Status() == kUntouched);
}

template <typename Exception>
bool Throws(const std::string& path) {
    FakeMetadata output;
    try {
        static_cast<void>(Read(path, output));
    } catch (const Exception&) {
        return g_fields.empty() && output.Status() == kUntouched;
    }
    return false;
}

void TestPlain() {
    const std::string path = Write(kDir + "/plain.png", Png(7, 5, 2, {}));
    ReadOk(path);
    const FileStat status = Stat(path);
    const std::vector<std::string> order{"title", "width", "height", "created_time", "birth_time", "last_updated_time",
        "imported_time", "size", "content_path", "content_path_ext", "orientation_type", "duration", "duration_ms"};
    Require(g_fields.size() == order.size());
    for (std::size_t i = 0; i < order.size(); ++i) Require(g_fields[i].key == order[i]);
    Require(TextOf("title") == "plain");
    Require(NumberOf("width") == 7 && NumberOf("height") == 5);
    Require(NumberOf("created_time") == status.st_birthtim.tv_sec);
    Require(NumberOf("last_updated_time") == status.st_ctim.tv_sec);
    Require(TextOf("content_path_ext") == "png");

    ReadOk(Write(kDir + "/rgba.png", Png(9, 4, 6, {}, false)));
    Require(NumberOf("width") == 9 && NumberOf("height") == 4);
    Require(sceSysmoduleIsLoadedInternal(0x80000020u) != 0);
}

void TestInternationalText() {
    ReadOk(Write(kDir + "/itxt.png", Png(1, 1, 2, {International("Game Title", "Some Game"), International("Title", "Shot one"), International("Comment", "nice")})));
    Require(TextOf("title") == "Shot one" && TextOf("embedded_title") == "Shot one");
    Require(TextOf("appli_title") == "Some Game");
    Require(TextOf("comment") == "nice");
    Require(g_fields[0].key == "title" && g_fields[1].key == "embedded_title");
    Require(g_fields[g_fields.size() - 5].key == "appli_title" && g_fields[g_fields.size() - 4].key == "comment");

    ReadOk(Write(kDir + "/language.png", Png(1, 1, 2, {Chunk("iTXt", std::string("Title\0\0\0en\0\0Shot", 16))})));
    Require(TextOf("title") == "" && TextOf("embedded_title") == "");

    ReadOk(Write(kDir + "/prefix.png", Png(1, 1, 2, {Chunk("iTXt", std::string("TitleX\0\0\0\0Hello", 15))})));
    Require(TextOf("title") == "Hello" && TextOf("embedded_title") == "Hello");

    ReadOk(Write(kDir + "/zero.png", Png(1, 1, 2, {International("Comment", std::string("one\0two", 7))})));
    Require(TextOf("comment") == "one");

    Require(Throws<std::runtime_error>(Write(kDir + "/itxt_short.png", Png(1, 1, 2, {Chunk("iTXt", std::string("Title\0\0", 7))}))));
}

void TestPlainText() {
    ReadOk(Write(kDir + "/text_all.png", Png(1, 1, 2, {Text("Title", "ab"), Text("Comment", "c"), Text("Game Title", "g"),
        Text("Title ID", "PPSA01234"), Text("Display Type", "VRHMD"), Text("Copyright", "x"), Text("Creation Time", "2021-06-07T08:09:10Z")})));
    Require(TextOf("title") == "text_all" && Find("embedded_title") == nullptr);
    for (const char* key : {"comment", "appli_title", "appli_title_id", "vrhmd"}) Require(Find(key) == nullptr);
    Require(NumberOf("created_time") == Stat(kDir + "/text_all.png").st_birthtim.tv_sec);

    ReadOk(Write(kDir + "/title_id.png", Png(1, 1, 2, {Text("Title ID", "PPSA01234")})));
    Require(TextOf("appli_title_id") == "PPSA01234");

    ReadOk(Write(kDir + "/title_id_prefix.png", Png(1, 1, 2, {Text("Title IDX", "PPSA01234")})));
    Require(TextOf("appli_title_id") == "");

    ReadOk(Write(kDir + "/short_stop.png", Png(1, 1, 2, {Text("ab", "c"), Text("Title ID", "PPSA01234")})));
    Require(Find("appli_title_id") == nullptr);

    ReadOk(Write(kDir + "/short_title.png", Png(1, 1, 2, {Chunk("iTXt", "abcd"), International("Title", "Shot")})));
    Require(TextOf("title") == "short_title" && Find("embedded_title") == nullptr);

    ReadOk(Write(kDir + "/skip_other.png", Png(1, 1, 2, {Text("Comment", "a long comment"), Text("Title ID", "PPSA01234")})));
    Require(TextOf("appli_title_id") == "PPSA01234");

    ReadOk(Write(kDir + "/after_end.png", Png(1, 1, 2, {}) + Text("Title ID", "PPSA01234")));
    Require(Find("appli_title_id") == nullptr);
}

void TestDisplayType() {
    ReadOk(Write(kDir + "/vr.png", Png(1, 1, 2, {Text("Display Type", "VRHMD")})));
    Require(NumberOf("vrhmd") == 1);
    Require(g_fields[g_fields.size() - 4].key == "vrhmd");
    ReadOk(Write(kDir + "/tv.png", Png(1, 1, 2, {Text("Display Type", "TV")})));
    Require(Find("vrhmd") == nullptr);
    ReadOk(Write(kDir + "/vr_long.png", Png(1, 1, 2, {Text("Display Type", "VRHMDX")})));
    Require(Find("vrhmd") == nullptr);
}

void TestTimes() {
    const std::string created = Write(kDir + "/created.png", Png(1, 1, 2, {Text("Creation Time", "2021-06-07T08:09:10Z")}));
    ReadOk(created);
    Require(NumberOf("created_time") == 1623053350);
    Require(NumberOf("birth_time") == Stat(created).st_birthtim.tv_sec);

    ReadOk(Write(kDir + "/created_bad.png", Png(1, 1, 2, {Text("Creation Time", "not a date")})));
    Require(Find("created_time") == nullptr);

    ReadOk(Write(kDir + "/created_itxt.png", Png(1, 1, 2, {International("Creation Time", "2021-06-07T08:09:10Z")})));
    Require(NumberOf("created_time") == Stat(kDir + "/created_itxt.png").st_birthtim.tv_sec);

    ReadOk(Write(kDir + "/time.png", Png(1, 1, 2, {Time(2020, 1, 2, 3, 4, 5)})));
    Require(NumberOf("last_updated_time") == 1577934245);
    ReadOk(Write(kDir + "/time_bad.png", Png(1, 1, 2, {Time(2020, 13, 2, 3, 4, 5)})));
    Require(Find("last_updated_time") == nullptr);
}

void TestBadFiles() {
    ReadFails(Write(kDir + "/text.png", "this is not a png file at all, just some text\n"), kBadFile);
    ReadFails(Write(kDir + "/empty.png", ""), kBadFile);
    ReadFails(Write(kDir + "/cut.png", Png(7, 5, 2, {}).substr(0, 20)), kBadFile);
    ReadFails(kDir + "/missing.png", kNotFound);
    const std::string whole = Png(7, 5, 2, {});
    Require(Throws<std::runtime_error>(Write(kDir + "/cut_after_header.png", whole.substr(0, 40))));
    Require(Throws<std::runtime_error>(Write(kDir + "/first_not_header.png", std::string("\x89PNG\r\n\x1a\n", 8) + Chunk("tEXt", std::string(13, 'a')) + Chunk("IEND", ""))));
}

void TestFileNames() {
    ReadOk(Write(kDir + "/a.b.c.PNG", Png(1, 1, 2, {})));
    Require(TextOf("title") == "a.b.c" && TextOf("content_path_ext") == "PNG");
    ReadOk(Write(kDir + "/noext", Png(1, 1, 2, {})));
    Require(Find("title") == nullptr && Find("content_path_ext") == nullptr);
    ReadOk(Write(kDir + "/a.dir/noext", Png(1, 1, 2, {})));
    Require(TextOf("title") == "noext" && TextOf("content_path_ext") == "dir/noext");
    ReadOk(Write(kDir + "/.hidden", Png(1, 1, 2, {})));
    Require(TextOf("title") == "" && TextOf("content_path_ext") == "hidden");
    ReadOk(Write("x.png", Png(1, 1, 2, {})));
    Require(Find("title") == nullptr && TextOf("content_path_ext") == "png");
    ReadOk(Write("guest_png_parser_root_file", Png(1, 1, 2, {})));
    Require(Find("title") == nullptr && Find("content_path_ext") == nullptr);
}

void TestArguments() {
    const std::string path = Write(kDir + "/args.png", Png(1, 1, 2, {}));
    const GuestString guestPath = MakePath(path);
    FakeMetadata output;
    for (int which = 0; which < 3; ++which) {
        bool threw = false;
        try {
            static_cast<void>(libScePngParserUnknown_MBQfjtzRyh0_nid_no_patch(which == 0 ? nullptr : &guestPath,
                which == 1 ? nullptr : output.bytes.data(), which == 2 ? nullptr : output.bytes.data()));
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        Require(threw);
    }
    bool threwUnmapped = false;
    try {
        static_cast<void>(libScePngParserUnknown_MBQfjtzRyh0_nid_no_patch(reinterpret_cast<const void*>(0x10), output.bytes.data(), output.bytes.data()));
    } catch (const std::invalid_argument&) {
        threwUnmapped = true;
    }
    Require(threwUnmapped);

    GuestString overlong = MakePath("a.png");
    overlong.size = 16;
    bool threwOverlong = false;
    try {
        static_cast<void>(libScePngParserUnknown_MBQfjtzRyh0_nid_no_patch(&overlong, output.bytes.data(), output.bytes.data()));
    } catch (const std::invalid_argument&) {
        threwOverlong = true;
    }
    Require(threwOverlong);

    const int probe = sceKernelOpen(path.c_str(), 0, 0);
    Require(probe >= 0 && sceKernelClose(probe) == 0);
    ReadOk(path);
    const int reopened = sceKernelOpen(path.c_str(), 0, 0);
    Require(reopened == probe && sceKernelClose(reopened) == 0);

    ReadFails("missing_15c.png", kNotFound);

    const int dirProbe = sceKernelOpen(path.c_str(), 0, 0);
    Require(dirProbe >= 0 && sceKernelClose(dirProbe) == 0);
    Require(Throws<std::runtime_error>(kDir));
    const int dirReopened = sceKernelOpen(path.c_str(), 0, 0);
    Require(dirReopened == dirProbe && sceKernelClose(dirReopened) == 0);

    g_fieldCount = 1;
    const std::uint64_t before = NotImplementedCalls_nid_no_patch();
    Require(Throws<std::runtime_error>(path));
    Require(NotImplementedCalls_nid_no_patch() == before + 1);
    g_fieldCount = 0;

    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberErrno);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(ClobberErrno);
    errno = 71;
    ReadOk(path);
    Require(g_errnoInCallback == 71);
    errno = 71;
    ReadFails(kDir + "/missing.png", kNotFound);
    Require(errno == 71);
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
}

}

int main() {
    g_table[0x18 / 8] = reinterpret_cast<std::uintptr_t>(&SetField);
    g_table[0x50 / 8] = reinterpret_cast<std::uintptr_t>(&FieldCount);
    std::filesystem::remove_all(kDir);
    std::filesystem::create_directories(kDir);

    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberErrno);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(ClobberErrno);
    bool threwUnmapped = false;
    errno = 71;
    try {
        static_cast<void>(libScePngParserUnknown_vJpgNFIBlCk_nid_no_patch(reinterpret_cast<const void*>(0x10)));
    } catch (const std::invalid_argument&) {
        threwUnmapped = true;
    }
    Require(threwUnmapped && errno == 71);

    std::array<unsigned char, 0x58> config{};
    for (std::size_t i = 0; i < config.size(); ++i) config[i] = static_cast<unsigned char>(i + 1);
    for (int call = 0; call < 2; ++call) {
        Require(libScePngParserUnknown_0Imz9zU_minus_Wgk_nid_no_patch() == static_cast<int>(0x809D4019u));
        Require(libScePngParserUnknown_5XfcHLzGE6E_nid_no_patch() == static_cast<int>(0x809D4054u));
        errno = 71;
        Require(libScePngParserUnknown_vJpgNFIBlCk_nid_no_patch(config.data()) == 0);
        Require(errno == 71);
        Require(_ZN23sceMetadataReaderWriter9pngParser17pngParserFinalizeEv() == 0);
    }
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);

    const std::string plain = Write(kDir + "/plain.png", Png(7, 5, 2, {}));
    Require(Throws<std::logic_error>(plain));
    Require(libScePngParserUnknown_vJpgNFIBlCk_nid_no_patch(config.data()) == 0);
    TestPlain();
    TestInternationalText();
    TestPlainText();
    TestDisplayType();
    TestTimes();
    TestBadFiles();
    TestFileNames();
    TestArguments();
    Require(_ZN23sceMetadataReaderWriter9pngParser17pngParserFinalizeEv() == 0);
    Require(Throws<std::logic_error>(plain));
    std::filesystem::remove_all(kDir);
    std::filesystem::remove("x.png");
    std::filesystem::remove("guest_png_parser_root_file");
    return 0;
}
