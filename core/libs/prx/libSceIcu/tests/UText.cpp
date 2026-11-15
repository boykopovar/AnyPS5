#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

namespace UTextChecks {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;

static_assert(std::is_same_v<decltype(&utext_close), UText* (APS5_VABI*)(UText*)>);
static_assert(std::is_same_v<decltype(&utext_setup), UText* (APS5_VABI*)(UText*, std::int32_t, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&utext_openUChars),
                             UText* (APS5_VABI*)(UText*, const char16_t*, std::int64_t, std::int32_t*)>);

constexpr std::uint32_t Magic = 0x345ad82c;
constexpr std::int32_t HeapAllocated = 1;
constexpr std::int32_t ExtraHeapAllocated = 2;
constexpr std::int32_t Open = 4;
constexpr std::int32_t LengthIsExpensive = 2;
constexpr std::int32_t StableChunks = 4;
constexpr std::int32_t OwnsText = 32;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t MemoryAllocation = 7;
constexpr std::int32_t BufferOverflow = 15;
constexpr std::int32_t NotTerminated = -124;
constexpr char16_t Sample[] = {u'a', u'b', 0xd83d, 0xde00, u'c', u'd', 0};

using Sizes = std::vector<std::size_t>;
using Pointers = std::vector<void*>;

int memoryContext = 0;
Pointers live;
Sizes requested;
Pointers released;
bool failAllocations = false;
int closes = 0;
UText* lastClosed = nullptr;

void* APS5_VABI Allocate_nid_no_patch(const void* context, std::size_t size) {
    Require(context == &memoryContext);
    requested.push_back(size);
    if (failAllocations || size > (std::size_t{1} << 20)) return nullptr;
    void* pointer = std::malloc(size);
    Require(pointer != nullptr);
    std::memset(pointer, 0xa5, size);
    live.push_back(pointer);
    return pointer;
}

void* APS5_VABI Reallocate_nid_no_patch(const void*, void*, std::size_t) {
    Require(false, "UText reallocated a block");
    return nullptr;
}

void APS5_VABI Release_nid_no_patch(const void* context, void* pointer) {
    Require(context == &memoryContext);
    released.push_back(pointer);
    if (!pointer) return;
    const auto found = std::find(live.begin(), live.end(), pointer);
    Require(found != live.end(), "UText released a block that is not live");
    live.erase(found);
    std::free(pointer);
}

void APS5_VABI CountClose_nid_no_patch(UText* ut) {
    ++closes;
    lastClosed = ut;
}

constexpr UTextFuncs CountingFunctions{sizeof(UTextFuncs), 0, 0, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                                       nullptr, nullptr, CountClose_nid_no_patch, nullptr, nullptr, nullptr};
constexpr UTextFuncs BareFunctions{sizeof(UTextFuncs), 0, 0, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                                   nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

UText Initial() {
    UText text{};
    text.magic = Magic;
    text.sizeOfStruct = sizeof(UText);
    return text;
}

void Reset() {
    requested.clear();
    released.clear();
    failAllocations = false;
    closes = 0;
    lastClosed = nullptr;
}

bool Zeroed(const void* memory, std::size_t size) {
    const auto* bytes = static_cast<const std::uint8_t*>(memory);
    return std::all_of(bytes, bytes + size, [](std::uint8_t byte) { return byte == 0; });
}

bool Blank(const UText& text) {
    return text.context == nullptr && text.chunkContents == nullptr && text.p == nullptr && text.q == nullptr &&
        text.r == nullptr && text.privP == nullptr && text.a == 0 && text.b == 0 && text.c == 0 && text.chunkOffset == 0 &&
        text.chunkLength == 0 && text.chunkNativeStart == 0 && text.chunkNativeLimit == 0 &&
        text.nativeIndexingLimit == 0 && text.providerProperties == 0 && text.privA == 0 && text.privB == 0 &&
        text.privC == 0;
}

void SetupHeap() {
    Reset();
    std::int32_t status = 0;
    UText* text = utext_setup(nullptr, 0, &status);
    Require(text != nullptr && status == 0 && requested == Sizes{144});
    Require(text->magic == Magic && text->flags == (HeapAllocated | Open) && text->sizeOfStruct == 144);
    Require(text->extraSize == 0 && text->pExtra == text + 1 && text->pFuncs == nullptr && Blank(*text));
    text->pFuncs = &CountingFunctions;
    Require(utext_close(text) == nullptr && closes == 1 && lastClosed == text);
    Require(released == Pointers{text} && live.empty());

    Reset();
    status = NotTerminated;
    text = utext_setup(nullptr, 40, &status);
    Require(text != nullptr && status == NotTerminated && requested == Sizes{184});
    Require(text->flags == (HeapAllocated | Open) && text->extraSize == 40 && text->pExtra == text + 1);
    Require(Zeroed(text->pExtra, 40) && Blank(*text));
    text->pFuncs = &BareFunctions;
    Require(utext_close(text) == nullptr && released == Pointers{text} && live.empty());

    Reset();
    status = 0;
    text = utext_setup(nullptr, -8, &status);
    Require(text != nullptr && status == 0 && requested == Sizes{144});
    Require(text->extraSize == -8 && text->pExtra == text + 1 && text->flags == (HeapAllocated | Open));
    text->pFuncs = &BareFunctions;
    Require(utext_close(text) == nullptr && live.empty());
}

void SetupCallerStorage() {
    Reset();
    std::int32_t status = 0;
    UText text = Initial();
    Require(utext_setup(&text, 0, &status) == &text && status == 0 && requested.empty());
    Require(text.magic == Magic && text.flags == Open && text.pExtra == nullptr && text.extraSize == 0 && Blank(text));

    text.pFuncs = &CountingFunctions;
    text.context = &text;
    text.chunkContents = Sample;
    text.p = &text;
    text.q = &text;
    text.r = &text;
    text.privP = &text;
    text.a = 9;
    text.b = 1;
    text.c = 2;
    text.chunkOffset = 3;
    text.chunkLength = 4;
    text.chunkNativeStart = 5;
    text.chunkNativeLimit = 6;
    text.nativeIndexingLimit = 7;
    text.providerProperties = 8;
    text.privA = 10;
    text.privB = 11;
    text.privC = 12;
    Require(utext_setup(&text, 24, &status) == &text && status == 0 && closes == 1 && lastClosed == &text);
    Require(requested == Sizes{24} && released.empty());
    Require(text.flags == (ExtraHeapAllocated | Open) && text.extraSize == 24 && text.pExtra == live.back());
    Require(Zeroed(text.pExtra, 24) && Blank(text) && text.pFuncs == &CountingFunctions);

    void* const first = text.pExtra;
    std::memset(first, 0x5a, 24);
    Require(utext_setup(&text, 16, &status) == &text && status == 0 && closes == 2);
    Require(requested == Sizes{24} && released.empty() && text.pExtra == first && text.extraSize == 24);
    Require(Zeroed(first, 24) && text.flags == (ExtraHeapAllocated | Open));

    Require(utext_setup(&text, 64, &status) == &text && status == 0 && closes == 3);
    Require(requested == Sizes{24, 64} && released == Pointers{first});
    Require(text.pExtra == live.back() && text.pExtra != nullptr && text.extraSize == 64 && Zeroed(text.pExtra, 64));

    void* const second = text.pExtra;
    Require(utext_close(&text) == &text && closes == 4 && released == Pointers{first, second} && live.empty());
    Require(text.magic == Magic && text.flags == 0 && text.pExtra == nullptr && text.extraSize == 0 && text.pFuncs == nullptr);
    const UText closed = text;
    Require(utext_close(&text) == &text && closes == 4 && std::memcmp(&text, &closed, sizeof(UText)) == 0);
}

void SetupFailures() {
    Reset();
    std::int32_t status = 0;
    UText blank{};
    Require(utext_setup(&blank, 8, &status) == &blank && status == IllegalArgument && Zeroed(&blank, sizeof(UText)));

    status = BufferOverflow;
    UText text = Initial();
    const UText before = text;
    Require(utext_setup(&text, 8, &status) == &text && status == BufferOverflow);
    Require(std::memcmp(&text, &before, sizeof(UText)) == 0);
    Require(utext_setup(nullptr, 8, &status) == nullptr && status == BufferOverflow && requested.empty());

    status = 0;
    failAllocations = true;
    Require(utext_setup(nullptr, 0, &status) == nullptr && status == MemoryAllocation && requested == Sizes{144});

    status = 0;
    Require(utext_setup(&text, 8, &status) == &text && status == MemoryAllocation);
    Require(text.flags == 0 && text.pExtra == nullptr && text.extraSize == 0);

    Reset();
    status = 0;
    Require(utext_setup(&text, 8, &status) == &text && status == 0 && text.flags == (ExtraHeapAllocated | Open));
    text.pFuncs = &BareFunctions;
    void* const extra = text.pExtra;
    failAllocations = true;
    Require(utext_setup(&text, 32, &status) == &text && status == MemoryAllocation);
    Require(requested == Sizes{8, 32} && released == Pointers{extra} && live.empty());
    Require(text.flags == ExtraHeapAllocated && text.pExtra == nullptr && text.extraSize == 0);
    Require(utext_close(&text) == &text && text.flags == ExtraHeapAllocated && released == Pointers{extra});

    Reset();
    status = 0;
    Require(utext_setup(nullptr, INT32_MAX, &status) == nullptr && status == MemoryAllocation);
    Require(requested == Sizes{0xffffffff8000008f} && live.empty());
}

void CloseIgnored() {
    Reset();
    Require(utext_close(nullptr) == nullptr);
    alignas(8) std::uint8_t dummy[16]{};
    Require(utext_close(reinterpret_cast<UText*>(dummy)) == reinterpret_cast<UText*>(dummy) && Zeroed(dummy, sizeof(dummy)));
    UText text = Initial();
    text.pFuncs = &CountingFunctions;
    const UText before = text;
    Require(utext_close(&text) == &text && closes == 0 && std::memcmp(&text, &before, sizeof(UText)) == 0);
}

void ExplicitLength() {
    Reset();
    std::int32_t status = 0;
    UText* text = utext_openUChars(nullptr, Sample, 6, &status);
    Require(text != nullptr && status == 0 && requested == Sizes{144});
    Require(text->magic == Magic && text->flags == (HeapAllocated | Open) && text->providerProperties == StableChunks);
    Require(text->a == 6 && text->context == Sample && text->chunkContents == Sample && text->chunkNativeStart == 0);
    Require(text->chunkNativeLimit == 6 && text->chunkLength == 6 && text->nativeIndexingLimit == 6 && text->chunkOffset == 0);
    const UTextFuncs* functions = text->pFuncs;
    Require(functions != nullptr && functions->tableSize == 112 && functions->reserved1 == 0 && functions->reserved2 == 0 &&
            functions->reserved3 == 0);
    Require(functions->clone && functions->nativeLength && functions->access && functions->extract && functions->close);
    Require(!functions->replace && !functions->copy && !functions->mapOffsetToNative && !functions->mapNativeIndexToUTF16 &&
            !functions->spare1 && !functions->spare2 && !functions->spare3);

    Require(functions->nativeLength(text) == 6);
    Require(functions->access(text, 3, 1) == 1 && text->chunkOffset == 2);
    Require(functions->access(text, 2, 1) == 1 && text->chunkOffset == 2);
    Require(functions->access(text, 4, 0) == 1 && text->chunkOffset == 4);
    Require(functions->access(text, 0, 0) == 0 && text->chunkOffset == 0);
    Require(functions->access(text, -7, 1) == 1 && text->chunkOffset == 0);
    Require(functions->access(text, 6, 1) == 0 && text->chunkOffset == 6);
    Require(functions->access(text, 6, 0) == 1 && text->chunkOffset == 6);
    Require(functions->access(text, INT64_MAX, 1) == 0 && text->chunkOffset == 6);
    Require(text->chunkNativeLimit == 6 && text->chunkLength == 6 && text->a == 6 && text->chunkContents == Sample);

    std::array<char16_t, 16> buffer;
    buffer.fill(0xffff);
    Require(functions->extract(text, 0, 6, buffer.data(), 16, &status) == 6 && status == 0 && text->chunkOffset == 6);
    Require(std::memcmp(buffer.data(), Sample, sizeof(Sample)) == 0 && buffer[7] == 0xffff);

    buffer.fill(0xffff);
    Require(functions->extract(text, 1, 3, buffer.data(), 16, &status) == 3 && status == 0 && text->chunkOffset == 4);
    Require(buffer[0] == u'b' && buffer[1] == 0xd83d && buffer[2] == 0xde00 && buffer[3] == 0 && buffer[4] == 0xffff);

    buffer.fill(0xffff);
    Require(functions->extract(text, 3, 5, buffer.data(), 16, &status) == 3 && status == 0 && text->chunkOffset == 5);
    Require(buffer[0] == 0xd83d && buffer[1] == 0xde00 && buffer[2] == u'c' && buffer[3] == 0);

    buffer.fill(0xffff);
    Require(functions->extract(text, -4, 99, buffer.data(), 6, &status) == 6 && status == NotTerminated);
    Require(std::memcmp(buffer.data(), Sample, 6 * sizeof(char16_t)) == 0 && buffer[6] == 0xffff && text->chunkOffset == 6);

    status = 0;
    Require(functions->extract(text, 0, 6, nullptr, 0, &status) == 6 && status == BufferOverflow && text->chunkOffset == 6);

    status = 0;
    buffer.fill(0xffff);
    Require(functions->extract(text, 0, 6, buffer.data(), 2, &status) == 6 && status == BufferOverflow);
    Require(buffer[0] == u'a' && buffer[1] == u'b' && buffer[2] == 0xffff);

    status = 0;
    buffer.fill(0xffff);
    Require(functions->extract(text, 0, 3, buffer.data(), 16, &status) == 4 && status == 0 && text->chunkOffset == 4);
    Require(buffer[2] == 0xd83d && buffer[3] == 0xde00 && buffer[4] == 0);
    Require(functions->extract(text, 0, 3, buffer.data(), 2, &status) == 3 && status == BufferOverflow && text->chunkOffset == 4);

    status = 0;
    functions->access(text, 1, 1);
    buffer.fill(0xffff);
    Require(functions->extract(text, 0, 6, buffer.data(), -1, &status) == 0 && status == IllegalArgument);
    status = 0;
    Require(functions->extract(text, 0, 6, nullptr, 4, &status) == 0 && status == IllegalArgument);
    status = 0;
    Require(functions->extract(text, 5, 2, buffer.data(), 16, &status) == 0 && status == IllegalArgument);
    status = MemoryAllocation;
    Require(functions->extract(text, 0, 6, buffer.data(), 16, &status) == 0 && status == MemoryAllocation);
    Require(buffer[0] == 0xffff && text->chunkOffset == 1);

    Require(utext_close(text) == nullptr && released == Pointers{text} && live.empty());

    constexpr char16_t unpaired[] = {u'x', 0xdc00, 0xd800, u'y'};
    status = 0;
    UText storage = Initial();
    Require(utext_openUChars(&storage, unpaired, 4, &status) == &storage && status == 0);
    Require(storage.pFuncs->access(&storage, 1, 1) == 1 && storage.chunkOffset == 1);
    Require(storage.pFuncs->access(&storage, 2, 1) == 1 && storage.chunkOffset == 2);
    Require(utext_close(&storage) == &storage && storage.flags == 0 && storage.pFuncs == nullptr);
}

void Terminated() {
    Reset();
    const std::u16string hundred(100, u'x');
    std::int32_t status = 0;
    UText storage = Initial();
    UText* text = utext_openUChars(&storage, hundred.c_str(), -1, &status);
    Require(text == &storage && status == 0 && requested.empty() && text->flags == Open);
    Require(text->providerProperties == (StableChunks | LengthIsExpensive) && text->a == -1);
    Require(text->chunkNativeLimit == 0 && text->chunkLength == 0 && text->nativeIndexingLimit == 0);
    const UTextFuncs* functions = text->pFuncs;

    Require(functions->access(text, 0, 1) == 1 && text->chunkOffset == 0 && text->a == -1);
    Require(text->chunkNativeLimit == 32 && text->chunkLength == 32 && text->nativeIndexingLimit == 32);
    Require(text->providerProperties == (StableChunks | LengthIsExpensive));
    Require(functions->access(text, 40, 0) == 1 && text->chunkOffset == 40 && text->chunkNativeLimit == 72 && text->a == -1);
    Require(functions->access(text, 10, 1) == 1 && text->chunkOffset == 10 && text->chunkNativeLimit == 72);
    Require(functions->access(text, 90, 1) == 1 && text->chunkOffset == 90 && text->a == 100);
    Require(text->chunkNativeLimit == 100 && text->chunkLength == 100 && text->nativeIndexingLimit == 100);
    Require(text->providerProperties == StableChunks);
    Require(functions->access(text, 500, 1) == 0 && text->chunkOffset == 100);
    Require(functions->nativeLength(text) == 100);
    Require(utext_close(text) == &storage);

    storage = Initial();
    Require(utext_openUChars(&storage, hundred.c_str(), -1, &status) == &storage);
    Require(functions->nativeLength(&storage) == 100 && storage.a == 100 && storage.chunkNativeLimit == 100);
    Require(storage.chunkLength == 100 && storage.nativeIndexingLimit == 100 && storage.providerProperties == StableChunks);
    Require(utext_close(&storage) == &storage);

    storage = Initial();
    Require(utext_openUChars(&storage, u"hi", -1, &status) == &storage);
    Require(functions->access(&storage, 7, 1) == 0 && storage.chunkOffset == 2 && storage.a == 2);
    Require(storage.chunkNativeLimit == 2 && storage.providerProperties == StableChunks);
    Require(utext_close(&storage) == &storage);

    std::u16string split(31, u'x');
    split += u"\xd83d\xde00";
    split.append(20, u'y');
    storage = Initial();
    Require(utext_openUChars(&storage, split.c_str(), -1, &status) == &storage);
    Require(functions->access(&storage, 0, 1) == 1 && storage.chunkNativeLimit == 31 && storage.chunkLength == 31);
    Require(storage.nativeIndexingLimit == 31 && storage.a == -1);
    Require(functions->access(&storage, 31, 1) == 1 && storage.chunkOffset == 31 && storage.a == 53);
    Require(utext_close(&storage) == &storage);

    storage = Initial();
    Require(utext_openUChars(&storage, split.c_str(), -1, &status) == &storage);
    Require(functions->access(&storage, 32, 1) == 1 && storage.chunkOffset == 31 && storage.a == 53);
    Require(utext_close(&storage) == &storage);

    const std::u16string forty(40, u'z');
    std::array<char16_t, 64> buffer;
    storage = Initial();
    buffer.fill(0xffff);
    Require(utext_openUChars(&storage, forty.c_str(), -1, &status) == &storage);
    Require(functions->extract(&storage, 0, 1000, buffer.data(), 64, &status) == 40 && status == 0);
    Require(std::memcmp(buffer.data(), forty.c_str(), 41 * sizeof(char16_t)) == 0 && buffer[41] == 0xffff);
    Require(storage.a == 40 && storage.chunkNativeLimit == 40 && storage.chunkLength == 40 && storage.chunkOffset == 40);
    Require(storage.nativeIndexingLimit == 40 && storage.providerProperties == (StableChunks | LengthIsExpensive));
    Require(utext_close(&storage) == &storage);

    storage = Initial();
    Require(utext_openUChars(&storage, forty.c_str(), -1, &status) == &storage);
    Require(functions->extract(&storage, 0, 1000, nullptr, 0, &status) == 40 && status == BufferOverflow && storage.a == 40);
    Require(utext_close(&storage) == &storage);

    status = 0;
    storage = Initial();
    buffer.fill(0xffff);
    Require(utext_openUChars(&storage, forty.c_str(), -1, &status) == &storage);
    Require(functions->extract(&storage, 2, 5, buffer.data(), 64, &status) == 3 && status == 0 && buffer[3] == 0);
    Require(storage.a == -1 && storage.chunkNativeLimit == 34 && storage.chunkOffset == 5);
    Require(utext_close(&storage) == &storage);

    storage = Initial();
    Require(utext_openUChars(&storage, forty.c_str(), -1, &status) == &storage);
    Require(functions->extract(&storage, 0, 36, buffer.data(), 64, &status) == 36 && status == 0 && buffer[36] == 0);
    Require(storage.a == 40 && storage.chunkNativeLimit == 40 && storage.chunkOffset == 36);
    Require(storage.providerProperties == StableChunks);
    Require(utext_close(&storage) == &storage && live.empty() && requested.empty());
}

void Clone() {
    Reset();
    std::int32_t status = 0;
    UText* source = utext_openUChars(nullptr, Sample, 6, &status);
    const UTextFuncs* functions = source->pFuncs;
    functions->access(source, 4, 1);

    Reset();
    UText* shallow = functions->clone(nullptr, source, 0, &status);
    Require(shallow != nullptr && shallow != source && status == 0 && requested == Sizes{144});
    Require(shallow->magic == Magic && shallow->flags == (HeapAllocated | Open) && shallow->pFuncs == functions);
    Require(shallow->context == Sample && shallow->chunkContents == Sample && shallow->a == 6 && shallow->chunkOffset == 4);
    Require(shallow->providerProperties == StableChunks && shallow->pExtra == shallow + 1 && shallow->extraSize == 0);
    Require(utext_close(shallow) == nullptr && released == Pointers{shallow});

    Reset();
    UText* deep = functions->clone(nullptr, source, 1, &status);
    Require(deep != nullptr && status == 0 && requested == Sizes{144, 14});
    Require(deep->context != Sample && std::memcmp(deep->context, Sample, sizeof(Sample)) == 0);
    Require(deep->providerProperties == (StableChunks | OwnsText) && deep->a == 6 && deep->chunkContents == Sample);
    void* const copy = const_cast<void*>(deep->context);
    Require(utext_close(deep) == nullptr && released == Pointers{copy, deep});

    Reset();
    status = MemoryAllocation;
    Require(functions->clone(nullptr, source, 1, &status) == nullptr && status == MemoryAllocation && requested.empty());
    status = 0;
    Require(utext_close(source) == nullptr && live.empty());

    Reset();
    const std::u16string words = u"hello";
    UText original = Initial();
    UText target = Initial();
    Require(utext_openUChars(&original, words.c_str(), -1, &status) == &original);
    Require(functions->clone(&target, &original, 1, &status) == &target && status == 0 && requested == Sizes{12});
    Require(target.flags == Open && target.a == 5 && target.providerProperties == (StableChunks | OwnsText));
    Require(target.context != words.c_str() && std::memcmp(target.context, words.c_str(), 12) == 0);
    Require(original.a == -1 && original.providerProperties == (StableChunks | LengthIsExpensive));
    void* const owned = const_cast<void*>(target.context);
    Require(utext_openUChars(&target, Sample, 6, &status) == &target && released == Pointers{owned});
    Require(target.context == Sample && target.providerProperties == StableChunks && target.a == 6);
    Require(utext_close(&target) == &target && utext_close(&original) == &original && live.empty());

    Reset();
    UText custom = Initial();
    Require(utext_setup(&custom, 16, &status) == &custom && status == 0);
    auto* extra = static_cast<std::uint8_t*>(custom.pExtra);
    for (std::uint8_t index = 0; index < 16; ++index) extra[index] = index;
    custom.pFuncs = functions;
    custom.context = Sample;
    custom.a = 6;
    custom.p = extra + 4;
    custom.q = &custom.b;
    custom.r = Sample + 1;
    custom.chunkContents = static_cast<const char16_t*>(custom.pExtra);
    Reset();
    UText* moved = functions->clone(nullptr, &custom, 0, &status);
    Require(moved != nullptr && status == 0 && requested == Sizes{160});
    Require(moved->flags == (HeapAllocated | Open) && moved->extraSize == 16 && moved->pExtra == moved + 1);
    Require(std::memcmp(moved->pExtra, extra, 16) == 0 && moved->p == static_cast<std::uint8_t*>(moved->pExtra) + 4);
    Require(moved->q == &moved->b && moved->r == Sample + 1 && moved->chunkContents == moved->pExtra);
    Require(moved->context == Sample);
    Require(utext_close(moved) == nullptr && utext_close(&custom) == &custom && live.empty());
}

void OpenArguments() {
    Reset();
    std::int32_t status = 0;
    UText* text = utext_openUChars(nullptr, nullptr, 0, &status);
    Require(text != nullptr && status == 0 && text->context != nullptr && text->a == 0 && text->chunkLength == 0);
    Require(*static_cast<const char16_t*>(text->context) == 0 && text->pFuncs->access(text, 0, 1) == 0);
    Require(utext_close(text) == nullptr);

    Reset();
    Require(utext_openUChars(nullptr, nullptr, 3, &status) == nullptr && status == IllegalArgument);
    status = 0;
    Require(utext_openUChars(nullptr, nullptr, -1, &status) == nullptr && status == IllegalArgument);
    status = 0;
    Require(utext_openUChars(nullptr, Sample, -2, &status) == nullptr && status == IllegalArgument);
    status = 0;
    Require(utext_openUChars(nullptr, Sample, std::int64_t{INT32_MAX} + 1, &status) == nullptr && status == IllegalArgument);
    Require(requested.empty());

    UText storage = Initial();
    const UText before = storage;
    status = BufferOverflow;
    Require(utext_openUChars(&storage, Sample, 6, &status) == nullptr && status == BufferOverflow);
    Require(std::memcmp(&storage, &before, sizeof(UText)) == 0);

    status = 0;
    UText blank{};
    Require(utext_openUChars(&blank, Sample, 6, &status) == &blank && status == IllegalArgument);
    Require(Zeroed(&blank, sizeof(UText)));

    status = 0;
    Require(utext_openUChars(&storage, Sample, INT32_MAX, &status) == &storage && status == 0);
    Require(storage.a == INT32_MAX && storage.chunkNativeLimit == INT32_MAX && storage.chunkLength == INT32_MAX);
    Require(utext_close(&storage) == &storage && live.empty());
}

void Rejections() {
    Reset();
    std::int32_t status = 0;
    RequireThrows([] { utext_setup(nullptr, 0, nullptr); });
    RequireThrows([] { utext_openUChars(nullptr, Sample, 6, nullptr); });
    Require(requested.empty());

    UText text = Initial();
    Require(utext_setup(&text, 0, &status) == &text && text.flags == Open && text.pFuncs == nullptr);
    const UText before = text;
    RequireThrows([&] { utext_close(&text); });
    RequireThrows([&] { utext_setup(&text, 0, &status); });
    Require(status == 0 && std::memcmp(&text, &before, sizeof(UText)) == 0);
    text.pFuncs = &BareFunctions;
    Require(utext_close(&text) == &text && text.flags == 0);
}

void Run() {
    std::int32_t status = 0;
    u_setMemoryFunctions_nid_postfix(&memoryContext, Allocate_nid_no_patch, Reallocate_nid_no_patch, Release_nid_no_patch,
                                     &status);
    Require(status == 0);
    SetupHeap();
    SetupCallerStorage();
    SetupFailures();
    CloseIgnored();
    ExplicitLength();
    Terminated();
    Clone();
    OpenArguments();
    Rejections();
    Require(live.empty());
    u_cleanup_nid_postfix();
}

} // namespace UTextChecks

void CheckIcuUText() {
    UTextChecks::Run();
}
