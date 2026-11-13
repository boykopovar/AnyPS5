#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include <cstdint>
#include <cstring>
#include <limits>
#include <initializer_list>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>
#include <type_traits>

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
    RequireThrows([&] { utext_close(reinterpret_cast<UText*>(dummy)); });
    for (const auto byte : dummy) Require(byte == 0);
}


static_assert(std::is_same_v<decltype(&utext_setup), UText* (APS5_VABI*)(UText*, std::int32_t, std::int32_t*)>);

int closeCount;
void* expectedExtra;
const void* expectedContext;

void APS5_VABI Close(UText* text) {
    Require((text->flags & UTEXT_OPEN) != 0);
    Require(text->pExtra == expectedExtra);
    Require(text->context == expectedContext);
    ++closeCount;
}

void CheckReset(const UText& text) {
    Require(text.context == nullptr && text.chunkContents == nullptr);
    Require(text.p == nullptr && text.q == nullptr && text.r == nullptr && text.privP == nullptr);
    Require(text.a == 0 && text.b == 0 && text.c == 0);
    Require(text.privA == 0 && text.privB == 0 && text.privC == 0);
    Require(text.chunkOffset == 0 && text.chunkLength == 0);
    Require(text.chunkNativeStart == 0 && text.chunkNativeLimit == 0);
    Require(text.nativeIndexingLimit == 0 && text.providerProperties == 0);
    for (std::int32_t i = 0; i < text.extraSize; ++i)
        Require(static_cast<const unsigned char*>(text.pExtra)[i] == 0);
}

void SetupAndClose() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    UTextFuncs funcs{};
    funcs.tableSize = sizeof(funcs);
    funcs.close = Close;
    std::int32_t status = 0;
    for (const std::int32_t extra : {0, 1, 37, -1}) {
        UText* text = utext_setup(nullptr, extra, &status);
        Require(text != nullptr && status == 0);
        Require(text->magic == UTEXT_MAGIC && text->sizeOfStruct == 144);
        Require(text->flags == (UTEXT_HEAP_ALLOCATED | UTEXT_OPEN));
        Require(text->extraSize == extra);
        Require(reinterpret_cast<std::uintptr_t>(text->pExtra) % alignof(std::max_align_t) == 0);
        Require(text->pExtra == reinterpret_cast<unsigned char*>(text) + 144);
        CheckReset(*text);
        expectedExtra = text->pExtra;
        expectedContext = nullptr;
        text->pFuncs = &funcs;
        int before = closeCount;
        Require(utext_close(text) == nullptr);
        Require(closeCount == before + 1);
    }
    UText* grown = utext_setup(nullptr, 8, &status);
    grown->pFuncs = &funcs;
    expectedExtra = grown->pExtra;
    expectedContext = nullptr;
    int initialCloses = closeCount;
    Require(utext_setup(grown, 128, &status) == grown);
    Require(closeCount == initialCloses + 1);
    Require(grown->flags == (UTEXT_HEAP_ALLOCATED | UTEXT_EXTRA_HEAP_ALLOCATED | UTEXT_OPEN));
    Require(grown->extraSize == 128);
    CheckReset(*grown);
    expectedExtra = grown->pExtra;
    Require(utext_close(grown) == nullptr && closeCount == initialCloses + 2);
    UText stack{UTEXT_MAGIC, 0, 0, sizeof(UText)};
    Require(utext_close(&stack) == &stack);
    Require(utext_setup(&stack, 16, &status) == &stack && status == 0);
    Require(stack.flags == (UTEXT_EXTRA_HEAP_ALLOCATED | UTEXT_OPEN));
    Require(stack.extraSize == 16 && stack.pExtra != nullptr);
    CheckReset(stack);
    stack.pFuncs = &funcs;
    for (const std::int32_t extra : {8, 32, 64}) {
        auto oldExtra = stack.pExtra;
        auto oldSize = stack.extraSize;
        std::memset(stack.pExtra, 0x5a, stack.extraSize);
        stack.context = &stack;
        stack.p = stack.q = stack.r = stack.privP = &stack;
        stack.chunkContents = u"text";
        stack.a = stack.b = stack.c = stack.privA = stack.privB = stack.privC = 7;
        stack.chunkOffset = stack.chunkLength = stack.nativeIndexingLimit = stack.providerProperties = 7;
        stack.chunkNativeStart = stack.chunkNativeLimit = 7;
        expectedExtra = oldExtra;
        expectedContext = &stack;
        int before = closeCount;
        Require(utext_setup(&stack, extra, &status) == &stack && status == 0);
        Require(closeCount == before + 1);
        Require(stack.pFuncs == &funcs);
        Require(stack.extraSize == (extra <= oldSize ? oldSize : extra));
        if (extra <= oldSize) Require(stack.pExtra == oldExtra);
        CheckReset(stack);
    }
    UText saved = stack;
    status = 7;
    int before = closeCount;
    Require(utext_setup(&stack, 128, &status) == &stack && status == 7);
    Require(std::memcmp(&saved, &stack, sizeof(stack)) == 0 && closeCount == before);
    Require(utext_setup(nullptr, 128, &status) == nullptr && status == 7);
    RequireThrows([&] { utext_setup(&stack, 0, nullptr); });
    status = 0;
    UText invalid{};
    saved = invalid;
    Require(utext_setup(&invalid, 32, &status) == &invalid && status == 1);
    Require(std::memcmp(&saved, &invalid, sizeof(invalid)) == 0);
    expectedExtra = stack.pExtra;
    expectedContext = nullptr;
    Require(utext_close(&stack) == &stack && closeCount == before + 1);
    Require(stack.flags == 0 && stack.pExtra == nullptr && stack.extraSize == 0);
    Require(stack.magic == UTEXT_MAGIC && stack.pFuncs == nullptr);
    Require(utext_close(&stack) == &stack && closeCount == before + 1);
    status = -1;
    Require(utext_setup(&stack, 0, &status) == &stack && status == -1);
    funcs.close = nullptr;
    stack.pFuncs = &funcs;
    Require(utext_close(&stack) == &stack && closeCount == before + 1);
    status = 0;
    RequireThrows([&] { utext_setup(nullptr, std::numeric_limits<std::int32_t>::max(), &status); });
}

} // namespace UtextClose

void CheckIcuUText() {
    UtextClose::Run();
    UtextClose::SetupAndClose();
}
