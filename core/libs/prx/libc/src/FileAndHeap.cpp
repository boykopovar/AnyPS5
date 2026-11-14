#include "prx/libc/include/Api.hpp"
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
#include <mutex>
#include <new>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <cerrno>
#include <cstring>
#include <sys/stat.h>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#include "prx/libc/include/WindowsDescriptor.hpp"
#include "prx/libc/include/WindowsVerbatimPath.hpp"
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
    std::error_code pathError;
    const auto resolved = NativeVerbatimPath_nid_no_patch(path, pathError);
    if (pathError) { errno = pathError.default_error_condition().value(); return std::nullopt; }
    return resolved.native();
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

static void ReentrantSwapElements(unsigned char* left, unsigned char* right, size_t size) {
    for (size_t index = 0; index < size; ++index) std::swap(left[index], right[index]);
}

static void ReentrantSwapRanges(unsigned char* left, unsigned char* right, size_t bytes) {
    if (bytes > 0) ReentrantSwapElements(left, right, bytes);
}

template <typename Compare>
static unsigned char* ReentrantMedianOfThree(unsigned char* a, unsigned char* b, unsigned char* c, Compare compare) {
    return compare(a, b) < 0
        ? (compare(b, c) < 0 ? b : (compare(a, c) < 0 ? c : a))
        : (compare(b, c) > 0 ? b : (compare(a, c) < 0 ? a : c));
}

template <typename Compare>
static void ReentrantInsertionSort(unsigned char* base, size_t count, size_t size, Compare compare) {
    for (unsigned char* next = base + size; next < base + count * size; next += size)
        for (unsigned char* element = next; element > base && compare(element - size, element) > 0; element -= size)
            ReentrantSwapElements(element, element - size, size);
}

template <typename Compare>
static void ReentrantSortElements(unsigned char* base, size_t count, size_t size, Compare compare) {
    for (;;) {
        if (count < 7) {
            ReentrantInsertionSort(base, count, size, compare);
            return;
        }
        unsigned char* middle = base + (count / 2) * size;
        if (count > 7) {
            unsigned char* low = base;
            unsigned char* high = base + (count - 1) * size;
            if (count > 40) {
                const size_t step = (count / 8) * size;
                low = ReentrantMedianOfThree(low, low + step, low + 2 * step, compare);
                middle = ReentrantMedianOfThree(middle - step, middle, middle + step, compare);
                high = ReentrantMedianOfThree(high - 2 * step, high - step, high, compare);
            }
            middle = ReentrantMedianOfThree(low, middle, high, compare);
        }
        ReentrantSwapElements(base, middle, size);
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
                    ReentrantSwapElements(equalLow, scanLow, size);
                    equalLow += size;
                }
                scanLow += size;
            }
            while (scanLow <= scanHigh && (result = compare(scanHigh, base)) >= 0) {
                if (result == 0) {
                    swapped = true;
                    ReentrantSwapElements(scanHigh, equalHigh, size);
                    equalHigh -= size;
                }
                scanHigh -= size;
            }
            if (scanLow > scanHigh) break;
            ReentrantSwapElements(scanLow, scanHigh, size);
            swapped = true;
            scanLow += size;
            scanHigh -= size;
        }
        if (!swapped) {
            ReentrantInsertionSort(base, count, size, compare);
            return;
        }
        unsigned char* const end = base + count * size;
        size_t moved = std::min<size_t>(equalLow - base, scanLow - equalLow);
        ReentrantSwapRanges(base, scanLow - moved, moved);
        moved = std::min<size_t>(equalHigh - scanHigh, end - equalHigh - size);
        ReentrantSwapRanges(scanLow, end - moved, moved);
        const size_t lowBytes = scanLow - equalLow;
        const size_t highBytes = equalHigh - scanHigh;
        if (lowBytes <= highBytes) {
            if (lowBytes > size) ReentrantSortElements(base, lowBytes / size, size, compare);
            if (highBytes <= size) return;
            base = end - highBytes;
            count = highBytes / size;
        } else {
            if (highBytes > size) ReentrantSortElements(end - highBytes, highBytes / size, size, compare);
            if (lowBytes <= size) return;
            count = lowBytes / size;
        }
    }
}

static void SwapElements(unsigned char* left, unsigned char* right, size_t size) {
    if (left == right) return;
    std::swap_ranges(left, left + size, right);
}

static void RotateRight(unsigned char* first, unsigned char* last, size_t size) {
    unsigned char buffer[256];
    for (size_t offset = 0; offset < size; offset += sizeof(buffer)) {
        const size_t chunk = std::min(sizeof(buffer), size - offset);
        std::memcpy(buffer, last + offset, chunk);
        for (unsigned char* element = last; element != first; element -= size) std::memcpy(element + offset, element - size + offset, chunk);
        std::memcpy(first + offset, buffer, chunk);
    }
}

template <typename Compare>
static void MedianOfThree(unsigned char* first, unsigned char* middle, unsigned char* last, size_t size, Compare& compare) {
    if (compare(middle, first) < 0) SwapElements(middle, first, size);
    if (compare(last, middle) < 0) SwapElements(last, middle, size);
    if (compare(middle, first) < 0) SwapElements(middle, first, size);
}

template <typename Compare>
static void Median(unsigned char* first, unsigned char* middle, unsigned char* last, size_t size, Compare& compare) {
    const size_t distance = static_cast<size_t>(last - first) / size;
    if (distance <= 40) {
        MedianOfThree(first, middle, last, size, compare);
        return;
    }
    const size_t step = (distance / 8 + 1) * size;
    MedianOfThree(first, first + step, first + 2 * step, size, compare);
    MedianOfThree(middle - step, middle, middle + step, size, compare);
    MedianOfThree(last - 2 * step, last - step, last, size, compare);
    MedianOfThree(first + step, middle, last - step, size, compare);
}

template <typename Compare>
static void AdjustHeap(unsigned char* base, size_t hole, size_t count, size_t size, Compare& compare) {
    const auto at = [base, size](size_t index) { return base + index * size; };
    size_t index = hole;
    while (2 * index + 2 <= count) {
        size_t child = 2 * index + 2;
        if (child == count || compare(at(child), at(child - 1)) < 0) --child;
        SwapElements(at(index), at(child), size);
        index = child;
    }
    while (index > hole) {
        const size_t parent = (index - 1) / 2;
        if (compare(at(index), at(parent)) <= 0) break;
        SwapElements(at(parent), at(index), size);
        index = parent;
    }
}

template <typename Compare>
static void HeapSort(unsigned char* base, size_t count, size_t size, Compare& compare) {
    for (size_t hole = count / 2; hole-- > 0;) AdjustHeap(base, hole, count, size, compare);
    for (size_t remaining = count; remaining >= 2;) {
        SwapElements(base, base + (remaining - 1) * size, size);
        if (--remaining < 2) break;
        AdjustHeap(base, 0, remaining, size, compare);
    }
}

template <typename Compare>
static void InsertionSort(unsigned char* base, size_t count, size_t size, Compare& compare) {
    for (size_t index = 1; index < count; ++index) {
        unsigned char* const next = base + index * size;
        if (compare(next, base) < 0) {
            RotateRight(base, next, size);
            continue;
        }
        unsigned char* stop = next - size;
        while (compare(next, stop) < 0) stop -= size;
        if (stop + size != next) RotateRight(stop + size, next, size);
    }
}

template <typename Compare>
static void SortElements(unsigned char* first, size_t count, size_t ideal, size_t size, Compare& compare) {
    while (ideal != 0 && count >= 33) {
        unsigned char* const last = first + count * size;
        unsigned char* const middle = first + count / 2 * size;
        Median(first, middle, last - size, size, compare);
        unsigned char* pivotFirst = middle;
        while (first < pivotFirst && compare(pivotFirst - size, pivotFirst) == 0) pivotFirst -= size;
        unsigned char* pivotLast = middle + size;
        while (pivotLast < last && compare(pivotLast, pivotFirst) == 0) pivotLast += size;
        unsigned char* greaterFirst = pivotLast;
        unsigned char* greaterLast = pivotFirst;
        for (;;) {
            for (; greaterFirst < last; greaterFirst += size) {
                const int result = compare(pivotFirst, greaterFirst);
                if (result < 0) continue;
                if (result > 0) break;
                SwapElements(pivotLast, greaterFirst, size);
                pivotLast += size;
            }
            for (; first < greaterLast; greaterLast -= size) {
                const int result = compare(greaterLast - size, pivotFirst);
                if (result < 0) continue;
                if (result > 0) break;
                pivotFirst -= size;
                SwapElements(pivotFirst, greaterLast - size, size);
            }
            if (greaterLast == first && greaterFirst == last) break;
            if (greaterLast == first) {
                if (pivotLast != greaterFirst) SwapElements(pivotFirst, pivotLast, size);
                SwapElements(pivotFirst, greaterFirst, size);
                greaterFirst += size;
                pivotFirst += size;
                pivotLast += size;
            } else if (greaterFirst == last) {
                greaterLast -= size;
                pivotFirst -= size;
                pivotLast -= size;
                if (greaterLast != pivotFirst) SwapElements(greaterLast, pivotFirst, size);
                SwapElements(pivotFirst, pivotLast, size);
            } else {
                greaterLast -= size;
                SwapElements(greaterFirst, greaterLast, size);
                greaterFirst += size;
            }
        }
        ideal = (ideal >> 1) + (ideal >> 2);
        const size_t lowCount = static_cast<size_t>(pivotFirst - first) / size;
        const size_t highCount = static_cast<size_t>(last - pivotLast) / size;
        if (lowCount > highCount) {
            SortElements(pivotLast, highCount, ideal, size, compare);
            count = lowCount;
        } else {
            SortElements(first, lowCount, ideal, size, compare);
            first = pivotLast;
            count = highCount;
        }
    }
    if (count >= 33) HeapSort(first, count, size, compare);
    else if (count >= 2) InsertionSort(first, count, size, compare);
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

FileStream* APS5_VABI fdopen_nid_postfix(int descriptor, const char* mode) {
    if (!mode) { errno = 22; return nullptr; }
    const auto parsed = ParseFileMode(mode);
    if (!parsed) { errno = 22; return nullptr; }
    if (descriptor < 0) return nullptr;
    AppArchiveRefuseDescriptor_nid_no_patch(descriptor, "fdopen");
    try {
        auto nativeMode = parsed->native;
        std::erase(nativeMode, 'x');
        const auto release = [](void* pointer) { ::operator delete(pointer); };
        std::unique_ptr<void, decltype(release)> storage(::operator new(sizeof(FileStream)), release);
        auto preallocatedGate = std::make_shared<std::recursive_mutex>();
#ifdef _WIN32
        const auto handle = WindowsDescriptorHandle_nid_no_patch(descriptor);
        if (handle == INVALID_HANDLE_VALUE) { errno = 9; return nullptr; }
        if (parsed->append && !AppendThroughDescriptor(descriptor)) return nullptr;
        auto* native = ::_fdopen(descriptor, nativeMode.c_str());
#else
        auto* native = ::fdopen(descriptor, nativeMode.c_str());
#endif
        if (!native) return nullptr;
        auto* stream = new (storage.get()) FileStream(native, true, FileStream::Registration::Deferred);
        stream->Register_nid_no_patch(std::move(preallocatedGate));
        storage.release();
        return stream;
    } catch (const std::bad_alloc&) { errno = 12; return nullptr; }
}

static bool IsRandomDevicePath(std::string_view path) {
    return path == "/dev/random" || path == "/dev/urandom";
}

#ifdef _WIN32
static const std::filesystem::path NullDevicePath = "NUL";
#else
static const std::filesystem::path NullDevicePath = "/dev/null";
#endif

static std::FILE* OpenNullDevice(const std::filesystem::path& = {}, const std::string& = {}) {
    return std::fopen(NullDevicePath.string().c_str(), "rb");
}

static int OpenFailure(const std::filesystem::path& path, bool writes, int hostError) {
    const int error = GuestErrnoFromHost_nid_no_patch(hostError);
    std::error_code status;
    return writes && error == 13 && std::filesystem::is_directory(path, status) ? 21 : error;
}

static void* (*g_beginClosingDescriptor)(int) = nullptr;
static void (*g_endClosingDescriptor)(void*) = nullptr;

void LibcRegisterClosedDescriptors_nid_no_patch(void* (*begin)(int), void (*end)(void*)) {
    g_beginClosingDescriptor = begin;
    g_endClosingDescriptor = end;
}

void* BeginClosingDescriptor_nid_no_patch(int descriptor) {
    return g_beginClosingDescriptor ? g_beginClosingDescriptor(descriptor) : nullptr;
}

void EndClosingDescriptor_nid_no_patch(void* closing) {
    if (g_endClosingDescriptor) g_endClosingDescriptor(closing);
}

#ifdef _WIN32
static int (*g_openDirectoryDescriptor)(const wchar_t*) = nullptr;
static void (*g_forgetDirectoryDescriptor)(int) = nullptr;
static void (*g_duplicateDirectoryDescriptor)(int, int) = nullptr;

void LibcRegisterDirectoryStreams_nid_no_patch(int (*open)(const wchar_t*), void (*forget)(int), void (*duplicate)(int, int)) {
    g_openDirectoryDescriptor = open;
    g_forgetDirectoryDescriptor = forget;
    g_duplicateDirectoryDescriptor = duplicate;
}

void ForgetDirectoryStream_nid_no_patch(int descriptor) {
    if (!g_forgetDirectoryDescriptor) throw std::runtime_error("ForgetDirectoryStream: libkernel has not registered its directory descriptors");
    g_forgetDirectoryDescriptor(descriptor);
}

void DuplicateDirectoryStream_nid_no_patch(int from, int to) {
    if (!g_duplicateDirectoryDescriptor) throw std::runtime_error("DuplicateDirectoryStream: libkernel has not registered its directory descriptors");
    g_duplicateDirectoryDescriptor(from, to);
}

static std::FILE* OpenDirectoryHandle(const std::filesystem::path& path, const std::string& = {}) {
    if (!g_openDirectoryDescriptor) throw std::runtime_error("OpenDirectoryHandle: libkernel has not registered its directory descriptors");
    const auto filename = HostOpenName(path);
    if (!filename) { errno = GuestErrnoFromHost_nid_no_patch(errno); return nullptr; }
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
    }
    return native;
}

static FileStream* OpenDirectoryStream(const std::filesystem::path& path) {
    const auto release = [](void* pointer) { ::operator delete(pointer); };
    std::unique_ptr<void, decltype(release)> storage(::operator new(sizeof(FileStream)), release);
    auto preallocatedGate = std::make_shared<std::recursive_mutex>();
    auto* native = OpenDirectoryHandle(path);
    if (!native) return nullptr;
    auto* stream = new (storage.get()) FileStream(native, true, FileStream::Registration::Deferred);
    stream->MarkDirectory();
    stream->Register_nid_no_patch(std::move(preallocatedGate));
    storage.release();
    return stream;
}
#endif

FileStream* APS5_VABI freopen_nid_postfix(const char* filename, const char* mode, FileStream* stream) {
    if (!stream || !mode) { errno = 22; return nullptr; }
    auto lease = AcquireFileStreamLease_nid_no_patch(stream, true);
    if (!lease.Valid()) { errno = 9; return nullptr; }
    try {
        std::lock_guard<std::recursive_mutex> gateLock(*lease.Gate());
        if (!filename) { errno = stream->IsOpen() ? 45 : 9; return nullptr; }
        const auto parsed = ParseFileMode(mode);
        if (!parsed) { errno = 22; return nullptr; }
        if (IsRandomDevicePath(filename)) {
            if (parsed->writes) throw std::runtime_error(std::string(__func__) + ": writing to " + filename + " is not implemented");
            const auto reservation = ReserveFileStreamReopen_nid_no_patch(stream);
            if (!reservation) { errno = 9; return nullptr; }
            if (!stream->Reopen(NullDevicePath, "rb", OpenNullDevice)) {
                const int error = errno;
                RetireFileStream_nid_no_patch(stream);
                CancelFileStreamReopenReservation_nid_no_patch(*reservation);
                errno = error;
                return nullptr;
            }
            stream->MarkRandomDevice();
            SynchronizeFileStreamAliases_nid_no_patch(stream);
            PublishFileStreamReopen_nid_no_patch(stream, *reservation);
            return stream;
        }
        std::filesystem::path path;
        if (const int error = *filename ? ResolvePathError_nid_no_patch(filename, path) : 2) {
            if (stream->IsHostStandard()) stream->Detach();
            else if (stream->IsOpen()) static_cast<void>(stream->CloseNativeUnderGate_nid_no_patch());
            RetireFileStream_nid_no_patch(stream);
            errno = error;
            return nullptr;
        }
        const auto reservation = ReserveFileStreamReopen_nid_no_patch(stream);
        if (!reservation) { errno = 9; return nullptr; }
        struct ReopenCleanup {
            FileStream* stream;
            std::uint64_t reservation;
            bool armed = false;
            bool committed = false;
            ~ReopenCleanup() {
                if (committed) return;
                const int error = errno;
                if (armed) {
                    if (stream->IsOpen()) static_cast<void>(stream->CloseNativeUnderGate_nid_no_patch());
                    RetireFileStream_nid_no_patch(stream);
                }
                CancelFileStreamReopenReservation_nid_no_patch(reservation);
                errno = error;
            }
        } cleanup{stream, *reservation, true};
#ifdef _WIN32
        const bool directory = !parsed->writes && [&] {
            std::error_code status;
            return std::filesystem::is_directory(path, status);
        }();
        bool reopened = stream->Reopen(path, parsed->native.c_str(), directory ? OpenDirectoryHandle : OpenNative, directory);
        if (reopened && directory) {
            stream->MarkDirectory();
            SynchronizeFileStreamAliases_nid_no_patch(stream);
        }
#else
        bool reopened = stream->Reopen(path, parsed->native.c_str(), OpenNative);
#endif
        cleanup.armed = reopened;
        int captured = 0;
#ifdef _WIN32
        if (reopened && parsed->append && !AppendOnlyDescriptor(_fileno(stream->GetHandle()))) {
            captured = errno;
            static_cast<void>(stream->CloseNativeUnderGate_nid_no_patch());
            reopened = false;
        }
#endif
        if (reopened) {
            if (parsed->append) static_cast<void>(std::fseek(stream->GetHandle(), 0, SEEK_END));
            if (!path.empty() && parsed->writes) RecordWrittenPath_nid_no_patch(path);
            PublishFileStreamReopen_nid_no_patch(stream, *reservation);
            cleanup.committed = true;
            return stream;
        }
        if (captured == 0) captured = errno;
        RetireFileStream_nid_no_patch(stream);
        int error = OpenFailure(path, parsed->writes, captured);
        if (error == 2) error = NotFoundError_nid_no_patch(path);
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
    if (IsRandomDevicePath(filename)) {
        if (parsed->writes) throw std::runtime_error(std::string(__func__) + ": writing to " + filename + " is not implemented");
        std::unique_ptr<std::FILE, int (*)(std::FILE*)> device(OpenNullDevice(), std::fclose);
        if (!device) return nullptr;
        auto preallocatedGate = std::make_shared<std::recursive_mutex>();
        auto stream = std::make_unique<FileStream>(device.get(), true, FileStream::Registration::Deferred);
        stream->MarkRandomDevice();
        stream->Register_nid_no_patch(std::move(preallocatedGate));
        device.release();
        return stream.release();
    }
    std::filesystem::path fpath;
    if (const int error = ResolvePathError_nid_no_patch(filename, fpath)) { errno = error; return nullptr; }
    std::unique_ptr<std::FILE, int (*)(std::FILE*)> handle(OpenNative(fpath, parsed->native), std::fclose);
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
    auto preallocatedGate = std::make_shared<std::recursive_mutex>();
    auto stream = std::make_unique<FileStream>(handle.get(), true, FileStream::Registration::Deferred);
    stream->Register_nid_no_patch(std::move(preallocatedGate));
    handle.release();
    return stream.release();
}

int APS5_VABI fopen_s_nid_postfix(FileStream** result, const char* filename, const char* mode) {
    constexpr int GuestEinval = 22;
    if (!result) {
        SonyInvokeConstraintHandler("fopen_s: bad FILE ** argument", nullptr, GuestEinval);
        return GuestEinval;
    }
    *result = nullptr;
    if (!filename || !mode) {
        SonyInvokeConstraintHandler("fopen_s: bad argument", nullptr, GuestEinval);
        return GuestEinval;
    }
    *result = fopen_nid_postfix(filename, mode);
    return *result ? 0 : errno;
}

int APS5_VABI freopen_s_nid_postfix(FileStream** result, const char* filename, const char* mode, FileStream* stream) {
    constexpr int GuestEinval = 22;
    if (!result) {
        SonyInvokeConstraintHandler("fopen_s: bad FILE ** argument", nullptr, GuestEinval);
        return GuestEinval;
    }
    *result = nullptr;
    if (!mode || !stream) {
        SonyInvokeConstraintHandler("fopen_s: bad argument", nullptr, GuestEinval);
        return GuestEinval;
    }
    *result = freopen_nid_postfix(filename, mode, stream);
    return *result ? 0 : errno;
}

}

extern "C" std::recursive_mutex& FileStreamRegistryMutex_nid_no_patch() {
    static std::recursive_mutex mutex;
    return mutex;
}

extern "C" FileStream*& FileStreamRegistryHead_nid_no_patch() {
    static FileStream* head = nullptr;
    return head;
}

namespace {

std::uint64_t& NextRegistryIdRef_nid_no_patch() {
    static std::uint64_t next = 1;
    return next;
}

std::uint64_t& ReservedRegistryIdsRef_nid_no_patch() {
    static std::uint64_t reserved = 0;
    return reserved;
}

}

void RegisterFileStream_nid_no_patch(FileStream* stream, std::shared_ptr<std::recursive_mutex> preallocatedGate) {
    std::lock_guard<std::recursive_mutex> lock(FileStreamRegistryMutex_nid_no_patch());
    if (stream->_registered_nid_no_patch) return;
    std::shared_ptr<std::recursive_mutex> gate;
    std::FILE* const native = stream->RawHandle_nid_no_patch();
    for (FileStream* cur = FileStreamRegistryHead_nid_no_patch(); cur != nullptr && !gate; cur = cur->_nextStream_nid_no_patch) {
        if (cur == stream) continue;
        if (cur->_gate_nid_no_patch && cur->RawHandle_nid_no_patch() == native) gate = cur->_gate_nid_no_patch;
    }
    if (!gate) gate = preallocatedGate ? std::move(preallocatedGate) : std::make_shared<std::recursive_mutex>();
    auto& nextId = NextRegistryIdRef_nid_no_patch();
    if (nextId == 0 || ReservedRegistryIdsRef_nid_no_patch() > std::numeric_limits<std::uint64_t>::max() - nextId)
        throw std::runtime_error("FileStream: registry id overflow");
    stream->_gate_nid_no_patch = std::move(gate);
    stream->_registryId_nid_no_patch = nextId++;
    stream->_nextStream_nid_no_patch = FileStreamRegistryHead_nid_no_patch();
    FileStreamRegistryHead_nid_no_patch() = stream;
    stream->_registered_nid_no_patch = true;
    stream->_retired_nid_no_patch = false;
}

void UnlinkFileStream_nid_no_patch(FileStream* stream) {
    std::lock_guard<std::recursive_mutex> lock(FileStreamRegistryMutex_nid_no_patch());
    for (FileStream** link = &FileStreamRegistryHead_nid_no_patch(); *link != nullptr; link = &(*link)->_nextStream_nid_no_patch) {
        if (*link == stream) {
            *link = stream->_nextStream_nid_no_patch;
            stream->_nextStream_nid_no_patch = nullptr;
            stream->_registered_nid_no_patch = false;
            return;
        }
    }
}

std::shared_ptr<std::recursive_mutex> PinFileStreamIfValid_nid_no_patch(FileStream* stream, bool allowRetiredStatic) {
    if (!stream) return nullptr;
    std::lock_guard<std::recursive_mutex> lock(FileStreamRegistryMutex_nid_no_patch());
    for (FileStream* cur = FileStreamRegistryHead_nid_no_patch(); cur != nullptr; cur = cur->_nextStream_nid_no_patch) {
        if (cur == stream) {
            if (stream->_retired_nid_no_patch && (!allowRetiredStatic || stream->IsDynamic())) return nullptr;
            if (stream->_pinCount_nid_no_patch == std::numeric_limits<std::uint32_t>::max())
                throw std::runtime_error("FileStream: pin count overflow");
            ++stream->_pinCount_nid_no_patch;
            return stream->_gate_nid_no_patch;
        }
    }
    return nullptr;
}

void UnpinFileStream_nid_no_patch(FileStream* stream) {
    if (!stream) return;
    bool destroy = false;
    {
        std::lock_guard<std::recursive_mutex> lock(FileStreamRegistryMutex_nid_no_patch());
        if (stream->_pinCount_nid_no_patch > 0) --stream->_pinCount_nid_no_patch;
        if (stream->_pinCount_nid_no_patch == 0 && stream->_retired_nid_no_patch
            && !stream->_registered_nid_no_patch && stream->IsDynamic()) destroy = true;
    }
    if (destroy) delete stream;
}

void RetireFileStream_nid_no_patch(FileStream* stream) {
    if (!stream) return;
    std::lock_guard<std::recursive_mutex> lock(FileStreamRegistryMutex_nid_no_patch());
    stream->_retired_nid_no_patch = true;
    if (stream->IsDynamic() && stream->_registered_nid_no_patch) UnlinkFileStream_nid_no_patch(stream);
}

std::optional<std::uint64_t> ReserveFileStreamReopen_nid_no_patch(FileStream* stream) {
    std::lock_guard<std::recursive_mutex> lock(FileStreamRegistryMutex_nid_no_patch());
    if (!stream->_registered_nid_no_patch || (stream->_retired_nid_no_patch && stream->IsDynamic())) return std::nullopt;
    if (!stream->_retired_nid_no_patch) return 0;
    const auto nextId = NextRegistryIdRef_nid_no_patch();
    auto& reserved = ReservedRegistryIdsRef_nid_no_patch();
    if (nextId == 0 || reserved >= std::numeric_limits<std::uint64_t>::max() - nextId + 1)
        throw std::runtime_error("FileStream: registry id overflow");
    ++reserved;
    return 1;
}

void PublishFileStreamReopen_nid_no_patch(FileStream* stream, std::uint64_t reservation) {
    if (reservation == 0) return;
    std::lock_guard<std::recursive_mutex> lock(FileStreamRegistryMutex_nid_no_patch());
    --ReservedRegistryIdsRef_nid_no_patch();
    stream->_registryId_nid_no_patch = NextRegistryIdRef_nid_no_patch()++;
    stream->_retired_nid_no_patch = false;
}

void CancelFileStreamReopenReservation_nid_no_patch(std::uint64_t reservation) {
    if (reservation == 0) return;
    std::lock_guard<std::recursive_mutex> lock(FileStreamRegistryMutex_nid_no_patch());
    --ReservedRegistryIdsRef_nid_no_patch();
}

bool IsFileStreamInSnapshot_nid_no_patch(FileStream* stream, std::uint64_t snapshot) {
    std::lock_guard<std::recursive_mutex> lock(FileStreamRegistryMutex_nid_no_patch());
    return stream->_registered_nid_no_patch && !stream->_retired_nid_no_patch
        && stream->_registryId_nid_no_patch <= snapshot;
}

std::pair<FileStream*, std::shared_ptr<std::recursive_mutex>>
PinNextFileStream_nid_no_patch(std::uint64_t& cursor) {
    std::lock_guard<std::recursive_mutex> lock(FileStreamRegistryMutex_nid_no_patch());
    FileStream* best = nullptr;
    for (FileStream* cur = FileStreamRegistryHead_nid_no_patch(); cur != nullptr; cur = cur->_nextStream_nid_no_patch) {
        if (cur->_retired_nid_no_patch) continue;
        if (cur->_registryId_nid_no_patch > cursor) continue;
        if (!best || cur->_registryId_nid_no_patch > best->_registryId_nid_no_patch) best = cur;
    }
    if (!best) return {nullptr, nullptr};
    if (best->_pinCount_nid_no_patch == std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("FileStream: pin count overflow");
    ++best->_pinCount_nid_no_patch;
    cursor = best->_registryId_nid_no_patch > 0 ? best->_registryId_nid_no_patch - 1 : 0;
    return {best, best->_gate_nid_no_patch};
}

void DetachFileStreamAliasesForNative_nid_no_patch(FileStream* source, std::FILE* native) {
    std::lock_guard<std::recursive_mutex> lock(FileStreamRegistryMutex_nid_no_patch());
    for (FileStream* cur = FileStreamRegistryHead_nid_no_patch(); cur != nullptr; cur = cur->_nextStream_nid_no_patch) {
        if (cur == source) continue;
        if (cur->RawHandle_nid_no_patch() == native) cur->Detach();
    }
}

void SynchronizeFileStreamAliases_nid_no_patch(FileStream* source) {
    std::lock_guard<std::recursive_mutex> lock(FileStreamRegistryMutex_nid_no_patch());
    for (FileStream* cur = FileStreamRegistryHead_nid_no_patch(); cur != nullptr; cur = cur->_nextStream_nid_no_patch) {
        if (cur == source || cur->_retired_nid_no_patch || cur->_gate_nid_no_patch != source->_gate_nid_no_patch) continue;
        cur->StoreHandleAtomic_nid_no_patch(source->RawHandle_nid_no_patch());
        cur->_guest = source->_guest;
        cur->guestError = source->guestError;
        cur->randomDevice = source->randomDevice;
        cur->_orientation.store(source->_orientation.load(std::memory_order_relaxed), std::memory_order_relaxed);
        cur->directory = source->directory;
        cur->lowSurrogate = source->lowSurrogate;
        cur->pushedBack = source->pushedBack;
    }
}

extern "C" {

int APS5_VABI fclose_nid_postfix(FileStream* stream) {
    if (!stream) { errno = 9; return -1; }
    auto lease = AcquireFileStreamLease_nid_no_patch(stream);
    if (!lease.Valid()) { errno = 9; return -1; }
    int result;
    {
        std::lock_guard<std::recursive_mutex> gateLock(*lease.Gate());
        if (!stream->IsOpen()) {
            errno = 9;
            result = -1;
        } else {
            result = stream->CloseNativeUnderGate_nid_no_patch();
        }
        RetireFileStream_nid_no_patch(stream);
    }
    return result;
}

void APS5_VABI fcloseall_nid_postfix() {
    std::uint64_t snapshot;
    std::FILE* const originalStdout = stdout;
    std::FILE* const originalStderr = stderr;
    {
        std::lock_guard<std::recursive_mutex> lock(FileStreamRegistryMutex_nid_no_patch());
        const auto nextId = NextRegistryIdRef_nid_no_patch();
        snapshot = nextId > 0 ? nextId - 1 : std::numeric_limits<std::uint64_t>::max();
    }
    std::uint64_t preservationCursor = snapshot;
    for (;;) {
        auto [selected, gate] = PinNextFileStream_nid_no_patch(preservationCursor);
        if (!selected) break;
        FileStreamLease_nid_no_patch lease(selected, gate);
        std::lock_guard<std::recursive_mutex> gateLock(*gate);
        if (IsFileStreamInSnapshot_nid_no_patch(selected, snapshot)) {
            std::FILE* const native = selected->RawHandle_nid_no_patch();
            if (native == originalStdout && KeepHostStandardOutput_nid_no_patch() < 0)
                throw std::runtime_error("fcloseall: cannot preserve the host standard output");
            if (native == originalStderr && KeepHostStandardError_nid_no_patch() < 0)
                throw std::runtime_error("fcloseall: cannot preserve the host standard error");
        }
        if (preservationCursor == 0) break;
    }
    std::uint64_t cursor = snapshot;
    for (;;) {
        auto [selected, gate] = PinNextFileStream_nid_no_patch(cursor);
        if (!selected) break;
        FileStreamLease_nid_no_patch lease(selected, gate);
        {
            std::lock_guard<std::recursive_mutex> gateLock(*gate);
            if (IsFileStreamInSnapshot_nid_no_patch(selected, snapshot)) {
                if (selected->IsOpen()) static_cast<void>(selected->CloseNativeUnderGate_nid_no_patch());
                RetireFileStream_nid_no_patch(selected);
            }
        }
        if (cursor == 0) break;
    }
}

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
    if (size == 0 || count == 0) return 0;
    if (count > std::numeric_limits<std::size_t>::max() / size) { errno = EINVAL; return 0; }
    if (stream && !stream->IsOpen()) { errno = 9; return 0; }
    auto* handle = GetStreamHandle(stream);
    stream->Orientation(-1);
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
    const int previousError = errno;
    errno = 0;
    if (!stream->SwitchToReading()) { errno = errno != 0 ? GuestErrnoFromHost_nid_no_patch(errno) : previousError; return 0; }
    const auto result = std::fread(buffer, size, count, handle);
    const int nativeError = errno;
    stream->SyncStatus();
    if (nativeError != 0) errno = GuestErrnoFromHost_nid_no_patch(nativeError);
    else errno = previousError;
    stream->KeepDirectoryReadError(result < count);
    return result;
}

size_t APS5_VABI fwrite_nid_postfix(const void* buffer, size_t size, size_t count, FileStream* stream) {
    if (size * count == 0) return 0;
    if (stream && !stream->IsOpen()) { errno = 9; return 0; }
    auto* handle = GetNativeStream(stream);
    stream->Orientation(-1);
    if (!buffer) throw std::runtime_error("fwrite: null buffer");
    if (count > std::numeric_limits<std::size_t>::max() / size) throw std::overflow_error("fwrite: buffer size overflow");
    const auto bytes = size * count;
    if (bytes > std::numeric_limits<std::uintptr_t>::max() - reinterpret_cast<std::uintptr_t>(buffer)) throw std::overflow_error("fwrite: buffer address overflow");
    if (!stream->SwitchToWriting()) return 0;
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(buffer, bytes);
    const int previousError = errno;
    errno = 0;
    const auto result = std::fwrite(buffer, size, count, handle);
    const int nativeError = errno;
    stream->SyncStatus();
    errno = nativeError != 0 ? GuestErrnoFromHost_nid_no_patch(nativeError) : previousError;
    return result;
}

static bool WritingStream(std::FILE* native) {
#ifdef _WIN32
    constexpr long UcrtWriting = 0x0002;
    std::atomic_ref<long> state(reinterpret_cast<UcrtStreamState*>(native)->flags);
    return (state.load() & UcrtWriting) != 0;
#elif defined(__GLIBC__)
    return ::__fwriting(native) != 0;
#else
    return true;
#endif
}

static std::optional<std::int64_t> RegularFileSize(std::FILE* native) {
#ifdef _WIN32
    struct _stat64 status;
    if (_fstat64(_fileno(native), &status) != 0 || (status.st_mode & _S_IFMT) != _S_IFREG) return std::nullopt;
#else
    struct stat status;
    if (::fstat(::fileno(native), &status) != 0 || !S_ISREG(status.st_mode)) return std::nullopt;
#endif
    if (status.st_size < 0) return std::nullopt;
    return status.st_size;
}

namespace {

class SeekLock {
    FileStream* stream;

public:
    explicit SeekLock(FileStream* value) : stream(value) { if (stream) flockfile_nid_postfix(stream); }
    ~SeekLock() { if (stream) funlockfile_nid_postfix(stream); }
    SeekLock(const SeekLock&) = delete;
    SeekLock& operator=(const SeekLock&) = delete;
};

}

static bool ResolveSeekTarget(FileStream* stream, std::int64_t& offset, int& origin) {
    std::int64_t base = 0;
    if (origin == SEEK_CUR) {
        base = ftello_nid_postfix(stream);
        if (base == -1) return false;
        if (base < 0) { errno = 29; return false; }
    } else if (origin == SEEK_END) {
        auto* native = GetNativeStream(stream);
        if (WritingStream(native)) return true;
        const auto size = RegularFileSize(native);
        if (!size) return true;
        base = *size;
    }
    if (offset > 0 && base > std::numeric_limits<std::int64_t>::max() - offset) { errno = 84; return false; }
    offset += base;
    if (offset < 0) { errno = 22; return false; }
    origin = SEEK_SET;
    return true;
}

int APS5_VABI fseeko_nid_postfix(FileStream* stream, std::int64_t offset, int origin) {
    if (stream && !stream->IsOpen()) { errno = 9; return -1; }
    if (origin != SEEK_SET && origin != SEEK_CUR && origin != SEEK_END) { errno = 22; return -1; }
#ifdef _WIN32
    auto* native = GetNativeStream(stream);
    if (IsPipeStream(native)) { errno = 29; return -1; }
#endif
    const SeekLock lock(stream);
    if (!ResolveSeekTarget(stream, offset, origin)) return -1;
#ifdef _WIN32
    const int result = _fseeki64(native, offset, origin);
#else
    static_assert(sizeof(off_t) == 8);
    const int result = ::fseeko(GetNativeStream(stream), offset, origin);
#endif
    const int nativeError = errno;
    stream->SyncStatus();
    if (result) errno = nativeError == EOVERFLOW ? 84 : nativeError;
    else stream->SetLowSurrogate(0);
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
    auto* native = GetNativeStream(stream);
    stream->Orientation(-1);
    if (*str != '\0' && !stream->SwitchToWriting()) return -1;
    const int previousError = errno;
    errno = 0;
    const int result = std::fputs(str, native);
    const int nativeError = errno;
    stream->SyncStatus();
    errno = nativeError != 0 ? GuestErrnoFromHost_nid_no_patch(nativeError) : previousError;
    return result;
}

int APS5_VABI fflush_nid_postfix(FileStream* stream) {
    if (stream && !stream->IsOpen()) { errno = 9; return -1; }
    std::FILE* native = stream ? GetStreamHandle(stream) : nullptr;
    if (stream && (stream->IsRandomDevice() || stream->IsDirectory())) return 0;
#ifdef __linux__
    if (native && !__fwriting(native)) return 0;
#elif defined(__APPLE__)
    if (native && !(native->_flags & __SWR)) return 0;
#endif
    const int previousError = errno;
    errno = 0;
    const int result = std::fflush(native);
    const int nativeError = errno;
    if (stream) stream->SyncStatus();
    errno = nativeError != 0 ? GuestErrnoFromHost_nid_no_patch(nativeError) : previousError;
    return result;
}

int APS5_VABI malloc_stats_nid_postfix(void* stats) {
    return ApplicationHeapStats_nid_no_patch(stats);
}

void* APS5_VABI _sceLibcGetMallocParam_nid_postfix() { return nullptr; }

int APS5_VABI malloc_stats_fast_nid_postfix(void* stats) {
    return ApplicationHeapStatsFast_nid_no_patch(stats);
}

void APS5_VABI malloc_initialize_nid_postfix() {
    ApplicationHeapEnsureInitialized_nid_no_patch();
}

void APS5_VABI malloc_finalize_nid_postfix() {
    ApplicationHeapFinalize_nid_no_patch();
}

int APS5_VABI _malloc_fini_nid_postfix() {
    malloc_finalize_nid_postfix();
    return 0;
}

size_t APS5_VABI malloc_usable_size_nid_postfix(void* pointer) {
    return ApplicationHeapUsableSize_nid_no_patch(pointer);
}

void* APS5_VABI malloc_nid_postfix(size_t size) {
    return heapAllocation(size, [size] { return ApplicationHeapAllocate_nid_no_patch(size); });
}

void APS5_VABI __std_free_crt_nid_postfix(void* ptr) {
    free_nid_postfix(ptr);
}

void APS5_VABI free_nid_postfix(void* ptr) {
    ApplicationHeapFree_nid_no_patch(ptr);
}

void APS5_VABI freezero_nid_postfix(void* ptr, size_t size) {
    if (ptr == nullptr) return;
    auto* bytes = static_cast<volatile unsigned char*>(ptr);
    for (size_t index = 0; index < size; ++index) bytes[index] = 0;
    free_nid_postfix(ptr);
}

void* APS5_VABI realloc_nid_postfix(void* ptr, size_t newSize) {
    return heapAllocation(newSize, [ptr, newSize] { return ApplicationHeapReallocate_nid_no_patch(ptr, newSize); });
}

void* APS5_VABI reallocf_nid_postfix(void* ptr, size_t newSize) {
    void* result = realloc_nid_postfix(ptr, newSize);
    if (result == nullptr && ptr != nullptr && newSize != 0) ApplicationHeapFree_nid_no_patch(ptr);
    return result;
}

void* APS5_VABI reallocarray_nid_postfix(void* ptr, size_t nmemb, size_t size) {
    if (size != 0 && nmemb > std::numeric_limits<size_t>::max() / size) {
        errno = 12;
        return nullptr;
    }
    return realloc_nid_postfix(ptr, nmemb * size);
}

/*
 * Copyright (c) 2008, 2017 Otto Moerbeek <otto@drijf.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
void* APS5_VABI recallocarray_nid_postfix(void* ptr, size_t oldnmemb, size_t nmemb, size_t size) {
    if (size != 0 && nmemb > std::numeric_limits<size_t>::max() / size) {
        errno = 12;
        return nullptr;
    }
    if (ptr == nullptr) {
        void* result = calloc_nid_postfix(nmemb, size);
        if (result == nullptr && nmemb != 0 && size != 0) errno = 12;
        return result;
    }
    if (size != 0 && oldnmemb > std::numeric_limits<size_t>::max() / size) {
        errno = 22;
        return nullptr;
    }
    const size_t newsize = nmemb * size;
    const size_t oldsize = oldnmemb * size;
    if (newsize <= oldsize) {
        const size_t d = oldsize - newsize;
        if (d < oldsize / 2 && d < 16384) {
            auto* bytes = static_cast<volatile unsigned char*>(ptr);
            for (size_t index = 0; index < d; ++index) bytes[newsize + index] = 0;
            return ptr;
        }
    }
    void* newptr = malloc_nid_postfix(newsize);
    if (newptr == nullptr) {
        errno = 12;
        return nullptr;
    }
    if (newsize > oldsize) {
        std::memcpy(newptr, ptr, oldsize);
        std::memset(static_cast<char*>(newptr) + oldsize, 0, newsize - oldsize);
    } else {
        std::memcpy(newptr, ptr, newsize);
    }
    auto* oldbytes = static_cast<volatile unsigned char*>(ptr);
    for (size_t index = 0; index < oldsize; ++index) oldbytes[index] = 0;
    free_nid_postfix(ptr);
    return newptr;
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
    if (size != 0 && count > std::numeric_limits<size_t>::max() / size) {
        errno = 12;
        return nullptr;
    }
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

void* APS5_VABI lfind_nid_postfix(const void* key, const void* base, size_t* count,
    size_t size, int (APS5_VABI *compare)(const void*, const void*)) {
    if (!count) throw std::invalid_argument("lfind: null element count");
    if (*count == 0) return nullptr;
    if (!key || !base || !compare || size == 0)
        throw std::invalid_argument("lfind: invalid arguments");
    if (*count > std::numeric_limits<size_t>::max() / size)
        throw std::overflow_error("lfind: array size overflow");
    const auto* bytes = static_cast<const unsigned char*>(base);
    for (size_t index = 0; index < *count; ++index) {
        const auto* element = bytes + index * size;
        if (compare(key, element) == 0) return const_cast<unsigned char*>(element);
    }
    return nullptr;
}

void* APS5_VABI lsearch_nid_postfix(const void* key, void* base, size_t* count,
    size_t size, int (APS5_VABI *compare)(const void*, const void*)) {
    if (!count) throw std::invalid_argument("lsearch: null element count");
    if (!key || !base || !compare || size == 0)
        throw std::invalid_argument("lsearch: invalid arguments");
    if (*count > std::numeric_limits<size_t>::max() / size)
        throw std::overflow_error("lsearch: array size overflow");
    auto* bytes = static_cast<unsigned char*>(base);
    for (size_t index = 0; index < *count; ++index) {
        auto* element = bytes + index * size;
        if (compare(key, element) == 0) return element;
    }
    if (*count >= std::numeric_limits<size_t>::max() / size)
        throw std::overflow_error("lsearch: append capacity overflow");
    auto* tail = bytes + *count * size;
    std::memcpy(tail, key, size);
    ++*count;
    return tail;
}

void APS5_VABI qsort_nid_postfix(void* base, size_t count, size_t size, int (APS5_VABI *compare)(const void*, const void*)) {
    if (!base || !compare) return;
    if (size == 0 && count >= 33) throw std::invalid_argument("qsort: zero element size");
    if (size != 0 && count > std::numeric_limits<size_t>::max() / size) throw std::overflow_error("qsort: array size overflow");
    SortElements(static_cast<unsigned char*>(base), count, count, size, compare);
}

int APS5_VABI qsort_s_nid_postfix(void* base, size_t count, size_t size, GuestContextCompare compare, void* context) {
    constexpr size_t RsizeMax = std::numeric_limits<size_t>::max() >> 3;
    if (count > RsizeMax || size > RsizeMax || (count != 0 && (!base || !compare))) {
        SonyInvokeConstraintHandler("qsort_s: bad argument", nullptr, 22);
        return 22;
    }
    if (count == 0) return 0;
    if (size == 0 && count >= 33) throw std::invalid_argument("qsort_s: zero element size");
    auto withContext = [compare, context](const void* left, const void* right) { return compare(left, right, context); };
    SortElements(static_cast<unsigned char*>(base), count, count, size, withContext);
    return 0;
}

void APS5_VABI qsort_r_nid_postfix(void* base, size_t count, size_t size, void* thunk, int (APS5_VABI *compare)(void*, const void*, const void*)) {
    if (!compare) throw std::invalid_argument("qsort_r: null comparator");
    if (size == 0) throw std::invalid_argument("qsort_r: zero element size");
    if (count == 0) return;
    if (!base) throw std::invalid_argument("qsort_r: null base");
    if (count > std::numeric_limits<size_t>::max() / size) throw std::overflow_error("qsort_r: array size overflow");
    if (count == 1) return;

    ReentrantSortElements(static_cast<unsigned char*>(base), count, size, [compare, thunk](const void* left, const void* right) { return compare(thunk, left, right); });
}

}
