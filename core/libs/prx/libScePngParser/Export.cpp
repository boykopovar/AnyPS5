#include "WriterState.hpp"
#include <cstring>
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libScePngParser/include/InventoryExports.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libSceRtc/include/Rtc.hpp"
#include "prx/libSceSysmodule/include/Api.hpp"
#include <array>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr int kResult809D4019 = static_cast<int>(0x809D4019u);
constexpr int kResult809D401E = static_cast<int>(0x809D401Eu);
constexpr int kResult809D4038 = static_cast<int>(0x809D4038u);
constexpr int kResult809D4054 = static_cast<int>(0x809D4054u);
constexpr int kStatNotFound = static_cast<int>(0x80020002u);
constexpr std::uint32_t kRtcModule = 0x80000020u;
constexpr std::uint32_t kReadDone = 0x2000Bu;
constexpr std::size_t kConfigSize = 0x58;
constexpr const char* kReadName = "libScePngParserUnknown_MBQfjtzRyh0_nid_no_patch";

std::mutex g_configMutex;
std::array<std::byte, kConfigSize> g_config{};
std::array<std::byte, kConfigSize> g_configCopy{};

struct GuestString {
    union {
        char inline_[16];
        const char* data;
    };
    std::uint64_t size;
    std::uint64_t capacity;
};
static_assert(sizeof(GuestString) == 0x20);
static_assert(offsetof(GuestString, size) == 0x10);
static_assert(offsetof(GuestString, capacity) == 0x18);
constexpr std::uint64_t kInlineCapacity = 15;

struct MetadataValue {
    std::int32_t type;
    std::int64_t size;
    std::int64_t payload;
};
static_assert(sizeof(MetadataValue) == 0x18);
static_assert(offsetof(MetadataValue, type) == 0x00);
static_assert(offsetof(MetadataValue, size) == 0x08);
static_assert(offsetof(MetadataValue, payload) == 0x10);
constexpr std::int32_t kValueNumber = 0;
constexpr std::int32_t kValueText = 2;

using MetadataSetField = int(APS5_VABI*)(void* self, const GuestString* key, const MetadataValue* value);
using MetadataFieldCount = int(APS5_VABI*)(void* self);
constexpr std::size_t kSetFieldSlot = 0x18;
constexpr std::size_t kFieldCountSlot = 0x50;
constexpr std::size_t kOutputStatus = 0x0c;

constexpr std::string_view kPngSignature("\x89PNG\r\n\x1a\n", 8);
constexpr std::size_t kIhdrEnd = 8 + 8 + 13 + 4;

bool CopyIn(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return copied;
}

template <typename Function>
Function ReadVirtual(void* object, std::size_t slot) {
    std::uintptr_t table = 0;
    if (!CopyIn(&table, object, sizeof(table)))
        throw std::invalid_argument("MBQfjtzRyh0: unreadable Metadata object");
    Function function = nullptr;
    if (table == 0 || !CopyIn(&function, reinterpret_cast<const void*>(table + slot), sizeof(function)) || function == nullptr)
        throw std::invalid_argument("MBQfjtzRyh0: unreadable Metadata virtual table");
    return function;
}

std::string ReadGuestString(const void* object) {
    GuestString header{};
    if (!CopyIn(&header, object, sizeof(header)))
        throw std::invalid_argument("MBQfjtzRyh0: unreadable path");
    if (header.capacity <= kInlineCapacity) {
        if (header.size > header.capacity)
            throw std::invalid_argument("MBQfjtzRyh0: path longer than its inline capacity");
        return std::string(header.inline_, header.size);
    }
    if (header.data == nullptr)
        NotImplemented_nid_no_patch(kReadName);
    std::string text(header.size, '\0');
    if (!CopyIn(text.data(), header.data, text.size()))
        throw std::invalid_argument("MBQfjtzRyh0: unreadable path text");
    return text;
}

GuestString MakeKey(std::string_view key) {
    GuestString result{};
    result.size = key.size();
    if (key.size() <= kInlineCapacity) {
        key.copy(result.inline_, key.size());
        result.capacity = kInlineCapacity;
    } else {
        result.data = key.data();
        result.capacity = key.size();
    }
    return result;
}

std::uint32_t ReadBigEndian32(std::string_view bytes, std::size_t offset) {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 4; ++i)
        value = value << 8 | static_cast<std::uint8_t>(bytes[offset + i]);
    return value;
}

struct PngChunk {
    std::string_view type;
    std::string_view data;
};

std::optional<std::vector<PngChunk>> ReadChunks(std::string_view file) {
    if (file.size() < kIhdrEnd || file.substr(0, kPngSignature.size()) != kPngSignature)
        return std::nullopt;
    std::vector<PngChunk> chunks;
    std::size_t offset = kPngSignature.size();
    while (offset < file.size()) {
        if (file.size() - offset < 12)
            NotImplemented_nid_no_patch(kReadName);
        const std::uint32_t length = ReadBigEndian32(file, offset);
        if (length > 0x7FFFFFFFu || file.size() - offset - 12 < length)
            NotImplemented_nid_no_patch(kReadName);
        const PngChunk chunk{file.substr(offset + 4, 4), file.substr(offset + 8, length)};
        if (chunks.empty() && (chunk.type != "IHDR" || chunk.data.size() != 13))
            NotImplemented_nid_no_patch(kReadName);
        chunks.push_back(chunk);
        if (chunk.type == "IEND")
            break;
        offset += 12 + static_cast<std::size_t>(length);
    }
    return chunks;
}

std::optional<std::string> FindText(const std::vector<PngChunk>& chunks, bool international, std::string_view keyword) {
    const std::string_view type = international ? "iTXt" : "tEXt";
    for (const PngChunk& chunk : chunks) {
        if (chunk.type != type)
            continue;
        if (chunk.data.size() <= keyword.size())
            return std::nullopt;
        if (chunk.data.substr(0, keyword.size()) != keyword)
            continue;
        const std::size_t start = keyword.size() + (international ? 5 : 1);
        if (start > chunk.data.size())
            NotImplemented_nid_no_patch(kReadName);
        const std::string_view text = chunk.data.substr(start);
        return std::string(text.substr(0, text.find('\0')));
    }
    return std::nullopt;
}

std::optional<std::int64_t> CreationTime(const std::string& text) {
    RtcTick tick{};
    RtcDateTime time{};
    std::int64_t seconds = 0;
    const int savedErrno = errno;
    const bool converted = sceRtcParseDateTime(&tick, text.c_str()) == 0 && sceRtcSetTick(&time, &tick) == 0 &&
                           sceRtcGetTime_t(&time, &seconds) == 0;
    errno = savedErrno;
    if (!converted)
        return std::nullopt;
    return seconds;
}

std::optional<std::int64_t> ModificationTime(std::string_view chunk) {
    if (chunk.size() != 7)
        NotImplemented_nid_no_patch(kReadName);
    RtcDateTime time{};
    time.year = static_cast<std::uint16_t>(static_cast<std::uint8_t>(chunk[0]) << 8 | static_cast<std::uint8_t>(chunk[1]));
    time.month = static_cast<std::uint8_t>(chunk[2]);
    time.day = static_cast<std::uint8_t>(chunk[3]);
    time.hour = static_cast<std::uint8_t>(chunk[4]);
    time.minute = static_cast<std::uint8_t>(chunk[5]);
    time.second = static_cast<std::uint8_t>(chunk[6]);
    std::int64_t seconds = 0;
    const int savedErrno = errno;
    const int result = sceRtcGetTime_t(&time, &seconds);
    errno = savedErrno;
    if (result != 0)
        return std::nullopt;
    return seconds;
}

std::optional<std::string> FileNameStem(const std::string& path) {
    const std::size_t slash = path.rfind('/');
    const std::size_t dot = path.rfind('.');
    if (slash == std::string::npos || dot == std::string::npos)
        return std::nullopt;
    const std::size_t end = dot > slash ? dot : path.size();
    return path.substr(slash + 1, end - slash - 1);
}

std::string ReadWholeFile(const std::string& path) {
    const int savedErrno = errno;
    const int descriptor = sceKernelOpen(path.c_str(), 0, 0);
    if (descriptor < 0) {
        errno = savedErrno;
        NotImplemented_nid_no_patch(kReadName);
    }
    std::string file;
    std::vector<char> buffer(0x10000);
    for (;;) {
        const std::int64_t read = sceKernelRead(descriptor, buffer.data(), buffer.size());
        if (read < 0) {
            static_cast<void>(sceKernelClose(descriptor));
            errno = savedErrno;
            NotImplemented_nid_no_patch(kReadName);
        }
        if (read == 0)
            break;
        file.append(buffer.data(), static_cast<std::size_t>(read));
    }
    static_cast<void>(sceKernelClose(descriptor));
    errno = savedErrno;
    return file;
}

class RtcModuleUse {
public:
    RtcModuleUse() {
        const int savedErrno = errno;
        const int result = sceSysmoduleLoadModuleInternal(kRtcModule);
        errno = savedErrno;
        if (result != 0)
            NotImplemented_nid_no_patch(kReadName);
    }
    ~RtcModuleUse() {
        const int savedErrno = errno;
        static_cast<void>(sceSysmoduleUnloadModuleInternal(kRtcModule));
        errno = savedErrno;
    }
    RtcModuleUse(const RtcModuleUse&) = delete;
    RtcModuleUse& operator=(const RtcModuleUse&) = delete;
};

struct ReadField {
    std::string_view key;
    std::int32_t type;
    std::int64_t number;
    std::string text;
};

class ReadFields {
public:
    void Text(std::string_view key, const std::optional<std::string>& value) {
        if (value)
            _fields.push_back({key, kValueText, 0, *value});
    }
    void Number(std::string_view key, const std::optional<std::int64_t>& value) {
        if (value)
            _fields.push_back({key, kValueNumber, *value, {}});
    }
    const std::vector<ReadField>& Fields() const { return _fields; }

private:
    std::vector<ReadField> _fields;
};

int ReadPngMetadata(const std::string& path, ReadFields& fields) {
    FileStat status{};
    const int savedErrno = errno;
    const int statResult = sceKernelStat(path.c_str(), &status);
    errno = savedErrno;
    if (statResult == kStatNotFound)
        return kResult809D401E;
    if (statResult != 0)
        NotImplemented_nid_no_patch(kReadName);
    const std::string file = ReadWholeFile(path);
    const std::optional<std::vector<PngChunk>> chunks = ReadChunks(file);
    if (!chunks)
        return kResult809D4038;

    const std::optional<std::string> embeddedTitle = FindText(*chunks, true, "Title");
    std::optional<std::int64_t> createdTime = status.st_birthtim.tv_sec;
    if (const std::optional<std::string> creation = FindText(*chunks, false, "Creation Time"))
        createdTime = CreationTime(*creation);
    std::optional<std::int64_t> updatedTime = status.st_ctim.tv_sec;
    for (const PngChunk& chunk : *chunks) {
        if (chunk.type == "tIME") {
            updatedTime = ModificationTime(chunk.data);
            break;
        }
    }
    std::optional<std::string> extension;
    if (const std::size_t dot = path.rfind('.'); dot != std::string::npos)
        extension = path.substr(dot + 1);
    const std::optional<std::string> displayType = FindText(*chunks, false, "Display Type");
    const std::string_view header = chunks->front().data;

    fields.Text("title", embeddedTitle ? embeddedTitle : FileNameStem(path));
    fields.Text("embedded_title", embeddedTitle);
    fields.Number("width", ReadBigEndian32(header, 0));
    fields.Number("height", ReadBigEndian32(header, 4));
    fields.Number("created_time", createdTime);
    fields.Number("birth_time", status.st_birthtim.tv_sec);
    fields.Number("last_updated_time", updatedTime);
    fields.Number("imported_time", status.st_birthtim.tv_sec);
    fields.Number("size", status.st_size);
    fields.Text("content_path", path);
    fields.Text("content_path_ext", extension);
    fields.Text("appli_title", FindText(*chunks, true, "Game Title"));
    fields.Text("appli_title_id", FindText(*chunks, false, "Title ID"));
    fields.Text("comment", FindText(*chunks, true, "Comment"));
    if (displayType == "VRHMD")
        fields.Number("vrhmd", 1);
    fields.Number("orientation_type", 1);
    fields.Number("duration", 0);
    fields.Number("duration_ms", 0);
    return 0;
}

} // namespace

extern "C" {

APS5_DUMMY_FUN

}

extern "C" {

int APS5_VABI _ZN23sceMetadataReaderWriter9pngParser17pngParserFinalizeEv() {
    const std::lock_guard<std::mutex> lock(g_configMutex);
    g_config.fill(std::byte{0});
    return 0;
}

int APS5_VABI libScePngParserUnknown_vJpgNFIBlCk_nid_no_patch(const void* config) {
    if (config == nullptr)
        throw std::invalid_argument("vJpgNFIBlCk: unreadable configuration");
    std::array<std::byte, kConfigSize> copy{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(copy.data(), config, copy.size());
    errno = savedErrno;
    if (!copied)
        throw std::invalid_argument("vJpgNFIBlCk: unreadable configuration");
    const std::lock_guard<std::mutex> lock(g_configMutex);
    g_config = copy;
    g_configCopy = copy;
    return 0;
}
APS5_EXPORT("vJpgNFIBlCk", libScePngParserUnknown_vJpgNFIBlCk_nid_no_patch);

int APS5_VABI libScePngParserUnknown_0Imz9zU_minus_Wgk_nid_no_patch() {
    return kResult809D4019;
}
APS5_EXPORT("0Imz9zU-Wgk", libScePngParserUnknown_0Imz9zU_minus_Wgk_nid_no_patch);

int APS5_VABI libScePngParserUnknown_5XfcHLzGE6E_nid_no_patch() {
    return kResult809D4054;
}
APS5_EXPORT("5XfcHLzGE6E", libScePngParserUnknown_5XfcHLzGE6E_nid_no_patch);

int APS5_VABI libScePngParserUnknown_MBQfjtzRyh0_nid_no_patch(const void* path, void* request, void* output) {
    if (path == nullptr || request == nullptr || output == nullptr)
        throw std::invalid_argument("MBQfjtzRyh0: null argument");
    {
        const std::lock_guard<std::mutex> lock(g_configMutex);
        std::uintptr_t allocate = 0;
        std::memcpy(&allocate, g_config.data(), sizeof(allocate));
        if (allocate == 0)
            throw std::logic_error("MBQfjtzRyh0: no allocator configured through vJpgNFIBlCk");
    }
    const std::string pathText = ReadGuestString(path);
    const RtcModuleUse rtc;
    if (ReadVirtual<MetadataFieldCount>(request, kFieldCountSlot)(request) > 0)
        NotImplemented_nid_no_patch(__func__);
    ReadFields fields;
    if (const int result = ReadPngMetadata(pathText, fields); result != 0)
        return result;
    const MetadataSetField setField = ReadVirtual<MetadataSetField>(output, kSetFieldSlot);
    for (const ReadField& field : fields.Fields()) {
        const GuestString key = MakeKey(field.key);
        MetadataValue value{field.type, 8, field.number};
        if (field.type == kValueText) {
            value.size = static_cast<std::int64_t>(field.text.size());
            value.payload = static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(field.text.c_str()));
        }
        static_cast<void>(setField(output, &key, &value));
    }
    const int savedErrno = errno;
    const bool written = GuestCopyOut_nid_no_patch(static_cast<std::byte*>(output) + kOutputStatus, &kReadDone, sizeof(kReadDone));
    errno = savedErrno;
    if (!written)
        throw std::invalid_argument("MBQfjtzRyh0: unwritable output Metadata");
    return 0;
}
APS5_EXPORT("MBQfjtzRyh0", libScePngParserUnknown_MBQfjtzRyh0_nid_no_patch);

int APS5_VABI _ZN23sceMetadataReaderWriter9pngParser19pngParserInitializeEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN23sceMetadataReaderWriter9pngParser21pngParserReadMetadataERKSsRKNS_8MetadataERS3_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN23sceMetadataReaderWriter9pngParser22pngParserWriteMetadataERKSsRKNS_8MetadataE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libScePngParserUnknown_G6G_minus_KRVQEwI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("G6G-KRVQEwI", libScePngParserUnknown_G6G_minus_KRVQEwI_nid_no_patch);

int APS5_VABI libScePngParserUnknown_ji_plus_m7tJEFUk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ji+m7tJEFUk", libScePngParserUnknown_ji_plus_m7tJEFUk_nid_no_patch);

int APS5_VABI _ZN23sceMetadataReaderWriter9pngParser25pngParserExtractThumbnailERKSsRNS_9ThumbnailE() {
    NotImplemented_nid_no_patch(__func__);
}

}

PngWriter::Allocator PngWriter::GetAllocator_nid_no_patch() {
    const std::lock_guard lock(g_configMutex);
    Allocator allocator{};
    std::memcpy(&allocator.allocate, g_config.data(), sizeof(allocator.allocate));
    std::memcpy(&allocator.release, g_config.data() + 0x18, sizeof(allocator.release));
    if (!allocator.allocate || !allocator.release) throw std::logic_error("PNG writer: missing configured allocator");
    return allocator;
}
