#include "SceTypes.hpp"
#include "Decoder/Png.hpp"
#include "Decoder/Jpeg.hpp"
#include "ContentCatalog.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_GIF
#define STBI_NO_STDIO
#include "stb_image.h"

namespace {

constexpr int Busy = static_cast<int>(0x809D3003);
constexpr int NoInit = static_cast<int>(0x809D3004);
constexpr int NoMemory = static_cast<int>(0x809D3006);
constexpr int FileNotFound = static_cast<int>(0x809D3011);
constexpr int UnsupportedFormat = static_cast<int>(0x809D3012);
constexpr int InvalidParam = static_cast<int>(0x809D3016);
constexpr int ExecutionMax = static_cast<int>(0x809D3018);
constexpr int UnsupportedThumbnail = static_cast<int>(0x809D3019);
constexpr int DataProvideError = static_cast<int>(0x809D301B);

struct Session { bool busy = false; };
struct State {
    std::mutex mutex;
    bool initialized = false;
    ContentExportInitParam2 allocator{};
    std::unordered_map<int, std::shared_ptr<Session>> sessions;
    int nextId = 1;
};

State& State_nid_no_patch() {
    static State state;
    return state;
}

class Operation_nid_no_patch {
public:
    explicit Operation_nid_no_patch(int id) {
        auto& state = State_nid_no_patch();
        std::lock_guard lock(state.mutex);
        if (!state.initialized) { result = NoInit; return; }
        const auto found = state.sessions.find(id);
        if (found == state.sessions.end()) throw std::invalid_argument("content export: invalid session");
        if (found->second->busy) { result = Busy; return; }
        session = found->second;
        allocator = state.allocator;
        session->busy = true;
    }
    ~Operation_nid_no_patch() {
        if (session) {
            auto& state = State_nid_no_patch();
            std::lock_guard lock(state.mutex);
            session->busy = false;
        }
    }
    Operation_nid_no_patch(const Operation_nid_no_patch&) = delete;
    Operation_nid_no_patch& operator=(const Operation_nid_no_patch&) = delete;
    int Result_nid_no_patch() const { return result; }
    const ContentExportInitParam2& Allocator_nid_no_patch() const { return allocator; }
private:
    std::shared_ptr<Session> session;
    ContentExportInitParam2 allocator{};
    int result = 0;
};

class Buffer_nid_no_patch {
public:
    explicit Buffer_nid_no_patch(const ContentExportInitParam2& param) : allocator(param) {
        size = allocator.buffer_size == 0 ? 64 * 1024 : allocator.buffer_size;
        if (size > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()))
            throw std::overflow_error("content export: allocator buffer too large");
        pointer = reinterpret_cast<ContentExportMalloc>(allocator.malloc_func)(size, allocator.user_data);
    }
    ~Buffer_nid_no_patch() {
        if (pointer) reinterpret_cast<ContentExportFree>(allocator.free_func)(pointer, allocator.user_data);
    }
    Buffer_nid_no_patch(const Buffer_nid_no_patch&) = delete;
    Buffer_nid_no_patch& operator=(const Buffer_nid_no_patch&) = delete;
    void* Data_nid_no_patch() const { return pointer; }
    std::size_t Size_nid_no_patch() const { return size; }
private:
    ContentExportInitParam2 allocator;
    void* pointer = nullptr;
    std::size_t size = 0;
};

struct MediaFormat {
    const char* mimeType;
    const char* extension;
    std::int32_t type;
};
constexpr std::array Formats{
    MediaFormat{"image/jpeg", "jpg", 1},
    MediaFormat{"image/png", "png", 1},
    MediaFormat{"image/gif", "gif", 1},
};

template<std::size_t TSize>
bool Terminated_nid_no_patch(const char (&value)[TSize]) {
    return std::memchr(value, 0, TSize) != nullptr;
}

const MediaFormat* Format_nid_no_patch(const ContentExportParam& param) {
    if (std::ranges::any_of(param.reserved, [](char value) { return value != 0; }))
        NotImplemented_nid_no_patch("content export: nonzero reserved/comment field");
    if (std::string_view(param.content_type).starts_with("video/"))
        NotImplemented_nid_no_patch("content export: video import semantics");
    const auto found = std::ranges::find_if(Formats, [&](const auto& format) {
        return std::strcmp(param.content_type, format.mimeType) == 0;
    });
    return found == Formats.end() ? nullptr : &*found;
}

int CopyFile_nid_no_patch(const char* source, const std::filesystem::path& destination,
                         const ContentExportInitParam2& allocator) {
    const auto native = ResolvePath_nid_no_patch(source);
    std::error_code error;
    if (!std::filesystem::exists(native, error)) {
        if (error) throw std::filesystem::filesystem_error("content export", native, error);
        return FileNotFound;
    }
    if (!std::filesystem::is_regular_file(native)) return UnsupportedFormat;
    std::ifstream input(native, std::ios::binary);
    if (!input) throw std::runtime_error("content export: source open failed");
    Buffer_nid_no_patch buffer(allocator);
    if (!buffer.Data_nid_no_patch()) return NoMemory;
    std::ofstream output(destination, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("content export: destination open failed");
    for (;;) {
        input.read(static_cast<char*>(buffer.Data_nid_no_patch()), static_cast<std::streamsize>(buffer.Size_nid_no_patch()));
        const auto size = input.gcount();
        if (size) output.write(static_cast<char*>(buffer.Data_nid_no_patch()), size);
        if (!output) throw std::runtime_error("content export: file write failed");
        if (input.eof()) break;
        if (!input) throw std::runtime_error("content export: file read failed");
    }
    output.close();
    if (!output) throw std::runtime_error("content export: file close failed");
    return 0;
}

int CopyData_nid_no_patch(const std::filesystem::path& destination, std::size_t expected,
                         ContentExportDataProvideFunction callback, void* userData,
                         const ContentExportInitParam2& allocator) {
    Buffer_nid_no_patch buffer(allocator);
    if (!buffer.Data_nid_no_patch()) return NoMemory;
    std::ofstream output(destination, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("content export: destination open failed");
    std::size_t written = 0;
    for (;;) {
        void* data = nullptr;
        std::size_t size = 0;
        const auto result = callback(&data, &size, userData);
        if ((result != 0 && result != 1) || size > expected - written || (size != 0 && !data)
            || (result == 0 && size == 0)) return DataProvideError;
        const auto* bytes = static_cast<const char*>(data);
        for (std::size_t offset = 0; offset < size;) {
            const auto count = std::min(buffer.Size_nid_no_patch(), size - offset);
            std::memcpy(buffer.Data_nid_no_patch(), bytes + offset, count);
            output.write(static_cast<char*>(buffer.Data_nid_no_patch()), static_cast<std::streamsize>(count));
            if (!output) throw std::runtime_error("content export: data write failed");
            offset += count;
        }
        written += size;
        if (result == 1) {
            if (written != expected) return DataProvideError;
            break;
        }
    }
    output.close();
    if (!output) throw std::runtime_error("content export: data close failed");
    return 0;
}

bool ReadMedia_nid_no_patch(const std::filesystem::path& path, const MediaFormat& format,
                          ContentCatalog_nid_no_patch::Entry& entry) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("content export: media open failed");
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    if (stream.bad()) throw std::runtime_error("content export: media read failed");
    if (std::string_view(format.mimeType) == "image/png") {
        const auto image = Decoder::Png::Decode(bytes);
        if (!image) return false;
        entry.width = image->width;
        entry.height = image->height;
    } else if (std::string_view(format.mimeType) == "image/jpeg") {
        const auto image = Decoder::Jpeg::Decode(bytes);
        if (!image) return false;
        entry.width = image->width;
        entry.height = image->height;
    } else {
        if (bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            NotImplemented_nid_no_patch("content export: GIF beyond decoder size limit");
        int width = 0;
        int height = 0;
        int channels = 0;
        const auto pixels = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width, &height, &channels, 4);
        if (!pixels) return false;
        stbi_image_free(pixels);
        entry.width = static_cast<std::uint32_t>(width);
        entry.height = static_cast<std::uint32_t>(height);
    }
    entry.hasDimensions = true;
    return entry.width > 0 && entry.height > 0;
}

template<typename TCopy>
int Export_nid_no_patch(const Operation_nid_no_patch& operation, const ContentExportParam& param,
                       const MediaFormat& format, char* path, std::size_t capacity,
                       const char* thumbnail, TCopy copy) {
    ContentCatalog_nid_no_patch::Transaction_nid_no_patch transaction;
    const auto filename = std::string("content.") + format.extension;
    const auto guest = transaction.GuestPath_nid_no_patch(filename);
    if (capacity <= guest.size()) return InvalidParam;
    const auto native = transaction.Directory_nid_no_patch() / filename;
    if (const auto result = copy(native); result != 0) return result;
    ContentCatalog_nid_no_patch::Entry entry;
    if (!ReadMedia_nid_no_patch(native, format, entry)) return UnsupportedFormat;
    const auto size = std::filesystem::file_size(native);
    if (size > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        throw std::overflow_error("content export: content too large");
    entry.id = transaction.Id_nid_no_patch();
    entry.title = param.title;
    entry.mimeType = format.mimeType;
    entry.path = guest;
    entry.createdTime = ContentCatalog_nid_no_patch::CurrentTick_nid_no_patch();
    entry.size = static_cast<std::int64_t>(size);
    entry.contentType = format.type;
    entry.accounts.fill(USER_SERVICE_USER_ID_INVALID);
    entry.accounts[0] = USER_SERVICE_INITIAL_USER_ID;
    if (thumbnail) {
        const auto thumbnailNative = transaction.Directory_nid_no_patch() / "thumbnail";
        if (const auto result = CopyFile_nid_no_patch(thumbnail, thumbnailNative, operation.Allocator_nid_no_patch()); result != 0)
            return result;
        bool recognized = false;
        for (const auto& thumbnailFormat : Formats) {
            if (thumbnailFormat.type != 1) continue;
            ContentCatalog_nid_no_patch::Entry ignored;
            if (!ReadMedia_nid_no_patch(thumbnailNative, thumbnailFormat, ignored)) continue;
            const auto name = std::string("thumbnail.") + thumbnailFormat.extension;
            std::filesystem::rename(thumbnailNative, transaction.Directory_nid_no_patch() / name);
            entry.iconPath = transaction.GuestPath_nid_no_patch(name);
            recognized = true;
            break;
        }
        if (!recognized) return UnsupportedThumbnail;
    }
    transaction.Commit_nid_no_patch(entry);
    std::memcpy(path, guest.c_str(), guest.size() + 1);
    return 0;
}

bool Parameters_nid_no_patch(const ContentExportParam* param, char* path, std::size_t capacity) {
    return param && path && capacity != 0 && Terminated_nid_no_patch(param->title)
        && Terminated_nid_no_patch(param->content_type);
}

}

extern "C" {

int APS5_VABI sceContentExportInit2(const ContentExportInitParam2* initParam) {
    if (!initParam) APS5_INVALID_ARG_EX;
    if (!initParam->malloc_func || !initParam->free_func) throw std::invalid_argument(std::string(__func__) + ": missing allocator functions");
    if (initParam->reserved0 != 0 || initParam->reserved1 != 0) throw std::invalid_argument(std::string(__func__) + ": reserved fields are not zero");
    if (initParam->buffer_size != 0 && initParam->buffer_size < 0x100) return InvalidParam;
    auto& state = State_nid_no_patch();
    std::lock_guard lock(state.mutex);
    if (state.initialized) throw std::logic_error(std::string(__func__) + ": already initialized");
    state.allocator = *initParam;
    state.initialized = true;
    return 0;
}

int APS5_VABI sceContentExportStart() {
    auto& state = State_nid_no_patch();
    std::lock_guard lock(state.mutex);
    if (!state.initialized) return NoInit;
    if (state.sessions.size() >= 10) return ExecutionMax;
    if (state.nextId == std::numeric_limits<int>::max()) throw std::overflow_error("content export: session ids exhausted");
    const int id = state.nextId++;
    state.sessions.emplace(id, std::make_shared<Session>());
    return id;
}

int APS5_VABI sceContentExportFinish(int expId) {
    auto& state = State_nid_no_patch();
    std::lock_guard lock(state.mutex);
    if (!state.initialized) return NoInit;
    const auto found = state.sessions.find(expId);
    if (found == state.sessions.end()) throw std::invalid_argument(std::string(__func__) + ": invalid session");
    if (found->second->busy) return Busy;
    state.sessions.erase(found);
    return 0;
}

int APS5_VABI sceContentExportFromData(int expId, const ContentExportParam* param,
    std::size_t contentLength, ContentExportDataProvideFunction func, void* userData, char* path, std::size_t pathBufferLength) {
    Operation_nid_no_patch operation(expId);
    if (operation.Result_nid_no_patch()) return operation.Result_nid_no_patch();
    if (!Parameters_nid_no_patch(param, path, pathBufferLength) || !func
        || contentLength > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max())) return InvalidParam;
    const auto* format = Format_nid_no_patch(*param);
    if (!format) return UnsupportedFormat;
    return Export_nid_no_patch(operation, *param, *format, path, pathBufferLength, nullptr,
        [&](const auto& native) {
            return CopyData_nid_no_patch(native, contentLength, func, userData, operation.Allocator_nid_no_patch());
        });
}

int APS5_VABI sceContentExportFromFile(int expId, const ContentExportParam* param,
    const char* srcPath, char* path, std::size_t pathBufferLength) {
    Operation_nid_no_patch operation(expId);
    if (operation.Result_nid_no_patch()) return operation.Result_nid_no_patch();
    if (!Parameters_nid_no_patch(param, path, pathBufferLength) || !srcPath || !*srcPath) return InvalidParam;
    const auto* format = Format_nid_no_patch(*param);
    if (!format) return UnsupportedFormat;
    return Export_nid_no_patch(operation, *param, *format, path, pathBufferLength, nullptr,
        [&](const auto& native) { return CopyFile_nid_no_patch(srcPath, native, operation.Allocator_nid_no_patch()); });
}

int APS5_VABI sceContentExportFromFileWithThumbnail(int expId, const ContentExportParam* param,
    const char* srcPath, const char* thumbnailPath, char* path, std::size_t pathBufferLength) {
    Operation_nid_no_patch operation(expId);
    if (operation.Result_nid_no_patch()) return operation.Result_nid_no_patch();
    if (!Parameters_nid_no_patch(param, path, pathBufferLength) || !srcPath || !*srcPath
        || !thumbnailPath || !*thumbnailPath) return InvalidParam;
    const auto* format = Format_nid_no_patch(*param);
    if (!format) return UnsupportedFormat;
    return Export_nid_no_patch(operation, *param, *format, path, pathBufferLength, thumbnailPath,
        [&](const auto& native) { return CopyFile_nid_no_patch(srcPath, native, operation.Allocator_nid_no_patch()); });
}

int APS5_VABI sceContentExportTerm() {
    auto& state = State_nid_no_patch();
    std::lock_guard lock(state.mutex);
    if (!state.initialized) throw std::logic_error(std::string(__func__) + ": not initialized");
    if (!state.sessions.empty()) return Busy;
    state.allocator = {};
    state.initialized = false;
    return 0;
}

}
