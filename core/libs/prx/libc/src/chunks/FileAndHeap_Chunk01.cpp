extern "C" {
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
#ifdef _WIN32
        const auto handle = WindowsDescriptorHandle_nid_no_patch(descriptor);
        if (handle == INVALID_HANDLE_VALUE) { errno = 9; return nullptr; }
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

}
