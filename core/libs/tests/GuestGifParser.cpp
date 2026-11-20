#include "prx/libSceGifParser/libSceGifParser.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libSceSysmodule/include/Api.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <variant>
#include <vector>
#include <stdexcept>

namespace {

void ClobberErrno(std::uintptr_t, std::size_t) {
    errno = 99;
}

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "GIF parser check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

struct String {
    union {
        char bytes[16];
        const char* pointer;
    };
    std::uint64_t size;
    std::uint64_t capacity;
};

String MakeString(const std::string& text) {
    String result{};
    result.size = text.size();
    result.capacity = text.size() < 16 ? 15 : text.size();
    if (text.size() < 16)
        std::memcpy(result.bytes, text.data(), text.size());
    else
        result.pointer = text.c_str();
    return result;
}

struct Value {
    std::int32_t type;
    std::int64_t size;
    union {
        std::int64_t number;
        const char* text;
    };
};

using Field = std::pair<std::string, std::variant<std::int64_t, std::string>>;

struct Metadata {
    const std::array<std::uintptr_t, 11>* vtable;
    std::uint32_t reserved = 0;
    std::uint32_t format = 0;
    std::vector<Field> fields;
    std::string rejectKey;
};

int g_allocations = 0;
int g_releases = 0;

void* APS5_VABI Allocate(std::size_t size) {
    ++g_allocations;
    return std::malloc(size);
}

void APS5_VABI Release(void* address) {
    ++g_releases;
    std::free(address);
}

int APS5_VABI KeyCount(const Metadata* request) {
    Require(sceSysmoduleIsLoadedInternal(0x80000020u) == 0);
    return static_cast<int>(request->fields.size());
}

void APS5_VABI SetKey(Metadata* output, const String* name, const Value* value) {
    std::string key(name->capacity < 16 ? name->bytes : name->pointer, name->size);
    if (key == output->rejectKey) throw std::runtime_error("setter rejected key");
    if (value->type == 2) {
        output->fields.emplace_back(key, std::string(value->text, value->size));
    } else {
        Require(value->type == 0 && value->size == 8);
        output->fields.emplace_back(key, value->number);
    }
}

void Append(std::string& data, std::initializer_list<unsigned char> bytes) {
    for (const auto byte : bytes) data.push_back(static_cast<char>(byte));
}

void Application(std::string& data, const char* id, const std::string& text) {
    Append(data, {0x21, 0xff, 11});
    data.append(id, 8);
    Append(data, {'X', 'Y', 'Z'});
    for (std::size_t pos = 0; pos < text.size(); pos += 255) {
        const auto count = std::min<std::size_t>(255, text.size() - pos);
        data.push_back(static_cast<char>(count));
        data.append(text, pos, count);
    }
    data.push_back(0);
}

void Image(std::string& data) {
    Append(data, {0x2c, 0, 0, 0, 0, 1, 0, 1, 0, 0, 2, 2, 0x44, 1, 0});
}

void Frame(std::string& data, unsigned char delay) {
    Append(data, {0x21, 0xf9, 4, 0, delay, 0, 0, 0});
    Image(data);
}

void TestRead() {
    const std::filesystem::path dir = "gif_metadata_test";
    std::filesystem::create_directories(dir);
    const std::string path = "/" + dir.generic_string() + "/a.b.GIF";
    const auto guestPath = MakeString(path);
    std::array<std::uintptr_t, 11> slots{};
    slots[3] = reinterpret_cast<std::uintptr_t>(SetKey);
    slots[10] = reinterpret_cast<std::uintptr_t>(KeyCount);
    Metadata request{&slots};
    Metadata output{&slots};
    auto read = [&](const std::string& file) {
        std::ofstream stream(dir / "a.b.GIF", std::ios::binary | std::ios::trunc);
        stream.write(file.data(), file.size());
        stream.close();
        Require(stream.good());
        output.fields.clear();
        output.format = 0;
        return libSceGifParserUnknown_T8p1TeFElWo_nid_no_patch(&guestPath, &request, &output);
    };
    std::array<std::uintptr_t, 11> config{};
    config[0] = reinterpret_cast<std::uintptr_t>(Allocate);
    config[3] = reinterpret_cast<std::uintptr_t>(Release);
    Require(libSceGifParserUnknown_WvV54DtSzmQ_nid_no_patch(config.data()) == 0);
    std::string header = "GIF89a";
    Append(header, {10, 0, 6, 0, 0x80, 0, 0, 0, 0, 0, 255, 255, 255});
    std::string animation = header;
    const std::string title(300, 'A');
    Application(animation, "PSGIF_TI", title);
    Application(animation, "PSGIF_CM", "embedded comment");
    Frame(animation, 10);
    Frame(animation, 20);
    Frame(animation, 30);
    animation.push_back(0x3b);
    const auto initiallyLoaded = sceSysmoduleIsLoadedInternal(0x80000020u);
    Require(read(animation) == 0);
    Require(sceSysmoduleIsLoadedInternal(0x80000020u) == initiallyLoaded);
    Require(g_allocations > 0 && g_releases == g_allocations);
    Require(output.format == 0x20002);
    const auto& fields = output.fields;
    Require(fields.size() == 17);
    Require(fields[0] == Field("title", title) && fields[1] == Field("embedded_title", title));
    Require(fields[2] == Field("comment", std::string("embedded comment")));
    Require(fields[3] == Field("width", 10L) && fields[4] == Field("height", 6L));
    Require(fields[5] == Field("duration", 0L) && fields[6] == Field("duration_ms", 600L));
    Require(fields[7].first == "created_time" && fields[8].first == "birth_time");
    Require(fields[9].first == "last_updated_time" && fields[10].first == "imported_time");
    Require(fields[7].second == fields[8].second && fields[8].second == fields[10].second);
    Require(fields[11] == Field("size", static_cast<std::int64_t>(animation.size())));
    Require(fields[12] == Field("content_path", path) && fields[13] == Field("content_path_ext", std::string("GIF")));
    Require(fields[14] == Field("is_animated", 1L) && fields[15] == Field("orientation_type", 1L));
    Require(fields[16] == Field("total_frame", 3L));

    std::string large = header;
    const std::string largeTitle(9000, 'B');
    Application(large, "PSGIF_TI", largeTitle);
    Frame(large, 20);
    large.push_back(0x3b);
    Require(read(large) == 0);
    Require(fields[0] == Field("title", largeTitle));
    Require(fields[5] == Field("duration_ms", 0L));
    std::string single = header;
    Append(single, {0x21, 0xfe, 2, 'a', 'b', 0});
    Image(single);
    single.push_back(0x3b);
    Require(read(single) == 0 && fields.front() == Field("title", std::string("a.b")));
    Require(fields.size() == 15 && fields.back() == Field("total_frame", 1L));

    std::string silent = header;
    Frame(silent, 0);
    Frame(silent, 0);
    silent.push_back(0x3b);
    Require(read(silent) == 0);
    Require(fields[fields.size() - 3] == Field("is_animated", 0L));
    Require(fields.back() == Field("total_frame", 2L));

    std::string direct = header;
    Image(direct);
    direct.push_back(0x3b);
    Require(read(direct) == static_cast<int>(0x809D4038u));
    Require(output.format == 0 && fields.empty());
    std::string secondDirect = header;
    Frame(secondDirect, 10);
    Image(secondDirect);
    secondDirect.push_back(0x3b);
    Require(read(secondDirect) == static_cast<int>(0x809D4038u));
    Require(output.format == 0 && fields.empty());
    direct[4] = '7';
    Require(read(direct) == 0 && fields.back() == Field("total_frame", 0L));
    Require(read(std::string(14, 'X')) == static_cast<int>(0x809D4053u));
    Require(read("") == static_cast<int>(0x809D4038u));
    Require(read(animation.substr(0, 20)) == static_cast<int>(0x809D4038u));
    Require(output.format == 0 && fields.empty());
    output.rejectKey = "last_updated_time";
    bool failedSetter = false;
    try {
        static_cast<void>(read(animation));
    } catch (const std::runtime_error&) {
        failedSetter = true;
    }
    Require(failedSetter && output.format == 0);
    Require(fields.size() == 9 && fields.back().first == "birth_time");
    Require(g_releases == g_allocations);
    Require(sceSysmoduleIsLoadedInternal(0x80000020u) == initiallyLoaded);
    output.rejectKey.clear();

    request.fields.emplace_back("width", 0L);
    bool unimplementedKeys = false;
    try {
        static_cast<void>(read(animation));
    } catch (const std::runtime_error&) {
        unimplementedKeys = true;
    }
    Require(unimplementedKeys && fields.empty() && output.format == 0);
    Require(sceSysmoduleIsLoadedInternal(0x80000020u) == initiallyLoaded);
    request.fields.clear();
    const std::string longName(1024, 'A');
    const auto longPath = MakeString(longName);
    Require(libSceGifParserUnknown_T8p1TeFElWo_nid_no_patch(&longPath, &request, &output)
        == static_cast<int>(0x809D4038u));
    Require(output.format == 0 && fields.empty());
    std::filesystem::remove(dir / "a.b.GIF");
    Require(libSceGifParserUnknown_T8p1TeFElWo_nid_no_patch(&guestPath, &request, &output) == static_cast<int>(0x809D401Eu));
    std::filesystem::remove(dir);
    Require(_ZN23sceMetadataReaderWriter9gifParser17gifParserFinalizeEv() == 0);
}

}

int main() {
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberErrno);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(ClobberErrno);
    constexpr int unsupported = static_cast<int>(0x809D4054u);
    Require(_ZN23sceMetadataReaderWriter9gifParser17gifParserFinalizeEv() == 0);
    Require(_ZN23sceMetadataReaderWriter9gifParser17gifParserFinalizeEv() == 0);

    bool threwNull = false;
    try {
        static_cast<void>(libSceGifParserUnknown_WvV54DtSzmQ_nid_no_patch(nullptr));
    } catch (const std::invalid_argument&) {
        threwNull = true;
    }
    Require(threwNull);

    bool threwUnmapped = false;
    errno = 71;
    try {
        static_cast<void>(libSceGifParserUnknown_WvV54DtSzmQ_nid_no_patch(reinterpret_cast<const void*>(0x10)));
    } catch (const std::invalid_argument&) {
        threwUnmapped = true;
    }
    Require(threwUnmapped && errno == 71);

    std::array<unsigned char, 0x58> config{};
    for (std::size_t i = 0; i < config.size(); ++i) config[i] = static_cast<unsigned char>(i + 1);
    const auto snapshot = config;
    for (int call = 0; call < 2; ++call) {
        Require(libSceGifParserUnknown_qiEL_minus_fPgaAA_nid_no_patch() == unsupported);
        Require(libSceGifParserUnknown__minus_ZzjSDCInl4_nid_no_patch() == unsupported);
        errno = 71;
        Require(libSceGifParserUnknown_WvV54DtSzmQ_nid_no_patch(config.data()) == 0);
        Require(errno == 71);
        Require(config == snapshot);
        Require(_ZN23sceMetadataReaderWriter9gifParser17gifParserFinalizeEv() == 0);
    }
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
    TestRead();
    return 0;
}
