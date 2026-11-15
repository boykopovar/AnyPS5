#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <source_location>
#include <type_traits>

namespace UtextLayout {

struct Funcs {
    std::int32_t tableSize;
    std::int32_t reserved[3];
    void* entries[8];
    void (APS5_VABI* close)(void*);
    void* spare[3];
};

struct Text {
    std::uint32_t magic;
    std::int32_t flags;
    std::int32_t providerProperties;
    std::int32_t sizeOfStruct;
    std::int64_t chunkNativeLimit;
    std::int32_t extraSize;
    std::int32_t nativeIndexingLimit;
    std::int64_t chunkNativeStart;
    std::int32_t chunkOffset;
    std::int32_t chunkLength;
    const char16_t* chunkContents;
    const Funcs* pFuncs;
    void* pExtra;
    const void* context;
    const void* p;
    const void* q;
    const void* r;
    void* privP;
    std::int64_t a;
    std::int32_t b;
    std::int32_t c;
    std::int64_t privA;
    std::int32_t privB;
    std::int32_t privC;
};

static_assert(sizeof(Text) == 144);
static_assert(offsetof(Text, pFuncs) == 56);
static_assert(offsetof(Funcs, close) == 80);

constexpr std::uint32_t Magic = 0x345ad82c;
constexpr std::int32_t ExtraHeapAllocated = 2;
constexpr std::int32_t Open = 4;
constexpr std::int32_t IllegalArgument = 1;

Text Initializer() {
    Text text{};
    text.magic = Magic;
    text.sizeOfStruct = sizeof(Text);
    return text;
}

UText* Guest(Text& text) {
    return reinterpret_cast<UText*>(&text);
}

Text& Host(UText* text) {
    return *reinterpret_cast<Text*>(text);
}

} // namespace UtextLayout

namespace UtextClose {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;

static_assert(std::is_same_v<decltype(&utext_close), UText* (APS5_VABI*)(UText*)>);

void Run() {
    UText* text = nullptr;
    Require(utext_close(nullptr) == nullptr);
    Require(utext_close(text) == nullptr);
    Require(text == nullptr);

    alignas(8) std::uint8_t dummy[16]{};
    Require(utext_close(reinterpret_cast<UText*>(dummy)) == reinterpret_cast<UText*>(dummy));
    for (const auto byte : dummy) Require(byte == 0);

    UtextLayout::Text closed = UtextLayout::Initializer();
    Require(utext_close(UtextLayout::Guest(closed)) == UtextLayout::Guest(closed));
    Require(closed.flags == 0 && closed.pFuncs == nullptr);
}

} // namespace UtextClose

namespace UtextOpenUChars {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;
using namespace UtextLayout;

static_assert(std::is_same_v<decltype(&utext_openUChars),
                             UText* (APS5_VABI*)(UText*, const char16_t*, std::int64_t, std::int32_t*)>);

void Run() {
    static const char16_t chars[] = u"abc";

    Text text = Initializer();
    std::int32_t status = 0;
    Require(utext_openUChars(Guest(text), chars, 3, &status) == Guest(text) && status == 0);
    Require(text.magic == Magic && (text.flags & Open) != 0 && text.pFuncs != nullptr);
    Require(text.context == chars && text.chunkContents == chars && text.a == 3);
    Require(text.chunkNativeStart == 0 && text.chunkNativeLimit == 3 && text.chunkLength == 3);
    Require(text.nativeIndexingLimit == 3 && text.chunkOffset == 0);
    const Funcs* native = text.pFuncs;

    Require(utext_openUChars(Guest(text), chars, -1, &status) == Guest(text) && status == 0);
    Require(text.pFuncs == native && text.a == -1 && text.chunkNativeLimit == 0 && text.chunkLength == 0);
    Require(utext_close(Guest(text)) == Guest(text));
    Require((text.flags & Open) == 0 && text.pFuncs == nullptr);

    UText* heap = utext_openUChars(nullptr, chars, 2, &status);
    Require(heap != nullptr && status == 0 && Host(heap).context == chars && Host(heap).chunkNativeLimit == 2);
    Require(utext_close(heap) == nullptr);

    Text empty = Initializer();
    Require(utext_openUChars(Guest(empty), nullptr, 0, &status) == Guest(empty) && status == 0);
    Require(empty.chunkContents != nullptr && empty.chunkNativeLimit == 0);
    utext_close(Guest(empty));

    Text rejected = Initializer();
    Require(utext_openUChars(Guest(rejected), nullptr, 1, &status) == nullptr && status == IllegalArgument);
    status = 0;
    Require(utext_openUChars(Guest(rejected), chars, -2, &status) == nullptr && status == IllegalArgument);
    Require(rejected.flags == 0 && rejected.context == nullptr);

    status = IllegalArgument;
    Require(utext_openUChars(Guest(rejected), chars, 3, &status) == nullptr && status == IllegalArgument);
    Require(rejected.flags == 0 && rejected.context == nullptr);

    Text wrongMagic = Initializer();
    wrongMagic.magic = 0;
    status = 0;
    Require(utext_openUChars(Guest(wrongMagic), chars, 3, &status) == Guest(wrongMagic) && status == IllegalArgument);
    Require(wrongMagic.flags == 0 && wrongMagic.context == nullptr);

    RequireThrows([&] { utext_openUChars(Guest(text), chars, 3, nullptr); });
}

} // namespace UtextOpenUChars

namespace UtextSetup {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;
using namespace UtextLayout;

static_assert(std::is_same_v<decltype(&utext_setup), UText* (APS5_VABI*)(UText*, std::int32_t, std::int32_t*)>);

int g_guestCloses = 0;

void APS5_VABI GuestClose(void*) {
    ++g_guestCloses;
}

void Run() {
    Funcs closeless{};
    closeless.tableSize = sizeof(Funcs);

    std::int32_t status = 0;
    UText* heap = utext_setup(nullptr, 16, &status);
    Require(heap != nullptr && status == 0 && (Host(heap).flags & Open) != 0);
    Require(Host(heap).pFuncs == nullptr);
    Require(Host(heap).pExtra != nullptr && Host(heap).extraSize == 16);
    const auto* extra = static_cast<const std::uint8_t*>(Host(heap).pExtra);
    for (int index = 0; index < 16; ++index) Require(extra[index] == 0);
    RequireThrows([&] { utext_close(heap); });
    Require((Host(heap).flags & Open) != 0);
    Host(heap).pFuncs = &closeless;
    Require(utext_close(heap) == nullptr);

    Text text = Initializer();
    Require(utext_setup(Guest(text), 8, &status) == Guest(text) && status == 0);
    Require((text.flags & Open) != 0 && (text.flags & ExtraHeapAllocated) != 0);
    Require(text.pExtra != nullptr && text.extraSize == 8);
    text.pFuncs = &closeless;
    Require(utext_close(Guest(text)) == Guest(text));
    Require(text.flags == 0 && text.pExtra == nullptr && text.extraSize == 0);

    status = IllegalArgument;
    Require(utext_setup(Guest(text), 0, &status) == Guest(text) && status == IllegalArgument && text.flags == 0);

    Funcs guest{};
    guest.tableSize = sizeof(Funcs);
    guest.close = GuestClose;
    Text provided = Initializer();
    provided.flags = Open;
    provided.pFuncs = &guest;
    status = 0;
    RequireThrows([&] { utext_close(Guest(provided)); });
    RequireThrows([&] { utext_setup(Guest(provided), 0, &status); });
    RequireThrows([&] { utext_openUChars(Guest(provided), u"a", 1, &status); });
    Require(g_guestCloses == 0 && provided.flags == Open && provided.pFuncs == &guest && status == 0);

    provided.pFuncs = nullptr;
    RequireThrows([&] { utext_close(Guest(provided)); });

    guest.close = nullptr;
    provided.pFuncs = &guest;
    Require(utext_setup(Guest(provided), 0, &status) == Guest(provided) && status == 0);
    Require((provided.flags & Open) != 0 && provided.pFuncs == &guest);
    Require(utext_close(Guest(provided)) == Guest(provided) && provided.flags == 0 && provided.pFuncs == nullptr);

    RequireThrows([&] { utext_setup(Guest(text), 0, nullptr); });
}

} // namespace UtextSetup

void CheckIcuUText() {
    UtextClose::Run();
    UtextOpenUChars::Run();
    UtextSetup::Run();
}
