#include "prx/libc/include/GuestFileNumber.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestDirectory.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/FilesystemError.hpp"
#include "prx/libc/include/AppArchive.hpp"
#include <dirent.h>
#include <unistd.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <unordered_set>
#include <vector>
#include <string>
#ifdef _WIN32
#include "prx/libc/include/WindowsDescriptor.hpp"
#include "prx/libc/include/WindowsVerbatimPath.hpp"
#include <cstddef>
#include <string_view>
#endif

namespace {
int DirectoryError(int error) {
    switch (error) {
    case ENOENT: return 2;
    case EACCES: return 13;
    case ENOTDIR: return 20;
    case EINVAL: return 22;
    case EMFILE: return 24;
    case ENFILE: return 23;
    case ENOMEM: return 12;
    case ENAMETOOLONG: return 63;
#ifdef ELOOP
    case ELOOP: return 62;
#endif
    default: return 5;
    }
}
int DirectoryError(const std::error_code& error) {
#ifdef _WIN32
    if (error.category() == std::system_category())
        return FilesystemError(std::error_code(WindowsOpenError_nid_no_patch(static_cast<unsigned long>(error.value())), std::generic_category()));
#endif
    return FilesystemError(error);
}
#ifdef _WIN32
struct NativeDirectory {
    HANDLE handle = INVALID_HANDLE_VALUE;
    std::uint32_t seed = 0;
    bool identifiers = true;
    bool restart = true;
    bool buffered = false;
    std::size_t next = 0;
    long position = 0;
    alignas(8) std::byte buffer[65536];
    ~NativeDirectory() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
};
int NativeOpenError(DWORD error) {
    if (error == ERROR_CANT_RESOLVE_FILENAME) return ELOOP;
    if (error == ERROR_DIRECTORY) return ENOTDIR;
    return WindowsOpenError_nid_no_patch(error);
}
NativeDirectory* OpenNative(const std::filesystem::path& path) {
    std::error_code error;
    const auto verbatim = NativeVerbatimPath_nid_no_patch(path, error, WindowsMaxDirectoryPathLength);
    const auto& openPath = error ? path : verbatim;
    const DWORD attributes = GetFileAttributesW(openPath.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) { errno = NativeOpenError(GetLastError()); return nullptr; }
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        errno = ENOTDIR;
        if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
            const HANDLE followed = CreateFileW(openPath.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
            if (followed != INVALID_HANDLE_VALUE) CloseHandle(followed);
            else if (GetLastError() == ERROR_CANT_RESOLVE_FILENAME) errno = ELOOP;
        }
        return nullptr;
    }
    auto native = std::make_unique_for_overwrite<NativeDirectory>();
    native->handle = CreateFileW(openPath.c_str(), FILE_LIST_DIRECTORY | SYNCHRONIZE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (native->handle == INVALID_HANDLE_VALUE) { errno = NativeOpenError(GetLastError()); return nullptr; }
    BY_HANDLE_FILE_INFORMATION identity{};
    if (GetFileInformationByHandle(native->handle, &identity)) {
        if ((identity.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) { errno = ENOTDIR; return nullptr; }
        native->seed = WindowsFileNumber_nid_no_patch(identity);
    }
    return native.release();
}
int CloseNative(NativeDirectory* native) { delete native; return 0; }
void RewindNative(NativeDirectory* native) {
    native->restart = true;
    native->buffered = false;
    native->position = 0;
}
bool QueryNative(NativeDirectory* native) {
    const auto query = [native](FILE_INFO_BY_HANDLE_CLASS restartClass, FILE_INFO_BY_HANDLE_CLASS nextClass) {
        return GetFileInformationByHandleEx(native->handle, native->restart ? restartClass : nextClass, native->buffer, sizeof(native->buffer)) != 0;
    };
    bool filled = native->identifiers ? query(FileIdBothDirectoryRestartInfo, FileIdBothDirectoryInfo) : query(FileFullDirectoryRestartInfo, FileFullDirectoryInfo);
    DWORD error = filled ? ERROR_SUCCESS : GetLastError();
    if (!filled && native->restart && native->identifiers && (error == ERROR_INVALID_PARAMETER || error == ERROR_INVALID_FUNCTION ||
        error == ERROR_INVALID_LEVEL || error == ERROR_NOT_SUPPORTED)) {
        native->identifiers = false;
        filled = query(FileFullDirectoryRestartInfo, FileFullDirectoryInfo);
        error = filled ? ERROR_SUCCESS : GetLastError();
    }
    if (!filled) {
        if (error != ERROR_NO_MORE_FILES && error != ERROR_FILE_NOT_FOUND) errno = EIO;
        return false;
    }
    native->restart = false;
    native->buffered = true;
    native->next = 0;
    return true;
}
const std::byte* NextRecord(NativeDirectory* native) {
    if (!native->buffered && !QueryNative(native)) return nullptr;
    const std::byte* record = native->buffer + native->next;
    const DWORD offset = native->identifiers ? reinterpret_cast<const FILE_ID_BOTH_DIR_INFO*>(record)->NextEntryOffset
                                             : reinterpret_cast<const FILE_FULL_DIR_INFO*>(record)->NextEntryOffset;
    if (offset == 0) native->buffered = false;
    native->next += offset;
    ++native->position;
    return record;
}
std::uint32_t StableFileNumber(std::uint32_t seed, std::wstring_view name) {
    std::uint32_t hash = 2166136261u ^ seed;
    for (const wchar_t character : name) hash = (hash ^ static_cast<std::uint16_t>(character)) * 16777619u;
    return hash == 0 ? 1 : hash;
}
long TellNative(NativeDirectory* native) { return native->position; }
void SeekNative(NativeDirectory* native, long location) {
    if (location < 0) return;
    RewindNative(native);
    while (native->position < location && NextRecord(native)) {}
}
bool ReadNative(NativeDirectory* native, std::filesystem::path& component, std::string& name, std::uint64_t& inode) {
    const std::byte* record = NextRecord(native);
    if (!record) return false;
    std::wstring_view fileName;
    std::uint64_t fileId = 0;
    if (native->identifiers) {
        const auto* information = reinterpret_cast<const FILE_ID_BOTH_DIR_INFO*>(record);
        fileName = std::wstring_view(information->FileName, information->FileNameLength / sizeof(WCHAR));
        fileId = static_cast<std::uint64_t>(information->FileId.QuadPart);
    } else {
        const auto* information = reinterpret_cast<const FILE_FULL_DIR_INFO*>(record);
        fileName = std::wstring_view(information->FileName, information->FileNameLength / sizeof(WCHAR));
    }
    component = fileName;
    const auto utf8 = component.u8string();
    name.assign(utf8.begin(), utf8.end());
    inode = fileId != 0 ? WindowsFileNumber_nid_no_patch(fileId) : StableFileNumber(native->seed, fileName);
    return true;
}
#else
using NativeDirectory = DIR;
NativeDirectory* OpenNative(const std::filesystem::path& path) { return ::opendir(path.c_str()); }
int CloseNative(NativeDirectory* native) { return ::closedir(native); }
void RewindNative(NativeDirectory* native) { ::rewinddir(native); }
long TellNative(NativeDirectory* native) { return ::telldir(native); }
void SeekNative(NativeDirectory* native, long location) { ::seekdir(native, location); }
bool ReadNative(NativeDirectory* native, std::filesystem::path& component, std::string& name, std::uint64_t& inode) {
    auto* entry = ::readdir(native);
    if (!entry) return false;
    name = entry->d_name;
    component = name;
    inode = static_cast<std::uint64_t>(entry->d_ino);
    return true;
}
#endif
struct Directory {
    NativeDirectory* native = nullptr;
    std::filesystem::path path;
    GuestDirectoryEntry entry{};
    std::vector<GuestDirectoryEntry> entries;
    std::vector<GuestDirectoryEntry> pending;
    std::size_t cursor = 0;
    std::size_t pendingCursor = 0;
    bool indexed = false;
    bool descriptorDirectory = false;
    std::mutex mutex;
    int descriptor = -1;
    int (APS5_VABI* closeDescriptor)(int) = nullptr;
    int (APS5_VABI* readDescriptor)(int, char*, int) = nullptr;
    std::int64_t (APS5_VABI* seekDescriptor)(int, std::int64_t, int) = nullptr;
    ~Directory() { if (native) CloseNative(native); }
};

struct ErrnoGuard {
    int savedErrno = errno;
    ~ErrnoGuard() { errno = savedErrno; }
};

bool TryCopyIn(void* destination, const void* source, std::size_t bytes) {
    if (!bytes) return true;
    if (!source) return false;
    ErrnoGuard guard;
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(source, bytes);
    return GuestReadable_nid_no_patch(source, bytes) && GuestCopyIn_nid_no_patch(destination, source, bytes);
}

class GuestWriteScope {
public:
    GuestWriteScope(void* pointer, std::size_t bytes)
        : saved(), guard(pointer ? pointer : nullptr, pointer ? bytes : 0),
          valid(bytes == 0 || (pointer != nullptr && guard.Open() && GuestWritable_nid_no_patch(pointer, bytes))) {}
    bool Valid() const { return valid; }
    GuestWriteScope(const GuestWriteScope&) = delete;
    GuestWriteScope& operator=(const GuestWriteScope&) = delete;

private:
    ErrnoGuard saved;
    GuestArena::HostWrite guard;
    bool valid = false;
};

bool TryCopyOut(void* destination, const void* source, std::size_t bytes) {
    if (!bytes) return true;
    GuestWriteScope writeScope(destination, bytes);
    return writeScope.Valid() && GuestCopyOut_nid_no_patch(destination, source, bytes);
}

struct DirectoryRegistry {
    std::mutex mutex;
    std::unordered_set<Directory*> live;
};

DirectoryRegistry& GetDirectoryRegistry() {
    static auto* registry = new DirectoryRegistry();
    return *registry;
}

void* RegisterDirectory(std::unique_ptr<Directory> directory) {
    Directory* raw = directory.get();
    auto& registry = GetDirectoryRegistry();
    std::lock_guard lock(registry.mutex);
    registry.live.insert(raw);
    return directory.release();
}

Directory* LookupDirectory(void* handle) {
    if (!handle) return nullptr;
    auto& registry = GetDirectoryRegistry();
    std::lock_guard lock(registry.mutex);
    const auto it = registry.live.find(static_cast<Directory*>(handle));
    return it == registry.live.end() ? nullptr : *it;
}

void SeekDirectoryLocked(Directory& directory, std::int64_t location) {
    if (directory.descriptorDirectory) {
        if (directory.seekDescriptor(directory.descriptor, location, 0) >= 0) {
            directory.pending.clear();
            directory.pendingCursor = 0;
        }
    } else if (directory.indexed) {
        if (location >= 0 && static_cast<std::uint64_t>(location) <= directory.entries.size()) directory.cursor = static_cast<std::size_t>(location);
        else errno = 22;
    } else {
        SeekNative(directory.native, static_cast<long>(location));
    }
}

bool ReadDirectoryEntryName(const GuestDirectoryEntry* const* pointer, std::string& out) {
    if (!pointer) return false;
    const GuestDirectoryEntry* entry = nullptr;
    if (!TryCopyIn(&entry, pointer, sizeof(entry)) || !entry) return false;
    const auto entryAddr = reinterpret_cast<std::uintptr_t>(entry);
    constexpr std::size_t nameOffset = offsetof(GuestDirectoryEntry, name);
    if (nameOffset > std::numeric_limits<std::uintptr_t>::max() - entryAddr) return false;
    const auto nameAddr = entryAddr + nameOffset;
    char buffer[sizeof(GuestDirectoryEntry::name)]{};
    for (std::size_t offset = 0; offset < sizeof(buffer);) {
        if (offset > std::numeric_limits<std::uintptr_t>::max() - nameAddr) return false;
        const auto current = nameAddr + offset;
        const std::size_t toPageEnd = 4096u - (current % 4096u);
        const std::size_t remaining = sizeof(buffer) - offset;
        const std::size_t chunk = std::min(toPageEnd, remaining);
        if (!TryCopyIn(buffer + offset, reinterpret_cast<const void*>(current), chunk)) return false;
        if (const auto* end = static_cast<const char*>(std::memchr(buffer + offset, '\0', chunk))) {
            out.assign(buffer, static_cast<std::size_t>(end - buffer));
            return true;
        }
        offset += chunk;
    }
    return false;
}
std::uint8_t DirectoryType(const std::filesystem::path& path) {
    std::error_code error;
    switch (std::filesystem::symlink_status(path, error).type()) {
    case std::filesystem::file_type::regular: return 8;
    case std::filesystem::file_type::directory: return 4;
    case std::filesystem::file_type::symlink: return 10;
    case std::filesystem::file_type::block: return 6;
    case std::filesystem::file_type::character: return 2;
    case std::filesystem::file_type::fifo: return 1;
    case std::filesystem::file_type::socket: return 12;
    default: return 0;
    }
}
GuestDirectoryEntry* ReadEntry(Directory& directory, int skip) {
    const int savedError = errno;
    if (directory.descriptorDirectory) {
        if (directory.pendingCursor >= directory.pending.size()) {
            directory.pending.clear();
            directory.pendingCursor = 0;
            std::array<char, sizeof(GuestDirectoryEntry)> bytes{};
            const int read = directory.readDescriptor(directory.descriptor, bytes.data(), static_cast<int>(bytes.size()));
            if (read < 0) {
                errno = static_cast<int>(static_cast<std::uint32_t>(read) & 0xffffu);
                return nullptr;
            }
            if (read == 0) { errno = savedError; return nullptr; }
            for (int offset = 0; offset < read;) {
                GuestDirectoryEntry entry{};
                std::uint16_t recordLength = 0;
                std::memcpy(&recordLength, bytes.data() + offset + 4, sizeof(recordLength));
                if (recordLength < 12 || offset + recordLength > read || recordLength > sizeof(entry)) { errno = 5; return nullptr; }
                std::memcpy(&entry, bytes.data() + offset, recordLength);
                directory.pending.push_back(entry);
                offset += recordLength;
            }
        }
        while (directory.pendingCursor < directory.pending.size()) {
            directory.entry = directory.pending[directory.pendingCursor++];
            if (directory.entry.fileNumber == 0 && skip) continue;
            errno = savedError;
            return &directory.entry;
        }
        errno = savedError;
        return nullptr;
    }
    if (directory.indexed) {
        while (directory.cursor < directory.entries.size()) {
            directory.entry = directory.entries[directory.cursor++];
            if (directory.entry.fileNumber == 0 && skip) continue;
            errno = savedError;
            return &directory.entry;
        }
        errno = savedError;
        return nullptr;
    }
    for (;;) {
        errno = 0;
        std::filesystem::path component;
        std::string name;
        std::uint64_t inode = 0;
        try {
            if (!ReadNative(directory.native, component, name, inode)) {
                errno = errno ? DirectoryError(errno) : savedError;
                return nullptr;
            }
        } catch (const std::bad_alloc&) { errno = 12; return nullptr; }
        if (inode == 0 && skip) continue;
        const auto length = name.size();
        if (length > 255) { errno = 63; return nullptr; }
        try {
            directory.entry = {};
            directory.entry.fileNumber = static_cast<std::uint32_t>(inode);
            directory.entry.recordLength = static_cast<std::uint16_t>(8 + ((length + 1 + 3) & ~3));
            directory.entry.nameLength = static_cast<std::uint8_t>(length);
            directory.entry.type = DirectoryType(directory.path / component);
            std::memcpy(directory.entry.name, name.c_str(), length + 1);
            errno = savedError;
            return &directory.entry;
        } catch (const std::bad_alloc&) { errno = 12; return nullptr; }
          catch (const std::system_error& error) { errno = DirectoryError(error.code()); return nullptr; }
          catch (const std::exception&) { errno = 5; return nullptr; }
    }
}

GuestDirectoryEntry MakeEntry(const std::string& name, std::uint64_t inode, std::uint8_t type) {
    if (name.size() > 255) throw std::system_error(ENAMETOOLONG, std::generic_category(), "directory entry name");
    GuestDirectoryEntry entry{};
    entry.fileNumber = GuestFileNumber_nid_no_patch(inode);
    entry.recordLength = static_cast<std::uint16_t>(8 + ((name.size() + 1 + 3) & ~std::size_t{3}));
    entry.nameLength = static_cast<std::uint8_t>(name.size());
    entry.type = type;
    std::memcpy(entry.name, name.c_str(), name.size() + 1);
    return entry;
}

void OpenIndexed(Directory& directory) {
    directory.indexed = true;
    std::vector<std::string> names;
    const auto archiveOnly = AppArchiveOnly_nid_no_patch(directory.path);
    if (!archiveOnly) {
        directory.native = OpenNative(directory.path);
        if (!directory.native) throw std::system_error(errno, std::generic_category(), "open merged directory");
        for (;;) {
            errno = 0;
            std::filesystem::path component;
            std::string name;
            std::uint64_t inode = 0;
            if (!ReadNative(directory.native, component, name, inode)) {
                if (errno != 0) throw std::system_error(errno, std::generic_category(), "read merged directory");
                break;
            }
            names.push_back(name);
            directory.entries.push_back(MakeEntry(name, inode, DirectoryType(directory.path / component)));
        }
        CloseNative(directory.native);
        directory.native = nullptr;
    }
    AppArchiveNode node{};
    if (!AppArchiveLookup_nid_no_patch(directory.path, &node) || !node.Directory)
        throw std::logic_error("OpenIndexed: app0.zar directory disappeared");
    const auto hasName = [&names](const std::string& name) {
        return std::any_of(names.begin(), names.end(), [&name](const std::string& existing) {
            return AppArchiveNameMatches_nid_no_patch(existing, name);
        });
    };
    if (!hasName(".")) {
        const auto dotNumber = archiveOnly ? static_cast<std::uint64_t>(node.Handle) + 1 : 0;
        directory.entries.push_back(MakeEntry(".", dotNumber, 4));
        names.push_back(".");
    }
    if (!hasName("..")) {
        AppArchiveNode parent{};
        const auto parentPath = directory.path.parent_path();
        const auto parentNumber = AppArchiveLookup_nid_no_patch(parentPath, &parent) && parent.Directory ? static_cast<std::uint64_t>(parent.Handle) + 1 : 0;
        directory.entries.push_back(MakeEntry("..", parentNumber, 4));
        names.push_back("..");
    }
    const auto archiveEntries = archiveOnly ? AppArchiveListDirectory_nid_no_patch(directory.path)
        : AppArchiveListUnshadowedDirectory_nid_no_patch(directory.path, names);
    for (const auto& entry : archiveEntries)
        directory.entries.push_back(MakeEntry(entry.Name, static_cast<std::uint64_t>(entry.Handle) + 1, entry.Directory ? 4 : 8));
}
}

extern "C" {
void* APS5_VABI OpenDirectory_nid_no_patch(const char* path) {
    if (!path) { errno = 14; return nullptr; }
    std::string copy;
    if (const int error = CopyGuestPath_nid_no_patch(path, copy)) { errno = error; return nullptr; }
    if (copy.empty()) { errno = 2; return nullptr; }
    try {
        auto directory = std::make_unique<Directory>();
        if (const int error = ResolvePathError_nid_no_patch(copy.c_str(), directory->path, false, true)) { errno = error; return nullptr; }
        if (AppArchiveHasDirectory_nid_no_patch(directory->path)) {
            OpenIndexed(*directory);
        } else {
            directory->native = OpenNative(directory->path);
            if (!directory->native) {
                errno = DirectoryError(errno);
                if (errno == 2) errno = NotFoundError_nid_no_patch(directory->path);
                return nullptr;
            }
        }
        return RegisterDirectory(std::move(directory));
    } catch (const std::bad_alloc&) { errno = 12; return nullptr; }
      catch (const std::system_error& error) { errno = DirectoryError(error.code()); return nullptr; }
      catch (const std::exception&) { errno = 5; return nullptr; }
}

GuestDirectoryEntry* APS5_VABI ReadDirectory_nid_no_patch(void* handle) {
    auto* directory = LookupDirectory(handle);
    if (!directory) { errno = 9; return nullptr; }
    std::lock_guard lock(directory->mutex);
    return ReadEntry(*directory, 1);
}

int APS5_VABI ReadDirectoryEntry_nid_no_patch(void* handle, GuestDirectoryEntry* entry, GuestDirectoryEntry** result) {
    auto* directory = LookupDirectory(handle);
    if (!directory) return 9;
    if (!entry || !result) throw std::invalid_argument("readdir_r: null entry or result");
    const int savedError = errno;
    errno = 0;
    GuestDirectoryEntry* next = nullptr;
    {
        std::lock_guard lock(directory->mutex);
        next = ReadEntry(*directory, 1);
        if (next && !TryCopyOut(entry, next, next->recordLength)) {
            errno = savedError;
            return 14;
        }
    }
    if (!next && errno) return errno;
    errno = savedError;
    const GuestDirectoryEntry* const output = next ? entry : nullptr;
    if (!TryCopyOut(result, &output, sizeof(output))) {
        errno = savedError;
        return 14;
    }
    return 0;
}

int APS5_VABI CloseDirectory_nid_no_patch(void* handle) {
    if (!handle) { errno = 9; return -1; }
    Directory* raw = nullptr;
    {
        auto& registry = GetDirectoryRegistry();
        std::lock_guard lock(registry.mutex);
        const auto it = registry.live.find(static_cast<Directory*>(handle));
        if (it == registry.live.end()) { errno = 9; return -1; }
        raw = *it;
        registry.live.erase(it);
    }
    std::unique_ptr<Directory> directory(raw);
    int result = directory->native ? CloseNative(directory->native) : 0;
    directory->native = nullptr;
    if (result) errno = DirectoryError(errno);
    if (directory->closeDescriptor && directory->closeDescriptor(directory->descriptor) != 0) result = -1;
    return result;
}

void* APS5_VABI opendir_nid_postfix(const char* path) {
    return OpenDirectory_nid_no_patch(path);
}

GuestDirectoryEntry* APS5_VABI readdir_nid_postfix(void* handle) {
    return ReadDirectory_nid_no_patch(handle);
}

int APS5_VABI readdir_r_nid_postfix(void* handle, GuestDirectoryEntry* entry, GuestDirectoryEntry** result) {
    return ReadDirectoryEntry_nid_no_patch(handle, entry, result);
}

int APS5_VABI closedir_nid_postfix(void* handle) {
    return CloseDirectory_nid_no_patch(handle);
}

namespace {

GuestDirectoryEntry* CopyDirectoryEntry(const GuestDirectoryEntry* source) {
    auto* copy = static_cast<GuestDirectoryEntry*>(malloc_nid_postfix(sizeof(GuestDirectoryEntry)));
    if (copy == nullptr) return nullptr;
    GuestDirectoryEntry staged{};
    std::memcpy(&staged, source, std::min<std::size_t>(source->recordLength, sizeof(staged)));
    staged.name[std::min<std::size_t>(staged.nameLength, sizeof(staged.name) - 1)] = '\0';
    if (!TryCopyOut(copy, &staged, sizeof(staged))) {
        free_nid_postfix(copy);
        return nullptr;
    }
    return copy;
}

using DirectoryCompare = int (APS5_VABI *)(const GuestDirectoryEntry* const*, const GuestDirectoryEntry* const*);

int APS5_VABI CompareThroughThunk(void* thunk, const void* left, const void* right) {
    return (*static_cast<const DirectoryCompare*>(thunk))(static_cast<const GuestDirectoryEntry* const*>(left), static_cast<const GuestDirectoryEntry* const*>(right));
}

void FreeDirectoryEntries(std::vector<GuestDirectoryEntry*>& entries) {
    for (auto* entry : entries) free_nid_postfix(entry);
    entries.clear();
}

}

int APS5_VABI scandir_nid_postfix(const char* path, GuestDirectoryEntry*** namelist, int (APS5_VABI *select)(const GuestDirectoryEntry*), int (APS5_VABI *compar)(const GuestDirectoryEntry* const*, const GuestDirectoryEntry* const*)) {
    if (path == nullptr || namelist == nullptr) throw std::invalid_argument("scandir: null path or result");
    void* handle = opendir_nid_postfix(path);
    if (handle == nullptr) return -1;
    std::vector<GuestDirectoryEntry*> entries;
    for (;;) {
        errno = 0;
        const auto* entry = readdir_nid_postfix(handle);
        if (entry == nullptr) break;
        if (select != nullptr && select(entry) == 0) continue;
        auto* copy = CopyDirectoryEntry(entry);
        if (copy == nullptr) {
            FreeDirectoryEntries(entries);
            closedir_nid_postfix(handle);
            errno = 12;
            return -1;
        }
        entries.push_back(copy);
    }
    const int readError = errno;
    closedir_nid_postfix(handle);
    if (readError != 0) {
        FreeDirectoryEntries(entries);
        errno = readError;
        return -1;
    }
    auto** array = static_cast<GuestDirectoryEntry**>(malloc_nid_postfix(std::max<std::size_t>(entries.size(), 1) * sizeof(GuestDirectoryEntry*)));
    if (array == nullptr) {
        FreeDirectoryEntries(entries);
        errno = 12;
        return -1;
    }
    if (!entries.empty() && !TryCopyOut(array, entries.data(), entries.size() * sizeof(GuestDirectoryEntry*))) {
        free_nid_postfix(array);
        FreeDirectoryEntries(entries);
        errno = 14;
        return -1;
    }
    if (!entries.empty() && compar != nullptr) qsort_r_nid_postfix(array, entries.size(), sizeof(*array), &compar, CompareThroughThunk);
    if (!TryCopyOut(namelist, &array, sizeof(array))) {
        free_nid_postfix(array);
        FreeDirectoryEntries(entries);
        errno = 14;
        return -1;
    }
    return static_cast<int>(entries.size());
}

int APS5_VABI alphasort_nid_postfix(const GuestDirectoryEntry* const* left, const GuestDirectoryEntry* const* right) {
    std::string leftName;
    std::string rightName;
    if (!ReadDirectoryEntryName(left, leftName) || !ReadDirectoryEntryName(right, rightName))
        throw std::invalid_argument("alphasort: unreadable directory entry");
    return strcoll_nid_postfix(leftName.c_str(), rightName.c_str());
}

void APS5_VABI rewinddir_nid_postfix(void* handle) {
    auto* directory = LookupDirectory(handle);
    if (!directory) { errno = 9; return; }
    std::lock_guard lock(directory->mutex);
    if (directory->descriptorDirectory) {
        if (directory->seekDescriptor(directory->descriptor, 0, 0) >= 0) {
            directory->pending.clear();
            directory->pendingCursor = 0;
        }
    } else if (directory->indexed) {
        auto entries = std::move(directory->entries);
        const auto cursor = directory->cursor;
        const auto indexed = directory->indexed;
        directory->entries.clear();
        directory->cursor = 0;
        directory->indexed = false;
        const auto restore = [directory, &entries, cursor, indexed] {
            if (directory->native) {
                CloseNative(directory->native);
                directory->native = nullptr;
            }
            directory->entries = std::move(entries);
            directory->cursor = cursor;
            directory->indexed = indexed;
        };
        try { OpenIndexed(*directory); }
        catch (const std::bad_alloc&) { restore(); errno = 12; }
        catch (const std::system_error& error) { restore(); errno = DirectoryError(error.code()); }
        catch (const std::exception&) { restore(); errno = 5; }
    }
    else RewindNative(directory->native);
}

std::int64_t APS5_VABI telldir_nid_postfix(void* handle) {
    auto* directory = LookupDirectory(handle);
    if (!directory) { errno = 9; return -1; }
    std::lock_guard lock(directory->mutex);
    if (directory->descriptorDirectory) {
        const auto position = directory->seekDescriptor(directory->descriptor, 0, 1);
        if (position < 0) return position;
        const auto unread = directory->pending.size() - directory->pendingCursor;
        return position >= static_cast<std::int64_t>(unread) ? position - static_cast<std::int64_t>(unread) : 0;
    }
    return directory->indexed ? static_cast<std::int64_t>(directory->cursor) : TellNative(directory->native);
}

void APS5_VABI seekdir_nid_postfix(void* handle, std::int64_t location) {
    auto* directory = LookupDirectory(handle);
    if (!directory) { errno = 9; return; }
    std::lock_guard lock(directory->mutex);
    SeekDirectoryLocked(*directory, location);
}

extern "C" GuestDirectoryEntry* APS5_VABI ReadDirectoryUnlocked_nid_no_patch(void* handle, int skip) {
    auto* directory = LookupDirectory(handle);
    if (!directory) { errno = 9; return nullptr; }
    return ReadEntry(*directory, skip);
}

extern "C" void APS5_VABI SeekDirectoryUnlocked_nid_no_patch(void* handle, std::int64_t location) {
    auto* directory = LookupDirectory(handle);
    if (!directory) { errno = 9; return; }
    SeekDirectoryLocked(*directory, location);
}

void* APS5_VABI OpenDescriptorDirectory_nid_no_patch(const char* hostPath, int descriptor, int (APS5_VABI* closeDescriptor)(int), std::uint64_t position,
    int (APS5_VABI* readDescriptor)(int, char*, int), std::int64_t (APS5_VABI* seekDescriptor)(int, std::int64_t, int)) {
    try {
        auto directory = std::make_unique<Directory>();
        directory->path = std::filesystem::path(reinterpret_cast<const char8_t*>(hostPath));
        if (readDescriptor && seekDescriptor) {
            directory->descriptor = descriptor;
            directory->closeDescriptor = closeDescriptor;
            directory->readDescriptor = readDescriptor;
            directory->seekDescriptor = seekDescriptor;
            directory->descriptorDirectory = true;
            return RegisterDirectory(std::move(directory));
        }
#ifdef _WIN32
        directory->native = OpenNative(directory->path);
        if (!directory->native) { errno = DirectoryError(errno); return nullptr; }
        std::filesystem::path component;
        std::string name;
        std::uint64_t inode = 0;
        for (std::uint64_t skipped = 0; skipped < position; ++skipped) {
            if (!ReadNative(directory->native, component, name, inode)) break;
        }
#else
        (void)position;
        const int duplicate = ::dup(descriptor);
        if (duplicate < 0) { errno = DirectoryError(errno); return nullptr; }
        directory->native = ::fdopendir(duplicate);
        if (!directory->native) { const int error = errno; ::close(duplicate); errno = DirectoryError(error); return nullptr; }
#endif
        directory->descriptor = descriptor;
        directory->closeDescriptor = closeDescriptor;
        return RegisterDirectory(std::move(directory));
    } catch (const std::bad_alloc&) { errno = 12; return nullptr; }
      catch (const std::system_error& error) { errno = DirectoryError(error.code()); return nullptr; }
}
}
