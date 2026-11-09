#include "prx/libcurl/libcurl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition, const std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "GuestCurl check failed at line %u\n", location.line());
        std::abort();
    }
}

std::size_t activeAllocations = 0;
std::size_t allocationCalls = 0;
int failureCountdown = 0;

void* APS5_VABI Allocate(std::size_t bytes) {
    ++allocationCalls;
    if (failureCountdown > 0 && --failureCountdown == 0) return nullptr;
    void* pointer = std::malloc(bytes);
    if (pointer != nullptr) ++activeAllocations;
    return pointer;
}

void APS5_VABI Free(void* pointer) {
    if (pointer == nullptr) return;
    Require(activeAllocations != 0);
    --activeAllocations;
    std::free(pointer);
}

void* APS5_VABI Reallocate(void*, std::size_t) { throw std::logic_error("unexpected reallocation"); }
void* APS5_VABI Calloc(std::size_t, std::size_t) { throw std::logic_error("unexpected calloc"); }
void* APS5_VABI Align(std::size_t, std::size_t) { throw std::logic_error("unexpected aligned allocation"); }
void* APS5_VABI Realign(void*, std::size_t, std::size_t) { throw std::logic_error("unexpected aligned reallocation"); }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { throw std::logic_error("unexpected posix_memalign"); }

void InitializeHeap() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Free),
        reinterpret_cast<void*>(&Calloc), reinterpret_cast<void*>(&Reallocate),
        reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign),
        reinterpret_cast<void*>(&PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api.data());
}

void CheckEncoded(const char* input, int length, std::string_view expected) {
    char* encoded = curl_easy_escape(nullptr, input, length);
    Require(encoded != nullptr);
    Require(std::string_view(encoded) == expected);
    char* legacy = curl_escape(input, length);
    Require(legacy != nullptr && legacy != encoded);
    Require(std::strcmp(encoded, legacy) == 0);
    curl_free(encoded);
    curl_free(legacy);
}

void CheckDecoded(const char* input, int length, std::string_view expected) {
    struct {
        int before = 0x12345678;
        int length = -17;
        int after = 0x23456789;
    } result;
    char* decoded = curl_easy_unescape(nullptr, input, length, &result.length);
    Require(decoded != nullptr);
    Require(result.length == static_cast<int>(expected.size()));
    Require(result.before == 0x12345678 && result.after == 0x23456789);
    Require(std::memcmp(decoded, expected.data(), expected.size()) == 0);
    Require(decoded[expected.size()] == '\0');
    char* withoutLength = curl_easy_unescape(nullptr, input, length, nullptr);
    char* legacy = curl_unescape(input, length);
    Require(withoutLength != nullptr && legacy != nullptr);
    Require(std::memcmp(decoded, withoutLength, expected.size() + 1) == 0);
    Require(std::memcmp(decoded, legacy, expected.size() + 1) == 0);
    curl_free(decoded);
    curl_free(withoutLength);
    curl_free(legacy);
}

void CheckEscapes() {
    CheckEncoded("", 0, "");
    CheckEncoded("AZaz09-._~", 0, "AZaz09-._~");
    CheckEncoded("a b+c/%?&=", 0, "a%20b%2Bc%2F%25%3F%26%3D");
    CheckEncoded("abcdef", 3, "abc");
    constexpr char binary[] = {'A', '\0', 'B', '%', '\0'};
    CheckEncoded(binary, 4, "A%00B%25");
    CheckEncoded(binary, 0, "A");
    CheckDecoded("", 0, "");
    CheckDecoded("a+b%20c", 0, "a+b c");
    CheckDecoded("%2541", 0, "%41");
    CheckDecoded("%%41", 0, "%A");
    CheckDecoded("%", 0, "%");
    CheckDecoded("%0", 0, "%0");
    CheckDecoded("%GG%4g%g4%+1% 1%1\n", 0, "%GG%4g%g4%+1% 1%1\n");
    CheckDecoded("%41%42", 4, "A%");
    CheckDecoded("%41%42", 2, "%4");
    CheckDecoded("%00%01%7f%80%fF", 0, std::string_view("\0\x01\x7f\x80\xff", 5));
    constexpr char encodedBinary[] = {'%', '4', '1', '\0', '%', '4', '2'};
    CheckDecoded(encodedBinary, 7, std::string_view("A\0B", 3));
    CheckDecoded(encodedBinary, 0, "A");

    std::array<char, 256> bytes{};
    for (std::size_t index = 0; index < bytes.size(); ++index) bytes[index] = static_cast<char>(index);
    constexpr std::string_view allEncoded =
        "%00%01%02%03%04%05%06%07%08%09%0A%0B%0C%0D%0E%0F"
        "%10%11%12%13%14%15%16%17%18%19%1A%1B%1C%1D%1E%1F"
        "%20%21%22%23%24%25%26%27%28%29%2A%2B%2C-.%2F"
        "0123456789%3A%3B%3C%3D%3E%3F%40ABCDEFGHIJKLMNOPQRSTUVWXYZ%5B%5C%5D%5E_"
        "%60abcdefghijklmnopqrstuvwxyz%7B%7C%7D~%7F"
        "%80%81%82%83%84%85%86%87%88%89%8A%8B%8C%8D%8E%8F"
        "%90%91%92%93%94%95%96%97%98%99%9A%9B%9C%9D%9E%9F"
        "%A0%A1%A2%A3%A4%A5%A6%A7%A8%A9%AA%AB%AC%AD%AE%AF"
        "%B0%B1%B2%B3%B4%B5%B6%B7%B8%B9%BA%BB%BC%BD%BE%BF"
        "%C0%C1%C2%C3%C4%C5%C6%C7%C8%C9%CA%CB%CC%CD%CE%CF"
        "%D0%D1%D2%D3%D4%D5%D6%D7%D8%D9%DA%DB%DC%DD%DE%DF"
        "%E0%E1%E2%E3%E4%E5%E6%E7%E8%E9%EA%EB%EC%ED%EE%EF"
        "%F0%F1%F2%F3%F4%F5%F6%F7%F8%F9%FA%FB%FC%FD%FE%FF";
    CheckEncoded(bytes.data(), static_cast<int>(bytes.size()), allEncoded);
    CheckDecoded(allEncoded.data(), 0, std::string_view(bytes.data(), bytes.size()));
    std::string lowercase(allEncoded);
    for (std::size_t index = 0; index < lowercase.size(); ++index) {
        if (lowercase[index] != '%') continue;
        for (int digit = 0; digit < 2; ++digit) {
            auto& byte = lowercase[++index];
            if (byte >= 'A' && byte <= 'F') byte += 'a' - 'A';
        }
    }
    CheckDecoded(lowercase.c_str(), 0, std::string_view(bytes.data(), bytes.size()));

    char handle = 0;
    char* encoded = curl_easy_escape(&handle, "?", 0);
    Require(encoded != nullptr && std::strcmp(encoded, "%3F") == 0);
    curl_free(encoded);
    int outputLength = -1;
    char* decoded = curl_easy_unescape(&handle, "%3F", 0, &outputLength);
    Require(decoded != nullptr && outputLength == 1 && decoded[0] == '?');
    curl_free(decoded);
}

void CheckLists() {
    curl_free(nullptr);
    curl_slist_free_all(nullptr);
    char source[] = "first";
    CurlSlist* first = curl_slist_append(nullptr, source);
    Require(first != nullptr && first->next == nullptr);
    Require(first->data != source && std::strcmp(first->data, "first") == 0);
    source[0] = 'X';
    Require(std::strcmp(first->data, "first") == 0);
    Require(curl_slist_append(first, "") == first);
    Require(curl_slist_append(first, first->data) == first);
    Require(first->next != nullptr && first->next->data[0] == '\0');
    Require(first->next->next != nullptr && first->next->next->next == nullptr);
    Require(std::strcmp(first->next->next->data, "first") == 0);
    CurlSlist* second = curl_slist_append(nullptr, "separate");
    Require(second != nullptr && second != first && second->next == nullptr);
    curl_slist_free_all(first);
    Require(std::strcmp(second->data, "separate") == 0);
    curl_slist_free_all(second);
    Require(activeAllocations == 0);

    for (int count = 0; count < 1024; ++count) {
        CurlSlist* repeated = curl_slist_append(nullptr, "repeated");
        Require(repeated != nullptr);
        curl_slist_free_all(repeated);
    }
    void* memory = malloc_nid_postfix(17);
    Require(memory != nullptr);
    curl_free(memory);
    Require(activeAllocations == 0);
}

void CheckFailures() {
    int outputLength = 123;
    const auto calls = allocationCalls;
    for (int length : {-1, -2147483647 - 1}) {
        Require(curl_easy_escape(nullptr, "input", length) == nullptr);
        Require(curl_escape("input", length) == nullptr);
        Require(curl_easy_unescape(nullptr, "input", length, &outputLength) == nullptr);
        Require(curl_unescape("input", length) == nullptr);
    }
    Require(curl_easy_escape(nullptr, nullptr, -1) == nullptr);
    Require(curl_escape(nullptr, -1) == nullptr);
    Require(curl_easy_unescape(nullptr, nullptr, -1, &outputLength) == nullptr);
    Require(curl_unescape(nullptr, -1) == nullptr);
    Require(outputLength == 123 && allocationCalls == calls);

    for (int failure : {1, 2}) {
        failureCountdown = failure;
        Require(curl_slist_append(nullptr, "new") == nullptr);
        Require(activeAllocations == 0);
        CurlSlist* list = curl_slist_append(nullptr, "original");
        Require(list != nullptr);
        char* original = list->data;
        failureCountdown = failure;
        Require(curl_slist_append(list, "appended") == nullptr);
        Require(activeAllocations == 2 && list->data == original && list->next == nullptr);
        Require(std::strcmp(list->data, "original") == 0);
        curl_slist_free_all(list);
        Require(activeAllocations == 0);
    }
    for (const char* input : {"", "hello", "%00%ff"}) {
        failureCountdown = 1;
        Require(curl_easy_escape(nullptr, input, 0) == nullptr);
        failureCountdown = 1;
        Require(curl_escape(input, 0) == nullptr);
        failureCountdown = 1;
        Require(curl_easy_unescape(nullptr, input, 0, &outputLength) == nullptr);
        Require(outputLength == 123);
        failureCountdown = 1;
        Require(curl_unescape(input, 0) == nullptr);
        Require(activeAllocations == 0);
    }
}

class GuardedInput {
    char* allocation;
    std::size_t pageSize;

public:
    GuardedInput() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        allocation = static_cast<char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(allocation != nullptr);
        DWORD previous;
        Require(VirtualProtect(allocation + pageSize, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        pageSize = static_cast<std::size_t>(size);
        allocation = static_cast<char*>(mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(allocation != MAP_FAILED);
        Require(mprotect(allocation + pageSize, pageSize, PROT_NONE) == 0);
#endif
    }

    ~GuardedInput() {
#ifdef _WIN32
        Require(VirtualFree(allocation, 0, MEM_RELEASE) != 0);
#else
        Require(munmap(allocation, pageSize * 2) == 0);
#endif
    }

    GuardedInput(const GuardedInput&) = delete;
    GuardedInput& operator=(const GuardedInput&) = delete;

    char* Place(std::string_view value) {
        Require(value.size() <= pageSize);
        char* start = allocation + pageSize - value.size();
        std::memcpy(start, value.data(), value.size());
        return start;
    }
};

void CheckBoundaries() {
    GuardedInput input;
    CheckEncoded(input.Place("a %"), 3, "a%20%25");
    CheckEncoded(input.Place(std::string_view("a %\0", 4)), 0, "a%20%25");
    CheckDecoded(input.Place("%"), 1, "%");
    CheckDecoded(input.Place("%4"), 2, "%4");
    CheckDecoded(input.Place("%41"), 3, "A");
    CheckDecoded(input.Place(std::string_view("%41\0", 4)), 0, "A");
    CurlSlist* list = curl_slist_append(nullptr, input.Place(std::string_view("list\0", 5)));
    Require(list != nullptr && std::strcmp(list->data, "list") == 0);
    curl_slist_free_all(list);
}

}

int main() {
    InitializeHeap();
    CheckEscapes();
    CheckLists();
    CheckFailures();
    CheckBoundaries();
    Require(activeAllocations == 0);
}
