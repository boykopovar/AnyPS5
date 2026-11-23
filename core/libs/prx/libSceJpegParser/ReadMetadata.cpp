#include "MetadataState.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

namespace {

using Bytes = std::span<const std::uint8_t>;
using Field = std::variant<std::int64_t, std::string>;
using Fields = std::map<std::string_view, Field>;

bool Fits(Bytes b, std::size_t offset, std::size_t size) {
    return offset <= b.size() && size <= b.size() - offset;
}

unsigned Read(Bytes bytes, bool little) {
    unsigned value = 0;
    for (std::size_t i = 0; i < bytes.size(); ++i)
        value = value << 8 | bytes[little ? bytes.size() - 1 - i : i];
    return value;
}

std::string_view Text(Bytes b) {
    return {reinterpret_cast<const char*>(b.data()), b.size()};
}

void MakerNote(Bytes b, Fields& fields) {
    constexpr char signature[] = "SONY PS\0\0\0\0\0SCE";
    if (b.size() < 24 || std::memcmp(b.data(), signature, 15) || b[15] != 1) return;
    const unsigned count = Read(b.subspan(22, 2), false);
    if (count > 0x7fff) return;
    b = b.subspan(24);
    for (unsigned i = 0; i < count; ++i) {
        if (b.size() < 19 || Text(b.first(4)) != "TXXX" || b[10] != 3) return;
        const auto size = Read(b.subspan(4, 4), false);
        if (size < 10 || size > 0x10008 || !Fits(b, 10, size)) return;
        const auto id = Text(b.subspan(14, 4));
        const auto text = Text(b.subspan(19, size - 9));
        if (id == "ALBM") fields["appli_title"] = std::string(text);
        else if (id == "GPNM") fields["appli_title_id"] = std::string(text);
        else if (id == "GCID") fields["appli_content_id"] = std::string(text);
        else if (id == "GDSC") fields["comment"] = std::string(text);
        else if (id == "DISP") fields["vrhmd"] = std::int64_t{1};
        b = b.subspan(10 + size);
    }
}

std::int64_t CaptureTime(Bytes bytes) {
    const std::string text(Text(bytes));
    GuestTm date{};
    int year, month;
    if (std::sscanf(text.c_str(), "%4d:%2d:%2d %2d:%2d:%2d", &year, &month, &date.tm_mday, &date.tm_hour,
                    &date.tm_min, &date.tm_sec) != 6)
        NotImplemented_nid_no_patch("JPEG metadata: partially converted DateTimeOriginal");
    date.tm_year = year - 1900;
    date.tm_mon = month - 1;
    date.tm_isdst = -1;
    return mktime_nid_postfix(&date);
}

void ReadExif(Bytes tiff, Fields& fields) {
    if (tiff.size() < 8 || tiff[0] != tiff[1] || (tiff[0] != 'I' && tiff[0] != 'M')) return;
    const bool little = tiff[0] == 'I';
    if (Read(tiff.subspan(2, 2), little) != 42) return;
    std::size_t directory = Read(tiff.subspan(4, 4), little);
    for (unsigned level = 0; level < 2 && directory; ++level) {
        if (!Fits(tiff, directory, 2)) return;
        const auto count = Read(tiff.subspan(directory, 2), little);
        if (!Fits(tiff, directory + 2, 12 * count)) return;
        std::size_t nested = 0;
        for (unsigned i = 0; i < count; ++i) {
            const auto entry = tiff.subspan(directory + 2 + 12 * i, 12);
            const auto tag = Read(entry.first(2), little);
            const auto type = Read(entry.subspan(2, 2), little);
            const auto units = Read(entry.subspan(4, 4), little);
            if (type != 2 && type != 3 && type != 4 && type != 7) continue;
            const std::uint64_t size = std::uint64_t{units} * (type == 3 ? 2 : type == 4 ? 4 : 1);
            if (size > tiff.size()) continue;
            const auto offset = size > 4 ? Read(entry.last(4), little) : directory + 2 + 12 * i + 8;
            if (!Fits(tiff, offset, size)) continue;
            const auto data = tiff.subspan(offset, size);
            if (!level) {
                if (tag == 0x010e && type == 2) {
                    fields["title"] = std::string(Text(data));
                    fields["embedded_title"] = fields["title"];
                } else if (tag == 0x0112 && type == 3 && units == 1) {
                    fields["orientation_type"] = std::int64_t{Read(data, little)};
                } else if (tag == 0x8298 && type == 2) {
                    fields["copyright"] = std::string(Text(data));
                } else if (tag == 0x8769 && type == 4 && units == 1) {
                    nested = Read(data, little);
                } else if (tag == 0x8825) {
                    NotImplemented_nid_no_patch("JPEG metadata: GPS directory");
                }
            } else if (tag == 0x9000 && type == 7 && size == 4) {
                fields["exif_version"] = std::string(Text(data));
            } else if (tag == 0x9003 && type == 2) {
                fields["created_time"] = CaptureTime(data);
            } else if (tag == 0x9286 && type == 7 && size >= 8 && std::memcmp(data.data(), "ASCII\0\0\0", 8) == 0) {
                fields["description"] = std::string(Text(data.subspan(8)));
            } else if (tag == 0x927c && type == 7) {
                MakerNote(data, fields);
            }
        }
        directory = nested;
    }
}

using JpegParser::GuestString;

struct Value {
    std::int32_t type;
    std::int64_t size;
    union { std::int64_t number; const char* text; };
};
static_assert(sizeof(GuestString) == 32 && sizeof(Value) == 24);

template<class TFunction>
TFunction Virtual(void* object, std::size_t slot) {
    std::uintptr_t table{};
    TFunction target{};
    if (!GuestCopyIn_nid_no_patch(&table, object, sizeof(table)) || !table || table > UINTPTR_MAX - slot ||
        !GuestCopyIn_nid_no_patch(&target, reinterpret_cast<const void*>(table + slot), sizeof(target)) || !target)
        throw std::invalid_argument("JPEG metadata: missing virtual method");
    return target;
}

using Setter = void (APS5_VABI*)(void*, const GuestString*, const Value*);

struct Output {
    void* object;
    Setter setter;
    JpegParser::Allocate allocate;
    JpegParser::ZeroAllocate zero;
    JpegParser::Free release;

    void Set(std::string_view name, const Field& field) const {
        GuestString key{};
        key.size = name.size();
        key.capacity = std::max<std::size_t>(15, name.size());
        std::unique_ptr<void, JpegParser::Free> longKey(nullptr, release);
        if (name.size() < 16) std::memcpy(key.bytes, name.data(), name.size());
        else {
            longKey.reset(allocate(name.size() + 1));
            if (!longKey) throw std::bad_alloc();
            if (!GuestCopyOut_nid_no_patch(longKey.get(), name.data(), name.size() + 1))
                throw std::invalid_argument("JPEG metadata: unwritable key");
            key.pointer = static_cast<const char*>(longKey.get());
        }
        if (const auto* number = std::get_if<std::int64_t>(&field)) {
            const Value value{0, 8, {.number = *number}};
            setter(object, &key, &value);
        } else {
            const auto& text = std::get<std::string>(field);
            constexpr auto maxTextSize = std::min<std::uintmax_t>(SIZE_MAX - 1, INT64_MAX);
            if (text.size() > maxTextSize) throw std::length_error("JPEG metadata: unrepresentable text length");
            const std::size_t allocationSize = text.size() + 1;
            if (!zero) throw std::logic_error("JPEG metadata: missing calloc callback");
            std::unique_ptr<void, JpegParser::Free> data(zero(1, allocationSize), release);
            if (!data) throw std::bad_alloc();
            if (!GuestCopyOut_nid_no_patch(data.get(), text.c_str(), allocationSize))
                throw std::invalid_argument("JPEG metadata: unwritable text");
            const Value value{2, static_cast<std::int64_t>(text.size()), {.text = static_cast<const char*>(data.get())}};
            setter(object, &key, &value);
        }
    }
};

} // namespace

int JpegParser::ReadFields_nid_no_patch(const char* path, void* request, void* output, Allocate allocate, ZeroAllocate zero, Free release) {
    using Count = int (APS5_VABI*)(void*);
    if (Virtual<Count>(request, 0x50)(request) > 0) NotImplemented_nid_no_patch("JPEG metadata: custom key list");
    ImageInfo image;
    if (const auto e = ReadImage_nid_no_patch(path, image)) return e;
    if (image.multiPicture) NotImplemented_nid_no_patch("JPEG metadata: multi-picture format");
    const std::string_view filename(path);
    Fields fields{
        {"width", std::int64_t{image.width}}, {"height", std::int64_t{image.height}},
        {"genre_type", std::int64_t{0}}, {"duration", std::int64_t{0}}, {"duration_ms", std::int64_t{0}},
        {"created_time", image.stat.st_birthtim.tv_sec}, {"birth_time", image.stat.st_birthtim.tv_sec},
        {"last_updated_time", image.stat.st_mtim.tv_sec}, {"imported_time", image.stat.st_birthtim.tv_sec},
        {"size", image.stat.st_size}, {"content_type", std::int64_t{4}}, {"content_path", std::string(filename)},
        {"codec_type", std::int64_t{17}}, {"base_id", std::int64_t{0}}};
    const auto slash = filename.rfind('/');
    const auto dot = filename.rfind('.');
    if (dot != std::string_view::npos) {
        fields["content_path_ext"] = std::string(filename.substr(dot + 1));
        if (slash != std::string_view::npos) {
            const auto end = dot > slash ? dot : filename.size();
            fields["title"] = std::string(filename.substr(slash + 1, end - slash - 1));
        }
    }
    ReadExif(image.exif, fields);
    constexpr std::array names{"title", "embedded_title", "genre_type", "duration", "duration_ms", "description",
        "created_time", "birth_time", "last_updated_time", "imported_time", "size", "content_type", "content_path",
        "content_path_ext", "appli_title", "appli_title_id", "appli_content_id", "comment", "vrhmd", "copyright",
        "width", "height", "orientation_type", "codec_type", "base_id", "exif_version"};
    const Output destination{output, Virtual<Setter>(output, 0x18), allocate, zero, release};
    for (const auto& name : names) {
        const auto field = fields.find(name);
        if (field != fields.end()) destination.Set(field->first, field->second);
    }
    constexpr std::uint32_t format = 0x20001;
    if (!GuestCopyOut_nid_no_patch(static_cast<std::byte*>(output) + 0x0c, &format, sizeof(format)))
        throw std::invalid_argument("JPEG metadata: unwritable format");
    return 0;
}
