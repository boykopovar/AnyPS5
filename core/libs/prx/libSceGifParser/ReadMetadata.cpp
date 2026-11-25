#include "prx/libSceGifParser/include/InventoryExports.hpp"
#include "Allocator.hpp"
#include "prx/libSceSysmodule/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/KernelErrors.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

constexpr int kFileError = static_cast<int>(0x809D4038u);
constexpr int kWrongFormat = static_cast<int>(0x809D4053u);
constexpr int kMissingPath = static_cast<int>(0x809D401Eu);
constexpr std::uint32_t kRtcModule = 0x80000020u;

struct GuestString {
    union {
        char bytes[16];
        const char* pointer;
    };
    std::uint64_t size;
    std::uint64_t capacity;
};
static_assert(sizeof(GuestString) == 32);

struct MetadataValue {
    std::int32_t type;
    std::int64_t size;
    union {
        std::int64_t number;
        const char* text;
    };
};
static_assert(sizeof(MetadataValue) == 24);

void CopyIn(void* destination, const void* source, std::size_t size) {
    const int saved = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, size);
    errno = saved;
    if (!copied) throw std::invalid_argument("GIF metadata: unreadable guest object");
}

template<typename Function>
Function Virtual(void* object, std::size_t slot) {
    std::uintptr_t table = 0;
    CopyIn(&table, object, sizeof(table));
    Function function{};
    if (table == 0 || table > std::numeric_limits<std::uintptr_t>::max() - slot)
        throw std::invalid_argument("GIF metadata: invalid virtual table");
    CopyIn(&function, reinterpret_cast<const void*>(table + slot), sizeof(function));
    if (!function) throw std::invalid_argument("GIF metadata: missing virtual function");
    return function;
}

int ReadPath(const GuestString& path, std::string& text) {
    if (path.size > path.capacity)
        throw std::invalid_argument("GIF metadata: invalid path size");
    if (path.capacity < 16) {
        text.assign(path.bytes, path.size);
        return 0;
    }
    const int saved = errno;
    const int error = path.size >= 1024
        ? CopyGuestPath_nid_no_patch(path.pointer, text)
        : CopyGuestString_nid_no_patch(path.pointer, path.size + 1, text);
    errno = saved;
    if (SceKernelError(error) == SCE_KERNEL_ERROR_ENAMETOOLONG && path.size >= 1024) return kFileError;
    if (error != 0) throw std::invalid_argument("GIF metadata: unreadable path");
    return 0;
}

class GifFile {
public:
    explicit GifFile(const char* path) : fd(sceKernelOpen(path, 0, 0)) {}
    ~GifFile() { if (fd >= 0) sceKernelClose(fd); }
    GifFile(const GifFile&) = delete;
    GifFile& operator=(const GifFile&) = delete;

    bool Read(std::uint8_t* destination, std::size_t size) {
        while (size > 0) {
            if (pos == end) {
                if (fd < 0) return false;
                const auto count = sceKernelRead(fd, buffer.data(), buffer.size());
                if (count <= 0) return false;
                pos = 0;
                end = static_cast<std::size_t>(count);
            }
            const auto count = std::min(size, end - pos);
            if (destination) {
                std::memcpy(destination, buffer.data() + pos, count);
                destination += count;
            }
            pos += count;
            size -= count;
        }
        return true;
    }

    bool Blocks(std::string* text = nullptr) {
        std::uint8_t length = 0;
        while (Read(&length, 1)) {
            if (length == 0) return true;
            if (text) {
                std::array<std::uint8_t, 255> block;
                if (!Read(block.data(), length)) return false;
                text->append(reinterpret_cast<const char*>(block.data()), length);
            } else if (!Read(nullptr, length)) return false;
        }
        return false;
    }

    bool Palette(std::uint8_t packed) {
        return !(packed & 0x80) || Read(nullptr, 6u << (packed & 7));
    }

private:
    int fd;
    std::array<std::uint8_t, 8192> buffer;
    std::size_t pos = 0;
    std::size_t end = 0;
};

std::uint16_t Little16(const std::uint8_t* bytes) {
    return static_cast<std::uint16_t>(bytes[0] | (bytes[1] << 8));
}

struct Gif {
    std::int64_t width = 0;
    std::int64_t height = 0;
    std::int64_t frames = 0;
    std::int64_t durationMs = 0;
    std::string title;
    std::string comment;
};

bool Extension(GifFile& file, Gif& gif) {
    std::uint8_t label = 0;
    if (!file.Read(&label, 1)) return false;
    if (label == 0xf9) {
        std::array<std::uint8_t, 6> control{};
        if (!file.Read(control.data(), control.size()) || control[0] != 4 || control[5] != 0)
            return false;
        const int delayMs = 10 * Little16(control.data() + 2);
        if (gif.durationMs > std::numeric_limits<std::int64_t>::max() - delayMs) return false;
        gif.durationMs += delayMs;
        return true;
    }
    if (label != 0xff) return file.Blocks();
    std::uint8_t length = 0;
    std::array<std::uint8_t, 11> identifier{};
    if (!file.Read(&length, 1) || length != identifier.size() || !file.Read(identifier.data(), length))
        return false;
    const std::string_view name(reinterpret_cast<const char*>(identifier.data()), 8);
    std::string* text = nullptr;
    if (name == "PSGIF_TI") text = &gif.title;
    if (name == "PSGIF_CM") text = &gif.comment;
    if (text) text->clear();
    return file.Blocks(text);
}

int Parse(GifFile& file, Gif& gif) {
    std::array<std::uint8_t, 13> header{};
    if (!file.Read(header.data(), header.size())) return kFileError;
    const std::string_view signature(reinterpret_cast<const char*>(header.data()), 6);
    if (signature != "GIF87a" && signature != "GIF89a") return kWrongFormat;
    gif.width = Little16(header.data() + 6);
    gif.height = Little16(header.data() + 8);
    if (!file.Palette(header[10])) return kFileError;
    bool afterExtension = false;
    std::uint8_t marker = 0;
    while (file.Read(&marker, 1)) {
        if (marker == 0x3b) {
            if (gif.frames < 2) gif.durationMs = 0;
            return 0;
        }
        if (marker == 0x21) {
            if (!Extension(file, gif)) return kFileError;
            afterExtension = true;
            continue;
        }
        if (marker != 0x2c || (!afterExtension && signature == "GIF89a")) return kFileError;
        std::array<std::uint8_t, 9> image{};
        if (!file.Read(image.data(), image.size()) || !file.Palette(image[8]) ||
            !file.Read(nullptr, 1) || !file.Blocks())
            return kFileError;
        if (afterExtension) ++gif.frames;
        afterExtension = false;
    }
    return kFileError;
}

class Output {
public:
    Output(void* object, GifParser::Allocator allocator)
        : object(object), set(Virtual<Setter>(object, 0x18)), allocator(allocator) {}

    void Number(std::string_view key, std::int64_t number) const {
        Field(key, MetadataValue{0, 8, {.number = number}});
    }
    void Text(std::string_view key, const std::string& text) const {
        Field(key, MetadataValue{2, static_cast<std::int64_t>(text.size()), {.text = text.c_str()}});
    }
private:
    using Setter = void(APS5_VABI*)(void*, const GuestString*, const MetadataValue*);
    void* object;
    Setter set;
    GifParser::Allocator allocator;

    void Field(std::string_view name, const MetadataValue& value) const {
        GuestString key{};
        key.size = name.size();
        key.capacity = std::max<std::size_t>(15, name.size());
        std::unique_ptr<void, decltype(allocator.release)> storage(nullptr, allocator.release);
        if (name.size() < 16) {
            std::memcpy(key.bytes, name.data(), name.size());
        } else {
            storage.reset(allocator.allocate(name.size() + 1));
            if (!storage) throw std::bad_alloc();
            const int saved = errno;
            const bool copied = GuestCopyOut_nid_no_patch(storage.get(), name.data(), name.size() + 1);
            errno = saved;
            if (!copied) throw std::invalid_argument("GIF metadata: unwritable allocated key");
            key.pointer = static_cast<const char*>(storage.get());
        }
        set(object, &key, &value);
    }
};

struct RtcUse {
    RtcUse() { sceSysmoduleLoadModuleInternal(kRtcModule); }
    ~RtcUse() { sceSysmoduleUnloadModuleInternal(kRtcModule); }
};

} // namespace

extern "C" int APS5_VABI libSceGifParserUnknown_T8p1TeFElWo_nid_no_patch(const void* path, void* request, void* output) {
    const RtcUse rtc;
    const auto allocator = GifParser::GetAllocator_nid_no_patch();
    using Count = int(APS5_VABI*)(void*);
    if (Virtual<Count>(request, 0x50)(request) > 0) NotImplemented_nid_no_patch(__func__);
    GuestString pathObject{};
    CopyIn(&pathObject, path, sizeof(pathObject));
    std::string filename;
    if (const int error = ReadPath(pathObject, filename); error != 0) return error;
    FileStat stat{};
    const int statResult = sceKernelStat(filename.c_str(), &stat);
    if (statResult == SCE_KERNEL_ERROR_ENOENT || statResult == SCE_KERNEL_ERROR_ENOTDIR ||
        statResult == SCE_KERNEL_ERROR_EINVAL) return kMissingPath;
    if (statResult != 0 || stat.st_size < 14) return kFileError;
    Gif gif;
    GifFile file(filename.c_str());
    if (const int error = Parse(file, gif); error != 0) return error;
    const auto dot = filename.rfind('.');
    const auto slash = filename.rfind('/');
    const Output fields(output, allocator);
    if (!gif.title.empty()) {
        fields.Text("title", gif.title);
        fields.Text("embedded_title", gif.title);
    } else if (dot != std::string::npos && slash != std::string::npos) {
        const auto end = dot > slash ? dot : filename.size();
        fields.Text("title", filename.substr(slash + 1, end - slash - 1));
    }
    if (!gif.comment.empty()) fields.Text("comment", gif.comment);
    fields.Number("width", gif.width);
    fields.Number("height", gif.height);
    fields.Number("duration", gif.durationMs / 1000);
    fields.Number("duration_ms", gif.durationMs);
    fields.Number("created_time", stat.st_birthtim.tv_sec);
    fields.Number("birth_time", stat.st_birthtim.tv_sec);
    fields.Number("last_updated_time", stat.st_ctim.tv_sec);
    fields.Number("imported_time", stat.st_birthtim.tv_sec);
    fields.Number("size", stat.st_size);
    fields.Text("content_path", filename);
    if (dot != std::string::npos) fields.Text("content_path_ext", filename.substr(dot + 1));
    fields.Number("is_animated", gif.frames >= 2 && gif.durationMs > 0);
    fields.Number("orientation_type", 1);
    fields.Number("total_frame", gif.frames);
    constexpr std::uint32_t format = 0x20002;
    const int saved = errno;
    const bool written = GuestCopyOut_nid_no_patch(static_cast<std::byte*>(output) + 0x0c, &format, sizeof(format));
    errno = saved;
    if (!written) throw std::invalid_argument("GIF metadata: unwritable output format");
    return 0;
}
APS5_EXPORT("T8p1TeFElWo", libSceGifParserUnknown_T8p1TeFElWo_nid_no_patch);
