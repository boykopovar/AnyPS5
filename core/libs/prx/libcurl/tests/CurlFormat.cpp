#include "prx/libcurl/src/CurlHost.hpp"
#include "prx/libcurl/libcurl.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#if defined(__clang__) && !defined(_WIN32)
#define __builtin_sysv_va_list va_list
#define __builtin_sysv_va_start va_start
#define __builtin_sysv_va_end va_end
#endif

static void Require(bool condition) {
    if (!condition) std::abort();
}

static bool Equals(const char* actual, const char* expected) {
    return actual != nullptr && std::strcmp(actual, expected) == 0;
}

struct AllocationTracker {
    std::vector<size_t> reallocSizes;
    std::size_t freeCalls = 0;
    std::size_t nullFrees = 0;
    std::size_t strdupCalls = 0;
    bool failRealloc = false;
    std::map<void*, std::size_t> live;
};

static AllocationTracker tracker;

static void* APS5_VABI TrackedMalloc(size_t size) {
    void* pointer = std::malloc(size);
    if (pointer) tracker.live[pointer] = size;
    return pointer;
}

static void APS5_VABI TrackedFree(void* pointer) {
    ++tracker.freeCalls;
    if (!pointer) {
        ++tracker.nullFrees;
        return;
    }
    Require(tracker.live.erase(pointer) == 1);
    std::free(pointer);
}

static void* APS5_VABI TrackedRealloc(void* pointer, size_t size) {
    tracker.reallocSizes.push_back(size);
    if (tracker.failRealloc) return nullptr;
    if (pointer) Require(tracker.live.erase(pointer) == 1);
    void* grown = std::realloc(pointer, size);
    Require(grown != nullptr);
    tracker.live[grown] = size;
    return grown;
}

static char* APS5_VABI TrackedStrdup(const char* text) {
    ++tracker.strdupCalls;
    const std::size_t size = std::strlen(text) + 1;
    auto* copy = static_cast<char*>(TrackedMalloc(size));
    if (copy) std::memcpy(copy, text, size);
    return copy;
}

static void* APS5_VABI TrackedCalloc(size_t count, size_t size) {
    void* pointer = std::calloc(count, size);
    if (pointer) tracker.live[pointer] = count * size;
    return pointer;
}

static void ResetTracker() {
    tracker.reallocSizes.clear();
    tracker.freeCalls = 0;
    tracker.nullFrees = 0;
    tracker.strdupCalls = 0;
    tracker.failRealloc = false;
}

static int APS5_VABI BoundedList(char* buffer, size_t size, const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    VaList list;
    std::memcpy(&list, args, sizeof(list));
    const VaList original = list;
    const int result = curl_mvsnprintf_nid_postfix(buffer, size, format, &list);
    Require(std::memcmp(&list, &original, sizeof(list)) == 0);
    __builtin_sysv_va_end(args);
    return result;
}

static int APS5_VABI UnboundedList(char* buffer, const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    VaList list;
    std::memcpy(&list, args, sizeof(list));
    const int result = curl_mvsprintf_nid_postfix(buffer, format, &list);
    __builtin_sysv_va_end(args);
    return result;
}

static char* APS5_VABI AllocatedList(const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    VaList list;
    std::memcpy(&list, args, sizeof(list));
    char* result = curl_mvaprintf_nid_postfix(format, &list);
    __builtin_sysv_va_end(args);
    return result;
}

static void TestIntegers() {
    char buffer[128];
    Require(curl_msnprintf_nid_postfix(buffer, sizeof(buffer), "%d|%5d|%-5d|%05d|%+d|% d", 42, 42, 42, 42, 42, 42) == 28);
    Require(Equals(buffer, "42|   42|42   |00042|+42| 42"));
    Require(curl_msnprintf_nid_postfix(buffer, sizeof(buffer), "%ld %lu %lld %zu %lx %qd", std::int64_t{-5000000000}, std::uint64_t{5000000000}, std::int64_t{-9000000000}, std::uint64_t{6000000000}, std::uint64_t{0x123456789}, std::int64_t{7000000000}) == 66);
    Require(Equals(buffer, "-5000000000 5000000000 -9000000000 6000000000 123456789 7000000000"));
    Require(curl_msnprintf_nid_postfix(buffer, sizeof(buffer), "%d %u %x %X %#x %#o %o", std::int64_t{0x100000005}, -1, -1, 255, 255, 8, 8) == 36);
    Require(Equals(buffer, "5 4294967295 ffffffff FF 0xff 010 10"));
    Require(curl_msnprintf_nid_postfix(buffer, sizeof(buffer), "%.4d|%8.3d|%-+6d|%d", 7, -7, 7, std::int64_t{-2147483648}) == 32);
    Require(Equals(buffer, "0007|    -007|+7    |-2147483648"));
    Require(curl_msnprintf_nid_postfix(buffer, sizeof(buffer), "%c|%3c|%-3c|", 'A', 'B', 'C') == 10);
    Require(Equals(buffer, "A|  B|C  |"));
}

static void TestStringsAndPointers() {
    char buffer[128];
    Require(curl_msnprintf_nid_postfix(buffer, sizeof(buffer), "%s|%.3s|%8s|%-8s|%s|%.2s|%S", "hello", "hello", "hi", "hi", static_cast<const char*>(nullptr), static_cast<const char*>(nullptr), "q") == 38);
    Require(Equals(buffer, "hello|hel|      hi|hi      |(nil)||\"q\""));
    Require(curl_msnprintf_nid_postfix(buffer, sizeof(buffer), "%p|%p|%8p|", static_cast<void*>(nullptr), reinterpret_cast<void*>(std::uintptr_t{0x1234}), static_cast<void*>(nullptr)) == 22);
    Require(Equals(buffer, "(nil)|0x1234|(nil)   |"));
    Require(curl_msnprintf_nid_postfix(buffer, sizeof(buffer), "100%% %y%d", static_cast<void*>(nullptr), 5) == 6);
    Require(Equals(buffer, "100% 5"));
}

static void TestParameters() {
    char buffer[128];
    Require(curl_msnprintf_nid_postfix(buffer, sizeof(buffer), "%2$s %1$s", "a", "b") == 3);
    Require(Equals(buffer, "b a"));
    Require(curl_msnprintf_nid_postfix(buffer, sizeof(buffer), "%*d|%-*d|%.*d|%*d|", 4, 7, 4, 7, 3, 7, -4, 7) == 19);
    Require(Equals(buffer, "   7|7   |007|7   |"));
    Require(curl_msnprintf_nid_postfix(buffer, sizeof(buffer), "%2$*1$d|", 5, 9) == 6);
    Require(Equals(buffer, "    9|"));
}

static void TestFloatingPoint() {
    char buffer[128];
    Require(curl_msnprintf_nid_postfix(buffer, sizeof(buffer), "%f|%.2f|%e|%g|%8.3f|%-8.1f|%+E|%05.1f", 1.5, 3.14159, 12345.678, 0.0001, 2.5, 2.5, 1.0, 2.5) == 71);
    Require(Equals(buffer, "1.500000|3.14|1.234568e+04|0.0001|   2.500|2.5     |+1.000000E+00|  2.5"));
    Require(curl_msnprintf_nid_postfix(buffer, sizeof(buffer), "%d %f %d", 1, 0.5, 2) == 12);
    Require(Equals(buffer, "1 0.500000 2"));
}

static void TestCounts() {
    char buffer[64];
    std::int32_t count = -1;
    std::int64_t wide = -1;
    std::int16_t narrow = -1;
    Require(curl_msnprintf_nid_postfix(buffer, sizeof(buffer), "abc%n%sxy%ln%hn", &count, "de", &wide, &narrow) == 7);
    Require(Equals(buffer, "abcdexy"));
    Require(count == 3);
    Require(wide == 7);
    Require(narrow == 7);
}

static void TestBounds() {
    char buffer[16];
    Require(curl_msnprintf_nid_postfix(buffer, 5, "%s", "abcdefgh") == 4);
    Require(Equals(buffer, "abcd"));
    Require(curl_msnprintf_nid_postfix(buffer, 4, "abc") == 3);
    Require(Equals(buffer, "abc"));
    buffer[0] = 'Z';
    Require(curl_msnprintf_nid_postfix(buffer, 0, "abc") == 0);
    Require(buffer[0] == 'Z');
    std::int32_t count = -1;
    Require(curl_msnprintf_nid_postfix(buffer, 3, "abcdef%n", &count) == 2);
    Require(Equals(buffer, "ab"));
    Require(count == -1);
    std::string tooMany;
    for (int index = 0; index < 129; ++index) tooMany += "%d";
    buffer[0] = 'Z';
    Require(curl_msnprintf_nid_postfix(buffer, sizeof(buffer), tooMany.c_str()) == -1);
    Require(buffer[0] == 'Z');
    Require(curl_msprintf_nid_postfix(buffer, tooMany.c_str()) == -1);
    Require(buffer[0] == '\0');
    Require(curl_maprintf_nid_postfix(tooMany.c_str()) == nullptr);
}

static void TestLists() {
    char buffer[64];
    Require(BoundedList(buffer, sizeof(buffer), "%s=%ld", "key", std::int64_t{-8000000000}) == 15);
    Require(Equals(buffer, "key=-8000000000"));
    Require(BoundedList(buffer, 4, "%d", 123456) == 3);
    Require(Equals(buffer, "123"));
    Require(UnboundedList(buffer, "%03d:%s", 5, "ok") == 6);
    Require(Equals(buffer, "005:ok"));
    char* text = AllocatedList("%s-%lu", "id", std::uint64_t{10000000000});
    Require(Equals(text, "id-10000000000"));
    curl_free(text);
}

static void TestUnboundedAndAllocated() {
    char buffer[64];
    std::memset(buffer, 'x', sizeof(buffer));
    Require(curl_msprintf_nid_postfix(buffer, "%d-%s", 7, "x") == 3);
    Require(Equals(buffer, "7-x"));
    char* text = curl_maprintf_nid_postfix("%s=%ld", "k", std::int64_t{9000000000});
    Require(Equals(text, "k=9000000000"));
    curl_free(text);
    char* empty = curl_maprintf_nid_postfix("");
    Require(Equals(empty, ""));
    curl_free(empty);
    std::string large(7999998, 'a');
    char* fits = curl_maprintf_nid_postfix("%s%c", large.c_str(), 'b');
    Require(fits != nullptr && std::strlen(fits) == 7999999);
    curl_free(fits);
    Require(curl_maprintf_nid_postfix("%s%s", large.c_str(), "bc") == nullptr);
}

static void TestGuestAllocators() {
    Require(curl_global_init_mem_nid_postfix(CURL_GLOBAL_DEFAULT, TrackedMalloc, TrackedFree, TrackedRealloc, TrackedStrdup, TrackedCalloc) == CURLE_OK);
    const std::size_t baseline = tracker.live.size();
    ResetTracker();
    const std::string forty(40, 'r');
    char* text = curl_maprintf_nid_postfix("%s", forty.c_str());
    Require(Equals(text, forty.c_str()));
    Require(tracker.reallocSizes == std::vector<size_t>{32, 64});
    Require(tracker.live.count(text) == 1 && tracker.live[text] == 64);
    curl_free(text);
    Require(tracker.freeCalls == 1 && tracker.live.size() == baseline);
    ResetTracker();
    char* empty = curl_maprintf_nid_postfix("");
    Require(Equals(empty, ""));
    Require(tracker.strdupCalls == 1 && tracker.reallocSizes.empty());
    curl_free(empty);
    Require(tracker.live.size() == baseline);
    ResetTracker();
    tracker.failRealloc = true;
    Require(curl_maprintf_nid_postfix("abc") == nullptr);
    Require(tracker.reallocSizes == std::vector<size_t>{32});
    Require(tracker.freeCalls == 2 && tracker.nullFrees == 2);
    ResetTracker();
    std::string tooMany;
    for (int index = 0; index < 129; ++index) tooMany += "%d";
    Require(curl_maprintf_nid_postfix(tooMany.c_str()) == nullptr);
    Require(tracker.reallocSizes.empty() && tracker.freeCalls == 1 && tracker.nullFrees == 1);
    ResetTracker();
    const std::string large(7999998, 'a');
    Require(curl_maprintf_nid_postfix("%s%s", large.c_str(), "bc") == nullptr);
    Require(tracker.freeCalls == 2 && tracker.nullFrees == 1);
    Require(tracker.live.size() == baseline);
    curl_global_cleanup_nid_postfix();
    Require(tracker.live.empty());
}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    TestIntegers();
    TestStringsAndPointers();
    TestParameters();
    TestFloatingPoint();
    TestCounts();
    TestBounds();
    TestLists();
    TestUnboundedAndAllocated();
    TestGuestAllocators();
    return 0;
}
