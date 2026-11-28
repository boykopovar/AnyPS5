#include "prx/libScePngParser/include/WriteMetadata.hpp"
#include "prx/libSceSysmodule/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "Decoder/Png.hpp"
#include "WriterState.hpp"

#include <zlib.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using Byte = std::uint8_t;
using Bytes = std::span<const Byte>;
constexpr int kInvalidFile = static_cast<int>(0x809d4038u);
constexpr std::size_t kWorkSize = 4u << 20;
constexpr char kTempPath[] = "/user/av_contents/content_tmp/D9C31768-CC44-4E0A-B369-78E2B97F0A8E.png";
std::mutex g_writer;

struct GuestString {
    union {
        char bytes[16];
        const char* pointer;
    };
    std::uint64_t size;
    std::uint64_t capacity;
};

struct Value {
    std::int32_t type;
    std::int64_t size;
    const char* text;
};
static_assert(sizeof(GuestString) == 32 && sizeof(Value) == 24);

void CopyIn(void* dst, const void* src, std::size_t size) {
    if (!GuestCopyIn_nid_no_patch(dst, src, size)) throw std::invalid_argument("PNG writer: unreadable guest data");
}

std::string PathText(const void* guest) {
    GuestString value{};
    CopyIn(&value, guest, sizeof(value));
    if (value.size > value.capacity) throw std::invalid_argument("PNG writer: invalid path length");
    if (value.capacity < 16) return std::string(value.bytes, value.size);
    std::string text;
    const int error = value.size >= 1024 ? CopyGuestPath_nid_no_patch(value.pointer, text)
                                         : CopyGuestString_nid_no_patch(value.pointer, value.size + 1, text);
    if (error) throw std::invalid_argument("PNG writer: invalid guest path");
    return text;
}

using Getter = int (APS5_VABI*)(void*, const GuestString*, Value*);

std::string GetText(void* object, Getter get, std::string_view field) {
    GuestString key{};
    key.size = field.size();
    key.capacity = 15;
    std::memcpy(key.bytes, field.data(), key.size);
    Value value{2, 0, nullptr};
    const int result = get(object, &key, &value);
    if (result == 1) return {};
    if (result != 0) NotImplemented_nid_no_patch("PNG writer: unverified metadata lookup result");
    if (value.size == 0) return {};
    if (value.type != 2 || value.size < 0 || value.size > 0x7fffffff - 12)
        throw std::invalid_argument("PNG writer: invalid iTXt text");
    std::string text(static_cast<std::size_t>(value.size), '\0');
    CopyIn(text.data(), value.text, text.size());
    return text;
}

struct PngFile {
    int fd = -1;
    explicit PngFile(int descriptor = -1) : fd(descriptor) {}
    PngFile(const PngFile&) = delete;
    PngFile& operator=(const PngFile&) = delete;
    ~PngFile() { if (fd >= 0) sceKernelClose(fd); }

    void Close() {
        const int descriptor = fd;
        fd = -1;
        if (descriptor >= 0 && sceKernelClose(descriptor) != 0) throw std::runtime_error("PNG writer: close failed");
    }

    bool Read(std::span<Byte> bytes) const {
        while (!bytes.empty()) {
            const auto n = sceKernelRead(fd, bytes.data(), bytes.size());
            if (n < 0) throw std::runtime_error("PNG writer: read failed");
            if (!n) return false;
            bytes = bytes.subspan(n);
        }
        return true;
    }

    void Write(Bytes bytes) const {
        if (fd < 0) return;
        while (!bytes.empty()) {
            const auto n = sceKernelWrite(fd, bytes.data(), bytes.size());
            if (n <= 0) throw std::runtime_error("PNG writer: write failed");
            bytes = bytes.subspan(n);
        }
    }
};

struct Temporary {
    PngFile file;
    bool owned = false;

    ~Temporary() {
        if (!owned) return;
        if (file.fd >= 0) { sceKernelClose(file.fd); file.fd = -1; }
        sceKernelUnlink(kTempPath);
    }

    void Open() {
        file.fd = sceKernelOpen(kTempPath, SCE_KERNEL_O_RDWR | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_EXCL, 0600);
        if (file.fd < 0) throw std::runtime_error("PNG writer: cannot create the temporary file");
        owned = true;
    }

    void Commit(const char* path, unsigned mode) {
        if (fchmod_nid_postfix(file.fd, mode & 0777) != 0 || sceKernelFsync(file.fd) != 0)
            throw std::runtime_error("PNG writer: cannot finish the temporary file");
        file.Close();
        if (sceKernelRename(kTempPath, path) != 0) throw std::runtime_error("PNG writer: replacement failed");
        owned = false;
    }
};

struct Workspace {
    void* data = nullptr;
    ~Workspace() {
        if (data) sceKernelMunmap(reinterpret_cast<std::uintptr_t>(data), kWorkSize);
    }
    std::span<Byte> Map() {
        if (sceKernelMapNamedFlexibleMemory(&data, kWorkSize, 3, 0, "SceMdrwFMem") != 0)
            throw std::runtime_error("PNG writer: flexible workspace unavailable");
        return {static_cast<Byte*>(data), kWorkSize};
    }
};

std::array<Byte, 4> BigEndian(std::uint32_t value) {
    return {static_cast<Byte>(value >> 24), static_cast<Byte>(value >> 16), static_cast<Byte>(value >> 8), static_cast<Byte>(value)};
}

void WriteText(const PngFile& file, std::string_view keyword, std::string_view text) {
    if (text.empty()) return;
    const std::array<Byte, 4> type{'i', 'T', 'X', 't'};
    const std::array<Byte, 5> separators{};
    const Bytes key(reinterpret_cast<const Byte*>(keyword.data()), keyword.size());
    const Bytes payload(reinterpret_cast<const Byte*>(text.data()), text.size());
    file.Write(BigEndian(static_cast<std::uint32_t>(key.size() + separators.size() + payload.size())));
    unsigned long crc = 0;
    for (auto part : {Bytes(type), key, Bytes(separators), payload}) {
        file.Write(part);
        crc = crc32_z(crc, part.data(), part.size());
    }
    file.Write(BigEndian(static_cast<std::uint32_t>(crc)));
}

bool CopyChunks(const PngFile& source, const PngFile& output, std::span<Byte> scratch, bool title, bool comment) {
    bool image = false;
    std::array<Byte, 8> header{};
    while (source.Read(header)) {
        const std::uint32_t length = std::uint32_t{header[0]} << 24 | std::uint32_t{header[1]} << 16 |
                                     std::uint32_t{header[2]} << 8 | header[3];
        const std::string_view type(reinterpret_cast<const char*>(header.data() + 4), 4);
        if (length > 0x7fffffff || type == "IHDR" || (type == "IEND" && (length || !image))) return false;
        image |= type == "IDAT";
        std::array<Byte, 8> keyword{};
        const auto peek = type == "iTXt" ? std::min<std::size_t>(length, keyword.size()) : 0;
        if (!source.Read(std::span(keyword).first(peek))) return false;
        const bool replace = (title && peek >= 6 && std::memcmp(keyword.data(), "Title\0", 6) == 0) ||
                             (comment && peek == 8 && std::memcmp(keyword.data(), "Comment\0", 8) == 0);
        if (!replace) {
            output.Write(header);
            output.Write(Bytes(keyword).first(peek));
        }
        for (std::size_t left = length - peek + 4; left;) {
            auto part = scratch.first(std::min(left, scratch.size()));
            if (!source.Read(part)) return false;
            if (!replace) output.Write(part);
            left -= part.size();
        }
        if (type == "IEND") return true;
    }
    return false;
}

struct Rtc {
    Rtc() { sceSysmoduleLoadModuleInternal(0x80000020u); }
    ~Rtc() { sceSysmoduleUnloadModuleInternal(0x80000020u); }
};

} // namespace

extern "C" int APS5_VABI libScePngParserWriteMetadata_nid_no_patch(const void* path, void* metadata) {
    const Rtc rtc;
    const auto allocator = PngWriter::GetAllocator_nid_no_patch();
    const auto name = PathText(path);
    std::uintptr_t table{};
    CopyIn(&table, metadata, sizeof(table));
    Getter get{};
    if (!table || table > UINTPTR_MAX - 0x10) throw std::invalid_argument("PNG writer: invalid metadata table");
    CopyIn(&get, reinterpret_cast<const void*>(table + 0x10), sizeof(get));
    if (!get) throw std::invalid_argument("PNG writer: missing metadata getter");
    const auto title = GetText(metadata, get, "title");
    const auto comment = GetText(metadata, get, "comment");
    const std::lock_guard lock(g_writer);
    PngFile source(sceKernelOpen(name.c_str(), SCE_KERNEL_O_RDONLY, 0));
    if (source.fd == SCE_KERNEL_ERROR_ENOENT || source.fd == SCE_KERNEL_ERROR_ENOTDIR) return kInvalidFile;
    if (source.fd < 0) throw std::runtime_error("PNG writer: cannot open source");
    std::array<Byte, 33> header{};
    if (!source.Read(header) || !Decoder::Png::ParseHeader(header)) return kInvalidFile;
    const bool updating = !title.empty() || !comment.empty();
    Temporary output;
    Workspace workspace;
    std::unique_ptr<void, decltype(allocator.release)> small(nullptr, allocator.release);
    std::span<Byte> scratch;
    if (updating) {
        scratch = workspace.Map();
        output.Open();
        source.Close();
        source.fd = sceKernelOpen(name.c_str(), SCE_KERNEL_O_RDONLY, 0);
        if (source.fd < 0) throw std::runtime_error("PNG writer: source changed before update");
        if (!source.Read(header) || !Decoder::Png::ParseHeader(header)) return kInvalidFile;
        output.file.Write(header);
        WriteText(output.file, "Title", title);
        WriteText(output.file, "Comment", comment);
    } else {
        constexpr std::size_t size = 4096;
        small.reset(allocator.allocate(size));
        if (!small) throw std::bad_alloc();
        if (!GuestWritable_nid_no_patch(small.get(), size)) throw std::invalid_argument("PNG writer: invalid scratch allocation");
        scratch = {static_cast<Byte*>(small.get()), size};
    }
    if (!CopyChunks(source, output.file, scratch, !title.empty(), !comment.empty())) return kInvalidFile;
    if (updating) {
        while (true) {
            const auto n = sceKernelRead(source.fd, scratch.data(), scratch.size());
            if (n < 0) throw std::runtime_error("PNG writer: trailing data read failed");
            if (n == 0) break;
            output.file.Write(scratch.first(n));
        }
        FileStat status{};
        if (sceKernelFstat(source.fd, &status) != 0) throw std::runtime_error("PNG writer: file stat failed");
        source.Close();
        output.Commit(name.c_str(), status.st_mode);
    }
    return 0;
}
APS5_EXPORT("pLHjV7MzAKI", libScePngParserWriteMetadata_nid_no_patch);
