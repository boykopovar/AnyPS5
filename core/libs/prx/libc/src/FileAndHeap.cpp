#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <filesystem>
#include <limits>
#include <utility>
#include <cerrno>

#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

[[noreturn]] void APS5_VABI _ZSt11_Xbad_allocv_nid_postfix();

FileStream* APS5_VABI freopen_nid_postfix(const char* filename, const char* mode, FileStream* stream) {
    if (!stream || !mode) { errno = 22; return nullptr; }
    if (!filename) { errno = 45; return nullptr; } // Mode-only reopening is not supported.
    const char* supported[] = {"r", "w", "a", "rb", "wb", "ab", "r+", "w+", "a+",
        "rb+", "wb+", "ab+", "r+b", "w+b", "a+b"};
    bool valid = false;
    for (const auto* candidate : supported) if (std::strcmp(mode, candidate) == 0) valid = true;
    if (!valid) { errno = 22; return nullptr; }
    try {
        const auto path = *filename ? ResolvePath_nid_no_patch(filename).string() : std::string{};
        if (stream->Reopen(path.c_str(), mode)) return stream;
        const int error = errno;
        if (stream->IsDynamic()) delete stream;
        errno = error;
        return nullptr;
    } catch (const std::bad_alloc&) { errno = 12; return nullptr; }
      catch (const std::filesystem::filesystem_error&) { errno = 5; return nullptr; }
}

FileStream* APS5_VABI fopen_nid_postfix(const char* filename, const char* mode) {
    if (!filename || !mode) throw std::runtime_error(std::string(__func__) + ": " + FOPEN_MSG_NULL_ARG);
    const std::filesystem::path fpath = ResolvePath_nid_no_patch(filename);
    const auto abs_path = fpath.string();
    std::unique_ptr<std::FILE, decltype(&std::fclose)> handle(std::fopen(abs_path.c_str(), mode), std::fclose);
    if (!handle) {
        const int error = errno;
        if (error == ENOENT) {
            errno = error;
            return nullptr;
        }
        const auto reason = std::strerror(error);
        std::error_code ec;
        std::filesystem::path sibling;
        for (const auto& entry : std::filesystem::directory_iterator(fpath.parent_path(), ec)) {
            if (ec) break;
            if (entry.path().stem() == fpath.stem() && entry.path().extension() != fpath.extension()) {
                sibling = std::filesystem::absolute(entry.path());
                break;
            }
        }
        if (!sibling.empty()) {
            // APS5_LOG_OUT("%s: \"%s\": %s. Sibling: \"%s\"", FOPEN_MSG_NOT_FOUND, abs_path.c_str(), reason, sibling.filename().string().c_str());
            return nullptr;
        }
        throw std::runtime_error(std::string(__func__) + ": " + FOPEN_MSG_OPEN_FAILED + ": \"" + abs_path + "\": " + reason);
    }
    // APS5_LOG_OUT("success: \"%s\"", abs_path.c_str());
    auto stream = std::make_unique<FileStream>(handle.get(), true);
    handle.release();
    return stream.release();
}

int APS5_VABI fopen_s_nid_postfix(FileStream** result, const char* filename, const char* mode) {
    constexpr int GuestEinval = 22;
    constexpr int GuestEnoent = 2;
    if (!result || !filename || !mode) return GuestEinval;
    *result = fopen_nid_postfix(filename, mode);
    return *result ? 0 : GuestEnoent;
}

int APS5_VABI fclose_nid_postfix(FileStream* stream) {
    GetNativeStream(stream);
    std::unique_ptr<FileStream> owner(stream->IsDynamic() ? stream : nullptr);
    stream->Close();
    return 0;
}

size_t APS5_VABI fread_nid_postfix(void* buffer, size_t size, size_t count, FileStream* stream) {
    auto* handle = GetNativeStream(stream);
    if (size == 0 || count == 0) return 0;
    if (!buffer) throw std::runtime_error("fread: null buffer");
    const auto result = std::fread(buffer, size, count, handle);
    stream->SyncStatus();
    if (std::ferror(handle)) throw std::runtime_error("fread: read failed");
    return result;
}

size_t APS5_VABI fwrite_nid_postfix(const void* buffer, size_t size, size_t count, FileStream* stream) {
    auto* handle = GetNativeStream(stream);
    if (size == 0 || count == 0) return 0;
    if (!buffer) throw std::runtime_error("fwrite: null buffer");
    const auto result = std::fwrite(buffer, size, count, handle);
    stream->SyncStatus();
    if (result != count || std::ferror(handle)) throw std::runtime_error("fwrite: write failed");
    return result;
}

int APS5_VABI fseeko_nid_postfix(FileStream* stream, std::int64_t offset, int origin) {
    if (origin != SEEK_SET && origin != SEEK_CUR && origin != SEEK_END) { errno = 22; return -1; }
#ifdef _WIN32
    const int result = _fseeki64(GetNativeStream(stream), offset, origin);
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
#ifdef _WIN32
    const auto result = _ftelli64(GetNativeStream(stream));
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

int APS5_VABI fputs_nid_postfix(const char* str, FileStream* stream) {
    if (!str) throw std::runtime_error("fputs: null string");
    const int result = std::fputs(str, GetNativeStream(stream));
    if (result == EOF) throw std::runtime_error("fputs: write failed");
    return result;
}

int APS5_VABI fflush_nid_postfix(FileStream* stream) {
    if (std::fflush(stream ? GetNativeStream(stream) : nullptr) != 0) throw std::runtime_error("fflush: flush failed");
    return 0;
}

void* APS5_VABI malloc_nid_postfix(size_t size) {
    return ApplicationHeapAllocate_nid_no_patch(size);
}

void APS5_VABI free_nid_postfix(void* ptr) {
    ApplicationHeapFree_nid_no_patch(ptr);
}

void* APS5_VABI realloc_nid_postfix(void* ptr, size_t newSize) {
    return ApplicationHeapReallocate_nid_no_patch(ptr, newSize);
}

void* APS5_VABI memalign_nid_postfix(size_t alignment, size_t size) {
    return ApplicationHeapAlign_nid_no_patch(alignment, size);
}

void* APS5_VABI aligned_alloc_nid_postfix(size_t alignment, size_t size) {
    return ApplicationHeapAlign_nid_no_patch(alignment, size);
}

void* APS5_VABI reallocalign_nid_postfix(void* ptr, size_t size, size_t alignment) {
    return ApplicationHeapRealign_nid_no_patch(ptr, size, alignment);
}

void* APS5_VABI calloc_nid_postfix(size_t count, size_t size) {
    return ApplicationHeapCalloc_nid_no_patch(count, size);
}

int APS5_VABI posix_memalign_nid_postfix(void** pointer, size_t alignment, size_t size) {
    return ApplicationHeapPosixAlign_nid_no_patch(pointer, alignment, size);
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

    auto* bytes = static_cast<unsigned char*>(base);
    const auto swapElements = [bytes, size](size_t left, size_t right) {
        auto* leftElement = bytes + left * size;
        auto* rightElement = bytes + right * size;
        for (size_t index = 0; index < size; ++index) std::swap(leftElement[index], rightElement[index]);
    };
    const auto siftDown = [bytes, size, compare, &swapElements](size_t root, size_t heapSize) {
        while (root < heapSize / 2) {
            size_t child = root * 2 + 1;
            if (child + 1 < heapSize && compare(bytes + child * size, bytes + (child + 1) * size) < 0) ++child;
            if (compare(bytes + root * size, bytes + child * size) >= 0) return;
            swapElements(root, child);
            root = child;
        }
    };

    for (size_t parent = count / 2; parent != 0; --parent) siftDown(parent - 1, count);
    for (size_t heapSize = count; heapSize > 1;) {
        swapElements(0, --heapSize);
        siftDown(0, heapSize);
    }
}

}
