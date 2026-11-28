#include "prx/libSceJpegParser/libSceJpegParser.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "Decoder/Jpeg.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {

using Bytes = std::vector<std::uint8_t>;
using Field = std::variant<std::int64_t, std::string>;

void Check(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "JPEG metadata: %s\n", message); std::abort(); }
}

struct String {
    union { char small[16]; const char* data; };
    std::uint64_t size;
    std::uint64_t capacity;
};
struct Value {
    int type;
    std::int64_t size;
    union { std::int64_t number; const char* text; };
};
struct Metadata {
    const std::uintptr_t* table;
    unsigned reserved;
    unsigned format;
    std::map<std::string, Field> fields;
    std::vector<std::string> order;
};

int keyCount = 0;
int APS5_VABI Count(void*) { return keyCount; }

void APS5_VABI Set(Metadata* out, const String* key, const Value* value) {
    std::string name(key->capacity < 16 ? key->small : key->data, key->size);
    out->order.push_back(name);
    if (value->type == 0) {
        Check(value->size == 8, "integer size");
        out->fields[name] = value->number;
    } else {
        Check(value->type == 2 && value->size >= 0, "text record");
        out->fields[name] = std::string(value->text, value->size);
    }
}

unsigned allocations = 0, releases = 0, zeroAllocations = 0;
std::size_t largestTextAllocation = 0;
void* APS5_VABI Allocate(std::size_t size) { ++allocations; return std::malloc(size); }
void* APS5_VABI Zero(std::size_t count, std::size_t size) {
    Check(count == 1, "single text allocation");
    largestTextAllocation = std::max(largestTextAllocation, size);
    ++zeroAllocations;
    return std::calloc(count, size);
}
void APS5_VABI Free(void* p) { ++releases; std::free(p); }

void Put(Bytes& dst, unsigned value, unsigned bytes, bool little) {
    for (unsigned i = 0; i < bytes; ++i) dst.push_back(static_cast<std::uint8_t>(value >> (8 * (little ? i : bytes - 1 - i))));
}

Bytes Text(std::string_view s) { return {s.begin(), s.end()}; }

Bytes Maker() {
    Bytes b = {'S','O','N','Y',' ','P','S',0,0,0,0,0,'S','C','E',1,0,0,0,0,0,0,0,5};
    for (std::string_view id : {"ALBM", "GPNM", "GCID", "GDSC", "DISP"}) {
        b.insert(b.end(), {'T','X','X','X'});
        Put(b, 13, 4, false);
        b.insert(b.end(), {0,0,3,'X','Y','Z'});
        b.insert(b.end(), id.begin(), id.end());
        b.push_back(0);
        b.insert(b.end(), id.begin(), id.end());
    }
    return b;
}

struct Tag { unsigned id; unsigned type; Bytes value; };

Bytes Exif(bool little) {
    Bytes pointer;
    Put(pointer, 8 + 2 + 4 * 12 + 4, 4, little);
    Bytes orientation; Put(orientation, 9, 2, little);
    const std::vector<Tag> primary{{0x010e, 2, {'f','i','l','m',0}}, {0x0112, 3, orientation},
                                   {0x8298, 2, {'A',0}}, {0x8769, 4, pointer}};
    const std::vector<Tag> detail{{0x9000, 7, {'0','2','3','0'}}, {0x9286, 7, Text(std::string_view("ASCII\0\0\0hello", 13))},
                                  {0x9003, 2, Text("2019:05:06 07:08:09")}, {0x927c, 7, Maker()}};
    const unsigned dataStart = 8 + 2 * (2 + 4 * 12 + 4);
    Bytes bytes{static_cast<std::uint8_t>(little ? 'I' : 'M'), static_cast<std::uint8_t>(little ? 'I' : 'M')}, data;
    Put(bytes, 42, 2, little); Put(bytes, 8, 4, little);
    for (const auto& tags : {primary, detail}) {
        Put(bytes, tags.size(), 2, little);
        for (const auto& tag : tags) {
            Put(bytes, tag.id, 2, little); Put(bytes, tag.type, 2, little);
            Put(bytes, tag.value.size() / (tag.type == 3 ? 2 : tag.type == 4 ? 4 : 1), 4, little);
            if (tag.value.size() > 4) {
                Put(bytes, dataStart + data.size(), 4, little);
                data.insert(data.end(), tag.value.begin(), tag.value.end());
            } else {
                bytes.insert(bytes.end(), tag.value.begin(), tag.value.end());
                bytes.insert(bytes.end(), 4 - tag.value.size(), 0);
            }
        }
        Put(bytes, 0, 4, little);
    }
    bytes.insert(bytes.end(), data.begin(), data.end());
    return bytes;
}

Bytes LargeTitle(bool little, unsigned declaredCount, unsigned bytes) {
    Bytes tiff(2, little ? 'I' : 'M');
    Put(tiff, 42, 2, little); Put(tiff, 8, 4, little); Put(tiff, 1, 2, little);
    Put(tiff, 0x010e, 2, little); Put(tiff, 2, 2, little); Put(tiff, declaredCount, 4, little);
    Put(tiff, 26, 4, little); Put(tiff, 0, 4, little);
    tiff.insert(tiff.end(), bytes, 's');
    tiff.back() = 0;
    return tiff;
}

Bytes WithExif(const Bytes& jpeg, const Bytes& tiff) {
    Bytes out{0xff, 0xd8, 0xff, 0xe1};
    Put(out, tiff.size() + 8, 2, false);
    out.insert(out.end(), {'E','x','i','f',0,0});
    out.insert(out.end(), tiff.begin(), tiff.end());
    out.insert(out.end(), jpeg.begin() + 2, jpeg.end());
    return out;
}

}

int main() {
    std::array<std::uintptr_t, 11> config{}, slots{};
    config[0] = reinterpret_cast<std::uintptr_t>(Allocate);
    config[1] = reinterpret_cast<std::uintptr_t>(Zero);
    config[3] = reinterpret_cast<std::uintptr_t>(Free);
    slots[3] = reinterpret_cast<std::uintptr_t>(Set);
    slots[10] = reinterpret_cast<std::uintptr_t>(Count);
    Check(libSceJpegParserUnknown_v3aOHSEXfjo_nid_no_patch(config.data()) == 0, "initialize");
    const std::string pathName = "/reader_picture.jpg";
    Metadata request{slots.data(), 0, 0, {}, {}}, output{slots.data(), 0x55, 0, {}, {}};
    const auto read = [&](const Bytes& img, std::string_view filePath = {}) {
        const std::string name(filePath.empty() ? std::string_view(pathName) : filePath);
        const auto host = name.front() == '/' ? name.substr(1) : name;
        String path{};
        path.size = name.size();
        path.capacity = std::max<std::size_t>(15, name.size());
        if (name.size() < 16) std::memcpy(path.small, name.data(), name.size());
        else path.data = name.c_str();
        if (img.empty()) std::filesystem::remove(host);
        else {
            std::ofstream file(host, std::ios::binary | std::ios::trunc);
            file.write(reinterpret_cast<const char*>(img.data()), img.size());
            file.close();
            Check(file.good(), "write fixture");
        }
        output.fields.clear(); output.order.clear(); output.format = 77;
        return libSceJpegParserReadMetadata_nid_no_patch(&path, &request, &output);
    };
    const auto jpeg = Decoder::Jpeg::Encode(Bytes(11 * 9 * 3, 81), 11, 9, 3, 72);
    for (bool little : {false, true}) {
        const auto bytes = WithExif(jpeg, Exif(little));
        Check(read(bytes) == 0 && output.format == 0x20001 && output.reserved == 0x55, "metadata format");
        const auto& f = output.fields;
        Check(f.at("width") == Field{11} && f.at("height") == Field{9} && f.at("orientation_type") == Field{9}, "frame and orientation");
        Check(f.at("title") == Field{std::string("film\0", 5)} && f.at("embedded_title") == f.at("title") &&
              f.at("copyright") == Field{std::string("A\0", 2)}, "EXIF strings with their stored count");
        Check(f.at("description") == Field{"hello"} && f.at("exif_version") == Field{"0230"}, "EXIF subdirectory");
        Check(f.at("appli_title") == Field{"ALBM"} && f.at("appli_title_id") == Field{"GPNM"} &&
              f.at("appli_content_id") == Field{"GCID"} && f.at("comment") == Field{"GDSC"} && f.at("vrhmd") == Field{1},
              "PlayStation MakerNote");
        GuestTm when{}; when.tm_year = 119; when.tm_mon = 4; when.tm_mday = 6; when.tm_hour = 7; when.tm_min = 8; when.tm_sec = 9; when.tm_isdst = -1;
        Check(f.at("created_time") == Field{mktime_nid_postfix(&when)}, "EXIF capture time");
        Check(f.at("size") == Field{static_cast<std::int64_t>(bytes.size())} && f.at("content_type") == Field{4} &&
              f.at("codec_type") == Field{17} && f.at("base_id") == Field{0}, "file facts");
        Check(output.order.front() == "title" && output.order.back() == "exif_version", "firmware key order");
    }
    constexpr unsigned maxCount = 65535 - 8 - 26;
    std::string maxTitle(maxCount, 's');
    maxTitle.back() = 0;
    for (bool little : {false, true}) {
        largestTextAllocation = 0;
        Check(read(WithExif(jpeg, LargeTitle(little, maxCount, maxCount))) == 0 &&
              output.fields.at("title") == Field{maxTitle} && output.fields.at("embedded_title") == Field{maxTitle} &&
              largestTextAllocation == maxCount + 1, "largest single-title APP1 and setter terminator");
        largestTextAllocation = 0;
        Check(read(WithExif(jpeg, LargeTitle(little, maxCount + 1, maxCount))) == 0 &&
              !output.fields.contains("embedded_title") && output.fields.at("title") == Field{"reader_picture"} &&
              largestTextAllocation < maxCount, "ignore tag count beyond APP1 before allocating its text");
    }
    struct PathCase { const char* name; const char* title; const char* extension; };
    const PathCase paths[] = {{"bare.jpg", nullptr, "jpg"}, {"/plain", nullptr, nullptr},
                              {"/reader.dir/item", "item", "dir/item"}, {"/reader.dir/item.JPEG", "item", "JPEG"},
                              {"/reader.dir/a.b.jpg", "a.b", "jpg"}, {"/reader.dir/.hidden", "", "hidden"}};
    std::filesystem::create_directory("reader.dir");
    for (const auto& entry : paths) {
        Check(read(jpeg, entry.name) == 0, "read JPEG at guest path");
        for (const auto& [key, value] : {std::pair{"title", entry.title}, std::pair{"content_path_ext", entry.extension}}) {
            const auto found = output.fields.find(key);
            Check(value ? found != output.fields.end() && found->second == Field{std::string(value)} :
                          found == output.fields.end(), "file defaults match whole-path rules");
        }
        Check(output.fields.at("content_path") == Field{std::string(entry.name)} && !output.fields.contains("embedded_title"),
              "plain path metadata");
        Check(read(WithExif(jpeg, Exif(true)), entry.name) == 0 && output.fields.at("title") == Field{std::string("film\0", 5)},
              "EXIF title overrides path defaults");
        const std::string name(entry.name);
        std::filesystem::remove(name.front() == '/' ? name.substr(1) : name);
    }
    std::filesystem::remove("reader.dir");
    auto tiff = Exif(true);
    const auto note = Maker();
    const auto start = std::search(tiff.begin(), tiff.end(), note.begin(), note.end());
    Check(start != tiff.end(), "locate test MakerNote");
    start[15] = 2;
    Check(read(WithExif(jpeg, tiff)) == 0 && !output.fields.contains("appli_title") && output.fields.contains("exif_version"), "unknown MakerNote version");
    start[15] = 1;
    start[24 + 23] = 'Z';
    Check(read(WithExif(jpeg, tiff)) == 0 && output.fields.at("appli_title") == Field{"ALBM"} &&
          !output.fields.contains("appli_title_id") && !output.fields.contains("comment"), "stop at unsupported text frame");
    keyCount = 1;
    bool rejected = false;
    try { static_cast<void>(read(jpeg)); } catch (const std::exception&) { rejected = true; }
    Check(rejected && output.fields.empty() && output.format == 77, "unknown requested-key contract");
    keyCount = 0;
    Bytes multi{0xff, 0xd8, 0xff, 0xe2, 0, 6, 'M', 'P', 'F', 0};
    multi.insert(multi.end(), jpeg.begin() + 2, jpeg.end());
    rejected = false;
    try { static_cast<void>(read(multi)); } catch (const std::exception&) { rejected = true; }
    Check(rejected && output.fields.empty() && output.format == 77, "multi-picture format");
    Check(read(jpeg) == 0 && output.fields.at("title") == Field{"reader_picture"} && !output.fields.contains("embedded_title") &&
          !output.fields.contains("orientation_type") && !output.fields.contains("vrhmd"), "plain JPEG");
    FileStat stat{}; Check(sceKernelStat(pathName.c_str(), &stat) == 0, "guest stat");
    Check(output.fields.at("created_time") == Field{stat.st_birthtim.tv_sec} && output.fields.at("last_updated_time") == Field{stat.st_mtim.tv_sec},
          "file times");
    Check(read(WithExif(jpeg, {0,1,2,3})) == 0 && !output.fields.contains("exif_version"), "ignore invalid TIFF header");
    Check(read({0xff, 0xd8, 0xff, 0xd9}) == static_cast<int>(0x809d4052u) && output.fields.empty() && output.format == 77, "no JPEG frame");
    Check(read({}) == static_cast<int>(0x809d401eu) && output.fields.empty(), "missing path");
    Check(_ZN23sceMetadataReaderWriter10jpegParser18jpegParserFinalizeEv() == 0, "finalize");
    Check(zeroAllocations && allocations + zeroAllocations == releases, "allocator ownership");
}
