#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <optional>
#include <filesystem>
#include <limits>
#include <new>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <cerrno>
#include <cstring>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#include "prx/libc/include/WindowsDescriptor.hpp"
#endif

#include "prx/libc/include/SonyCrt.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/FilesystemError.hpp"
#include "prx/libc/include/HostErrno.hpp"
#include "prx/libc/include/AppArchive.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/HostEntropy.hpp"

#ifdef __linux__
#include <stdio_ext.h>
#endif

struct GuestFileMode {
    std::string native;
    bool writes = false;
    bool exclusive = false;
    bool append = false;
};

static std::optional<GuestFileMode> ParseFileMode(const char* mode) {
    const char access = mode[0];
    if (access != 'r' && access != 'w' && access != 'a') return std::nullopt;
    bool update = false;
    bool exclusive = false;
    for (const char* flag = mode + 1; *flag != '\0' && std::strchr("b+xev", *flag) != nullptr; ++flag) {
        if (*flag == '+') update = true;
        if (*flag == 'x') exclusive = true;
    }
    if (exclusive && access == 'r' && !update) return std::nullopt;
    GuestFileMode result{std::string(1, access), access != 'r' || update, exclusive, access == 'a'};
    if (update) result.native += '+';
#ifdef _WIN32
    result.native += 'b';
#endif
    if (exclusive) result.native += 'x';
    return result;
}

#ifdef _WIN32
static constexpr DWORD AppendOnlyAccess = FILE_GENERIC_WRITE & ~FILE_WRITE_DATA;

static std::optional<std::wstring> HostOpenName(const std::filesystem::path& path) {
    auto filename = path.native();
    if (!filename.starts_with(L"\\\\?\\") && !filename.starts_with(L"\\\\.\\")) {
        std::error_code pathError;
        auto absolutePath = std::filesystem::absolute(path, pathError);
        if (pathError) { errno = pathError.default_error_condition().value(); return std::nullopt; }
        const auto absolute = absolutePath.lexically_normal().make_preferred().native();
        if (absolute.size() >= MAX_PATH)
            filename = absolute.starts_with(L"\\\\") ? L"\\\\?\\UNC\\" + absolute.substr(2) : L"\\\\?\\" + absolute;
    }
    return filename;
}
#endif

static std::FILE* OpenNative(const std::filesystem::path& path, const std::string& nativeMode) {
#ifdef _WIN32
    const char access = nativeMode[0];
    const bool update = nativeMode.find('+') != std::string::npos;
    const DWORD writes = access == 'a' ? AppendOnlyAccess : GENERIC_WRITE;
    const DWORD desired = update ? GENERIC_READ | writes : access == 'r' ? GENERIC_READ : writes;
    const DWORD disposition = access == 'r' ? OPEN_EXISTING
        : nativeMode.find('x') != std::string::npos ? CREATE_NEW : access == 'w' ? CREATE_ALWAYS : OPEN_ALWAYS;
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    const auto filename = HostOpenName(path);
    if (!filename) return nullptr;
    const auto handle = CreateFileW(filename->c_str(), desired, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, &security,
        disposition, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        errno = WindowsOpenError_nid_no_patch(GetLastError());
        return nullptr;
    }
    const int descriptor = ::_open_osfhandle(reinterpret_cast<std::intptr_t>(handle), access == 'a' ? _O_APPEND : 0);
    if (descriptor < 0) {
        CloseHandle(handle);
        return nullptr;
    }
    std::wstring streamMode;
    for (const char character : nativeMode)
        if (character != 'x') streamMode.push_back(static_cast<unsigned char>(character));
    std::FILE* stream = ::_wfdopen(descriptor, streamMode.c_str());
    if (!stream) ::_close(descriptor);
    return stream;
#else
    return std::fopen(path.c_str(), nativeMode.c_str());
#endif
}

static int CloseOwnedStream(std::FILE* stream) {
#ifdef _WIN32
    AppArchiveDetach_nid_no_patch(::_fileno(stream));
#else
    AppArchiveDetach_nid_no_patch(::fileno(stream));
#endif
    return std::fclose(stream);
}

#ifdef _WIN32
static bool MoveAppendingHandle(int descriptor, HANDLE handle, DWORD access, DWORD options) {
    const int translation = ::_setmode(descriptor, _O_BINARY);
    if (translation == -1 || ::_setmode(descriptor, translation) == -1) return false;
    HANDLE copy = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), handle, GetCurrentProcess(), &copy, access, TRUE, options)) {
        errno = EBADF;
        return false;
    }
    const int appending = ::_open_osfhandle(reinterpret_cast<std::intptr_t>(copy), _O_APPEND | translation);
    if (appending < 0) {
        CloseHandle(copy);
        return false;
    }
    auto* const standard = HostStandardHandle_nid_no_patch(descriptor);
    const int moved = ::_dup2(appending, descriptor);
    const int error = errno;
    ::_close(appending);
    KeepStandardHandle_nid_no_patch(descriptor, standard);
    errno = error;
    return moved == 0;
}

static bool AppendThroughDescriptor(int descriptor) {
    const auto handle = reinterpret_cast<HANDLE>(::_get_osfhandle(descriptor));
    if (handle == INVALID_HANDLE_VALUE) return false;
    if (descriptor <= 2 || GetFileType(handle) != FILE_TYPE_DISK) return true;
    return MoveAppendingHandle(descriptor, handle, 0, DUPLICATE_SAME_ACCESS);
}

static bool AppendOnlyDescriptor(int descriptor) {
    const auto handle = reinterpret_cast<HANDLE>(::_get_osfhandle(descriptor));
    if (handle == INVALID_HANDLE_VALUE) return false;
    if (GetFileType(handle) != FILE_TYPE_DISK) return true;
    const ACCESS_MASK access = WindowsHandleAccess_nid_no_patch(handle, descriptor, "freopen");
    if (WindowsHandleOnlyAppends_nid_no_patch(access)) return true;
    return MoveAppendingHandle(descriptor, handle, ((access & FILE_READ_DATA) != 0 ? FILE_GENERIC_READ : 0) | AppendOnlyAccess, 0);
}

static bool IsPipeStream(std::FILE* native) {
    return GetFileType(reinterpret_cast<HANDLE>(::_get_osfhandle(_fileno(native)))) == FILE_TYPE_PIPE;
}
#endif

using GuestCompare = int (APS5_VABI *)(const void*, const void*);
using GuestContextCompare = int (APS5_VABI *)(const void*, const void*, void*);

static void SwapElements(unsigned char* left, unsigned char* right, size_t size) {
    for (size_t index = 0; index < size; ++index) std::swap(left[index], right[index]);
}

static void SwapRanges(unsigned char* left, unsigned char* right, size_t bytes) {
    if (bytes > 0) SwapElements(left, right, bytes);
}

template <typename Compare>
static unsigned char* MedianOfThree(unsigned char* a, unsigned char* b, unsigned char* c, Compare compare) {
    return compare(a, b) < 0
        ? (compare(b, c) < 0 ? b : (compare(a, c) < 0 ? c : a))
        : (compare(b, c) > 0 ? b : (compare(a, c) < 0 ? a : c));
}

template <typename Compare>
static void InsertionSort(unsigned char* base, size_t count, size_t size, Compare compare) {
    for (unsigned char* next = base + size; next < base + count * size; next += size)
        for (unsigned char* element = next; element > base && compare(element - size, element) > 0; element -= size)
            SwapElements(element, element - size, size);
}

template <typename Compare>
static void SortElements(unsigned char* base, size_t count, size_t size, Compare compare) {
    for (;;) {
        if (count < 7) {
            InsertionSort(base, count, size, compare);
            return;
        }
        unsigned char* middle = base + (count / 2) * size;
        if (count > 7) {
            unsigned char* low = base;
            unsigned char* high = base + (count - 1) * size;
            if (count > 40) {
                const size_t step = (count / 8) * size;
                low = MedianOfThree(low, low + step, low + 2 * step, compare);
                middle = MedianOfThree(middle - step, middle, middle + step, compare);
                high = MedianOfThree(high - 2 * step, high - step, high, compare);
            }
            middle = MedianOfThree(low, middle, high, compare);
        }
        SwapElements(base, middle, size);
        unsigned char* equalLow = base + size;
        unsigned char* scanLow = equalLow;
        unsigned char* scanHigh = base + (count - 1) * size;
        unsigned char* equalHigh = scanHigh;
        bool swapped = false;
        for (;;) {
            int result = 0;
            while (scanLow <= scanHigh && (result = compare(scanLow, base)) <= 0) {
                if (result == 0) {
                    swapped = true;
                    SwapElements(equalLow, scanLow, size);
                    equalLow += size;
                }
                scanLow += size;
            }
            while (scanLow <= scanHigh && (result = compare(scanHigh, base)) >= 0) {
                if (result == 0) {
                    swapped = true;
                    SwapElements(scanHigh, equalHigh, size);
                    equalHigh -= size;
                }
                scanHigh -= size;
            }
            if (scanLow > scanHigh) break;
            SwapElements(scanLow, scanHigh, size);
            swapped = true;
            scanLow += size;
            scanHigh -= size;
        }
        if (!swapped) {
            InsertionSort(base, count, size, compare);
            return;
        }
        unsigned char* const end = base + count * size;
        size_t moved = std::min<size_t>(equalLow - base, scanLow - equalLow);
        SwapRanges(base, scanLow - moved, moved);
        moved = std::min<size_t>(equalHigh - scanHigh, end - equalHigh - size);
        SwapRanges(scanLow, end - moved, moved);
        const size_t lowBytes = scanLow - equalLow;
        const size_t highBytes = equalHigh - scanHigh;
        if (lowBytes <= highBytes) {
            if (lowBytes > size) SortElements(base, lowBytes / size, size, compare);
            if (highBytes <= size) return;
            base = end - highBytes;
            count = highBytes / size;
        } else {
            if (highBytes > size) SortElements(end - highBytes, highBytes / size, size, compare);
            if (lowBytes <= size) return;
            count = lowBytes / size;
        }
    }
}

namespace {

template<typename TAllocate>
void* allocationOrNull(TAllocate allocate, int error = 12) {
    try {
        return allocate();
    } catch (const std::bad_alloc&) {
    } catch (const std::length_error&) {
    }
    errno = error;
    return nullptr;
}

bool validAlignment(std::size_t alignment) {
    return alignment != 0 && (alignment & (alignment - 1)) == 0;
}

constexpr std::size_t DefaultHeapAlignment = 32;
constexpr std::size_t DefaultHeapAlignmentLimit = std::size_t{1} << 63;
constexpr std::size_t DefaultHeapOverhead = 0x1008f;
constexpr std::size_t DefaultHeapAlignedOverhead = 0x100af;

std::size_t defaultHeapAlignment(std::size_t alignment) {
    return std::bit_ceil(std::max(alignment, DefaultHeapAlignment));
}

bool defaultHeapRequestOverflows(std::size_t bytes, std::size_t alignment) {
    const std::size_t overhead = alignment <= DefaultHeapAlignment ? DefaultHeapOverhead : defaultHeapAlignment(alignment) + DefaultHeapAlignedOverhead;
    return bytes > std::numeric_limits<std::size_t>::max() - overhead;
}

template<typename TAllocate>
void* defaultHeapAllocation(std::size_t bytes, std::size_t alignment, TAllocate allocate) {
    if (defaultHeapRequestOverflows(bytes, alignment)) return nullptr;
    return allocationOrNull(allocate, 22);
}

template<typename TAllocate>
void* heapAllocation(std::size_t bytes, TAllocate allocate) {
    if (!ApplicationHeapDefault_nid_no_patch()) return allocationOrNull(allocate);
    return defaultHeapAllocation(bytes, 0, allocate);
}

}

extern "C" {

[[noreturn]] void APS5_VABI _ZSt11_Xbad_allocv_nid_postfix();

FileStream* APS5_VABI fdopen_nid_postfix(int descriptor, const char* mode) {
    if (descriptor < 0) { errno = 9; return nullptr; }
    if (!mode) { errno = 22; return nullptr; }
    AppArchiveRefuseDescriptor_nid_no_patch(descriptor, "fdopen");
    const auto parsed = ParseFileMode(mode);
    if (!parsed) { errno = 22; return nullptr; }
    try {
        auto nativeMode = parsed->native;
        std::erase(nativeMode, 'x');
        const auto release = [](void* pointer) { ::operator delete(pointer); };
        std::unique_ptr<void, decltype(release)> storage(::operator new(sizeof(FileStream)), release);
#ifdef _WIN32
        const auto handle = WindowsDescriptorHandle_nid_no_patch(descriptor);
        if (handle == INVALID_HANDLE_VALUE) { errno = 9; return nullptr; }
        const int access = WindowsDescriptorAccess_nid_no_patch(handle, descriptor, "fdopen");
        const int requested = nativeMode.find('+') != std::string::npos ? 2 : nativeMode[0] == 'r' ? 0 : 1;
        if (access != 2 && access != requested) { errno = 22; return nullptr; }
        if (parsed->append && !AppendThroughDescriptor(descriptor)) return nullptr;
        auto* native = ::_fdopen(descriptor, nativeMode.c_str());
#else
        auto* native = ::fdopen(descriptor, nativeMode.c_str());
#endif
        if (!native) return nullptr;
        auto* stream = new (storage.get()) FileStream(native, true);
        storage.release();
        return stream;
    } catch (const std::bad_alloc&) { errno = 12; return nullptr; }
}

static int OpenFailure(const std::filesystem::path& path, bool writes, int hostError) {
    const int error = GuestErrnoFromHost_nid_no_patch(hostError);
    std::error_code status;
    return writes && error == 13 && std::filesystem::is_directory(path, status) ? 21 : error;
}

#ifdef _WIN32
static int (*g_openDirectoryDescriptor)(const wchar_t*) = nullptr;
static void (*g_forgetDirectoryDescriptor)(int) = nullptr;

void LibcRegisterDirectoryStreams_nid_no_patch(int (*open)(const wchar_t*), void (*forget)(int)) {
    g_openDirectoryDescriptor = open;
    g_forgetDirectoryDescriptor = forget;
}

void ForgetDirectoryStream_nid_no_patch(int descriptor) {
    if (!g_forgetDirectoryDescriptor) throw std::runtime_error("ForgetDirectoryStream: libkernel has not registered its directory descriptors");
    g_forgetDirectoryDescriptor(descriptor);
}

static FileStream* OpenDirectoryStream(const std::filesystem::path& path) {
    if (!g_openDirectoryDescriptor) throw std::runtime_error("fopen: libkernel has not registered its directory descriptors");
    const auto filename = HostOpenName(path);
    if (!filename) { errno = GuestErrnoFromHost_nid_no_patch(errno); return nullptr; }
    const auto release = [](void* pointer) { ::operator delete(pointer); };
    std::unique_ptr<void, decltype(release)> storage(::operator new(sizeof(FileStream)), release);
    const int descriptor = g_openDirectoryDescriptor(filename->c_str());
    if (descriptor < 0) {
        const int error = GuestErrnoFromHost_nid_no_patch(errno);
        errno = error == 2 ? NotFoundError_nid_no_patch(path) : error;
        return nullptr;
    }
    auto* native = ::_fdopen(descriptor, "rb");
    if (!native) {
        const int error = GuestErrnoFromHost_nid_no_patch(errno);
        ForgetDirectoryStream_nid_no_patch(descriptor);
        ::_close(descriptor);
        errno = error;
        return nullptr;
    }
    auto* stream = new (storage.get()) FileStream(native, true);
    storage.release();
    stream->MarkDirectory();
    return stream;
}
#endif

FileStream* APS5_VABI freopen_nid_postfix(const char* filename, const char* mode, FileStream* stream) {
    if (!stream || !mode) { errno = 22; return nullptr; }
    if (!filename) { errno = 45; return nullptr; } // Mode-only reopening is not supported.
    const auto parsed = ParseFileMode(mode);
    if (!parsed) { errno = 22; return nullptr; }
    try {
        std::filesystem::path path;
        if (const int error = *filename ? ResolvePathError_nid_no_patch(filename, path) : 2) {
            if (stream->IsHostStandard()) stream->Detach();
            else stream->Close();
            if (stream->IsDynamic()) delete stream;
            errno = error;
            return nullptr;
        }
        if (stream->Reopen(path, parsed->native.c_str(), OpenNative)) {
#ifdef _WIN32
            if (parsed->append && !AppendOnlyDescriptor(_fileno(stream->GetHandle()))) {
                const int error = errno;
                stream->Close();
                if (stream->IsDynamic()) delete stream;
                errno = error;
                return nullptr;
            }
#endif
            if (parsed->append) static_cast<void>(std::fseek(stream->GetHandle(), 0, SEEK_END));
            if (!path.empty() && parsed->writes) RecordWrittenPath_nid_no_patch(path);
            return stream;
        }
        int error = OpenFailure(path, parsed->writes, errno);
        if (error == 2) error = NotFoundError_nid_no_patch(path);
        if (stream->IsDynamic()) delete stream;
        errno = error;
        return nullptr;
    } catch (const std::bad_alloc&) { errno = 12; return nullptr; }
      catch (const std::filesystem::filesystem_error&) { errno = 5; return nullptr; }
}

FileStream* APS5_VABI fopen_nid_postfix(const char* filename, const char* mode) {
    if (!filename || !mode) throw std::runtime_error(std::string(__func__) + ": " + FOPEN_MSG_NULL_ARG);
    const auto parsed = ParseFileMode(mode);
    if (!parsed) {
        errno = EINVAL;
        return nullptr;
    }
    if (const std::string_view path(filename); path == "/dev/random" || path == "/dev/urandom") {
        if (parsed->writes) throw std::runtime_error(std::string(__func__) + ": writing to " + filename + " is not implemented");
#ifdef _WIN32
        std::unique_ptr<std::FILE, int (*)(std::FILE*)> device(std::fopen("NUL", "rb"), std::fclose);
#else
        std::unique_ptr<std::FILE, int (*)(std::FILE*)> device(std::fopen("/dev/null", "rb"), std::fclose);
#endif
        if (!device) return nullptr;
        auto stream = std::make_unique<FileStream>(device.get(), true);
        device.release();
        stream->MarkRandomDevice();
        return stream.release();
    }
    std::filesystem::path fpath;
    if (const int error = ResolvePathError_nid_no_patch(filename, fpath, false, true)) { errno = error; return nullptr; }
    std::unique_ptr<std::FILE, int (*)(std::FILE*)> handle(
        AppArchiveOnly_nid_no_patch(fpath) ? AppArchiveOpenStream_nid_no_patch(fpath, parsed->native.c_str()) : OpenNative(fpath, parsed->native),
        CloseOwnedStream);
    if (!handle) {
        const int hostError = errno;
#ifdef _WIN32
        std::error_code status;
        if (!parsed->writes && GuestErrnoFromHost_nid_no_patch(hostError) == 13 && std::filesystem::is_directory(fpath, status))
            return OpenDirectoryStream(fpath);
#endif
        errno = OpenFailure(fpath, parsed->writes, hostError);
        if (errno == 2) errno = NotFoundError_nid_no_patch(fpath);
        return nullptr;
    }
    // APS5_LOG_OUT("success: \"%s\"", abs_path.c_str());
    if (parsed->append) static_cast<void>(std::fseek(handle.get(), 0, SEEK_END));
    if (parsed->writes) RecordWrittenPath_nid_no_patch(fpath);
    auto stream = std::make_unique<FileStream>(handle.get(), true);
    handle.release();
    return stream.release();
}

int APS5_VABI fopen_s_nid_postfix(FileStream** result, const char* filename, const char* mode) {
    constexpr int GuestEinval = 22;
    if (!result) return GuestEinval;
    *result = nullptr;
    if (!filename || !mode) return GuestEinval;
    *result = fopen_nid_postfix(filename, mode);
    return *result ? 0 : errno;
}

int APS5_VABI fclose_nid_postfix(FileStream* stream) {
    if (stream && !stream->IsOpen()) {
        std::unique_ptr<FileStream> owner(stream->IsDynamic() ? stream : nullptr);
        errno = 9;
        return -1;
    }
    GetStreamHandle(stream);
    std::unique_ptr<FileStream> owner(stream->IsDynamic() ? stream : nullptr);
    return stream->Close();
}

int APS5_VABI fseek_nid_postfix(FileStream* stream, std::int64_t offset, int origin);

FileStream* APS5_VABI _ZSt7_FiopenPKcNSt5_IosbIiE9_OpenmodeEi_nid_postfix(const char* filename, int mode, int protection) {
    static_cast<void>(protection);
    constexpr int In = 0x01, Out = 0x02, Ate = 0x04, App = 0x08, Trunc = 0x10, Nocreate = 0x20, Noreplace = 0x40,
        Binary = 0x80;
    constexpr std::pair<int, const char*> modes[] = {
        {In, "r"}, {Out, "w"}, {Out | Trunc, "w"}, {Out | App, "a"},
        {In | Binary, "rb"}, {Out | Binary, "wb"}, {Out | Trunc | Binary, "wb"}, {Out | App | Binary, "ab"},
        {In | Out, "r+"}, {In | Out | Trunc, "w+"}, {In | Out | App, "a+"},
        {In | Out | Binary, "r+b"}, {In | Out | Trunc | Binary, "w+b"}, {In | Out | App | Binary, "a+b"},
        {Out | Noreplace, "wx"}, {Out | Trunc | Noreplace, "wx"}, {In | Out | Trunc | Noreplace, "w+x"},
        {Out | Binary | Noreplace, "wbx"}, {Out | Trunc | Binary | Noreplace, "wbx"},
        {In | Out | Trunc | Binary | Noreplace, "w+bx"}};
    const int open = (mode & (In | Out | App | Trunc | Binary)) | ((mode & Nocreate) ? In : 0) | ((mode & App) ? Out : 0) |
        (((mode & Noreplace) && (mode & (Out | App))) ? Noreplace : 0);
    const char* openMode = nullptr;
    for (const auto& [flags, text] : modes) if (flags == open) openMode = text;
    if (!openMode) return nullptr;
    auto* stream = fopen_nid_postfix(filename, openMode);
    if (!stream || !(mode & Ate) || fseek_nid_postfix(stream, 0, SEEK_END) == 0) return stream;
    fclose_nid_postfix(stream);
    return nullptr;
}

size_t APS5_VABI fread_nid_postfix(void* buffer, size_t size, size_t count, FileStream* stream) {
    if (size != 0 && count > std::numeric_limits<std::size_t>::max() / size) { errno = EINVAL; return 0; }
    if (stream && !stream->IsOpen()) { errno = 9; return 0; }
    auto* handle = GetStreamHandle(stream);
    if (size == 0 || count == 0) return 0;
    if (!buffer) throw std::runtime_error("fread: null buffer");
    const auto bytes = size * count;
    if (bytes > std::numeric_limits<std::uintptr_t>::max() - reinterpret_cast<std::uintptr_t>(buffer)) throw std::overflow_error("fread: buffer address overflow");
    const GuestArena::HostWrite destination(buffer, bytes);
    if (!destination.Open()) throw std::runtime_error("fread: buffer is not writable");
    if (stream->IsRandomDevice()) {
        HostEntropy_nid_no_patch(buffer, bytes);
        stream->SyncStatus();
        return count;
    }
    if (!stream->SwitchToReading()) return 0;
    const auto result = std::fread(buffer, size, count, handle);
    stream->SyncStatus();
    stream->KeepDirectoryReadError(result < count);
    return result;
}

size_t APS5_VABI fwrite_nid_postfix(const void* buffer, size_t size, size_t count, FileStream* stream) {
    if (stream && !stream->IsOpen()) { errno = 9; return 0; }
    auto* handle = GetNativeStream(stream);
    if (size == 0 || count == 0) return 0;
    if (!buffer) throw std::runtime_error("fwrite: null buffer");
    if (count > std::numeric_limits<std::size_t>::max() / size) throw std::overflow_error("fwrite: buffer size overflow");
    const auto bytes = size * count;
    if (bytes > std::numeric_limits<std::uintptr_t>::max() - reinterpret_cast<std::uintptr_t>(buffer)) throw std::overflow_error("fwrite: buffer address overflow");
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(buffer, bytes);
    const auto result = std::fwrite(buffer, size, count, handle);
    stream->SyncStatus();
    return result;
}

int APS5_VABI fseeko_nid_postfix(FileStream* stream, std::int64_t offset, int origin) {
    if (stream && !stream->IsOpen()) { errno = 9; return -1; }
    if (origin != SEEK_SET && origin != SEEK_CUR && origin != SEEK_END) { errno = 22; return -1; }
#ifdef _WIN32
    auto* native = GetNativeStream(stream);
    if (IsPipeStream(native)) { errno = 29; return -1; }
    const int result = _fseeki64(native, offset, origin);
#else
    static_assert(sizeof(off_t) == 8);
    const int result = ::fseeko(GetNativeStream(stream), offset, origin);
#endif
    const int nativeError = errno;
    stream->SyncStatus();
    if (result) errno = nativeError == EOVERFLOW ? 84 : nativeError;
    return result;
}

std::int64_t APS5_VABI ftello_nid_postfix(FileStream* stream) {
    if (stream && !stream->IsOpen()) { errno = 9; return -1; }
#ifdef _WIN32
    auto* native = GetNativeStream(stream);
    if (IsPipeStream(native)) { errno = 29; return -1; }
    const auto result = _ftelli64(native);
#else
    static_assert(sizeof(off_t) == 8);
    const auto result = ::ftello(GetNativeStream(stream));
#endif
    if (result == -1 && errno == EOVERFLOW) errno = 84;
    return result;
}

int APS5_VABI fseek_nid_postfix(FileStream* stream, std::int64_t offset, int origin) {
    return fseeko_nid_postfix(stream, offset, origin);
}

std::int64_t APS5_VABI ftell_nid_postfix(FileStream* stream) { return ftello_nid_postfix(stream); }

int APS5_VABI fgetpos_nid_postfix(FileStream* stream, std::int64_t* position) {
    if (!position) throw std::invalid_argument("fgetpos: null position");
    const auto result = ftello_nid_postfix(stream);
    if (result == -1) return -1;
    *position = result;
    return 0;
}

int APS5_VABI fsetpos_nid_postfix(FileStream* stream, const std::int64_t* position) {
    if (!position) throw std::invalid_argument("fsetpos: null position");
    return fseeko_nid_postfix(stream, *position, SEEK_SET);
}

void APS5_VABI rewind_nid_postfix(FileStream* stream) {
    const int saved = errno;
    if (fseeko_nid_postfix(stream, 0, SEEK_SET) == 0) errno = saved;
    stream->ClearError();
}

int APS5_VABI fputs_nid_postfix(const char* str, FileStream* stream) {
    if (!str) throw std::runtime_error("fputs: null string");
    if (stream && !stream->IsOpen()) { errno = 9; return -1; }
    const int result = std::fputs(str, GetNativeStream(stream));
    stream->SyncStatus();
    return result;
}

int APS5_VABI fflush_nid_postfix(FileStream* stream) {
    if (stream && !stream->IsOpen()) { errno = 9; return -1; }
    std::FILE* native = stream ? GetStreamHandle(stream) : nullptr;
    if (stream && (stream->IsRandomDevice() || stream->IsDirectory())) return 0;
#ifdef __linux__
    if (native && !__fwriting(native)) return 0;
#endif
    const int result = std::fflush(native);
    if (stream) stream->SyncStatus();
    return result;
}

int APS5_VABI malloc_stats_fast_nid_postfix(void* stats) {
    return ApplicationHeapStatsFast_nid_no_patch(stats);
}

size_t APS5_VABI malloc_usable_size_nid_postfix(void* pointer) {
    return ApplicationHeapUsableSize_nid_no_patch(pointer);
}

void* APS5_VABI malloc_nid_postfix(size_t size) {
    return heapAllocation(size, [size] { return ApplicationHeapAllocate_nid_no_patch(size); });
}

void APS5_VABI free_nid_postfix(void* ptr) {
    ApplicationHeapFree_nid_no_patch(ptr);
}

void* APS5_VABI realloc_nid_postfix(void* ptr, size_t newSize) {
    return heapAllocation(newSize, [ptr, newSize] { return ApplicationHeapReallocate_nid_no_patch(ptr, newSize); });
}

void* APS5_VABI reallocf_nid_postfix(void* ptr, size_t newSize) {
    void* result = realloc_nid_postfix(ptr, newSize);
    if (result == nullptr && ptr != nullptr && newSize != 0) ApplicationHeapFree_nid_no_patch(ptr);
    return result;
}

void* APS5_VABI memalign_nid_postfix(size_t alignment, size_t size) {
    if (!ApplicationHeapDefault_nid_no_patch()) {
        if (!validAlignment(alignment)) {
            errno = 22;
            return nullptr;
        }
        return allocationOrNull([alignment, size] { return ApplicationHeapAlign_nid_no_patch(alignment, size); });
    }
    if (alignment >= DefaultHeapAlignmentLimit) return nullptr;
    const std::size_t rounded = defaultHeapAlignment(alignment);
    return defaultHeapAllocation(size, alignment, [rounded, size] { return ApplicationHeapAlign_nid_no_patch(rounded, size); });
}

void* APS5_VABI aligned_alloc_nid_postfix(size_t alignment, size_t size) {
    if (ApplicationHeapDefault_nid_no_patch() && !validAlignment(alignment)) return nullptr;
    return memalign_nid_postfix(alignment, size);
}

void* APS5_VABI reallocalign_nid_postfix(void* ptr, size_t size, size_t alignment) {
    if (!ApplicationHeapDefault_nid_no_patch()) {
        if (size != 0 && !validAlignment(alignment)) {
            errno = 22;
            return nullptr;
        }
        return allocationOrNull([ptr, size, alignment] { return ApplicationHeapRealign_nid_no_patch(ptr, size, alignment); });
    }
    if (alignment >= DefaultHeapAlignmentLimit) return nullptr;
    const std::size_t rounded = defaultHeapAlignment(alignment);
    return defaultHeapAllocation(size, alignment, [ptr, size, rounded] { return ApplicationHeapRealign_nid_no_patch(ptr, size, rounded); });
}

void* APS5_VABI calloc_nid_postfix(size_t count, size_t size) {
    const auto allocate = [count, size] { return ApplicationHeapCalloc_nid_no_patch(count, size); };
    if (!ApplicationHeapDefault_nid_no_patch()) return allocationOrNull(allocate);
    if (size != 0 && count > std::numeric_limits<size_t>::max() / size) return nullptr;
    return defaultHeapAllocation(count * size, 0, allocate);
}

int APS5_VABI posix_memalign_nid_postfix(void** pointer, size_t alignment, size_t size) {
    if (!pointer || alignment < sizeof(void*) || (alignment & (alignment - 1)) != 0) return 22;
    const int savedError = errno;
    const int result = ApplicationHeapPosixAlign_nid_no_patch(pointer, alignment, size);
    const bool failureSetsError = result != 0 && ApplicationHeapDefault_nid_no_patch() && alignment < DefaultHeapAlignmentLimit &&
        !defaultHeapRequestOverflows(size, alignment);
    errno = failureSetsError ? 22 : savedError;
    return result;
}

void* APS5_VABI bsearch_nid_postfix(const void* key, const void* base, size_t count,
    size_t size, int (APS5_VABI *compare)(const void*, const void*)) {
    if (count == 0) return nullptr;
    if (!key || !base || !compare || size == 0)
        throw std::invalid_argument("bsearch: invalid arguments");
    if (count > std::numeric_limits<size_t>::max() / size)
        throw std::overflow_error("bsearch: array size overflow");
    const auto* bytes = static_cast<const unsigned char*>(base);
    size_t first = 0;
    while (count != 0) {
        const size_t half = count / 2;
        const size_t middle = first + half;
        const auto* element = bytes + middle * size;
        const int result = compare(key, element);
        if (result == 0) return const_cast<unsigned char*>(element);
        if (result < 0) count = half;
        else { first = middle + 1; count -= half + 1; }
    }
    return nullptr;
}

int APS5_VABI mergesort_nid_postfix(void* base, size_t count, size_t size, GuestCompare compare) {
    if (size < 4) {
        errno = 22;
        return -1;
    }
    if (count == 0) return 0;
    if (!compare) throw std::invalid_argument("mergesort: null comparator");
    if (!base) throw std::invalid_argument("mergesort: null base");
    if (count > (std::numeric_limits<size_t>::max() - 8) / size) throw std::overflow_error("mergesort: array size overflow");
    auto* const scratch = static_cast<unsigned char*>(malloc_nid_postfix(count * size + 8));
    if (!scratch) return -1;
    auto* source = static_cast<unsigned char*>(base);
    auto* target = scratch;
    for (size_t width = 1; width < count; width *= 2) {
        for (size_t low = 0; low < count; low += 2 * width) {
            const size_t middle = std::min(low + width, count);
            const size_t high = std::min(low + 2 * width, count);
            size_t left = low, right = middle, out = low;
            while (left < middle && right < high) {
                const size_t taken = compare(source + left * size, source + right * size) <= 0 ? left++ : right++;
                std::memcpy(target + out++ * size, source + taken * size, size);
            }
            std::memcpy(target + out * size, source + left * size, (middle - left) * size);
            out += middle - left;
            std::memcpy(target + out * size, source + right * size, (high - right) * size);
        }
        std::swap(source, target);
    }
    if (source != base) std::memcpy(base, source, count * size);
    free_nid_postfix(scratch);
    return 0;
}

void APS5_VABI qsort_nid_postfix(void* base, size_t count, size_t size, int (APS5_VABI *compare)(const void*, const void*)) {
    if (!compare) throw std::invalid_argument("qsort: null comparator");
    if (size == 0) throw std::invalid_argument("qsort: zero element size");
    if (count == 0) return;
    if (!base) throw std::invalid_argument("qsort: null base");
    if (count > std::numeric_limits<size_t>::max() / size) throw std::overflow_error("qsort: array size overflow");
    if (count == 1) return;

    SortElements(static_cast<unsigned char*>(base), count, size, compare);
}

int APS5_VABI qsort_s_nid_postfix(void* base, size_t count, size_t size, GuestContextCompare compare, void* context) {
    constexpr size_t RsizeMax = std::numeric_limits<size_t>::max() >> 1;
    const char* const violation = count > RsizeMax ? "qsort_s : n > RSIZE_MAX"
        : size > RsizeMax ? "qsort_s : es > RSIZE_MAX"
        : count != 0 && !base ? "qsort_s : a == NULL"
        : count != 0 && !compare ? "qsort_s : cmp == NULL"
        : nullptr;
    if (violation) {
        SonyInvokeConstraintHandler(violation, nullptr, 22);
        return 22;
    }
    if (count > 1 && size != 0)
        SortElements(static_cast<unsigned char*>(base), count, size, [compare, context](const void* left, const void* right) { return compare(left, right, context); });
    return 0;
}

}
