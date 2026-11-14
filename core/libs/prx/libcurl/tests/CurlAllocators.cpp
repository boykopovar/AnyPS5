#include "prx/libcurl/src/CurlHost.hpp"
#include "prx/libcurl/libcurl.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <fstream>
#include <limits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>
#include <source_location>
#include <string>

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Require failed at %s:%u\n", location.file_name(), location.line());
        std::abort();
    }
}

static bool throwNextGuestCalloc = false;
struct GuestAllocationFailure {};

#ifndef _WIN32
static std::size_t failHostAllocationOfSize = 0;
static void (*hostFaultProbe)() = nullptr;
static bool captureNextCalloc = false;
static CURL* capturedDuplicate = nullptr;
static std::size_t capturedDuplicateBytes = 0;
static bool duplicateRegisteredAtFault = false;

void* operator new(std::size_t bytes) {
    if (bytes != 0 && failHostAllocationOfSize == bytes) {
        failHostAllocationOfSize = 0;
        if (hostFaultProbe) hostFaultProbe();
        throw std::bad_alloc();
    }
    if (void* pointer = std::malloc(bytes ? bytes : 1)) return pointer;
    throw std::bad_alloc();
}
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
#endif

struct AllocationTracker {
    std::size_t mallocCalls = 0;
    std::size_t freeCalls = 0;
    std::size_t reallocCalls = 0;
    std::size_t strdupCalls = 0;
    std::size_t callocCalls = 0;

    std::map<void*, std::size_t> livePointers;

    bool failNextMalloc = false;
    bool failNextRealloc = false;
    bool failNextStrdup = false;
    bool failNextCalloc = false;

    void ResetCounts() {
        mallocCalls = 0;
        freeCalls = 0;
        reallocCalls = 0;
        strdupCalls = 0;
        callocCalls = 0;
    }

    void ClearFailures() {
        failNextMalloc = false;
        failNextRealloc = false;
        failNextStrdup = false;
        failNextCalloc = false;
    }

    std::size_t TotalAllocations() const {
        return mallocCalls + callocCalls + reallocCalls + strdupCalls;
    }

    std::size_t LiveCount() const {
        return livePointers.size();
    }
};

static AllocationTracker trackerA;
static AllocationTracker trackerB;

static void* APS5_VABI AllocatorA_Malloc(size_t size) {
    ++trackerA.mallocCalls;
    if (trackerA.failNextMalloc) {
        trackerA.failNextMalloc = false;
        return nullptr;
    }
    void* ptr = std::malloc(size);
    if (ptr != nullptr) {
        Require(trackerB.livePointers.find(ptr) == trackerB.livePointers.end());
        Require(trackerA.livePointers.find(ptr) == trackerA.livePointers.end());
        trackerA.livePointers[ptr] = size;
    }
    return ptr;
}

static void APS5_VABI AllocatorA_Free(void* ptr) {
    ++trackerA.freeCalls;
    if (ptr == nullptr) {
        return;
    }
    Require(trackerB.livePointers.find(ptr) == trackerB.livePointers.end());
    auto it = trackerA.livePointers.find(ptr);
    Require(it != trackerA.livePointers.end());
    trackerA.livePointers.erase(it);
    std::free(ptr);
}

static void* APS5_VABI AllocatorA_Realloc(void* ptr, size_t size) {
    ++trackerA.reallocCalls;
    if (trackerA.failNextRealloc) {
        trackerA.failNextRealloc = false;
        return nullptr;
    }
    if (ptr == nullptr) {
        void* newPtr = std::malloc(size);
        if (newPtr != nullptr) {
            Require(trackerB.livePointers.find(newPtr) == trackerB.livePointers.end());
            trackerA.livePointers[newPtr] = size;
        }
        return newPtr;
    }
    Require(trackerB.livePointers.find(ptr) == trackerB.livePointers.end());
    auto it = trackerA.livePointers.find(ptr);
    Require(it != trackerA.livePointers.end());
    if (size == 0) {
        trackerA.livePointers.erase(it);
        std::free(ptr);
        return nullptr;
    }
    void* newPtr = std::realloc(ptr, size);
    if (newPtr != nullptr) {
        trackerA.livePointers.erase(it);
        trackerA.livePointers[newPtr] = size;
    }
    return newPtr;
}

static char* APS5_VABI AllocatorA_Strdup(const char* str) {
    ++trackerA.strdupCalls;
    if (trackerA.failNextStrdup) {
        trackerA.failNextStrdup = false;
        return nullptr;
    }
    if (str == nullptr) {
        return nullptr;
    }
    const size_t len = std::strlen(str) + 1;
    char* ptr = static_cast<char*>(std::malloc(len));
    if (ptr != nullptr) {
        std::memcpy(ptr, str, len);
        Require(trackerB.livePointers.find(ptr) == trackerB.livePointers.end());
        Require(trackerA.livePointers.find(ptr) == trackerA.livePointers.end());
        trackerA.livePointers[ptr] = len;
    }
    return ptr;
}

static void* APS5_VABI AllocatorA_Calloc(size_t nmemb, size_t size) {
    ++trackerA.callocCalls;
    if (throwNextGuestCalloc) {
        throwNextGuestCalloc = false;
        throw GuestAllocationFailure{};
    }
    if (trackerA.failNextCalloc) {
        trackerA.failNextCalloc = false;
        return nullptr;
    }
    void* ptr = std::calloc(nmemb, size);
    if (ptr != nullptr) {
        Require(trackerB.livePointers.find(ptr) == trackerB.livePointers.end());
        Require(trackerA.livePointers.find(ptr) == trackerA.livePointers.end());
        trackerA.livePointers[ptr] = nmemb * size;
#ifndef _WIN32
        if (captureNextCalloc) {
            captureNextCalloc = false;
            capturedDuplicate = static_cast<CURL*>(ptr);
            capturedDuplicateBytes = nmemb * size;
        }
#endif
    }
    return ptr;
}

static void* APS5_VABI AllocatorB_Malloc(size_t size) {
    ++trackerB.mallocCalls;
    if (trackerB.failNextMalloc) {
        trackerB.failNextMalloc = false;
        return nullptr;
    }
    void* ptr = std::malloc(size);
    if (ptr != nullptr) {
        Require(trackerA.livePointers.find(ptr) == trackerA.livePointers.end());
        Require(trackerB.livePointers.find(ptr) == trackerB.livePointers.end());
        trackerB.livePointers[ptr] = size;
    }
    return ptr;
}

static void APS5_VABI AllocatorB_Free(void* ptr) {
    ++trackerB.freeCalls;
    if (ptr == nullptr) {
        return;
    }
    Require(trackerA.livePointers.find(ptr) == trackerA.livePointers.end());
    auto it = trackerB.livePointers.find(ptr);
    Require(it != trackerB.livePointers.end());
    trackerB.livePointers.erase(it);
    std::free(ptr);
}

static void* APS5_VABI AllocatorB_Realloc(void* ptr, size_t size) {
    ++trackerB.reallocCalls;
    if (trackerB.failNextRealloc) {
        trackerB.failNextRealloc = false;
        return nullptr;
    }
    if (ptr == nullptr) {
        void* newPtr = std::malloc(size);
        if (newPtr != nullptr) {
            Require(trackerA.livePointers.find(newPtr) == trackerA.livePointers.end());
            trackerB.livePointers[newPtr] = size;
        }
        return newPtr;
    }
    Require(trackerA.livePointers.find(ptr) == trackerA.livePointers.end());
    auto it = trackerB.livePointers.find(ptr);
    Require(it != trackerB.livePointers.end());
    if (size == 0) {
        trackerB.livePointers.erase(it);
        std::free(ptr);
        return nullptr;
    }
    void* newPtr = std::realloc(ptr, size);
    if (newPtr != nullptr) {
        trackerB.livePointers.erase(it);
        trackerB.livePointers[newPtr] = size;
    }
    return newPtr;
}

static char* APS5_VABI AllocatorB_Strdup(const char* str) {
    ++trackerB.strdupCalls;
    if (trackerB.failNextStrdup) {
        trackerB.failNextStrdup = false;
        return nullptr;
    }
    if (str == nullptr) {
        return nullptr;
    }
    const size_t len = std::strlen(str) + 1;
    char* ptr = static_cast<char*>(std::malloc(len));
    if (ptr != nullptr) {
        std::memcpy(ptr, str, len);
        Require(trackerA.livePointers.find(ptr) == trackerA.livePointers.end());
        Require(trackerB.livePointers.find(ptr) == trackerB.livePointers.end());
        trackerB.livePointers[ptr] = len;
    }
    return ptr;
}

static void* APS5_VABI AllocatorB_Calloc(size_t nmemb, size_t size) {
    ++trackerB.callocCalls;
    if (trackerB.failNextCalloc) {
        trackerB.failNextCalloc = false;
        return nullptr;
    }
    void* ptr = std::calloc(nmemb, size);
    if (ptr != nullptr) {
        Require(trackerA.livePointers.find(ptr) == trackerA.livePointers.end());
        Require(trackerB.livePointers.find(ptr) == trackerB.livePointers.end());
        trackerB.livePointers[ptr] = nmemb * size;
    }
    return ptr;
}

static void TestNullCallbackRejection() {
    trackerA.ResetCounts();
    trackerA.ClearFailures();

    Require(curl_global_init_mem_nid_postfix(std::numeric_limits<long long>::max(), nullptr, AllocatorA_Free, AllocatorA_Realloc, AllocatorA_Strdup, AllocatorA_Calloc) == CURLE_FAILED_INIT);
    Require(curl_global_init_mem_nid_postfix(CURL_GLOBAL_DEFAULT, AllocatorA_Malloc, nullptr, AllocatorA_Realloc, AllocatorA_Strdup, AllocatorA_Calloc) == CURLE_FAILED_INIT);
    Require(curl_global_init_mem_nid_postfix(CURL_GLOBAL_DEFAULT, AllocatorA_Malloc, AllocatorA_Free, nullptr, AllocatorA_Strdup, AllocatorA_Calloc) == CURLE_FAILED_INIT);
    Require(curl_global_init_mem_nid_postfix(CURL_GLOBAL_DEFAULT, AllocatorA_Malloc, AllocatorA_Free, AllocatorA_Realloc, nullptr, AllocatorA_Calloc) == CURLE_FAILED_INIT);
    Require(curl_global_init_mem_nid_postfix(CURL_GLOBAL_DEFAULT, AllocatorA_Malloc, AllocatorA_Free, AllocatorA_Realloc, AllocatorA_Strdup, nullptr) == CURLE_FAILED_INIT);
    Require(curl_global_init_mem_nid_postfix(CURL_GLOBAL_DEFAULT, nullptr, nullptr, nullptr, nullptr, nullptr) == CURLE_FAILED_INIT);

    Require(trackerA.TotalAllocations() == 0);
    Require(trackerA.LiveCount() == 0);

    curl_global_cleanup_nid_postfix();
    Require(trackerA.LiveCount() == 0);
}

static void TestFailedGlobalInitRetainsCallbacks() {
    Require(trackerA.LiveCount() == 0);
    trackerA.failNextCalloc = true;
    Require(curl_global_init_mem_nid_postfix(CURL_GLOBAL_DEFAULT, AllocatorA_Malloc, AllocatorA_Free, AllocatorA_Realloc, AllocatorA_Strdup, AllocatorA_Calloc) == CURLE_FAILED_INIT);
    Require(!trackerA.failNextCalloc && trackerA.LiveCount() == 0);
    CurlSlist* list = curl_slist_append(nullptr, "after_failed_init");
    Require(list != nullptr && trackerA.livePointers.contains(list));
    curl_slist_free_all(list);
    Require(trackerA.LiveCount() == 0);
    curl_global_cleanup_nid_postfix();
    Require(curl_global_init_nid_postfix(CURL_GLOBAL_DEFAULT) == CURLE_OK);
    const auto calls = trackerA.TotalAllocations();
    list = curl_slist_append(nullptr, "default_after_failed_init");
    Require(list != nullptr && trackerA.TotalAllocations() == calls);
    curl_slist_free_all(list);
    curl_global_cleanup_nid_postfix();
    Require(trackerA.LiveCount() == 0);
}

static void TestNestedInitAndReplacementIgnored() {
    trackerA.ResetCounts();
    trackerB.ResetCounts();
    trackerA.ClearFailures();
    trackerB.ClearFailures();

    Require(curl_global_init_mem_nid_postfix(CURL_GLOBAL_DEFAULT, AllocatorA_Malloc, AllocatorA_Free, AllocatorA_Realloc, AllocatorA_Strdup, AllocatorA_Calloc) == CURLE_OK);
    const auto globalPointersA = trackerA.livePointers;

    Require(curl_global_init_mem_nid_postfix(CURL_GLOBAL_DEFAULT, nullptr, AllocatorA_Free, AllocatorA_Realloc, AllocatorA_Strdup, AllocatorA_Calloc) == CURLE_FAILED_INIT);
    Require(curl_global_init_mem_nid_postfix(CURL_GLOBAL_DEFAULT, AllocatorA_Malloc, nullptr, AllocatorA_Realloc, AllocatorA_Strdup, AllocatorA_Calloc) == CURLE_FAILED_INIT);
    Require(curl_global_init_mem_nid_postfix(CURL_GLOBAL_DEFAULT, AllocatorA_Malloc, AllocatorA_Free, nullptr, AllocatorA_Strdup, AllocatorA_Calloc) == CURLE_FAILED_INIT);
    Require(curl_global_init_mem_nid_postfix(CURL_GLOBAL_DEFAULT, AllocatorA_Malloc, AllocatorA_Free, AllocatorA_Realloc, nullptr, AllocatorA_Calloc) == CURLE_FAILED_INIT);
    Require(curl_global_init_mem_nid_postfix(CURL_GLOBAL_DEFAULT, AllocatorA_Malloc, AllocatorA_Free, AllocatorA_Realloc, AllocatorA_Strdup, nullptr) == CURLE_FAILED_INIT);
    Require(curl_global_init_mem_nid_postfix(CURL_GLOBAL_DEFAULT, nullptr, nullptr, nullptr, nullptr, nullptr) == CURLE_FAILED_INIT);

    Require(curl_global_init_mem_nid_postfix(std::numeric_limits<long long>::max(), AllocatorB_Malloc, AllocatorB_Free, AllocatorB_Realloc, AllocatorB_Strdup, AllocatorB_Calloc) == CURLE_OK);
    Require(curl_global_init_nid_postfix(std::numeric_limits<long long>::min()) == CURLE_OK);

    Require(trackerB.TotalAllocations() == 0);

    CurlSlist* list = curl_slist_append(nullptr, "nested_verify");
    Require(list != nullptr);
    Require(trackerA.TotalAllocations() > 0);
    Require(trackerB.TotalAllocations() == 0);
    curl_slist_free_all(list);
    Require(trackerA.livePointers == globalPointersA);
    Require(trackerB.TotalAllocations() == 0);

    curl_global_cleanup_nid_postfix();
    curl_global_cleanup_nid_postfix();

    char* escaped = curl_easy_escape(nullptr, "nested_after_dec", 16);
    Require(escaped != nullptr);
    Require(trackerA.livePointers.find(escaped) != trackerA.livePointers.end());
    Require(trackerB.TotalAllocations() == 0);
    curl_free(escaped);
    Require(trackerA.livePointers == globalPointersA);

    curl_global_cleanup_nid_postfix();
    Require(trackerA.LiveCount() == 0);
}

static void TestCallbacksRetainedAfterCleanup() {
    Require(curl_global_init_mem_nid_postfix(CURL_GLOBAL_DEFAULT, AllocatorA_Malloc, AllocatorA_Free, AllocatorA_Realloc, AllocatorA_Strdup, AllocatorA_Calloc) == CURLE_OK);
    char* retained = curl_easy_escape(nullptr, "allocated before cleanup", 0);
    Require(retained != nullptr && trackerA.livePointers.contains(retained));
    curl_global_cleanup_nid_postfix();
    Require(trackerA.livePointers.contains(retained));
    curl_free(retained);
    Require(trackerA.LiveCount() == 0);
    const auto freeCallsBefore = trackerA.freeCalls;
    curl_free(nullptr);
    Require(trackerA.freeCalls == freeCallsBefore + 1);

    char* escaped = curl_easy_escape(nullptr, "retained_after_cleanup", 22);
    Require(escaped != nullptr);
    Require(trackerA.livePointers.find(escaped) != trackerA.livePointers.end());
    Require(trackerB.TotalAllocations() == 0);
    curl_free(escaped);
    Require(trackerA.LiveCount() == 0);

    CurlSlist* list = curl_slist_append(nullptr, "retained_list");
    Require(list != nullptr);
    Require(trackerA.livePointers.find(list) != trackerA.livePointers.end());
    Require(trackerB.TotalAllocations() == 0);
    curl_slist_free_all(list);
    Require(trackerA.LiveCount() == 0);
}

static void TestDefaultResetByExplicitGlobalInit() {
    Require(trackerA.LiveCount() == 0);
    Require(trackerB.LiveCount() == 0);

    Require(curl_global_init_nid_postfix(CURL_GLOBAL_DEFAULT) == CURLE_OK);

    const auto aCallsBefore = trackerA.TotalAllocations();
    const auto bCallsBefore = trackerB.TotalAllocations();

    CurlSlist* list = curl_slist_append(nullptr, "default_reset_explicit");
    Require(list != nullptr);
    Require(trackerA.TotalAllocations() == aCallsBefore);
    Require(trackerB.TotalAllocations() == bCallsBefore);
    curl_slist_free_all(list);
    Require(trackerA.TotalAllocations() == aCallsBefore);
    Require(trackerB.TotalAllocations() == bCallsBefore);

    char* escaped = curl_easy_escape(nullptr, "default_escape_explicit", 23);
    Require(escaped != nullptr);
    Require(trackerA.TotalAllocations() == aCallsBefore);
    Require(trackerB.TotalAllocations() == bCallsBefore);
    curl_free(escaped);
    Require(trackerA.TotalAllocations() == aCallsBefore);
    Require(trackerB.TotalAllocations() == bCallsBefore);

    curl_global_cleanup_nid_postfix();
}

static void TestFamilyBInstallation() {
    Require(trackerA.LiveCount() == 0);
    Require(trackerB.LiveCount() == 0);
    trackerA.ResetCounts();
    trackerB.ResetCounts();

    Require(curl_global_init_mem_nid_postfix(CURL_GLOBAL_DEFAULT, AllocatorB_Malloc, AllocatorB_Free, AllocatorB_Realloc, AllocatorB_Strdup, AllocatorB_Calloc) == CURLE_OK);
    const auto globalPointersB = trackerB.livePointers;

    const auto aCallsBefore = trackerA.TotalAllocations();

    CurlSlist* list = curl_slist_append(nullptr, "family_b_first");
    list = curl_slist_append(list, "family_b_second");
    Require(list != nullptr);
    Require(trackerB.TotalAllocations() > 0);
    Require(trackerA.TotalAllocations() == aCallsBefore);
    Require(trackerB.LiveCount() == globalPointersB.size() + 4);

    curl_slist_free_all(list);
    Require(trackerB.livePointers == globalPointersB);
    Require(trackerA.TotalAllocations() == aCallsBefore);

    char* escaped = curl_easy_escape(nullptr, "family_b_escape", 15);
    Require(escaped != nullptr);
    Require(trackerB.livePointers.find(escaped) != trackerB.livePointers.end());
    Require(trackerA.TotalAllocations() == aCallsBefore);
    curl_free(escaped);
    Require(trackerB.livePointers == globalPointersB);

    curl_global_cleanup_nid_postfix();
    Require(trackerB.LiveCount() == 0);
}

static void TestDefaultResetByImplicitEasyInit() {
    Require(trackerA.LiveCount() == 0);
    Require(trackerB.LiveCount() == 0);

    const auto aCallsBefore = trackerA.TotalAllocations();
    const auto bCallsBefore = trackerB.TotalAllocations();

    CURL* easy = curl_easy_init_nid_postfix();
    Require(easy != nullptr);
    Require(trackerA.TotalAllocations() == aCallsBefore);
    Require(trackerB.TotalAllocations() == bCallsBefore);

    curl_easy_cleanup_nid_postfix(easy);
    Require(trackerA.TotalAllocations() == aCallsBefore);
    Require(trackerB.TotalAllocations() == bCallsBefore);

    curl_global_cleanup_nid_postfix();
    Require(trackerA.TotalAllocations() == aCallsBefore);
    Require(trackerB.TotalAllocations() == bCallsBefore);
}

static void TestObservabilityOfAllFiveCallbacks() {
    Require(trackerA.LiveCount() == 0);
    Require(trackerB.LiveCount() == 0);
    trackerA.ResetCounts();
    trackerB.ResetCounts();

    Require(curl_global_init_mem_nid_postfix(CURL_GLOBAL_DEFAULT, AllocatorA_Malloc, AllocatorA_Free, AllocatorA_Realloc, AllocatorA_Strdup, AllocatorA_Calloc) == CURLE_OK);
    const auto globalPointersA = trackerA.livePointers;

    const auto callocCallsBefore = trackerA.callocCalls;
    CURLU* urlHandle = curl_url_nid_postfix();
    Require(urlHandle != nullptr);
    Require(trackerA.callocCalls > callocCallsBefore);
    Require(trackerA.livePointers.find(urlHandle) != trackerA.livePointers.end());

    Require(curl_url_set_nid_postfix(urlHandle, CURLUPART_SCHEME, "https", 0) == CURLUE_OK);
    Require(curl_url_set_nid_postfix(urlHandle, CURLUPART_HOST, "example.com", 0) == CURLUE_OK);
    Require(trackerA.strdupCalls > 0);

    char* scheme = nullptr;
    Require(curl_url_get_nid_postfix(urlHandle, CURLUPART_SCHEME, &scheme, 0) == CURLUE_OK);
    Require(scheme != nullptr && std::strcmp(scheme, "https") == 0);
    Require(trackerA.livePointers.find(scheme) != trackerA.livePointers.end());

    const auto freeCallsBefore = trackerA.freeCalls;
    curl_free(scheme);
    Require(trackerA.freeCalls > freeCallsBefore);
    Require(trackerA.livePointers.find(scheme) == trackerA.livePointers.end());

    curl_url_cleanup_nid_postfix(urlHandle);

    const auto mallocCallsBefore = trackerA.mallocCalls;
    CurlSlist* slist = curl_slist_append(nullptr, "test_malloc");
    Require(slist != nullptr);
    Require(trackerA.mallocCalls > mallocCallsBefore);
    curl_slist_free_all(slist);
    Require(trackerA.livePointers == globalPointersA);

    const auto reallocCallsBefore = trackerA.reallocCalls;
    std::string longInput(64, 'x');
    char* escaped = curl_easy_escape(nullptr, longInput.c_str(), static_cast<int>(longInput.size()));
    Require(escaped != nullptr);
    Require(trackerA.reallocCalls > reallocCallsBefore);
    Require(trackerA.livePointers.find(escaped) != trackerA.livePointers.end());
    curl_free(escaped);
    Require(trackerA.livePointers == globalPointersA);

    Require(trackerA.mallocCalls > 0);
    Require(trackerA.freeCalls > 0);
    Require(trackerA.reallocCalls > 0);
    Require(trackerA.strdupCalls > 0);
    Require(trackerA.callocCalls > 0);

    curl_global_cleanup_nid_postfix();
    Require(trackerA.LiveCount() == 0);
}

static void TestBinaryEscapingAndResultLengths() {
    Require(curl_global_init_mem_nid_postfix(CURL_GLOBAL_DEFAULT, AllocatorA_Malloc, AllocatorA_Free, AllocatorA_Realloc, AllocatorA_Strdup, AllocatorA_Calloc) == CURLE_OK);
    const auto globalPointersA = trackerA.livePointers;

    const char binaryInput[] = {'H', 'e', 'l', 'l', 'o', '\0', 'W', 'o', 'r', 'l', 'd', static_cast<char>(0x80), static_cast<char>(0xFF), '%', ' '};
    const int binaryLen = static_cast<int>(sizeof(binaryInput));

    char* escaped = curl_easy_escape(nullptr, binaryInput, binaryLen);
    Require(escaped != nullptr);
    Require(trackerA.livePointers.find(escaped) != trackerA.livePointers.end());
    Require(std::strcmp(escaped, "Hello%00World%80%FF%25%20") == 0);

    int outLength = -1;
    char* unescaped = curl_easy_unescape(nullptr, escaped, static_cast<int>(std::strlen(escaped)), &outLength);
    Require(unescaped != nullptr);
    Require(trackerA.livePointers.find(unescaped) != trackerA.livePointers.end());
    Require(outLength == binaryLen);
    Require(std::memcmp(unescaped, binaryInput, binaryLen) == 0);

    curl_free(unescaped);
    curl_free(escaped);
    Require(trackerA.livePointers == globalPointersA);

    char* esc2 = curl_escape(binaryInput, binaryLen);
    Require(esc2 != nullptr);
    Require(trackerA.livePointers.find(esc2) != trackerA.livePointers.end());
    Require(std::strcmp(esc2, "Hello%00World%80%FF%25%20") == 0);

    char* unesc2 = curl_unescape(esc2, static_cast<int>(std::strlen(esc2)));
    Require(unesc2 != nullptr);
    Require(trackerA.livePointers.find(unesc2) != trackerA.livePointers.end());
    Require(std::memcmp(unesc2, binaryInput, binaryLen) == 0);

    curl_free(unesc2);
    curl_free(esc2);
    Require(trackerA.livePointers == globalPointersA);

    char* emptyEsc = curl_easy_escape(nullptr, "", 0);
    Require(emptyEsc != nullptr);
    Require(emptyEsc[0] == '\0');
    int emptyLen = -1;
    char* emptyUnesc = curl_easy_unescape(nullptr, emptyEsc, 0, &emptyLen);
    Require(emptyUnesc != nullptr);
    Require(emptyLen == 0);
    Require(emptyUnesc[0] == '\0');
    curl_free(emptyUnesc);
    curl_free(emptyEsc);
    Require(trackerA.livePointers == globalPointersA);

    curl_global_cleanup_nid_postfix();
    Require(trackerA.LiveCount() == 0);
}

static void TestDeterministicFailureInjection() {
    Require(curl_global_init_mem_nid_postfix(CURL_GLOBAL_DEFAULT, AllocatorA_Malloc, AllocatorA_Free, AllocatorA_Realloc, AllocatorA_Strdup, AllocatorA_Calloc) == CURLE_OK);
    const auto globalPointersA = trackerA.livePointers;

    trackerA.ClearFailures();
    trackerA.failNextStrdup = true;
    CurlSlist* slistStrdupFail = curl_slist_append(nullptr, "strdup_fail");
    Require(slistStrdupFail == nullptr);
    Require(trackerA.livePointers == globalPointersA);

    trackerA.ClearFailures();
    trackerA.failNextMalloc = true;
    CurlSlist* slistMallocFail = curl_slist_append(nullptr, "malloc_fail");
    Require(slistMallocFail == nullptr);
    Require(trackerA.livePointers == globalPointersA);

    trackerA.ClearFailures();
    trackerA.failNextRealloc = true;
    char* escFail = curl_easy_escape(nullptr, "realloc_fail", 12);
    Require(escFail == nullptr);
    Require(trackerA.livePointers == globalPointersA);

    trackerA.ClearFailures();
    trackerA.failNextCalloc = true;
    CURLU* urlFail = curl_url_nid_postfix();
    Require(urlFail == nullptr);
    Require(trackerA.livePointers == globalPointersA);

    CURLU* urlValid = curl_url_nid_postfix();
    Require(urlValid != nullptr);
    Require(curl_url_set_nid_postfix(urlValid, CURLUPART_SCHEME, "https", 0) == CURLUE_OK);
    trackerA.ClearFailures();
    trackerA.failNextStrdup = true;
    char* partFail = nullptr;
    Require(curl_url_get_nid_postfix(urlValid, CURLUPART_SCHEME, &partFail, 0) == CURLUE_OUT_OF_MEMORY);
    Require(partFail == nullptr);
    curl_url_cleanup_nid_postfix(urlValid);
    Require(trackerA.livePointers == globalPointersA);

    trackerA.ClearFailures();
    trackerA.failNextCalloc = true;
    CURL* easyFail = curl_easy_init_nid_postfix();
    Require(easyFail == nullptr);
    Require(trackerA.livePointers == globalPointersA);

    trackerA.ClearFailures();
    curl_global_cleanup_nid_postfix();
    Require(trackerA.LiveCount() == 0);
}

static void TestBackendObjectsLifecycle() {
    Require(curl_global_init_mem_nid_postfix(CURL_GLOBAL_DEFAULT, AllocatorA_Malloc, AllocatorA_Free, AllocatorA_Realloc, AllocatorA_Strdup, AllocatorA_Calloc) == CURLE_OK);
    const auto globalPointersA = trackerA.livePointers;

    CURL* easy = curl_easy_init_nid_postfix();
    Require(easy != nullptr);
    Require(trackerA.livePointers.contains(easy));

    CURL* duplicate = curl_easy_duphandle_nid_postfix(easy);
    Require(duplicate != nullptr && duplicate != easy);

    curl_easy_cleanup_nid_postfix(duplicate);
    curl_easy_cleanup_nid_postfix(easy);
    Require(trackerA.livePointers == globalPointersA);

    CURLU* url = curl_url_nid_postfix();
    Require(url != nullptr);
    Require(curl_url_set_nid_postfix(url, CURLUPART_URL, "https://example.com/test", 0) == CURLUE_OK);

    CURLU* urlDup = curl_url_dup_nid_postfix(url);
    Require(urlDup != nullptr && urlDup != url);

    char* fullUrl = nullptr;
    Require(curl_url_get_nid_postfix(urlDup, CURLUPART_URL, &fullUrl, 0) == CURLUE_OK);
    Require(fullUrl != nullptr);
    Require(std::strcmp(fullUrl, "https://example.com/test") == 0);
    curl_free(fullUrl);

    curl_url_cleanup_nid_postfix(urlDup);
    curl_url_cleanup_nid_postfix(url);
    Require(trackerA.livePointers == globalPointersA);

    curl_global_cleanup_nid_postfix();
    Require(trackerA.LiveCount() == 0);
}

struct DuplicateSource {
    int frees = 0;
};

static std::size_t APS5_VABI DuplicateRead(char*, std::size_t, std::size_t, void*) { return 0; }
static void APS5_VABI DuplicateFree(void* opaque) { ++static_cast<DuplicateSource*>(opaque)->frees; }

#ifndef _WIN32
static void ProbeDuplicateRegistration() {
    duplicateRegisteredAtFault = false;
    if (!capturedDuplicate || !trackerA.livePointers.contains(capturedDuplicate)) return;
    try {
        duplicateRegisteredAtFault = curl_easy_setopt_nid_postfix(capturedDuplicate, CURLOPT_PRIVATE, &duplicateRegisteredAtFault) == CURLE_OK;
    } catch (...) {}
}
#endif

static void TestDuplicateBookkeepingFailure() {
    Require(curl_global_init_mem_nid_postfix(CURL_GLOBAL_DEFAULT, AllocatorA_Malloc, AllocatorA_Free, AllocatorA_Realloc, AllocatorA_Strdup, AllocatorA_Calloc) == CURLE_OK);
    const auto globalPointers = trackerA.livePointers;
    for (bool copiedSource : {false, true}) {
        DuplicateSource source;
        CURL* easy = curl_easy_init_nid_postfix();
        Require(easy != nullptr);
        auto* mime = curl_mime_init_nid_postfix(easy);
        Require(mime != nullptr);
        auto* part = curl_mime_addpart_nid_postfix(mime);
        Require(part != nullptr);
        Require(curl_mime_data_cb_nid_postfix(part, 0, DuplicateRead, nullptr, DuplicateFree, &source) == CURLE_OK);
        Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_MIMEPOST, mime) == CURLE_OK);
        if (copiedSource) {
            CURL* copy = curl_easy_duphandle_nid_postfix(easy);
            Require(copy != nullptr);
            curl_mime_free_nid_postfix(mime);
            mime = nullptr;
            curl_easy_cleanup_nid_postfix(easy);
            easy = copy;
        }

        CURL* warm = curl_easy_duphandle_nid_postfix(easy);
        Require(warm != nullptr);
        curl_easy_cleanup_nid_postfix(warm);
        const auto before = trackerA.livePointers;
        int expectedFrees = source.frees;
        throwNextGuestCalloc = true;
        bool guestException = false;
        try {
            curl_easy_duphandle_nid_postfix(easy);
        } catch (const GuestAllocationFailure&) {
            guestException = true;
        }
        Require(guestException && !throwNextGuestCalloc);
        Require(trackerA.livePointers == before && source.frees == expectedFrees);

#ifndef _WIN32
        const auto nativeBytes = trackerA.livePointers.at(easy);
        capturedDuplicate = nullptr;
        capturedDuplicateBytes = 0;
        duplicateRegisteredAtFault = false;
        captureNextCalloc = true;
        hostFaultProbe = ProbeDuplicateRegistration;
        failHostAllocationOfSize = sizeof(std::shared_ptr<void>);
        CURL* failed = nullptr;
        bool escaped = false;
        try {
            failed = curl_easy_duphandle_nid_postfix(easy);
        } catch (const std::bad_alloc&) {
            escaped = true;
        }
        const bool consumed = failHostAllocationOfSize == 0;
        failHostAllocationOfSize = 0;
        hostFaultProbe = nullptr;
        const bool captured = !captureNextCalloc;
        captureNextCalloc = false;
        if (!consumed || !captured || capturedDuplicateBytes != nativeBytes || !duplicateRegisteredAtFault) {
            std::fprintf(stderr, "curl duplicate fault stage invalid: consumed=%d captured=%d bytes=%zu expected=%zu registered=%d\n",
                consumed, captured, capturedDuplicateBytes, nativeBytes, duplicateRegisteredAtFault);
            std::abort();
        }
        ++expectedFrees;
        if (failed || escaped || trackerA.livePointers != before || source.frees != expectedFrees) {
            std::fprintf(stderr, "curl duplicate bookkeeping cleanup failed: copied=%d registered=%d escaped=%d before=%zu after=%zu frees=%d expected=%d\n",
                copiedSource, duplicateRegisteredAtFault, escaped, before.size(), trackerA.LiveCount(), source.frees, expectedFrees);
            std::abort();
        }
        Require(!trackerA.livePointers.contains(capturedDuplicate) && trackerB.LiveCount() == 0);
#endif

        CURL* successful = curl_easy_duphandle_nid_postfix(easy);
        Require(successful != nullptr);
        curl_easy_cleanup_nid_postfix(successful);
        Require(source.frees == ++expectedFrees);
        if (mime) {
            curl_mime_free_nid_postfix(mime);
            Require(source.frees == ++expectedFrees);
        }
        curl_easy_cleanup_nid_postfix(easy);
        if (copiedSource) ++expectedFrees;
        Require(source.frees == expectedFrees && trackerA.livePointers == globalPointers && trackerB.LiveCount() == 0);
    }
    curl_global_cleanup_nid_postfix();
    Require(trackerA.LiveCount() == 0 && trackerB.LiveCount() == 0);
}

static void TestRepeatSetupTeardown() {
    for (int cycle = 0; cycle < 3; ++cycle) {
        Require(trackerA.LiveCount() == 0);
        Require(trackerB.LiveCount() == 0);

        Require(curl_global_init_mem_nid_postfix(CURL_GLOBAL_DEFAULT, AllocatorA_Malloc, AllocatorA_Free, AllocatorA_Realloc, AllocatorA_Strdup, AllocatorA_Calloc) == CURLE_OK);
        const auto globalPointersA = trackerA.livePointers;
        CurlSlist* listA = curl_slist_append(nullptr, "cycle_a");
        Require(listA != nullptr);
        Require(trackerA.livePointers.find(listA) != trackerA.livePointers.end());
        curl_slist_free_all(listA);
        Require(trackerA.livePointers == globalPointersA);
        curl_global_cleanup_nid_postfix();
        Require(trackerA.LiveCount() == 0);

        Require(curl_global_init_nid_postfix(CURL_GLOBAL_DEFAULT) == CURLE_OK);
        curl_global_cleanup_nid_postfix();

        Require(curl_global_init_mem_nid_postfix(CURL_GLOBAL_DEFAULT, AllocatorB_Malloc, AllocatorB_Free, AllocatorB_Realloc, AllocatorB_Strdup, AllocatorB_Calloc) == CURLE_OK);
        const auto globalPointersB = trackerB.livePointers;
        CurlSlist* listB = curl_slist_append(nullptr, "cycle_b");
        Require(listB != nullptr);
        Require(trackerB.livePointers.find(listB) != trackerB.livePointers.end());
        curl_slist_free_all(listB);
        Require(trackerB.livePointers == globalPointersB);
        curl_global_cleanup_nid_postfix();
        Require(trackerB.LiveCount() == 0);

        Require(curl_global_init_nid_postfix(CURL_GLOBAL_DEFAULT) == CURLE_OK);
        curl_global_cleanup_nid_postfix();
    }
}

static std::size_t APS5_VABI CollectBytes(char* bytes, std::size_t size, std::size_t count, void* opaque) {
    const auto length = size * count;
    static_cast<std::string*>(opaque)->append(bytes, length);
    return length;
}

static void TestFileTransferUsesCustomHeap() {
    Require(trackerA.LiveCount() == 0 && trackerB.LiveCount() == 0);
    Require(curl_global_init_mem_nid_postfix(CURL_GLOBAL_DEFAULT, AllocatorA_Malloc, AllocatorA_Free, AllocatorA_Realloc, AllocatorA_Strdup, AllocatorA_Calloc) == CURLE_OK);
    const auto globalPointersA = trackerA.livePointers;
    const auto path = std::filesystem::temp_directory_path() /
        ("anyps5-curl-allocator-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const char bytes[] = {'a', '\0', 'b', static_cast<char>(0xff), '\r', '\n'};
    {
        std::ofstream file(path, std::ios::binary);
        Require(static_cast<bool>(file.write(bytes, sizeof(bytes))));
        file.close();
        Require(!file.fail());
    }
    auto* easy = curl_easy_init_nid_postfix();
    Require(easy != nullptr && trackerA.livePointers.contains(easy));
    const auto url = "file://" + path.generic_string();
    std::string output;
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_URL, url.c_str()) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_WRITEFUNCTION, CollectBytes) == CURLE_OK);
    Require(curl_easy_setopt_nid_postfix(easy, CURLOPT_WRITEDATA, &output) == CURLE_OK);
    Require(curl_easy_perform_nid_postfix(easy) == CURLE_OK);
    Require(output == std::string(bytes, sizeof(bytes)));
    curl_easy_cleanup_nid_postfix(easy);
    Require(trackerA.livePointers == globalPointersA && trackerB.LiveCount() == 0);
    Require(std::filesystem::remove(path));
    curl_global_cleanup_nid_postfix();
    Require(trackerA.LiveCount() == 0 && trackerB.LiveCount() == 0);
}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());

    TestNullCallbackRejection();
    TestFailedGlobalInitRetainsCallbacks();
    TestNestedInitAndReplacementIgnored();
    TestCallbacksRetainedAfterCleanup();
    TestDefaultResetByExplicitGlobalInit();
    TestFamilyBInstallation();
    TestDefaultResetByImplicitEasyInit();
    TestObservabilityOfAllFiveCallbacks();
    TestBinaryEscapingAndResultLengths();
    TestDeterministicFailureInjection();
    TestBackendObjectsLifecycle();
    TestDuplicateBookkeepingFailure();
    TestRepeatSetupTeardown();
    TestFileTransferUsesCustomHeap();

    return 0;
}
