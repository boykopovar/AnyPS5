#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {

constexpr std::int32_t Success = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t Unsupported = 16;

struct Words {
    std::int32_t index = 0;
    std::int32_t closes = 0;
    std::int32_t lastLength = -1;
    UEnumeration* closed = nullptr;
};

constexpr std::array<const char*, 3> Narrow{"alpha", "be", ""};
constexpr std::array<const char16_t*, 3> Wide{u"alpha", u"be", u""};

std::vector<void*> freed;

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) { return std::malloc(size); }
void APS5_VABI Free_nid_no_patch(void* pointer) {
    freed.push_back(pointer);
    std::free(pointer);
}
void* APS5_VABI Calloc_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected calloc"); }
void* APS5_VABI Reallocate_nid_no_patch(void*, std::size_t) { throw std::runtime_error("unexpected realloc"); }
void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI Realign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI PosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU enumeration: %s\n", message);
    std::abort();
}

template<class F>
bool Rejects(F call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

Words& State(UEnumeration* enumeration) {
    return *static_cast<Words*>(enumeration->context);
}

void APS5_VABI Close(UEnumeration* enumeration) {
    ++State(enumeration).closes;
    State(enumeration).closed = enumeration;
}

std::int32_t APS5_VABI Count(UEnumeration*, std::int32_t* status) {
    if (*status == Success) *status = -124;
    return static_cast<std::int32_t>(Narrow.size());
}

const char* APS5_VABI Next(UEnumeration* enumeration, std::int32_t* length, std::int32_t*) {
    auto& words = State(enumeration);
    if (words.index >= static_cast<std::int32_t>(Narrow.size())) return nullptr;
    const char* word = Narrow[static_cast<std::size_t>(words.index++)];
    *length = static_cast<std::int32_t>(std::strlen(word));
    words.lastLength = *length;
    return word;
}

const char16_t* APS5_VABI UnicodeNext(UEnumeration* enumeration, std::int32_t* length, std::int32_t*) {
    auto& words = State(enumeration);
    if (words.index >= static_cast<std::int32_t>(Wide.size())) return nullptr;
    const char16_t* word = Wide[static_cast<std::size_t>(words.index++)];
    if (length) *length = static_cast<std::int32_t>(std::strlen(Narrow[static_cast<std::size_t>(words.index - 1)]));
    return word;
}

UEnumeration Enumeration(Words& words) {
    return UEnumeration{nullptr, &words, Close, Count, UnicodeNext, Next, nullptr};
}

void Dispatch() {
    Words words;
    UEnumeration enumeration = Enumeration(words);
    std::int32_t status = Success;
    Require(uenum_count_nid_postfix(&enumeration, &status) == 3 && status == -124, "count dispatches and keeps the callback status");
    status = Success;
    std::int32_t length = -1;
    const char* word = uenum_next_nid_postfix(&enumeration, &length, &status);
    Require(word == Narrow[0] && length == 5 && status == Success, "next returns the callback string and length");
    word = uenum_next_nid_postfix(&enumeration, nullptr, &status);
    Require(word == Narrow[1] && words.lastLength == 2, "next passes a length slot when the caller gives none");
    const char16_t* unicodeWord = uenum_unext_nid_postfix(&enumeration, &length, &status);
    Require(unicodeWord == Wide[2] && length == 0, "unext returns the callback string and length");
    Require(uenum_next_nid_postfix(&enumeration, &length, &status) == nullptr && status == Success, "end of enumeration");
    words.index = 0;
    unicodeWord = uenum_unext_nid_postfix(&enumeration, nullptr, &status);
    Require(unicodeWord == Wide[0], "unext forwards a null length");
    uenum_close_nid_postfix(&enumeration);
    Require(words.closes == 1 && words.closed == &enumeration, "close calls the close callback");
}

void Failures() {
    Words words;
    UEnumeration enumeration = Enumeration(words);
    std::int32_t status = IllegalArgument;
    std::int32_t length = 77;
    Require(uenum_count_nid_postfix(&enumeration, &status) == -1, "count with an error set");
    Require(uenum_next_nid_postfix(&enumeration, &length, &status) == nullptr && length == 77 && words.index == 0, "next with an error set");
    Require(uenum_unext_nid_postfix(&enumeration, &length, &status) == nullptr && words.index == 0, "unext with an error set");
    Require(status == IllegalArgument, "a set error is kept");
    status = Success;
    Require(uenum_count_nid_postfix(nullptr, &status) == -1 && status == Success, "null enumeration count");
    Require(uenum_next_nid_postfix(nullptr, &length, &status) == nullptr && status == Success, "null enumeration next");
    Require(uenum_unext_nid_postfix(nullptr, &length, &status) == nullptr && status == Success, "null enumeration unext");
    Require(uenum_count_nid_postfix(nullptr, nullptr) == -1 && uenum_next_nid_postfix(nullptr, nullptr, nullptr) == nullptr, "null enumeration ignores a null status");
    Require(Rejects([&] { uenum_count_nid_postfix(&enumeration, nullptr); }), "null status count");
    Require(Rejects([&] { uenum_next_nid_postfix(&enumeration, &length, nullptr); }), "null status next");
    Require(Rejects([&] { uenum_unext_nid_postfix(&enumeration, &length, nullptr); }), "null status unext");
    uenum_close_nid_postfix(nullptr);
}

void MissingCallbacks() {
    Words words;
    UEnumeration enumeration{nullptr, &words, Close, nullptr, nullptr, nullptr, nullptr};
    std::int32_t status = Success;
    Require(uenum_count_nid_postfix(&enumeration, &status) == -1 && status == Unsupported, "count without a callback");
    status = Success;
    Require(uenum_next_nid_postfix(&enumeration, nullptr, &status) == nullptr && status == Unsupported, "next without a callback");
    status = Success;
    std::int32_t length = 9;
    Require(uenum_unext_nid_postfix(&enumeration, &length, &status) == nullptr && status == Unsupported && length == 9, "unext without a callback");
    uenum_close_nid_postfix(&enumeration);
    Require(words.closes == 1, "close without other callbacks");
}

void GuestMemory() {
    freed.clear();
    Words words;
    UEnumeration enumeration = Enumeration(words);
    void* base = malloc_nid_postfix(32);
    enumeration.baseContext = base;
    uenum_close_nid_postfix(&enumeration);
    Require(words.closes == 1 && words.closed == &enumeration, "close calls the close callback");
    Require(freed == std::vector<void*>{base}, "close frees the base context with the guest allocator");
    freed.clear();
    void* leftover = malloc_nid_postfix(16);
    auto* unowned = static_cast<UEnumeration*>(malloc_nid_postfix(sizeof(UEnumeration)));
    *unowned = UEnumeration{leftover, &words, nullptr, Count, nullptr, nullptr, nullptr};
    uenum_close_nid_postfix(unowned);
    Require(words.closes == 1 && freed == std::vector<void*>{unowned}, "without a close callback only the enumeration is freed");
    free_nid_postfix(leftover);
}

}

int main() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch), reinterpret_cast<void*>(Calloc_nid_no_patch),
        reinterpret_cast<void*>(Reallocate_nid_no_patch), reinterpret_cast<void*>(Align_nid_no_patch), reinterpret_cast<void*>(Realign_nid_no_patch),
        reinterpret_cast<void*>(PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    Dispatch();
    Failures();
    MissingCallbacks();
    GuestMemory();
    return 0;
}
