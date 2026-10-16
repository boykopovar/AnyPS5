#include <algorithm>
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

#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/FilesystemError.hpp"
#include "prx/libc/include/AppArchive.hpp"
#include "prx/libc/include/GuestArena.hpp"

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

static std::FILE* OpenNative(const std::filesystem::path& path, const std::string& nativeMode) {
#ifdef _WIN32
    const char access = nativeMode[0];
    const bool update = nativeMode.find('+') != std::string::npos;
    const DWORD desired = update ? GENERIC_READ | GENERIC_WRITE : access == 'r' ? GENERIC_READ : GENERIC_WRITE;
    const DWORD disposition = access == 'r' ? OPEN_EXISTING
        : nativeMode.find('x') != std::string::npos ? CREATE_NEW : access == 'w' ? CREATE_ALWAYS : OPEN_ALWAYS;
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    const auto handle = CreateFileW(path.c_str(), desired, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, &security,
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

#ifdef _WIN32
static bool AppendThroughDescriptor(int descriptor) {
    const auto handle = reinterpret_cast<HANDLE>(::_get_osfhandle(descriptor));
    if (handle == INVALID_HANDLE_VALUE) return false;
    if (descriptor <= 2 || GetFileType(handle) != FILE_TYPE_DISK) return true;
    const int translation = ::_setmode(descriptor, _O_BINARY);
    if (translation == -1 || ::_setmode(descriptor, translation) == -1) return false;
    HANDLE copy = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), handle, GetCurrentProcess(), &copy, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
        errno = EBADF;
        return false;
    }
    const int appending = ::_open_osfhandle(reinterpret_cast<std::intptr_t>(copy), _O_APPEND | translation);
    if (appending < 0) {
        CloseHandle(copy);
        return false;
    }
    const int moved = ::_dup2(appending, descriptor);
    const int error = errno;
    ::_close(appending);
    errno = error;
    return moved == 0;
}

static bool IsPipeStream(std::FILE* native) {
    return GetFileType(reinterpret_cast<HANDLE>(::_get_osfhandle(_fileno(native)))) == FILE_TYPE_PIPE;
}
#endif

using GuestCompare = int (APS5_VABI *)(const void*, const void*);

static void SwapElements(unsigned char* left, unsigned char* right, size_t size) {
    for (size_t index = 0; index < size; ++index) std::swap(left[index], right[index]);
}

static void SwapRanges(unsigned char* left, unsigned char* right, size_t bytes) {
    if (bytes > 0) SwapElements(left, right, bytes);
}

static unsigned char* MedianOfThree(unsigned char* a, unsigned char* b, unsigned char* c, GuestCompare compare) {
    return compare(a, b) < 0
        ? (compare(b, c) < 0 ? b : (compare(a, c) < 0 ? c : a))
        : (compare(b, c) > 0 ? b : (compare(a, c) < 0 ? a : c));
}

static void InsertionSort(unsigned char* base, size_t count, size_t size, GuestCompare compare) {
    for (unsigned char* next = base + size; next < base + count * size; next += size)
        for (unsigned char* element = next; element > base && compare(element - size, element) > 0; element -= size)
            SwapElements(element, element - size, size);
}

static void SortElements(unsigned char* base, size_t count, size_t size, GuestCompare compare) {
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
void* allocationOrNull(TAllocate allocate) {
    try {
        return allocate();
    } catch (const std::bad_alloc&) {
    } catch (const std::length_error&) {
    }
    errno = 12;
    return nullptr;
}

bool validAlignment(std::size_t alignment) {
    return alignment != 0 && (alignment & (alignment - 1)) == 0;
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

FileStream* APS5_VABI freopen_nid_postfix(const char* filename, const char* mode, FileStream* stream) {
    if (!stream || !mode) { errno = 22; return nullptr; }
    if (!filename) { errno = 45; return nullptr; } // Mode-only reopening is not supported.
    const auto parsed = ParseFileMode(mode);
    if (!parsed) { errno = 22; return nullptr; }
    try {
        std::filesystem::path path;
        if (*filename) {
            if (const int error = ResolvePathError_nid_no_patch(filename, path)) {
                stream->Close();
                if (stream->IsDynamic()) delete stream;
                errno = error;
                return nullptr;
            }
        }
        if (stream->Reopen(path, parsed->native.c_str())) {
            if (parsed->append) static_cast<void>(std::fseek(stream->GetHandle(), 0, SEEK_END));
            if (!path.empty() && parsed->writes) RecordWrittenPath_nid_no_patch(path);
            return stream;
        }
        const int error = errno;
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
    std::filesystem::path fpath;
    if (const int error = ResolvePathError_nid_no_patch(filename, fpath)) { errno = error; return nullptr; }
    std::unique_ptr<std::FILE, int (*)(std::FILE*)> handle(OpenNative(fpath, parsed->native), std::fclose);
    if (!handle) {
        const int error = errno;
        errno = error;
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
    constexpr int GuestEnoent = 2;
    if (!result) return GuestEinval;
    *result = nullptr;
    if (!filename || !mode) return GuestEinval;
    *result = fopen_nid_postfix(filename, mode);
    return *result ? 0 : GuestEnoent;
}

int APS5_VABI fclose_nid_postfix(FileStream* stream) {
    if (stream && !stream->IsOpen()) {
        std::unique_ptr<FileStream> owner(stream->IsDynamic() ? stream : nullptr);
        errno = 9;
        return -1;
    }
    GetNativeStream(stream);
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
        {In | Out | Binary, "r+b"}, {In | Out | Trunc | Binary, "w+b"}, {In | Out | App | Binary, "a+b"}};
    const int open = (mode & (In | Out | App | Trunc | Binary)) | ((mode & Nocreate) ? In : 0) | ((mode & App) ? Out : 0);
    const char* openMode = nullptr;
    for (const auto& [flags, text] : modes) if (flags == open) openMode = text;
    if (!openMode) return nullptr;
    if ((mode & Noreplace) && (open & Out)) {
        if (auto* existing = fopen_nid_postfix(filename, "r")) {
            fclose_nid_postfix(existing);
            return nullptr;
        }
    }
    auto* stream = fopen_nid_postfix(filename, openMode);
    if (!stream || !(mode & Ate) || fseek_nid_postfix(stream, 0, SEEK_END) == 0) return stream;
    fclose_nid_postfix(stream);
    return nullptr;
}

size_t APS5_VABI fread_nid_postfix(void* buffer, size_t size, size_t count, FileStream* stream) {
    if (stream && !stream->IsOpen()) { errno = 9; return 0; }
    auto* handle = GetNativeStream(stream);
    if (size == 0 || count == 0) return 0;
    if (!buffer) throw std::runtime_error("fread: null buffer");
    if (count > std::numeric_limits<std::size_t>::max() / size) throw std::overflow_error("fread: buffer size overflow");
    const auto bytes = size * count;
    if (bytes > std::numeric_limits<std::uintptr_t>::max() - reinterpret_cast<std::uintptr_t>(buffer)) throw std::overflow_error("fread: buffer address overflow");
    const GuestArena::HostWrite destination(buffer, bytes);
    if (!destination.Open()) throw std::runtime_error("fread: buffer is not writable");
    const auto result = std::fread(buffer, size, count, handle);
    stream->SyncStatus();
    return result;
}

size_t APS5_VABI fwrite_nid_postfix(const void* buffer, size_t size, size_t count, FileStream* stream) {
    if (stream && !stream->IsOpen()) { errno = 9; return 0; }
    auto* handle = GetNativeStream(stream);
    if (size == 0 || count == 0) return 0;
    if (!buffer) throw std::runtime_error("fwrite: null buffer");
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
    std::FILE* native = stream ? GetNativeStream(stream) : nullptr;
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
    return allocationOrNull([size] { return ApplicationHeapAllocate_nid_no_patch(size); });
}

void APS5_VABI free_nid_postfix(void* ptr) {
    ApplicationHeapFree_nid_no_patch(ptr);
}

void* APS5_VABI realloc_nid_postfix(void* ptr, size_t newSize) {
    return allocationOrNull([ptr, newSize] { return ApplicationHeapReallocate_nid_no_patch(ptr, newSize); });
}

void* APS5_VABI reallocf_nid_postfix(void* ptr, size_t newSize) {
    try {
        return ApplicationHeapReallocate_nid_no_patch(ptr, newSize);
    } catch (const std::bad_alloc&) {
    } catch (const std::length_error&) {
    }
    ApplicationHeapFree_nid_no_patch(ptr);
    errno = 12;
    return nullptr;
}

void* APS5_VABI memalign_nid_postfix(size_t alignment, size_t size) {
    if (!validAlignment(alignment)) {
        errno = 22;
        return nullptr;
    }
    return allocationOrNull([alignment, size] { return ApplicationHeapAlign_nid_no_patch(alignment, size); });
}

void* APS5_VABI aligned_alloc_nid_postfix(size_t alignment, size_t size) {
    return memalign_nid_postfix(alignment, size);
}

void* APS5_VABI reallocalign_nid_postfix(void* ptr, size_t size, size_t alignment) {
    if (size != 0 && !validAlignment(alignment)) {
        errno = 22;
        return nullptr;
    }
    return allocationOrNull([ptr, size, alignment] { return ApplicationHeapRealign_nid_no_patch(ptr, size, alignment); });
}

void* APS5_VABI calloc_nid_postfix(size_t count, size_t size) {
    return allocationOrNull([count, size] { return ApplicationHeapCalloc_nid_no_patch(count, size); });
}

int APS5_VABI posix_memalign_nid_postfix(void** pointer, size_t alignment, size_t size) {
    if (!pointer || alignment < sizeof(void*) || (alignment & (alignment - 1)) != 0) return 22;
    const int savedError = errno;
    const int result = ApplicationHeapPosixAlign_nid_no_patch(pointer, alignment, size);
    errno = savedError;
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

void APS5_VABI qsort_nid_postfix(void* base, size_t count, size_t size, int (APS5_VABI *compare)(const void*, const void*)) {
    if (!compare) throw std::invalid_argument("qsort: null comparator");
    if (size == 0) throw std::invalid_argument("qsort: zero element size");
    if (count == 0) return;
    if (!base) throw std::invalid_argument("qsort: null base");
    if (count > std::numeric_limits<size_t>::max() / size) throw std::overflow_error("qsort: array size overflow");
    if (count == 1) return;

    SortElements(static_cast<unsigned char*>(base), count, size, compare);
}

}
