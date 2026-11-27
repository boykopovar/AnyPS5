#include "prx/libSceJpegParser/include/Api.hpp"

#include "Decoder/Jpeg.hpp"
#include "MetadataState.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <span>
#include <stdexcept>
#include <vector>

namespace {

constexpr int kParserError = static_cast<int>(0x809D4014u);
constexpr int kNoThumbnail = static_cast<int>(0x809D4038u);
constexpr int kInvalidJpeg = static_cast<int>(0x809D4052u);
using Allocate = void* (APS5_VABI*)(std::size_t);
using Free = void (APS5_VABI*)(void*);

struct Config {
    Allocate allocate;
    JpegParser::ZeroAllocate zero;
    std::uintptr_t other;
    Free release;
    std::uintptr_t rest[7];
};
static_assert(sizeof(Config) == 0x58);

struct Parser {
    Config config{};
    void* context = nullptr;
    bool initializing = false;
    bool finalizePending = false;
};

std::mutex g_mutex;
Parser g_parser;

using JpegParser::GuestString;
static_assert(offsetof(JpegParserThumbnail, mappingSize) == 8 && offsetof(JpegParserThumbnail, format) == 0x20);
static_assert(offsetof(JpegParserThumbnail, width) == 0x28);

void CopyIn(void* destination, const void* source, std::size_t size) {
    const int saved = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, size);
    errno = saved;
    if (!copied) throw std::invalid_argument("JPEG thumbnail: unreadable guest object");
}

class JpegFile {
public:
    JpegFile(const char* path, std::int64_t size) : size(size), fd(sceKernelOpen(path, 0, 0)) {
        if (fd < 0) throw std::runtime_error("JPEG thumbnail: opening a file after stat failed");
    }
    ~JpegFile() { sceKernelClose(fd); }
    JpegFile(const JpegFile&) = delete;
    JpegFile& operator=(const JpegFile&) = delete;

    bool Read(std::int64_t pos, std::span<std::uint8_t> bytes) const {
        if (pos > size || bytes.size() > static_cast<std::uint64_t>(size - pos)) return false;
        while (!bytes.empty()) {
            const auto n = sceKernelPread(fd, bytes.data(), bytes.size(), pos);
            if (n < 0) throw std::runtime_error("JPEG thumbnail: file read failed");
            if (!n) return false;
            bytes = bytes.subspan(n);
            pos += n;
        }
        return true;
    }

    const std::int64_t size;
private:
    const int fd;
};

using Bytes = std::span<const std::uint8_t>;

std::uint32_t Integer(Bytes bytes, bool little) {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < bytes.size(); ++i)
        value = (value << 8) | bytes[little ? bytes.size() - i - 1 : i];
    return value;
}

bool Fits(Bytes data, std::size_t offset, std::size_t size) {
    return offset <= data.size() && size <= data.size() - offset;
}

Bytes ThumbnailData(Bytes tiff) {
    if (tiff.size() < 8 || tiff[0] != tiff[1] || (tiff[0] != 'I' && tiff[0] != 'M')) return {};
    const bool little = tiff[0] == 'I';
    auto read = [&](std::size_t offset, unsigned size) { return Integer(tiff.subspan(offset, size), little); };
    if (read(2, 2) != 42) return {};
    const std::size_t first = read(4, 4);
    if (!Fits(tiff, first, 2)) return {};
    const auto nextOffset = first + 2 + 12 * read(first, 2);
    if (!Fits(tiff, nextOffset, 4)) return {};
    const std::size_t second = read(nextOffset, 4);
    if (second == 0 || !Fits(tiff, second, 2)) return {};
    const auto count = read(second, 2);
    if (!Fits(tiff, second + 2, 12 * count)) return {};
    std::uint32_t start = 0, length = 0;
    for (unsigned i = 0; i < count; ++i) {
        const auto entry = second + 2 + 12 * i;
        if (read(entry + 2, 2) != 4 || read(entry + 4, 4) != 1) continue;
        const auto tag = read(entry, 2);
        if (tag == 0x201) start = read(entry + 8, 4);
        if (tag == 0x202) length = read(entry + 8, 4);
    }
    if (!start || !length || !Fits(tiff, start, length)) return {};
    return tiff.subspan(start, length);
}

int ReadJpeg(const JpegFile& file, JpegParser::ImageInfo& image) {
    std::array<std::uint8_t, 2> pair{};
    if (!file.Read(0, pair) || pair[0] != 0xff || pair[1] != 0xd8) return kInvalidJpeg;
    auto& app1 = image.exif;
    for (std::int64_t pos = 2; pos < file.size;) {
        if (!file.Read(pos, pair) || pair[0] != 0xff) return kInvalidJpeg;
        const auto marker = pair[1];
        if (marker == 0xff) { ++pos; continue; }
        pos += 2;
        if (marker == 0xda || marker == 0xd9) break;
        if (marker == 1 || (marker >= 0xd0 && marker <= 0xd7)) continue;
        if (!file.Read(pos, pair)) return kInvalidJpeg;
        const auto length = Integer(pair, false);
        if (length < 2 || length > file.size - pos) return kInvalidJpeg;
        if (marker == 0xe1 && app1.empty() && length >= 8) {
            std::array<std::uint8_t, 6> signature{};
            if (!file.Read(pos + 2, signature)) return kInvalidJpeg;
            if (std::memcmp(signature.data(), "Exif\0\0", 6) == 0) {
                app1.resize(length - 8);
                if (!file.Read(pos + 8, app1)) return kInvalidJpeg;
            }
        }
        if (marker == 0xe2 && length >= 6) {
            std::array<std::uint8_t, 4> id{};
            if (!file.Read(pos + 2, id)) return kInvalidJpeg;
            if (std::memcmp(id.data(), "MPF\0", 4) == 0) image.multiPicture = true;
        }
        if (marker >= 0xc0 && marker <= 0xcf && marker != 0xc4 && marker != 0xc8 && marker != 0xcc) {
            std::array<std::uint8_t, 6> fields{};
            if (length < 8 || !file.Read(pos + 2, fields) || !fields[5] || length != 8u + 3u * fields[5] ||
                !Integer(Bytes(fields).subspan(1, 2), false) || !Integer(Bytes(fields).subspan(3, 2), false))
                return kInvalidJpeg;
            image.height = Integer(Bytes(fields).subspan(1, 2), false);
            image.width = Integer(Bytes(fields).subspan(3, 2), false);
        }
        pos += length;
    }
    return image.width ? 0 : kInvalidJpeg;
}

int Extract(const char* path, JpegParserThumbnail* output) {
    JpegParser::ImageInfo image;
    const int result = JpegParser::ReadImage_nid_no_patch(path, image);
    if (result) return result;
    const auto bytes = ThumbnailData(image.exif);
    if (bytes.size() < 2 || bytes[0] != 0xff || bytes[1] != 0xd8) return kNoThumbnail;
    const auto info = Decoder::Jpeg::ParseHeader(bytes);
    if (!info) return kNoThumbnail;
    JpegParserThumbnail value{};
    value.type = 1;
    value.size = bytes.size();
    value.mappingSize = (bytes.size() + 0x3fff) & ~std::size_t{0x3fff};
    value.format = 0x20001;
    value.width = info->width;
    value.height = info->height;
    if (!GuestWritable_nid_no_patch(output, sizeof(*output)))
        throw std::invalid_argument("JPEG thumbnail: unwritable output");
    if (sceKernelMapNamedFlexibleMemory(&value.data, value.mappingSize, 3, 0, "SceMdrwFMem") != 0)
        throw std::runtime_error("JPEG thumbnail: flexible memory mapping failed");
    std::memcpy(value.data, bytes.data(), bytes.size());
    if (!GuestCopyOut_nid_no_patch(&output->type, &value.type, sizeof(value.type)) ||
        !GuestCopyOut_nid_no_patch(&output->mappingSize, &value.mappingSize, 0x1c) ||
        !GuestCopyOut_nid_no_patch(&output->width, &value.width, 0x10)) {
        sceKernelMunmap(reinterpret_cast<std::uintptr_t>(value.data), value.mappingSize);
        throw std::invalid_argument("JPEG thumbnail: unwritable output");
    }
    return 0;
}

template<class TOperation>
int WithPath(const void* pathObject, TOperation operation) {
    GuestString path{};
    CopyIn(&path, pathObject, sizeof(path));
    if (path.size > path.capacity || path.size == UINT64_MAX)
        throw std::invalid_argument("JPEG thumbnail: invalid guest string size");
    Parser state;
    {
        const std::lock_guard lock(g_mutex);
        state = g_parser;
    }
    if (!state.config.allocate || !state.config.release)
        throw std::logic_error("JPEG thumbnail: allocator has not been initialized");
    std::unique_ptr<void, Free> buffer(state.config.allocate(path.size + 1), state.config.release);
    if (!buffer) throw std::bad_alloc();
    char* text = static_cast<char*>(buffer.get());
    if (!GuestWritable_nid_no_patch(text, path.size + 1))
        throw std::invalid_argument("JPEG thumbnail: allocator returned unwritable storage");
    CopyIn(text, path.capacity < 16 ? path.bytes : path.pointer, path.size);
    text[path.size] = '\0';
    if (!state.context) return kParserError;
    return operation(text, state.config);
}

} // namespace

int JpegParser::ReadImage_nid_no_patch(const char* path, ImageInfo& image) {
    const int error = sceKernelStat(path, &image.stat);
    if (error == SCE_KERNEL_ERROR_ENOENT || error == SCE_KERNEL_ERROR_ENOTDIR || error == SCE_KERNEL_ERROR_EINVAL)
        return static_cast<int>(0x809D401Eu);
    if (error) return kParserError;
    const JpegFile file(path, image.stat.st_size);
    return ReadJpeg(file, image);
}

extern "C" {

int APS5_VABI libSceJpegParserReadMetadata_nid_no_patch(const void* path, void* request, void* output) {
    return WithPath(path, [&](const char* text, const Config& config) {
        return JpegParser::ReadFields_nid_no_patch(text, request, output, config.allocate, config.zero, config.release);
    });
}
APS5_EXPORT("cT6Gbjt5xYE", libSceJpegParserReadMetadata_nid_no_patch);

int APS5_VABI libSceJpegParserUnknown_v3aOHSEXfjo_nid_no_patch(const void* config) {
    Config callbacks{};
    {
        const std::lock_guard lock(g_mutex);
        if (g_parser.context || g_parser.initializing) return kParserError;
        CopyIn(&callbacks, config, sizeof(callbacks));
        if (!callbacks.allocate || !callbacks.release)
            throw std::invalid_argument("JPEG parser: missing allocator callback");
        g_parser.config = callbacks;
        g_parser.initializing = true;
    }
    void* context;
    try {
        context = callbacks.allocate(0x168);
    } catch (...) {
        const std::lock_guard lock(g_mutex);
        if (g_parser.finalizePending) g_parser = {};
        else g_parser.initializing = false;
        throw;
    }
    {
        const std::lock_guard lock(g_mutex);
        if (!g_parser.finalizePending) {
            g_parser.initializing = false;
            g_parser.context = context;
            return context ? 0 : static_cast<int>(0x809D4029u);
        }
        g_parser = {};
    }
    if (context) callbacks.release(context);
    return kParserError;
}
APS5_EXPORT("v3aOHSEXfjo", libSceJpegParserUnknown_v3aOHSEXfjo_nid_no_patch);

int APS5_VABI _ZN23sceMetadataReaderWriter10jpegParser18jpegParserFinalizeEv() {
    Parser saved;
    {
        const std::lock_guard lock(g_mutex);
        if (g_parser.initializing) {
            g_parser.finalizePending = true;
            return 0;
        }
        if (!g_parser.context) return 0;
        saved = g_parser;
        g_parser = {};
    }
    saved.config.release(saved.context);
    return 0;
}

int APS5_VABI libSceJpegParserUnknown_minus_0oLMyS3AcA_nid_no_patch(const void* pathObject, JpegParserThumbnail* output) {
    return WithPath(pathObject, [&](const char* text, const Config&) { return Extract(text, output); });
}
APS5_EXPORT("-0oLMyS3AcA", libSceJpegParserUnknown_minus_0oLMyS3AcA_nid_no_patch);

}
