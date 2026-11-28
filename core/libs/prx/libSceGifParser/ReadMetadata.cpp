#include "prx/libSceGifParser/include/InventoryExports.hpp"
#include "Allocator.hpp"
#include "prx/libSceSysmodule/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

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
    explicit GifFile(const char* path, std::string* snapshot = nullptr) : fd(sceKernelOpen(path, 0, 0)), snapshot(snapshot) {}
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
                filled += end;
                if (snapshot) snapshot->append(reinterpret_cast<const char*>(buffer.data()), end);
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

    std::size_t Offset() const { return filled - (end - pos); }

private:
    int fd;
    std::string* snapshot;
    std::array<std::uint8_t, 8192> buffer;
    std::size_t pos = 0;
    std::size_t end = 0;
    std::size_t filled = 0;
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

bool Extension(GifFile& file, Gif& gif, std::string_view* tag = nullptr) {
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
    if (tag && text) *tag = text == &gif.title ? "PSGIF_TI" : "PSGIF_CM";
    return file.Blocks(text);
}

struct Block {
    std::size_t begin;
    std::size_t end;
    bool image;
    bool frame;
    std::string_view tag;
};

struct Layout {
    std::size_t header = 0;
    std::vector<Block> blocks;
};

int Parse(GifFile& file, Gif& gif, Layout* layout = nullptr) {
    std::array<std::uint8_t, 13> header{};
    if (!file.Read(header.data(), header.size())) return kFileError;
    const std::string_view signature(reinterpret_cast<const char*>(header.data()), 6);
    if (signature != "GIF87a" && signature != "GIF89a") return kWrongFormat;
    gif.width = Little16(header.data() + 6);
    gif.height = Little16(header.data() + 8);
    if (!file.Palette(header[10])) return kFileError;
    if (layout) layout->header = file.Offset();
    bool afterExtension = false;
    std::uint8_t marker = 0;
    while (file.Read(&marker, 1)) {
        const auto begin = file.Offset() - 1;
        if (marker == 0x3b) {
            if (gif.frames < 2) gif.durationMs = 0;
            return 0;
        }
        if (marker == 0x21) {
            std::string_view tag;
            if (!Extension(file, gif, &tag)) return kFileError;
            if (layout) layout->blocks.push_back({begin, file.Offset(), false, false, tag});
            afterExtension = true;
            continue;
        }
        if (marker != 0x2c || (!afterExtension && signature == "GIF89a")) return kFileError;
        std::array<std::uint8_t, 9> image{};
        if (!file.Read(image.data(), image.size()) || !file.Palette(image[8]) ||
            !file.Read(nullptr, 1) || !file.Blocks())
            return kFileError;
        if (layout) layout->blocks.push_back({begin, file.Offset(), true, afterExtension, {}});
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

constexpr const char* kTemporaryDirectory = "/user/av_contents/content_tmp";
constexpr const char* kTemporaryPath = "/user/av_contents/content_tmp/DFC73C9C-72CB-4609-AC4C-82D98752290C.gif";
std::mutex writerMutex;

std::optional<std::string> Given(void* meta, std::string_view key) {
    using Getter = int(APS5_VABI*)(void*, const GuestString*, MetadataValue*);
    const auto get = Virtual<Getter>(meta, 0x10);
    GuestString name{};
    name.size = key.size();
    name.capacity = 15;
    std::memcpy(name.bytes, key.data(), key.size());
    MetadataValue value{2, 0, {.text = nullptr}};
    const int result = get(meta, &name, &value);
    if (result == 1) return std::nullopt;
    if (result != 0) NotImplemented_nid_no_patch("gifParserWriteMetadata: a metadata lookup result other than 0 or 1");
    if (value.size == 0) return std::nullopt;
    if (value.size < 0 || value.text == nullptr) throw std::invalid_argument("GIF metadata: invalid metadata text");
    std::string text(static_cast<std::size_t>(value.size), '\0');
    CopyIn(text.data(), value.text, text.size());
    return text;
}

void AppendApplication(std::string& out, std::string_view identifier, const std::string& text) {
    out.append("\x21\xff\x0b", 3);
    out.append(identifier);
    out.append(3, '\0');
    for (std::size_t pos = 0; pos < text.size(); pos += 255) {
        const auto count = std::min<std::size_t>(255, text.size() - pos);
        out.push_back(static_cast<char>(count));
        out.append(text, pos, count);
    }
    out.push_back('\0');
}

void Replace(const std::string& path, const std::string& bytes) {
    static_cast<void>(sceKernelMkdir("/user", 0777));
    static_cast<void>(sceKernelMkdir("/user/av_contents", 0777));
    static_cast<void>(sceKernelMkdir(kTemporaryDirectory, 0777));
    int fd = sceKernelOpen(kTemporaryPath, SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_EXCL, 0666);
    if (fd < 0) NotImplemented_nid_no_patch("gifParserWriteMetadata: cannot create the temporary file exclusively");
    try {
        std::size_t done = 0;
        while (done < bytes.size()) {
            const auto count = sceKernelWrite(fd, bytes.data() + done, bytes.size() - done);
            if (count <= 0) NotImplemented_nid_no_patch("gifParserWriteMetadata: a failing write");
            done += static_cast<std::size_t>(count);
        }
        const int closed = sceKernelClose(fd);
        fd = -1;
        if (closed != 0) NotImplemented_nid_no_patch("gifParserWriteMetadata: a failing close");
        if (sceKernelRename(kTemporaryPath, path.c_str()) != 0)
            NotImplemented_nid_no_patch("gifParserWriteMetadata: a failing rename");
    } catch (...) {
        if (fd >= 0) sceKernelClose(fd);
        sceKernelUnlink(kTemporaryPath);
        throw;
    }
}

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

extern "C" int APS5_VABI libSceGifParserUnknown_SZeQGmAx_plus_N0_nid_no_patch(const void* path, void* meta) {
    const RtcUse rtc;
    static_cast<void>(GifParser::GetAllocator_nid_no_patch());
    GuestString pathObject{};
    CopyIn(&pathObject, path, sizeof(pathObject));
    std::string filename;
    if (const int error = ReadPath(pathObject, filename); error != 0) return error;
    FileStat temporary{};
    static_cast<void>(sceKernelStat(kTemporaryPath, &temporary));
    const auto title = Given(meta, "title");
    const auto comment = Given(meta, "comment");
    const std::lock_guard lock(writerMutex);
    const bool writing = title || comment;
    std::string source;
    Gif gif;
    Layout layout;
    int parsed;
    {
        GifFile file(filename.c_str(), writing ? &source : nullptr);
        parsed = Parse(file, gif, writing ? &layout : nullptr);
    }
    if (!writing) {
        if (parsed != 0) NotImplemented_nid_no_patch("gifParserWriteMetadata: no key given for a source that does not parse");
        return 0;
    }
    if (parsed != 0) return parsed;
    std::string written = source.substr(0, layout.header);
    if (comment) AppendApplication(written, "PSGIF_CM", *comment);
    if (title) AppendApplication(written, "PSGIF_TI", *title);
    for (const auto& block : layout.blocks) {
        if (block.image && !block.frame) continue;
        if ((title && block.tag == "PSGIF_TI") || (comment && block.tag == "PSGIF_CM")) continue;
        written.append(source, block.begin, block.end - block.begin);
    }
    written.push_back(0x3b);
    Replace(filename, written);
    return 0;
}
APS5_EXPORT("SZeQGmAx+N0", libSceGifParserUnknown_SZeQGmAx_plus_N0_nid_no_patch);
