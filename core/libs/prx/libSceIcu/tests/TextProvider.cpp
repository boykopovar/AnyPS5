#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU text provider check failed at line %u\n", location.line());
    std::abort();
}

UText Initialized() {
    UText text{};
    text.magic = 0x345ad82c;
    text.sizeOfStruct = sizeof(text);
    return text;
}

struct Memory {
    std::unordered_set<void*> allocations;
    int remaining = -1;
};

void* APS5_VABI Allocate(const void* context, std::size_t bytes) {
    auto& state = *const_cast<Memory*>(static_cast<const Memory*>(context));
    if (state.remaining == 0) return nullptr;
    if (state.remaining > 0) --state.remaining;
    void* result = std::malloc(bytes);
    if (result) Require(state.allocations.insert(result).second);
    return result;
}

void APS5_VABI Free(const void* context, void* pointer) {
    if (!pointer) return;
    auto& state = *const_cast<Memory*>(static_cast<const Memory*>(context));
    Require(state.allocations.erase(pointer) == 1);
    std::free(pointer);
}

void* APS5_VABI Reallocate(const void*, void*, std::size_t) {
    std::abort();
}

void APS5_VABI CountClose(UText* text) {
    ++*static_cast<int*>(const_cast<void*>(text->context));
}

void Lifetime(Memory& memory) {
    UText text = Initialized();
    std::int32_t status = 0;
    Require(utext_setup(&text, 24, &status) == &text && status == 0);
    Require(text.flags == 6 && text.extraSize == 24 && text.pExtra && memory.allocations.size() == 1);
    const auto* bytes = static_cast<const unsigned char*>(text.pExtra);
    Require(std::all_of(bytes, bytes + 24, [](auto value) { return value == 0; }));
    void* first = text.pExtra;
    UTextFuncs functions{};
    functions.tableSize = sizeof(functions);
    functions.close = CountClose;
    int closes = 0;
    text.pFuncs = &functions;
    text.context = &closes;
    text.a = 91;
    text.p = &closes;
    std::memset(text.pExtra, 0x5a, 24);
    Require(utext_setup(&text, 8, &status) == &text && closes == 1);
    Require(text.pExtra == first && text.extraSize == 24 && text.context == nullptr && text.p == nullptr && text.a == 0);
    Require(std::all_of(bytes, bytes + 24, [](auto value) { return value == 0; }));
    text.context = &closes;
    Require(utext_setup(&text, 80, &status) == &text && closes == 2);
    Require(text.extraSize == 80 && memory.allocations.size() == 1);
    text.context = &closes;
    Require(utext_close(&text) == &text && closes == 3);
    Require(text.flags == 0 && text.pFuncs == nullptr && text.pExtra == nullptr && text.extraSize == 0);
    Require(memory.allocations.empty());
    Require(utext_close(&text) == &text && closes == 3);
    Require(utext_openUChars(&text, nullptr, 0, &status) == &text);
    Require(text.pFuncs->nativeLength(&text) == 0 && text.pFuncs->access(&text, 0, 1) == 0);
    Require(utext_close(&text) == &text && memory.allocations.empty());
}

void AccessAndExtract(Memory& memory) {
    const char16_t source[] = {u'A', 0xd83d, 0xde42, u'B', 0};
    std::int32_t status = 0;
    UText* text = utext_openUChars(nullptr, source, 4, &status);
    Require(text && status == 0 && memory.allocations.size() == 1);
    Require(text->flags == 5 && text->providerProperties == 4 && text->context == source && text->chunkContents == source);
    Require(text->pFuncs->nativeLength(text) == 4);
    Require(text->pFuncs->access(text, 2, 1) && text->chunkOffset == 1);
    Require(text->pFuncs->access(text, -100, 1) && text->chunkOffset == 0);
    Require(text->pFuncs->access(text, std::numeric_limits<std::int64_t>::max(), 0) && text->chunkOffset == 4);
    Require(!text->pFuncs->access(text, 4, 1) && text->chunkOffset == 4);
    Require(!text->pFuncs->access(text, 0, 0) && text->chunkOffset == 0);
    for (std::int32_t capacity = 0; capacity <= 6; ++capacity) {
        std::array<char16_t, 8> output;
        output.fill(0x5a5a);
        status = 0;
        Require(text->pFuncs->extract(text, -100, 100, output.data(), capacity, &status) == 4);
        Require(status == (capacity < 4 ? 15 : capacity == 4 ? -124 : 0));
        Require(std::equal(output.begin(), output.begin() + std::min(capacity, 4), source));
        if (capacity > 4) Require(output[4] == 0);
        Require(output[capacity] == 0x5a5a && text->chunkOffset == 4);
    }
    std::array<char16_t, 8> output{};
    status = 0;
    Require(text->pFuncs->extract(text, 2, 4, output.data(), 8, &status) == 3);
    Require(output[0] == 0xd83d && output[1] == 0xde42 && output[2] == u'B' && output[3] == 0);
    Require(text->chunkOffset == 4);
    status = 0;
    Require(text->pFuncs->extract(text, 4, 2, output.data(), 8, &status) == 0 && status == 1);
    Require(utext_close(text) == nullptr && memory.allocations.empty());
}

void LazyAndClone(Memory& memory) {
    std::vector<char16_t> source(71, u'a');
    source[31] = 0xd83d;
    source[32] = 0xde42;
    source.back() = 0;
    std::int32_t status = 0;
    UText* text = utext_openUChars(nullptr, source.data(), -1, &status);
    Require(text && text->a == -1 && text->chunkLength == 0 && text->providerProperties == 6);
    Require(text->pFuncs->access(text, 0, 1) && text->chunkNativeLimit == 31 && text->a == -1);
    Require(text->pFuncs->access(text, 32, 1) && text->chunkOffset == 31 && text->a == -1);
    Require(text->pFuncs->nativeLength(text) == 70 && text->chunkOffset == 31 && text->providerProperties == 4);
    UText* shallow = text->pFuncs->clone(nullptr, text, 0, &status);
    Require(shallow && status == 0 && shallow->context == text->context && shallow->chunkOffset == 31);
    Require(shallow->pFuncs->access(shallow, 1, 1) && shallow->chunkOffset == 1 && text->chunkOffset == 31);
    UText* deep = text->pFuncs->clone(nullptr, text, 1, &status);
    Require(deep && status == 0 && deep->context != text->context && deep->chunkContents == deep->context);
    Require((deep->providerProperties & 32) != 0 && deep->chunkOffset == 31);
    Require(utext_close(text) == nullptr);
    Require(utext_close(shallow) == nullptr);
    std::vector<char16_t>().swap(source);
    std::array<char16_t, 4> output{};
    Require(deep->pFuncs->extract(deep, 31, 33, output.data(), 4, &status) == 2);
    Require(status == 0 && output[0] == 0xd83d && output[1] == 0xde42 && output[2] == 0);
    Require(utext_close(deep) == nullptr && memory.allocations.empty());
}

void Errors(Memory& memory) {
    std::int32_t status = 0;
    std::uint32_t invalid = 0;
    Require(utext_setup(reinterpret_cast<UText*>(&invalid), 0, &status) == reinterpret_cast<UText*>(&invalid));
    Require(status == 1 && invalid == 0);
    for (const auto length : {-2ll, static_cast<long long>(std::numeric_limits<std::int32_t>::max()) + 1}) {
        status = 0;
        Require(!utext_openUChars(nullptr, u"A", length, &status) && status == 1);
    }
    status = 0;
    Require(!utext_openUChars(nullptr, nullptr, -1, &status) && status == 1);
    status = 15;
    auto* absent = reinterpret_cast<UText*>(std::uintptr_t{1});
    Require(utext_setup(absent, 128, &status) == absent);
    Require(!utext_openUChars(absent, nullptr, -1, &status) && status == 15);

    memory.remaining = 0;
    status = 0;
    Require(!utext_setup(nullptr, 32, &status) && status == 7 && memory.allocations.empty());
    memory.remaining = -1;
    status = 0;
    UText* text = utext_openUChars(nullptr, u"sample", -1, &status);
    Require(text && status == 0);
    memory.remaining = 1;
    UText* clone = text->pFuncs->clone(nullptr, text, 1, &status);
    Require(clone && status == 7 && (clone->providerProperties & 32) == 0);
    Require(utext_close(clone) == nullptr && memory.allocations.size() == 1);
    memory.remaining = 0;
    status = 0;
    Require(!text->pFuncs->clone(nullptr, text, 0, &status) && status == 7);
    memory.remaining = -1;
    status = 0;
    bool rejected = false;
    try { text->pFuncs->clone(text, text, 0, &status); }
    catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected && text->pFuncs->nativeLength(text) == 6);
    Require(utext_close(text) == nullptr && memory.allocations.empty());
}

}

int main() {
    Memory memory;
    std::int32_t status = 0;
    u_setMemoryFunctions_nid_postfix(&memory, Allocate, Reallocate, Free, &status);
    Require(status == 0);
    Lifetime(memory);
    AccessAndExtract(memory);
    LazyAndClone(memory);
    Errors(memory);
    Require(memory.allocations.empty());
    u_cleanup_nid_postfix();
    std::puts("ICU UTF-16 text provider tests passed");
}
