#include "prx/libSceIcu/libSceIcu.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <vector>

namespace {

using Text = Icu::Text;
using String = Icu::Abi::UnicodeString;

constexpr std::int32_t Success = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t MemoryAllocationError = 7;
constexpr std::int32_t IndexOutOfBounds = 8;
constexpr std::int32_t BufferOverflow = 15;
constexpr std::int32_t NoWritePermission = 30;
constexpr std::int32_t NotTerminated = -124;
constexpr std::uint32_t Magic = 0x345ad82c;

struct Heap {
    std::vector<void*> live;
    std::int32_t allocations = 0;
    std::int32_t releases = 0;
    std::size_t lastSize = 0;
    bool exhausted = false;
};

Heap heap;

void* APS5_VABI Allocate_nid_no_patch(const void*, std::size_t size) {
    heap.lastSize = size;
    if (heap.exhausted) return nullptr;
    void* block = std::malloc(size);
    heap.live.push_back(block);
    ++heap.allocations;
    return block;
}

void* APS5_VABI Reallocate_nid_no_patch(const void*, void*, std::size_t) {
    std::abort();
}

void APS5_VABI Release_nid_no_patch(const void*, void* block) {
    ++heap.releases;
    const auto found = std::find(heap.live.begin(), heap.live.end(), block);
    if (found == heap.live.end()) std::abort();
    heap.live.erase(found);
    std::free(block);
}

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU text providers: %s\n", message);
    std::abort();
}

template<class F>
bool Throws(F call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

bool Live(const void* block) {
    return std::find(heap.live.begin(), heap.live.end(), block) != heap.live.end();
}

Text Initializer() {
    Text text{};
    text.magic = Magic;
    text.sizeOfStruct = sizeof(Text);
    return text;
}

void Close(Text* text) {
    if (text->functions && text->functions->close) text->functions->close(text);
    if (text->flags & 2) IcuReleaseMemory_nid_no_patch(text->extra);
    if (text->flags & 1) IcuReleaseMemory_nid_no_patch(text);
}

std::vector<std::int32_t> Points(Text* text) {
    std::vector<std::int32_t> points;
    utext_setNativeIndex_nid_postfix(text, 0);
    for (auto point = utext_next32_nid_postfix(text); point != -1; point = utext_next32_nid_postfix(text))
        points.push_back(point);
    return points;
}

int closes = 0;

void APS5_VABI CountClose_nid_no_patch(Text*) {
    ++closes;
}

void Setup() {
    std::int32_t status = Success;
    Text* text = utext_setup_nid_postfix(nullptr, 0, &status);
    Require(status == Success && Live(text) && heap.lastSize == 144, "setup allocates a bare UText");
    Require(text->magic == Magic && text->flags == 5 && text->sizeOfStruct == 144, "setup header");
    Require(text->extraSize == 0 && text->extra == reinterpret_cast<char*>(text) + 144, "setup extra pointer without extra space");
    Require(!text->functions && !text->context && text->providerProperties == 0, "setup leaves no provider");
    Close(text);

    status = Success;
    text = utext_setup_nid_postfix(nullptr, 24, &status);
    Require(status == Success && heap.lastSize == 168 && text->extraSize == 24, "setup appends extra space");
    Require(text->extra == reinterpret_cast<char*>(text) + 144, "extra space follows the UText");
    const auto* bytes = static_cast<const unsigned char*>(text->extra);
    Require(std::all_of(bytes, bytes + 24, [](unsigned char byte) { return byte == 0; }), "extra space is zeroed");
    Close(text);

    heap.exhausted = true;
    status = Success;
    Require(!utext_setup_nid_postfix(nullptr, 0, &status) && status == MemoryAllocationError, "setup allocation failure");
    heap.exhausted = false;

    Icu::TextFunctions functions{};
    functions.tableSize = sizeof(Icu::TextFunctions);
    functions.close = CountClose_nid_no_patch;
    Text stack = Initializer();
    stack.flags = 4;
    stack.functions = &functions;
    stack.context = &stack;
    stack.chunkOffset = 3;
    stack.chunkLength = 9;
    stack.a = 7;
    stack.privatePointer = &stack;
    status = Success;
    Require(utext_setup_nid_postfix(&stack, 16, &status) == &stack && status == Success, "setup reuses a caller UText");
    Require(closes == 1 && stack.flags == 6 && stack.extraSize == 16 && Live(stack.extra), "setup closes the old provider and allocates extra");
    Require(!stack.context && stack.chunkOffset == 0 && stack.chunkLength == 0 && stack.a == 0 && !stack.privatePointer, "setup clears the fields");
    Require(stack.functions == &functions, "setup keeps the function table");
    void* extra = stack.extra;
    std::memset(extra, 0x5a, 16);
    status = Success;
    utext_setup_nid_postfix(&stack, 8, &status);
    Require(closes == 2 && stack.extra == extra && stack.extraSize == 16 && static_cast<unsigned char*>(extra)[15] == 0, "setup keeps larger extra and zeroes it");
    const auto releases = heap.releases;
    const auto allocations = heap.allocations;
    status = Success;
    utext_setup_nid_postfix(&stack, 32, &status);
    Require(status == Success && closes == 3 && heap.releases == releases + 1 && heap.allocations == allocations + 1 && heap.lastSize == 32, "setup frees a separate extra block before growing it");
    Require(stack.flags == 6 && stack.extraSize == 32 && Live(stack.extra) && static_cast<unsigned char*>(stack.extra)[31] == 0, "setup grows separate extra space");
    extra = stack.extra;
    stack.functions = nullptr;
    Close(&stack);
    Require(!Live(extra), "separate extra space released");

    status = Success;
    text = utext_setup_nid_postfix(nullptr, 8, &status);
    void* inlineExtra = text->extra;
    text->functions = &functions;
    const auto inlineReleases = heap.releases;
    utext_setup_nid_postfix(text, 16, &status);
    Require(status == Success && closes == 4 && heap.releases == inlineReleases && text->extra != inlineExtra && Live(text->extra), "setup does not free inline extra space when growing");
    Require(text->flags == 7 && text->extraSize == 16, "setup marks grown extra space as separate");
    Close(text);

    Text closed = Initializer();
    status = Success;
    utext_setup_nid_postfix(&closed, 0, &status);
    Require(status == Success && closed.flags == 4, "setup opens a closed UText without a table");

    Text bad{};
    status = Success;
    Require(utext_setup_nid_postfix(&bad, 0, &status) == &bad && status == IllegalArgument, "setup rejects a bad magic");
    status = BufferOverflow;
    Require(utext_setup_nid_postfix(&bad, 0, &status) == &bad && status == BufferOverflow, "setup keeps a failure");
    Require(Throws([] { utext_setup_nid_postfix(nullptr, 0, nullptr); }), "setup null status");
}

void UChars() {
    const char16_t units[] = {u'a', 0xd83d, 0xde00, u'b', 0xdc00, 0};
    std::int32_t status = Success;
    Text* text = utext_openUChars_nid_postfix(nullptr, units, 5, &status);
    Require(status == Success && Live(text), "open counted UChars");
    Require(text->providerProperties == 4 && text->a == 5 && text->chunkContents == units, "counted UChars fields");
    Require(text->chunkNativeLimit == 5 && text->chunkLength == 5 && text->nativeIndexingLimit == 5, "counted UChars chunk");
    Require(!utext_isLengthExpensive_nid_postfix(text) && !utext_isWritable_nid_postfix(text), "counted UChars properties");
    Require(utext_nativeLength_nid_postfix(text) == 5, "counted UChars length");
    Require((Points(text) == std::vector<std::int32_t>{u'a', 0x1f600, u'b', 0xdc00}), "counted UChars iteration");
    Require(utext_char32At_nid_postfix(text, 2) == 0x1f600 && utext_getNativeIndex_nid_postfix(text) == 1, "index snaps to a code point start");

    char16_t buffer[8]{};
    status = Success;
    Require(utext_extract_nid_postfix(text, 2, 3, buffer, 8, &status) == 2 && status == Success, "extract snaps the start and completes the pair");
    Require(buffer[0] == 0xd83d && buffer[1] == 0xde00 && buffer[2] == 0, "extract units");
    Require(utext_getNativeIndex_nid_postfix(text) == 3, "extract leaves the index after the text");
    status = Success;
    Require(utext_extract_nid_postfix(text, 0, 5, buffer, 2, &status) == 5 && status == BufferOverflow, "extract reports overflow");
    status = Success;
    Require(utext_extract_nid_postfix(text, 0, 2, buffer, 3, &status) == 3 && status == NotTerminated, "extract without room for the terminator");
    status = Success;
    Require(utext_extract_nid_postfix(text, 3, 1, buffer, 8, &status) == 0 && status == IllegalArgument, "extract reversed range");
    status = Success;
    Require(utext_extract_nid_postfix(text, 0, 1, nullptr, 1, &status) == 0 && status == IllegalArgument, "extract null buffer");
    status = Success;
    Require(utext_extract_nid_postfix(text, 0, 5, nullptr, 0, &status) == 5 && status == BufferOverflow, "preflight extract counts the units");
    Require(text->chunkOffset == 5, "preflight extract moves the index to the limit");
    status = Success;
    Require(utext_extract_nid_postfix(text, 1, 1, nullptr, 0, &status) == 0 && status == NotTerminated, "empty preflight extract");
    Require(!text->functions->replace && !text->functions->copy && !text->functions->mapOffsetToNative, "UChars is read only");

    status = Success;
    Text* clone = utext_clone_nid_postfix(nullptr, text, 1, 0, &status);
    Require(status == Success && clone != text && Live(clone) && Live(clone->context), "deep clone copies the string");
    Require((clone->providerProperties & 32) && !(text->providerProperties & 32), "deep clone owns its copy");
    const auto* copy = static_cast<const char16_t*>(clone->context);
    Require(std::equal(units, units + 5, copy) && copy[5] == 0, "deep clone copy is terminated");
    const void* owned = clone->context;
    Close(clone);
    Require(!Live(owned), "close releases the copy");

    status = Success;
    clone = utext_clone_nid_postfix(nullptr, text, 0, 1, &status);
    Require(status == Success && clone->context == units && !(clone->providerProperties & 32), "shallow clone shares the string");
    Close(clone);

    Text reused = Initializer();
    status = Success;
    Require(utext_openUChars_nid_postfix(&reused, units, -1, &status) == &reused && status == Success, "open terminated UChars in place");
    Require(reused.a == -1 && reused.chunkNativeLimit == 0 && utext_isLengthExpensive_nid_postfix(&reused), "terminated UChars starts unscanned");
    Require(utext_nativeLength_nid_postfix(&reused) == 5 && reused.chunkLength == 5, "length scan");
    Require(!utext_isLengthExpensive_nid_postfix(&reused), "length scan clears the expensive flag");
    Close(&reused);

    std::vector<char16_t> longUnits(100, u'x');
    longUnits[39] = 0xd800;
    longUnits[40] = 0xdc00;
    longUnits.push_back(0);
    status = Success;
    utext_openUChars_nid_postfix(&reused, longUnits.data(), -1, &status);
    Require(utext_char32At_nid_postfix(&reused, 8) == u'x', "lazy scan reads ahead");
    Require(reused.a == -1 && reused.chunkNativeLimit == 39, "lazy scan stops before a lead surrogate");
    Require(utext_char32At_nid_postfix(&reused, 40) == 0x10000, "lazy scan joins a pair across chunks");
    Require(utext_char32At_nid_postfix(&reused, 200) == -1 && reused.a == 100 && reused.chunkLength == 100, "lazy scan finds the end");
    Close(&reused);

    status = Success;
    utext_openUChars_nid_postfix(&reused, longUnits.data(), -1, &status);
    Require(utext_extract_nid_postfix(&reused, 98, 1000, buffer, 8, &status) == 2 && reused.a == 100, "extract finds the terminator");
    Close(&reused);

    status = Success;
    utext_openUChars_nid_postfix(&reused, units, -1, &status);
    clone = utext_clone_nid_postfix(nullptr, &reused, 1, 0, &status);
    Require(status == Success && reused.a == -1 && utext_isLengthExpensive_nid_postfix(&reused), "deep clone leaves the source unscanned");
    Require(clone->a == 5 && !utext_isLengthExpensive_nid_postfix(clone) && clone->context != units && Live(clone->context), "deep clone measures an unscanned text");
    const auto* measured = static_cast<const char16_t*>(clone->context);
    Require(std::equal(units, units + 5, measured) && measured[5] == 0, "deep clone copies up to the terminator");
    Require((Points(clone) == std::vector<std::int32_t>{u'a', 0x1f600, u'b', 0xdc00}), "deep clone of an unscanned text iterates");
    Close(clone);
    status = Success;
    Require(utext_extract_nid_postfix(&reused, 0, 100, nullptr, 0, &status) == 5 && status == BufferOverflow, "preflight extract of an unscanned text");
    Require(reused.a == 5 && reused.chunkNativeLimit == 5, "preflight extract finds the terminator");
    Close(&reused);

    char16_t inner[8];
    Text holderText = Initializer();
    holderText.extra = inner;
    holderText.extraSize = sizeof(inner);
    Text* holder = &holderText;
    status = Success;
    utext_openUChars_nid_postfix(holder, inner, 2, &status);
    inner[0] = u'h';
    inner[1] = u'i';
    holder->p = inner + 3;
    holder->q = &holder->b;
    holder->r = units;
    status = Success;
    clone = utext_clone_nid_postfix(nullptr, holder, 0, 0, &status);
    Require(status == Success && clone->extraSize == 16 && clone->extra == reinterpret_cast<char*>(clone) + 144, "clone allocates the source's extra space");
    Require(clone->context == clone->extra && clone->chunkContents == clone->extra, "clone moves pointers into the extra space");
    Require(clone->p == static_cast<char16_t*>(clone->extra) + 3 && clone->q == &clone->b, "clone moves pointers into the extra space and the UText");
    Require(clone->r == units && holder->context == inner, "clone keeps outside pointers and the source");
    inner[0] = u'X';
    Require((Points(clone) == std::vector<std::int32_t>{u'h', u'i'}), "clone reads its own extra space");
    Close(clone);
    Close(holder);

    status = Success;
    Text* empty = utext_openUChars_nid_postfix(nullptr, nullptr, 0, &status);
    Require(status == Success && utext_nativeLength_nid_postfix(empty) == 0 && utext_next32_nid_postfix(empty) == -1, "null string of length 0 is empty");
    Close(empty);
    status = Success;
    Require(!utext_openUChars_nid_postfix(&reused, nullptr, 1, &status) && status == IllegalArgument, "null string");
    status = Success;
    Require(!utext_openUChars_nid_postfix(&reused, units, -2, &status) && status == IllegalArgument, "negative length");
    status = Success;
    Require(!utext_openUChars_nid_postfix(&reused, units, 0x80000000LL, &status) && status == IllegalArgument, "length over INT32_MAX");
    status = IndexOutOfBounds;
    Require(!utext_openUChars_nid_postfix(&reused, units, 1, &status) && status == IndexOutOfBounds, "open keeps a failure");
    Close(text);
}

void UnicodeStrings() {
    const char16_t units[] = {u'a', 0xd83d, 0xde00, u'b', u'c'};
    String string{};
    _ZN6icu_6813UnicodeStringC1EPKDsi_nid_postfix(&string, units, 5);
    std::int32_t status = Success;
    Text* text = utext_openConstUnicodeString_nid_postfix(nullptr, &string, &status);
    Require(status == Success && text->context == &string && text->providerProperties == 4, "open const UnicodeString");
    Require(text->chunkContents == string.Buffer() && text->chunkLength == 5 && text->nativeIndexingLimit == 5, "UnicodeString chunk");
    Require(utext_nativeLength_nid_postfix(text) == 5 && !utext_isWritable_nid_postfix(text), "UnicodeString length and properties");
    Require((Points(text) == std::vector<std::int32_t>{u'a', 0x1f600, u'b', u'c'}), "UnicodeString iteration");

    char16_t buffer[8]{};
    status = Success;
    Require(utext_extract_nid_postfix(text, 2, 4, buffer, 8, &status) == 3 && status == Success, "extract snaps both ends");
    Require(buffer[0] == 0xd83d && buffer[2] == u'b' && buffer[3] == 0 && text->chunkOffset == 4, "extract units and index");
    status = Success;
    Require(utext_extract_nid_postfix(text, 0, 9, buffer, 2, &status) == 5 && status == BufferOverflow && buffer[1] == 0xd83d, "extract trims to the capacity");
    status = Success;
    Require(utext_extract_nid_postfix(text, -1, 2, buffer, 8, &status) == 0 && status == IndexOutOfBounds, "extract negative start");
    status = Success;
    Require(utext_extract_nid_postfix(text, 0, 9, nullptr, 0, &status) == 5 && status == BufferOverflow && text->chunkOffset == 0, "preflight extract counts the units");
    status = Success;
    Require(utext_extract_nid_postfix(text, 2, 2, nullptr, 0, &status) == 0 && status == NotTerminated && text->chunkOffset == 1, "empty preflight extract snaps the index");
    status = Success;
    Require(utext_replace_nid_postfix(text, 0, 1, u"z", 1, &status) == 0 && status == NoWritePermission, "const UnicodeString is not writable");

    status = Success;
    Text* shallow = utext_clone_nid_postfix(nullptr, text, 0, 0, &status);
    Require(status == Success && shallow->context == &string && !(shallow->providerProperties & 32) && !utext_isWritable_nid_postfix(shallow), "shallow clone shares the string");
    Require(shallow->chunkContents == string.Buffer() && (Points(shallow) == std::vector<std::int32_t>{u'a', 0x1f600, u'b', u'c'}), "shallow clone reads the source");
    Close(shallow);
    Require(string.Length() == 5 && string.Buffer()[0] == u'a', "closing a shallow clone keeps the string");

    status = Success;
    Text* clone = utext_clone_nid_postfix(nullptr, text, 1, 0, &status);
    Require(status == Success && clone->context != &string && (clone->providerProperties & 40) == 40, "deep clone owns a writable copy");
    status = Success;
    Require(utext_replace_nid_postfix(clone, 2, 3, u"XYZ", 3, &status) == 1 && status == Success, "replace in the copy");
    const auto& copy = *static_cast<const String*>(clone->context);
    Require(copy.Length() == 6 && copy.Buffer()[1] == u'X' && copy.Buffer()[4] == u'b', "replace snaps to the code point start");
    Require(clone->chunkContents == copy.Buffer() && clone->chunkLength == 6 && clone->chunkOffset == 4, "replace updates the chunk");
    Require(string.Length() == 5 && string.Buffer()[1] == 0xd83d, "replace leaves the source");
    status = Success;
    utext_copy_nid_postfix(clone, 0, 2, 6, 0, &status);
    Require(status == Success && copy.Length() == 8 && copy.Buffer()[6] == u'a' && copy.Buffer()[7] == u'X', "copy appends");
    Require(clone->chunkLength == 8 && clone->chunkOffset == 8, "copy updates the chunk");
    status = Success;
    utext_copy_nid_postfix(clone, 0, 1, 8, 1, &status);
    Require(status == Success && copy.Length() == 8 && copy.Buffer()[0] == u'X' && copy.Buffer()[7] == u'a', "move");
    Require(clone->chunkOffset == 8, "move puts the index at the destination");
    status = Success;
    utext_copy_nid_postfix(clone, 1, 4, 2, 0, &status);
    Require(status == IndexOutOfBounds, "copy into its own range");
    Close(clone);

    String bogus{};
    _ZN6icu_6813UnicodeStringC1EPKDsi_nid_postfix(&bogus, units, 1);
    _ZN6icu_6813UnicodeString10setToBogusEv_nid_postfix(&bogus);
    status = Success;
    Require(utext_openConstUnicodeString_nid_postfix(text, &bogus, &status) == text && status == IllegalArgument, "bogus string");
    Require(text->context != &bogus && utext_nativeLength_nid_postfix(text) == 0, "bogus string leaves an empty text");
    status = IndexOutOfBounds;
    Require(utext_openConstUnicodeString_nid_postfix(text, nullptr, &status) == text && status == IndexOutOfBounds, "open keeps a failure");
    status = Success;
    Require(Throws([&] { utext_openConstUnicodeString_nid_postfix(text, nullptr, &status); }), "null UnicodeString");
    Close(text);
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&bogus);
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&string);
}

}

int main() {
    std::int32_t status = Success;
    u_setMemoryFunctions_nid_postfix(nullptr, Allocate_nid_no_patch, Reallocate_nid_no_patch, Release_nid_no_patch, &status);
    Require(status == Success, "memory functions");
    Setup();
    UChars();
    UnicodeStrings();
    Require(heap.live.empty(), "every block released");
    std::puts("ICU text provider tests passed");
    return 0;
}
