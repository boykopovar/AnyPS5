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

int APS5_VABI fsetpos_nid_postfix(FileStream* stream, const std::int64_t* position) {
    if (!position) throw std::invalid_argument("fsetpos: null position");
    const int savedErrno = errno;
    std::int64_t target = 0;
    if (!GuestCopyIn_nid_no_patch(&target, position, sizeof(target))) {
        errno = savedErrno;
        throw std::invalid_argument("fsetpos: unreadable position");
    }
    errno = savedErrno;
    return fseeko_nid_postfix(stream, target, SEEK_SET);
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
    if (result == EOF) stream->DropFailedUnbufferedWrite();
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
    size_t elements = 0;
    const int savedErrno = errno;
    const bool readable = GuestCopyIn_nid_no_patch(&elements, count, sizeof(elements));
    errno = savedErrno;
    if (!readable) throw std::invalid_argument("lfind: unreadable element count");
    if (elements == 0) return nullptr;
    if (!key || !base || !compare || size == 0)
        throw std::invalid_argument("lfind: invalid arguments");
    if (elements > std::numeric_limits<size_t>::max() / size)
        throw std::overflow_error("lfind: array size overflow");
    const auto* bytes = static_cast<const unsigned char*>(base);
    for (size_t index = 0; index < elements; ++index) {
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
    size_t elements = 0;
    const int savedErrno = errno;
    const bool readable = GuestCopyIn_nid_no_patch(&elements, count, sizeof(elements));
    errno = savedErrno;
    if (!readable) throw std::invalid_argument("lsearch: unreadable element count");
    if (elements > std::numeric_limits<size_t>::max() / size)
        throw std::overflow_error("lsearch: array size overflow");
    auto* bytes = static_cast<unsigned char*>(base);
    for (size_t index = 0; index < elements; ++index) {
        auto* element = bytes + index * size;
        if (compare(key, element) == 0) return element;
    }
    if (elements >= std::numeric_limits<size_t>::max() / size)
        throw std::overflow_error("lsearch: append capacity overflow");
    const size_t appended = elements + 1;
    const int appendErrno = errno;
    const bool writable = GuestCopyOut_nid_no_patch(count, &appended, sizeof(appended));
    errno = appendErrno;
    if (!writable) throw std::invalid_argument("lsearch: unwritable element count");
    auto* tail = bytes + elements * size;
    std::memcpy(tail, key, size);
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
