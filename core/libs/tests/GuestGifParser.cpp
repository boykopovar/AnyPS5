#include "prx/libSceGifParser/libSceGifParser.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libSceSysmodule/include/Api.hpp"

#include <algorithm>
#include <array>
#include <chrono>
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
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

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

struct Given {
    const std::array<std::uintptr_t, 11>* vtable;
    std::vector<std::pair<std::string, std::string>> texts;
    std::vector<std::string> asked;
    int missing = 1;
};

int APS5_VABI GetKey(Given* meta, const String* name, Value* value) {
    Require(sceSysmoduleIsLoadedInternal(0x80000020u) == 0);
    Require(value->type == 2);
    const std::string key(name->capacity < 16 ? name->bytes : name->pointer, name->size);
    meta->asked.push_back(key);
    for (const auto& [given, text] : meta->texts) {
        if (given != key) continue;
        value->size = static_cast<std::int64_t>(text.size());
        value->text = text.c_str();
        return 0;
    }
    return meta->missing;
}

std::string Block(const char* id, const std::string& text) {
    std::string block;
    Append(block, {0x21, 0xff, 11});
    block.append(id, 8);
    block.append(3, '\0');
    for (std::size_t pos = 0; pos < text.size(); pos += 255) {
        const auto count = std::min<std::size_t>(255, text.size() - pos);
        block.push_back(static_cast<char>(count));
        block.append(text, pos, count);
    }
    block.push_back(0);
    return block;
}

std::string Contents(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

void TestWrite() {
    const std::string path = "/a.b.GIF";
    const auto guestPath = MakeString(path);
    const std::filesystem::path host = "a.b.GIF";
    const std::filesystem::path temporary = "user/av_contents/content_tmp/DFC73C9C-72CB-4609-AC4C-82D98752290C.gif";
    std::array<std::uintptr_t, 11> slots{};
    slots[2] = reinterpret_cast<std::uintptr_t>(GetKey);
    slots[3] = reinterpret_cast<std::uintptr_t>(SetKey);
    slots[10] = reinterpret_cast<std::uintptr_t>(KeyCount);
    std::array<std::uintptr_t, 11> config{};
    config[0] = reinterpret_cast<std::uintptr_t>(Allocate);
    config[3] = reinterpret_cast<std::uintptr_t>(Release);
    Require(libSceGifParserUnknown_WvV54DtSzmQ_nid_no_patch(config.data()) == 0);
    const auto initiallyLoaded = sceSysmoduleIsLoadedInternal(0x80000020u);
    auto write = [&](const std::string& source, std::vector<std::pair<std::string, std::string>> texts) {
        if (!source.empty() || std::filesystem::exists(host)) {
            std::ofstream stream(host, std::ios::binary | std::ios::trunc);
            stream.write(source.data(), static_cast<std::streamsize>(source.size()));
            stream.close();
            Require(stream.good());
        }
        Given meta{&slots, std::move(texts)};
        const int result = libSceGifParserUnknown_SZeQGmAx_plus_N0_nid_no_patch(&guestPath, &meta);
        Require(meta.asked == std::vector<std::string>({"title", "comment"}));
        Require(sceSysmoduleIsLoadedInternal(0x80000020u) == initiallyLoaded);
        Require(!std::filesystem::exists(temporary));
        return result;
    };
    std::string header = "GIF89a";
    Append(header, {10, 0, 6, 0, 0x80, 0, 0, 0, 0, 0, 255, 255, 255});
    std::string frame;
    Frame(frame, 10);
    const std::string plain = header + frame + "\x3b";
    const std::string title = "New title";
    const std::string comment = "New comment";

    Require(write(plain, {{"title", title}, {"comment", comment}}) == 0);
    Require(Contents(host) == header + Block("PSGIF_CM", comment) + Block("PSGIF_TI", title) + frame + "\x3b");
    Require(write(plain, {{"title", title}}) == 0);
    Require(Contents(host) == header + Block("PSGIF_TI", title) + frame + "\x3b");

    {
        const auto writeFile = [](const std::filesystem::path& name, const std::string& bytes) {
            std::ofstream stream(name, std::ios::binary);
            stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            stream.close();
            Require(stream.good());
        };
        writeFile(host, plain);
        writeFile(temporary, plain);
        const std::string temporaryGuestPath = "/" + temporary.generic_string();
        for (const auto& target : {path, temporaryGuestPath}) {
            const auto input = MakeString(target);
            Given meta{&slots, {{"title", title}}};
            bool rejected = false;
            try {
                libSceGifParserUnknown_SZeQGmAx_plus_N0_nid_no_patch(&input, &meta);
            } catch (const std::runtime_error&) {
                rejected = true;
            }
            Require(rejected && Contents(host) == plain && Contents(temporary) == plain);
        }
        Require(std::filesystem::remove(temporary));
#ifdef _WIN32
        const auto locked = CreateFileW(host.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        Require(locked != INVALID_HANDLE_VALUE);
        bool renameFailed = false;
        try {
            Given meta{&slots, {{"title", title}}};
            libSceGifParserUnknown_SZeQGmAx_plus_N0_nid_no_patch(&guestPath, &meta);
        } catch (const std::runtime_error&) {
            renameFailed = true;
        }
        CloseHandle(locked);
        Require(renameFailed && Contents(host) == plain && !std::filesystem::exists(temporary));
#endif
    }
    const std::string oldTitle = Block("PSGIF_TI", "Old title");
    const std::string oldComment = Block("PSGIF_CM", "Old comment");
    Require(write(header + oldTitle + frame + "\x3b", {{"title", title}, {"comment", comment}}) == 0);
    Require(Contents(host) == header + Block("PSGIF_CM", comment) + Block("PSGIF_TI", title) + frame + "\x3b");
    Require(write(header + oldComment + frame + "\x3b", {{"title", title}}) == 0);
    Require(Contents(host) == header + Block("PSGIF_TI", title) + oldComment + frame + "\x3b");
    Require(write(header + oldTitle + frame + "\x3b", {{"comment", comment}}) == 0);
    Require(Contents(host) == header + Block("PSGIF_CM", comment) + oldTitle + frame + "\x3b");

    std::string animation;
    Frame(animation, 10);
    Frame(animation, 20);
    Frame(animation, 30);
    Require(write(header + animation + "\x3b", {{"title", title}}) == 0);
    Require(Contents(host) == header + Block("PSGIF_TI", title) + animation + "\x3b");
    const std::string longTitle(300, 'L');
    Require(write(plain, {{"title", longTitle}}) == 0);
    const auto split = Contents(host);
    Require(split == header + Block("PSGIF_TI", longTitle) + frame + "\x3b");
    Require(static_cast<unsigned char>(split[header.size() + 14]) == 255 && static_cast<unsigned char>(split[header.size() + 14 + 256]) == 45);

    const std::string tail = plain + "TAIL";
    Require(write(tail, {}) == 0 && Contents(host) == tail);
    Require(write(tail, {{"title", ""}, {"comment", ""}}) == 0 && Contents(host) == tail);

    Require(write(header + oldTitle + frame + "\x3b", {{"title", title}, {"comment", comment}}) == 0);
    std::array<std::uintptr_t, 11> readSlots{};
    readSlots[3] = reinterpret_cast<std::uintptr_t>(SetKey);
    readSlots[10] = reinterpret_cast<std::uintptr_t>(KeyCount);
    Metadata request{&readSlots};
    Metadata output{&readSlots};
    Require(libSceGifParserUnknown_T8p1TeFElWo_nid_no_patch(&guestPath, &request, &output) == 0);
    Require(output.fields[0] == Field("title", title) && output.fields[2] == Field("comment", comment));
    Require(output.fields.back() == Field("total_frame", 1L));

    std::string direct = header;
    Image(direct);
    direct.push_back(0x3b);
    Require(write(direct, {{"title", title}}) == static_cast<int>(0x809D4038u) && Contents(host) == direct);
    direct[4] = '7';
    Require(write(direct, {{"title", title}}) == 0);
    Require(Contents(host) == header.substr(0, 4) + "7" + header.substr(5) + Block("PSGIF_TI", title) + "\x3b");
    Require(write("not a gif image file", {{"title", title}}) == static_cast<int>(0x809D4053u));
    Require(Contents(host) == "not a gif image file");
    std::filesystem::remove(host);
    Require(write("", {{"title", title}}) == static_cast<int>(0x809D4038u) && !std::filesystem::exists(host));

    bool refused = false;
    try {
        Given meta{&slots, {}, {}, 2};
        static_cast<void>(libSceGifParserUnknown_SZeQGmAx_plus_N0_nid_no_patch(&guestPath, &meta));
    } catch (const std::runtime_error&) {
        refused = true;
    }
    Require(refused && sceSysmoduleIsLoadedInternal(0x80000020u) == initiallyLoaded);
    Require(g_releases == g_allocations);
    Require(_ZN23sceMetadataReaderWriter9gifParser17gifParserFinalizeEv() == 0);
}

}

int main() {
    const auto previous = std::filesystem::current_path();
    const auto dir = std::filesystem::temp_directory_path() / ("anyps5-gif-write-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(dir));
    std::filesystem::current_path(dir);
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
    TestWrite();
    std::filesystem::current_path(previous);
    std::filesystem::remove_all(dir);
    return 0;
}
