#include "prx/libSceContentExport/include/Api.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <atomic>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
constexpr std::int32_t MaxExecution = 10;
constexpr std::uint32_t ContentTypeSdkVersion = 0x02500000;
constexpr std::uint32_t MemoryPoolSdkVersion = 0x04000000;
constexpr std::uint32_t ThumbnailTypeSdkVersion = 0x04000000;
constexpr std::uint32_t ProgressSdkVersion = 0x06000000;
constexpr std::uint32_t MaximumValidatedContents = 0x100;
constexpr std::size_t MaximumPathLength = 0x400;
constexpr std::size_t MaximumTitleIdListLength = 15000;
constexpr std::uint32_t FileCheckSdkVersion = 0x05500000;
constexpr std::size_t MaximumCheckedPathLength = 0x10000;
constexpr std::size_t MaximumTitleIds = 1000;
constexpr std::size_t TitleIdLength = 9;
constexpr std::size_t MaximumJsonDepth = 1000;
constexpr int KernelErrorNoEntry = static_cast<int>(0x80020002u);
constexpr int ErrorNoFile = static_cast<int>(0x809D3011u);
constexpr int ErrorFileInUse = static_cast<int>(0x809D301Au);
constexpr int ErrorReservedName = static_cast<int>(0x809D301Cu);
constexpr std::string_view ThumbnailType = "image/jpeg";
constexpr int ErrorGeneric = static_cast<int>(0x809D3001u);
constexpr int ErrorInvalidState = static_cast<int>(0x809D3002u);
constexpr int ErrorBusy = static_cast<int>(0x809D3003u);
constexpr int ErrorNotInitialized = static_cast<int>(0x809D3004u);
constexpr int ErrorNoMemory = static_cast<int>(0x809D3006u);
constexpr int ErrorInvalidContentType = static_cast<int>(0x809D3012u);
constexpr int ErrorInvalidArgument = static_cast<int>(0x809D3016u);
constexpr int ErrorStorage = static_cast<int>(0x809D3017u);
constexpr int ErrorExecutionMax = static_cast<int>(0x809D3018u);
constexpr int ErrorThumbnailType = static_cast<int>(0x809D3019u);
constexpr int ErrorDataCallback = static_cast<int>(0x809D301Bu);
enum class SessionState { Idle, Ended, AfterFromData };
std::atomic<bool> g_initialized{false};
std::mutex g_sessionMutex;
std::map<std::int32_t, SessionState> g_sessions;

bool NotInitializedForCaller() {
    return ApplicationCompiledSdkVersion() >= MemoryPoolSdkVersion && !g_initialized.load();
}

bool SessionIdInRange(std::int32_t expId) {
    return static_cast<std::uint32_t>(expId) < static_cast<std::uint32_t>(MaxExecution);
}

bool ExportContentType(const char* contentType) {
    constexpr std::array<std::string_view, 5> types{"video/mp4", "image/jpeg", "image/png", "image/gif", "video/webm"};
    return std::find(types.begin(), types.end(), contentType) != types.end();
}

int CopyExportParam(const ContentExportParam* param, ContentExportParam& localParam) {
    if (!GuestCopyIn_nid_no_patch(&localParam, param, sizeof(localParam))) return ErrorInvalidArgument;
    if (std::any_of(std::begin(localParam.comment), std::end(localParam.comment), [](char value) { return value != '\0'; })) return ErrorInvalidArgument;
    localParam.content_type[sizeof(localParam.content_type) - 1] = '\0';
    return 0;
}

struct FileIdentity {
    std::uint32_t device = 0;
    std::uint32_t inode = 0;
    bool operator==(const FileIdentity&) const = default;
};

int OpenedFileIdentity(const std::string& path, FileIdentity& identity) {
    const int descriptor = sceKernelOpen(path.c_str(), 0, 0);
    if (descriptor == KernelErrorNoEntry) return ErrorNoFile;
    if (descriptor < 0) return descriptor;
    FileStat status{};
    const int result = sceKernelFstat(descriptor, &status);
    sceKernelClose(descriptor);
    if (result < 0) return result;
    identity = {status.st_dev, status.st_ino};
    return 0;
}

bool ReservedFileName(std::string_view path) {
    const auto slash = path.rfind('/');
    return path.substr(slash == std::string_view::npos ? 0 : slash + 1).starts_with("sce_");
}

int CheckExportFiles(const char* sourcePath, const std::string& thumbnail) {
    std::string source;
    const int copied = CopyGuestString_nid_no_patch(sourcePath, MaximumCheckedPathLength + 1, source);
    if (copied == 63) NotImplemented_nid_no_patch("sceContentExportFromFileWithTitleIdList: a source path of 0x10000 bytes or more");
    if (copied != 0) return ErrorInvalidArgument;
    FileIdentity sourceIdentity{};
    if (!source.empty()) {
        if (const int error = OpenedFileIdentity(source, sourceIdentity)) return error;
    }
    FileIdentity thumbnailIdentity{};
    if (const int error = OpenedFileIdentity(thumbnail, thumbnailIdentity)) return error;
    if (!source.empty() && sourceIdentity == thumbnailIdentity) return ErrorFileInUse;
    if (ReservedFileName(source) || ReservedFileName(thumbnail)) return ErrorReservedName;
    return 0;
}

enum class JsonKind { Object, Array, String, Number, Literal };

struct JsonElement {
    JsonKind kind = JsonKind::Literal;
    std::size_t length = 0;
    bool plain = true;
};

class JsonDocument {
public:
    explicit JsonDocument(std::string_view text) : text(text) {}

    bool Read(JsonKind& top, std::vector<JsonElement>& elements) {
        Space();
        if (Peek() == '[') {
            top = JsonKind::Array;
            if (!Array(0, &elements)) return false;
        } else {
            JsonElement element;
            if (!Value(0, element)) return false;
            top = element.kind;
        }
        Space();
        return position == text.size();
    }

private:
    char Peek() const { return position < text.size() ? text[position] : '\0'; }

    void Space() {
        while (position < text.size() && (text[position] == ' ' || text[position] == '\t' || text[position] == '\n' || text[position] == '\r')) ++position;
    }

    bool Take(char expected) {
        Space();
        if (Peek() != expected) return false;
        ++position;
        return true;
    }

    bool Digits() {
        const std::size_t start = position;
        while (Peek() >= '0' && Peek() <= '9') ++position;
        return position > start;
    }

    bool Value(std::size_t depth, JsonElement& element) {
        if (depth >= MaximumJsonDepth) NotImplemented_nid_no_patch("sceContentExportFromFileWithTitleIdList: a title-id list nested 1000 deep");
        Space();
        const char next = Peek();
        if (next == '{') {
            element.kind = JsonKind::Object;
            return Object(depth + 1);
        }
        if (next == '[') {
            element.kind = JsonKind::Array;
            return Array(depth + 1, nullptr);
        }
        if (next == '"') {
            element.kind = JsonKind::String;
            return String(element);
        }
        if (next == '-' || (next >= '0' && next <= '9')) {
            element.kind = JsonKind::Number;
            return Number();
        }
        element.kind = JsonKind::Literal;
        for (const std::string_view literal : {"true", "false", "null"}) {
            if (text.substr(position, literal.size()) == literal) {
                position += literal.size();
                return true;
            }
        }
        return false;
    }

    bool Number() {
        if (Peek() == '-') ++position;
        if (Peek() == '0') ++position;
        else if (!Digits()) return false;
        if (Peek() == '.') {
            ++position;
            if (!Digits()) return false;
        }
        if (Peek() == 'e' || Peek() == 'E') {
            ++position;
            if (Peek() == '+' || Peek() == '-') ++position;
            if (!Digits()) return false;
        }
        return true;
    }

    bool String(JsonElement& element) {
        ++position;
        while (position < text.size()) {
            const auto character = static_cast<unsigned char>(text[position++]);
            if (character == '"') return true;
            if (character < 0x20) return false;
            if (character >= 0x80) element.plain = false;
            if (character == '\\') {
                element.plain = false;
                const char escape = Peek();
                ++position;
                if (escape == 'u') {
                    for (int index = 0; index < 4; ++index, ++position) {
                        const char digit = Peek();
                        if (!((digit >= '0' && digit <= '9') || (digit >= 'a' && digit <= 'f') || (digit >= 'A' && digit <= 'F'))) return false;
                    }
                } else if (std::string_view("\"\\/bfnrt").find(escape) == std::string_view::npos || escape == '\0') {
                    return false;
                }
            }
            ++element.length;
        }
        return false;
    }

    bool Array(std::size_t depth, std::vector<JsonElement>* elements) {
        ++position;
        if (Take(']')) return true;
        for (;;) {
            JsonElement element;
            if (!Value(depth, element)) return false;
            if (elements != nullptr) elements->push_back(element);
            if (Take(']')) return true;
            if (!Take(',')) return false;
        }
    }

    bool Object(std::size_t depth) {
        ++position;
        if (Take('}')) return true;
        for (;;) {
            Space();
            JsonElement key;
            if (Peek() != '"' || !String(key) || !Take(':')) return false;
            JsonElement value;
            if (!Value(depth, value)) return false;
            if (Take('}')) return true;
            if (!Take(',')) return false;
        }
    }

    std::string_view text;
    std::size_t position = 0;
};

int CheckTitleIdList(std::string_view list) {
    JsonKind top = JsonKind::Literal;
    std::vector<JsonElement> elements;
    if (!JsonDocument(list).Read(top, elements)) NotImplemented_nid_no_patch("sceContentExportFromFileWithTitleIdList: a title-id list that is not valid JSON");
    if (top != JsonKind::Array || elements.size() > MaximumTitleIds) return ErrorInvalidArgument;
    for (const JsonElement& element : elements) {
        if (element.kind != JsonKind::String || !element.plain || element.length == TitleIdLength + 2)
            NotImplemented_nid_no_patch("sceContentExportFromFileWithTitleIdList: a title-id element whose rendered length is unsettled");
        if (element.length != TitleIdLength) return ErrorInvalidArgument;
    }
    return 0;
}

void RequireSession(const char* function, std::int32_t expId) {
    if (!g_initialized.load()) throw std::logic_error(std::string(function) + ": not initialized");
    if (!g_sessions.contains(expId)) throw std::invalid_argument(std::string(function) + ": unknown export session");
}

bool SupportedContentType(const char* contentType, std::int32_t sdkVersion) {
    if (sdkVersion < static_cast<std::int32_t>(ContentTypeSdkVersion)) return true;
    return std::strcmp(contentType, "video/mp4") == 0 || std::strcmp(contentType, "image/jpeg") == 0 ||
        std::strcmp(contentType, "image/png") == 0 || std::strcmp(contentType, "image/gif") == 0;
}
}

extern "C" {

int APS5_VABI sceContentExportInit2(const ContentExportInitParam2* init_param) {
    if (init_param == nullptr) APS5_INVALID_ARG_EX;
    ContentExportInitParam2 localParam{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&localParam, init_param, sizeof(localParam));
    errno = savedErrno;
    if (!copied) APS5_INVALID_ARG_EX;
    if (localParam.malloc_func == nullptr || localParam.free_func == nullptr) throw std::invalid_argument(std::string(__func__) + ": missing allocator functions");
    if (localParam.reserved0 != 0 || localParam.reserved1 != 0) throw std::invalid_argument(std::string(__func__) + ": reserved fields are not zero");
    bool expected = false;
    if (!g_initialized.compare_exchange_strong(expected, true)) throw std::logic_error(std::string(__func__) + ": already initialized");
    return 0;
}

int APS5_VABI sceContentExportFinish(std::int32_t expId) {
    std::lock_guard lock(g_sessionMutex);
    RequireSession(__func__, expId);
    g_sessions.erase(expId);
    return 0;
}

int APS5_VABI sceContentExportFromData(std::int32_t expId, const ContentExportParam* param, std::size_t contentLength, ContentExportDataCallback callback, void* userData, char* path, std::size_t pathBufferLength) {
    const std::int32_t sdkVersion = static_cast<std::int32_t>(ApplicationCompiledSdkVersion());
    if (sdkVersion >= static_cast<std::int32_t>(MemoryPoolSdkVersion) && !g_initialized.load()) return ErrorNotInitialized;
    if (expId < 0 || expId >= MaxExecution || param == nullptr || callback == nullptr) return ErrorInvalidArgument;
    ContentExportParam localParam{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&localParam, param, sizeof(localParam));
    errno = savedErrno;
    if (!copied) return ErrorInvalidArgument;
    {
        std::lock_guard lock(g_sessionMutex);
        if (!g_sessions.contains(expId)) return ErrorInvalidArgument;
        g_sessions[expId] = SessionState::AfterFromData;
    }
    if (std::memchr(localParam.content_type, '\0', sizeof(localParam.content_type)) == nullptr) return ErrorInvalidArgument;
    if (!SupportedContentType(localParam.content_type, sdkVersion)) return ErrorInvalidContentType;
    const char* subtype = std::strrchr(localParam.content_type, '/');
    if (subtype == nullptr || std::strlen(subtype + 1) < 2) return ErrorInvalidArgument;
    std::array<char, MaximumPathLength + 1> guestPath{};
    std::snprintf(guestPath.data(), guestPath.size(), "/av_contents/content_tmp/sce_cmgcnt%p.%s",
        reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(&g_sessionMutex) + static_cast<std::uintptr_t>(expId)), subtype + 1);
    try {
        const auto hostPath = ResolveDiskPath_nid_no_patch(guestPath.data());
        std::error_code directoryError;
        std::filesystem::create_directories(hostPath.parent_path(), directoryError);
        if (directoryError) return ErrorStorage;
        std::ofstream file(hostPath, std::ios::binary | std::ios::trunc);
        if (!file) return ErrorGeneric;
        const void* data = nullptr;
        std::size_t size = 0;
        std::size_t written = 0;
        std::array<char, 4096> staging{};
        const bool modernCallback = sdkVersion >= static_cast<std::int32_t>(MemoryPoolSdkVersion);
        for (;;) {
            const std::int32_t result = callback(&data, &size, userData);
            if (modernCallback && result != 0 && result != 1) return ErrorDataCallback;
            if (size != 0) {
                if (data == nullptr) return ErrorStorage;
                const auto address = reinterpret_cast<std::uintptr_t>(data);
                if (size > std::numeric_limits<std::uintptr_t>::max() - address) return ErrorStorage;
                for (std::size_t offset = 0; offset < size;) {
                    const std::size_t count = std::min(staging.size(), size - offset);
                    const int savedErrno = errno;
                    const bool copied = GuestCopyIn_nid_no_patch(staging.data(), reinterpret_cast<const void*>(address + offset), count);
                    errno = savedErrno;
                    if (!copied) return ErrorStorage;
                    file.write(staging.data(), static_cast<std::streamsize>(count));
                    if (!file) return ErrorStorage;
                    offset += count;
                }
                written += size;
            }
            if (result == 1 || (!modernCallback && result < 0)) break;
        }
        file.close();
        if (!file) return ErrorStorage;
        if (written != contentLength) return ErrorInvalidArgument;
    } catch (const std::bad_alloc&) {
        return ErrorNoMemory;
    } catch (const std::filesystem::filesystem_error&) {
        return ErrorGeneric;
    }
    if (path != nullptr) {
        const std::size_t length = std::min(pathBufferLength, MaximumPathLength);
        const int savedErrno = errno;
        const bool copied = GuestCopyOut_nid_no_patch(path, guestPath.data(), length);
        errno = savedErrno;
        if (!copied) return ErrorInvalidArgument;
    }
    return 0;
}

int APS5_VABI sceContentExportFromFile(std::int32_t expId, const ContentExportParam* param, const char* sourcePath, char* path, std::size_t pathBufferLength) {
    std::lock_guard lock(g_sessionMutex);
    RequireSession(__func__, expId);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceContentExportFromFileWithThumbnail(std::int32_t expId, const ContentExportParam* param, const char* sourcePath, const char* thumbnailPath, char* path, std::size_t pathBufferLength) {
    std::lock_guard lock(g_sessionMutex);
    RequireSession(__func__, expId);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceContentExportAddToFavorites(std::int64_t contentId) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceContentExportDataProvideFunction(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceContentExportFree(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceContentExportFrom(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceContentExportFromFileForSf(std::int32_t expId, const ContentExportParam* param, const ContentExportSfParam* sfParam, const char* sourcePath, const char* thumbnailPath, char* path, std::size_t pathBufferLength) {
    std::lock_guard lock(g_sessionMutex);
    RequireSession(__func__, expId);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceContentExportFromFileWithContentIdList(std::int32_t expId, const ContentExportParam* param, const char* sourcePath, const char* thumbnailPath, const std::int64_t* contentIds, std::uint32_t contentCount, char* path, std::size_t pathBufferLength) {
    std::lock_guard lock(g_sessionMutex);
    RequireSession(__func__, expId);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceContentExportFromFileWithTitleIdList(std::int32_t expId, const ContentExportParam* param, const char* sourcePath, const char* thumbnailPath, const char* titleIdList, char* path, std::size_t pathBufferLength) {
    if (NotInitializedForCaller()) return ErrorNotInitialized;
    if (!SessionIdInRange(expId) || param == nullptr || sourcePath == nullptr || thumbnailPath == nullptr) return ErrorInvalidArgument;
    std::string thumbnail;
    if (CopyGuestString_nid_no_patch(thumbnailPath, MaximumPathLength + 1, thumbnail) != 0 || thumbnail.empty()) return ErrorInvalidArgument;
    std::string list;
    if (titleIdList != nullptr && (CopyGuestString_nid_no_patch(titleIdList, MaximumTitleIdListLength + 1, list) != 0 || list.empty())) return ErrorInvalidArgument;
    ContentExportParam localParam{};
    if (const int error = CopyExportParam(param, localParam)) return error;
    std::lock_guard lock(g_sessionMutex);
    const auto session = g_sessions.find(expId);
    if (session == g_sessions.end()) return ErrorInvalidArgument;
    const auto endExport = [&session](int error) {
        session->second = SessionState::Ended;
        return error;
    };
    if (!ExportContentType(localParam.content_type)) return endExport(ErrorInvalidContentType);
    if (ApplicationCompiledSdkVersion() >= FileCheckSdkVersion) {
        if (const int error = CheckExportFiles(sourcePath, thumbnail)) return endExport(error);
    }
    if (titleIdList != nullptr) {
        if (const int error = CheckTitleIdList(list)) return endExport(error);
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceContentExportInit(const ContentExportInitParam* param) {
    if (param == nullptr) APS5_INVALID_ARG_EX;
    ContentExportInitParam localParam{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&localParam, param, sizeof(localParam));
    errno = savedErrno;
    if (!copied) APS5_INVALID_ARG_EX;
    if (localParam.mallocFunc == nullptr || localParam.freeFunc == nullptr) throw std::invalid_argument(std::string(__func__) + ": missing allocator functions");
    bool expected = false;
    if (!g_initialized.compare_exchange_strong(expected, true)) throw std::logic_error(std::string(__func__) + ": already initialized");
    return 0;
}

int APS5_VABI sceContentExportMalloc(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceContentExportRemoveFromFavorites(std::int64_t contentId) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceContentExportRenameContent(std::int64_t contentId, const char* name) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceContentExportValidateContents(const std::int64_t* contentIds, std::uint32_t count) {
    if (!g_initialized.load()) return ErrorNotInitialized;
    if (count > MaximumValidatedContents || (contentIds == nullptr && count != 0)) return ErrorInvalidArgument;
    if (count == 0) return 0;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceContentExportStart(void) {
    std::lock_guard lock(g_sessionMutex);
    if (!g_initialized.load()) return ErrorNotInitialized;
    for (std::int32_t expId = 0; expId < MaxExecution; ++expId) {
        if (g_sessions.emplace(expId, SessionState::Idle).second) return expId;
    }
    return ErrorExecutionMax;
}

int APS5_VABI sceContentExportGetProgress(std::int32_t expId) {
    if (NotInitializedForCaller()) return ErrorNotInitialized;
    if (!SessionIdInRange(expId)) return ErrorInvalidArgument;
    std::lock_guard lock(g_sessionMutex);
    const auto session = g_sessions.find(expId);
    if (session == g_sessions.end()) return ErrorInvalidArgument;
    if (session->second == SessionState::AfterFromData) NotImplemented_nid_no_patch(__func__);
    if (ApplicationCompiledSdkVersion() < ProgressSdkVersion) return ErrorInvalidState;
    return session->second == SessionState::Ended ? 100 : 0;
}

int APS5_VABI sceContentExportCancel(std::int32_t expId) {
    std::lock_guard lock(g_sessionMutex);
    RequireSession(__func__, expId);
    return ErrorInvalidState;
}

int APS5_VABI sceContentExportFromDataWithThumbnail(std::int32_t expId, const ContentExportParam* param, const ContentExportCallbackParam* contentCallbackParam, const char* thumbnailType, const ContentExportCallbackParam* thumbnailCallbackParam, char* path, std::size_t pathBufferLength) {
    if (NotInitializedForCaller()) return ErrorNotInitialized;
    if (!SessionIdInRange(expId) || param == nullptr || contentCallbackParam == nullptr || thumbnailCallbackParam == nullptr) return ErrorInvalidArgument;
    ContentExportParam localParam{};
    if (const int error = CopyExportParam(param, localParam)) return error;
    std::lock_guard lock(g_sessionMutex);
    const auto session = g_sessions.find(expId);
    if (session == g_sessions.end()) return ErrorInvalidArgument;
    ContentExportCallbackParam content{};
    ContentExportCallbackParam thumbnail{};
    if (!GuestCopyIn_nid_no_patch(&content, contentCallbackParam, sizeof(content)) || !GuestCopyIn_nid_no_patch(&thumbnail, thumbnailCallbackParam, sizeof(thumbnail))) return ErrorInvalidArgument;
    if (content.content_length == 0 || content.func == nullptr || thumbnail.content_length == 0 || thumbnail.func == nullptr) return ErrorInvalidArgument;
    const auto endExport = [&session](int error) {
        session->second = SessionState::Ended;
        return error;
    };
    if (!ExportContentType(localParam.content_type)) return endExport(ErrorInvalidContentType);
    if (ApplicationCompiledSdkVersion() >= ThumbnailTypeSdkVersion) {
        std::string type;
        if (thumbnailType == nullptr || CopyGuestString_nid_no_patch(thumbnailType, ThumbnailType.size() + 2, type) != 0 || type != ThumbnailType) return endExport(ErrorThumbnailType);
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceContentExportTerm(void) {
    std::lock_guard lock(g_sessionMutex);
    if (!g_sessions.empty()) return ErrorBusy;
    bool expected = true;
    if (!g_initialized.compare_exchange_strong(expected, false)) return ErrorNotInitialized;
    return 0;
}

}
