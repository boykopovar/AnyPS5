#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "unicode/uenum.h"
#include "unicode/utypes.h"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

static_assert(std::is_same_v<decltype(&uloc_openKeywordList_nid_postfix),
                             UEnumeration* (APS5_VABI*)(const char*, std::int32_t, std::int32_t*)>);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU keyword list: %s\n", message);
    std::abort();
}

template<class TCall>
void RejectsInvalid(TCall call, const char* message) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, message);
}

void* APS5_VABI Allocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI Free(void* pointer) { std::free(pointer); }
void* APS5_VABI Calloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Reallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Align(std::size_t alignment, std::size_t size) {
#ifdef _WIN32
    return _aligned_malloc(size, alignment);
#else
    void* result = nullptr;
    return posix_memalign(&result, alignment, size) == 0 ? result : nullptr;
#endif
}
void* APS5_VABI Realign(void* pointer, std::size_t size, std::size_t alignment) {
#ifdef _WIN32
    return _aligned_realloc(pointer, size, alignment);
#else
    void* result = Align(alignment, size);
    std::free(pointer);
    return result;
#endif
}
int APS5_VABI PosixAlign(void** pointer, std::size_t alignment, std::size_t size) {
#ifdef _WIN32
    *pointer = _aligned_malloc(size, alignment);
    return *pointer ? 0 : 12;
#else
    return posix_memalign(pointer, alignment, size);
#endif
}

std::vector<std::string> Drain(UEnumeration* enumeration, std::int32_t& status) {
    std::vector<std::string> items;
    for (;;) {
        std::int32_t length = -1;
        const char* item = uenum_next_nid_postfix(enumeration, &length, &status);
        if (!item) {
            Require(length == 0, "exhausted enumeration reports zero length");
            return items;
        }
        Require(length == static_cast<std::int32_t>(std::strlen(item)), "next length matches the keyword");
        items.emplace_back(item, static_cast<std::size_t>(length));
    }
}

void CheckKeywordList() {
    static constexpr char list[] = "calendar\0collation\0";
    std::int32_t status = 0;
    UEnumeration* keywords = uloc_openKeywordList_nid_postfix(list, sizeof(list) - 1, &status);
    Require(keywords && status == 0, "keyword list opens");
    Require(uenum_count_nid_postfix(keywords, &status) == 2, "keyword list count");
    Require(Drain(keywords, status) == std::vector<std::string>{"calendar", "collation"}, "keyword list order is kept");
    keywords->reset(keywords, &status);
    Require(status == 0 && Drain(keywords, status).size() == 2, "reset restarts the list");
    uenum_close_nid_postfix(keywords);

    static constexpr char unterminated[] = {'a', 'b', 'c'};
    RejectsInvalid([&] { uloc_openKeywordList_nid_postfix(unterminated, sizeof(unterminated), &status); },
                   "list without a final NUL is rejected");
    RejectsInvalid([&] { uloc_openKeywordList_nid_postfix(list, sizeof(list) - 2, &status); },
                   "size cutting the final NUL is rejected");
    Require(status == 0, "rejected list keeps status");

    static constexpr char stopped[] = "a\0\0b\0";
    keywords = uloc_openKeywordList_nid_postfix(stopped, sizeof(stopped) - 1, &status);
    Require(keywords && uenum_count_nid_postfix(keywords, &status) == 1, "empty keyword ends the count");
    Require(Drain(keywords, status) == std::vector<std::string>{"a"}, "empty keyword ends the list");
    uenum_close_nid_postfix(keywords);

    keywords = uloc_openKeywordList_nid_postfix(nullptr, 0, &status);
    Require(keywords && status == 0, "empty keyword list opens without reading");
    Require(uenum_count_nid_postfix(keywords, &status) == 0 && Drain(keywords, status).empty(), "empty keyword list has no keywords");
    uenum_close_nid_postfix(keywords);

    status = -128;
    keywords = uloc_openKeywordList_nid_postfix(list, sizeof(list) - 1, &status);
    Require(keywords && status == -128, "warning status is kept");
    uenum_close_nid_postfix(keywords);

    for (std::int32_t size : {-1, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()}) {
        status = 0;
        RejectsInvalid([&] { uloc_openKeywordList_nid_postfix(list, size, &status); }, "invalid list size is rejected");
    }
}

void CheckStatus() {
    for (std::int32_t initial : {1, 15, std::numeric_limits<std::int32_t>::max()}) {
        std::int32_t status = initial;
        Require(!uloc_openKeywordList_nid_postfix("a\0", 2, &status) && status == initial,
                "prefailed status is preserved");
        status = initial;
        Require(!uloc_openKeywordList_nid_postfix(nullptr, -1, &status) && status == initial,
                "prefailed status skips argument checks");
    }
    RejectsInvalid([] { uloc_openKeywordList_nid_postfix("a\0", 2, nullptr); }, "null status is rejected");
}

void CheckUnreadableList() {
#ifdef _WIN32
    const std::size_t pageSize = 4096;
    auto* pages = static_cast<char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    Require(pages != nullptr, "allocate list pages");
    DWORD oldProtection = 0;
    Require(VirtualProtect(pages + pageSize, pageSize, PAGE_NOACCESS, &oldProtection) != 0, "protect unreadable page");
#else
    const std::size_t pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE) > 0 ? sysconf(_SC_PAGESIZE) : 4096);
    void* mapping = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(mapping != MAP_FAILED, "allocate list pages");
    auto* pages = static_cast<char*>(mapping);
    Require(mprotect(pages + pageSize, pageSize, PROT_NONE) == 0, "protect unreadable page");
#endif
    std::memcpy(pages + pageSize - 2, "a", 2);
    std::int32_t status = 0;
    UEnumeration* keywords = uloc_openKeywordList_nid_postfix(pages + pageSize - 2, 2, &status);
    Require(keywords && Drain(keywords, status) == std::vector<std::string>{"a"}, "list ending at a page boundary is read");
    uenum_close_nid_postfix(keywords);
    errno = 71;
    RejectsInvalid([&] { uloc_openKeywordList_nid_postfix(pages + pageSize - 2, 4, &status); }, "unreadable list is rejected");
    Require(errno == 71 && status == 0, "unreadable list keeps errno and status");
#ifdef _WIN32
    VirtualFree(pages, 0, MEM_RELEASE);
#else
    munmap(pages, pageSize * 2);
#endif
}

}

int main() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Free), reinterpret_cast<void*>(Calloc),
        reinterpret_cast<void*>(Reallocate), reinterpret_cast<void*>(Align), reinterpret_cast<void*>(Realign),
        reinterpret_cast<void*>(PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    CheckKeywordList();
    CheckStatus();
    CheckUnreadableList();
    return 0;
}
