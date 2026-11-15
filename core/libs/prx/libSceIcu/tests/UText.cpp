#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/UText.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>

namespace UtextClose {

using IcuRecoveredTest::Require;

static_assert(std::is_same_v<decltype(&utext_close), UText* (APS5_VABI*)(UText*)>);
static_assert(std::is_same_v<decltype(UTextFuncs::close), void (APS5_VABI*)(UText*)>);

constexpr std::uint32_t Magic = 0x345ad82c;
constexpr std::int32_t Heap = 1;
constexpr std::int32_t Extra = 2;
constexpr std::int32_t Open = 4;
constexpr std::int32_t UnknownFlag = 0x40000000;
constexpr char16_t Content[] = u"provider text";

struct State {
    UText* text = nullptr;
    UText beforeCallback{};
    std::array<void*, 2> released{};
    std::array<UText, 2> atRelease{};
    std::size_t releaseCount = 0;
    int callbackCount = 0;
    int allocations = 0;
    int reallocations = 0;
    std::int32_t setFlags = 0;
    std::int32_t clearFlags = 0;
    void* replacement = nullptr;
    bool replaceExtra = false;
    bool changeFields = false;
    bool retainOnExtraFree = false;
};

void* APS5_VABI Allocate(const void* context, std::size_t size) {
    ++const_cast<State*>(static_cast<const State*>(context))->allocations;
    return std::malloc(size);
}

void* APS5_VABI Reallocate(const void* context, void* memory, std::size_t size) {
    ++const_cast<State*>(static_cast<const State*>(context))->reallocations;
    return std::realloc(memory, size);
}

void APS5_VABI Release(const void* context, void* memory) {
    auto& state = *const_cast<State*>(static_cast<const State*>(context));
    Require(state.releaseCount < state.released.size());
    state.released[state.releaseCount] = memory;
    state.atRelease[state.releaseCount++] = *state.text;
    if (state.retainOnExtraFree && memory != state.text) state.text->flags &= ~Heap;
}

void APS5_VABI CloseProvider(UText* text) {
    auto& state = *const_cast<State*>(static_cast<const State*>(text->context));
    Require(text == state.text);
    Require((text->flags & Open) != 0);
    state.beforeCallback = *text;
    ++state.callbackCount;
    text->flags = (text->flags | state.setFlags) & ~state.clearFlags;
    if (state.replaceExtra) {
        text->pExtra = state.replacement;
        text->extraSize = 48;
    }
    if (state.changeFields) {
        text->b = -471;
        text->pFuncs = nullptr;
    }
}

struct GuardedText {
    UText text;
    std::array<std::uint8_t, 16> guard;
};

class Fixture {
public:
    State state;
    UTextFuncs functions{};
    GuardedText local{};
    GuardedText* storage;
    void* extra;
    void* replacement;

    explicit Fixture(std::int32_t flags, bool heapStorage = false)
        : storage(heapStorage ? static_cast<GuardedText*>(std::malloc(sizeof(GuardedText))) : &local),
          extra(std::malloc(32)), replacement(std::malloc(48)) {
        Require(storage && extra && replacement);
        functions.tableSize = sizeof(UTextFuncs);
        functions.close = CloseProvider;
        storage->guard.fill(0x5a);
        state.text = &storage->text;
        state.replacement = replacement;
        storage->text = {Magic, flags | UnknownFlag, 37, sizeof(UText), 999, 32, 41,
                         221, 7, 13, Content, &functions, extra, &state,
                         Content + 1, Content + 2, Content + 3, replacement,
                         0x123456789abcdef, -19, 81, -999, 94, -31};
        std::int32_t status = 0;
        u_setMemoryFunctions_nid_postfix(&state, Allocate, Reallocate, Release, &status);
        Require(status == 0);
    }

    ~Fixture() {
        for (const auto byte : storage->guard) Require(byte == 0x5a);
        Require(state.allocations == 0 && state.reallocations == 0);
        u_cleanup_nid_postfix();
        std::free(extra);
        std::free(replacement);
        if (storage != &local) std::free(storage);
    }

    Fixture(const Fixture&) = delete;
    Fixture& operator=(const Fixture&) = delete;
};

void RequireSame(const UText& actual, const UText& expected) {
    Require(std::memcmp(&actual, &expected, sizeof(UText)) == 0);
}

void CheckEarlyReturns() {
    Require(utext_close(nullptr) == nullptr);

    alignas(8) std::uint8_t dummy[16]{};
    auto* invalid = reinterpret_cast<UText*>(dummy);
    Require(utext_close(invalid) == invalid);
    for (const auto byte : dummy) Require(byte == 0);

    for (std::int32_t flags = 0; flags != 8; ++flags) {
        Fixture fixture(flags);
        fixture.state.text->magic = 0;
        fixture.state.text->pFuncs = reinterpret_cast<const UTextFuncs*>(std::uintptr_t{1});
        const UText before = *fixture.state.text;
        Require(utext_close(fixture.state.text) == fixture.state.text);
        RequireSame(*fixture.state.text, before);
        Require(fixture.state.callbackCount == 0 && fixture.state.releaseCount == 0);
        if (!(flags & Open)) {
            fixture.state.text->magic = Magic;
            const UText closed = *fixture.state.text;
            Require(utext_close(fixture.state.text) == fixture.state.text);
            RequireSame(*fixture.state.text, closed);
        }
    }
}

void CheckOwnership() {
    for (const bool provider : {false, true}) {
        for (std::int32_t flags = 0; flags != 8; ++flags) {
            Fixture fixture(flags, (flags & Heap) != 0);
            if (!provider) fixture.functions.close = nullptr;
            const UText before = *fixture.state.text;
            const bool opened = (flags & Open) != 0;
            const bool releaseText = opened && (flags & Heap);
            const bool releaseExtra = opened && (flags & Extra);
            Require(utext_close(fixture.state.text) == (releaseText ? nullptr : fixture.state.text));
            Require(fixture.state.callbackCount == static_cast<int>(opened && provider));
            Require(fixture.state.releaseCount == static_cast<std::size_t>(releaseText + releaseExtra));
            if (opened && provider) RequireSame(fixture.state.beforeCallback, before);
            UText expected = before;
            if (opened) {
                expected.flags &= ~(Open | Extra);
                expected.pFuncs = nullptr;
                if (releaseExtra) {
                    expected.pExtra = nullptr;
                    expected.extraSize = 0;
                    Require(fixture.state.released[0] == fixture.extra);
                    UText atExtra = before;
                    atExtra.flags &= ~Open;
                    RequireSame(fixture.state.atRelease[0], atExtra);
                }
                if (releaseText) {
                    expected.magic = 0;
                    Require(fixture.state.released[releaseExtra] == fixture.state.text);
                    RequireSame(fixture.state.atRelease[releaseExtra], expected);
                }
            }
            RequireSame(*fixture.state.text, expected);
            if (!releaseText) {
                const auto releases = fixture.state.releaseCount;
                const auto callbacks = fixture.state.callbackCount;
                Require(utext_close(fixture.state.text) == fixture.state.text);
                RequireSame(*fixture.state.text, expected);
                Require(fixture.state.releaseCount == releases && fixture.state.callbackCount == callbacks);
            }
        }
    }
}

void CheckProviderChanges() {
    {
        Fixture fixture(Open, true);
        fixture.state.setFlags = Heap | Extra;
        fixture.state.replaceExtra = true;
        fixture.state.changeFields = true;
        const UText before = *fixture.state.text;
        Require(utext_close(fixture.state.text) == nullptr);
        RequireSame(fixture.state.beforeCallback, before);
        Require(fixture.state.releaseCount == 2);
        Require(fixture.state.released[0] == fixture.replacement);
        Require(fixture.state.released[1] == fixture.state.text);
        const auto& atExtra = fixture.state.atRelease[0];
        Require(atExtra.flags == (UnknownFlag | Heap | Extra));
        Require(atExtra.extraSize == 48 && atExtra.pExtra == fixture.replacement);
        Require(atExtra.b == -471 && atExtra.pFuncs == nullptr);
        UText expected = before;
        expected.flags = UnknownFlag | Heap;
        expected.magic = 0;
        expected.pExtra = nullptr;
        expected.extraSize = 0;
        expected.b = -471;
        expected.pFuncs = nullptr;
        RequireSame(fixture.state.atRelease[1], expected);
        RequireSame(*fixture.state.text, expected);
    }
    {
        Fixture fixture(Open | Heap | Extra, true);
        fixture.state.clearFlags = Heap | Extra | Open;
        fixture.state.changeFields = true;
        UText expected = *fixture.state.text;
        expected.flags = UnknownFlag;
        expected.b = -471;
        expected.pFuncs = nullptr;
        Require(utext_close(fixture.state.text) == fixture.state.text);
        Require(fixture.state.releaseCount == 0 && fixture.state.callbackCount == 1);
        RequireSame(*fixture.state.text, expected);
    }
    {
        Fixture fixture(Open | Heap | Extra, true);
        fixture.state.retainOnExtraFree = true;
        Require(utext_close(fixture.state.text) == fixture.state.text);
        Require(fixture.state.releaseCount == 1 && fixture.state.released[0] == fixture.extra);
        Require(fixture.state.atRelease[0].flags == (UnknownFlag | Heap | Extra));
        Require(fixture.state.text->flags == UnknownFlag);
        Require(fixture.state.text->magic == Magic && fixture.state.text->pFuncs == nullptr);
        Require(fixture.state.text->pExtra == nullptr && fixture.state.text->extraSize == 0);
    }
}

void CheckExtraPointers() {
    {
        Fixture fixture(Open);
        fixture.state.text->pExtra = fixture.storage->guard.data();
        fixture.state.text->extraSize = -3;
        UText expected = *fixture.state.text;
        expected.flags = UnknownFlag;
        expected.pFuncs = nullptr;
        Require(utext_close(fixture.state.text) == fixture.state.text);
        RequireSame(*fixture.state.text, expected);
        Require(fixture.state.releaseCount == 0);
    }
    for (const bool zeroAllocation : {false, true}) {
        Fixture fixture(Open | Extra);
        fixture.state.text->pExtra = zeroAllocation ? uprv_malloc_nid_postfix(0) : nullptr;
        Require(utext_close(fixture.state.text) == fixture.state.text);
        Require(fixture.state.releaseCount == static_cast<std::size_t>(!zeroAllocation));
        if (!zeroAllocation) Require(fixture.state.released[0] == nullptr);
        Require(fixture.state.text->pExtra == nullptr && fixture.state.text->extraSize == 0);
        Require(fixture.state.text->pFuncs == nullptr && fixture.state.text->flags == UnknownFlag);
    }
}

void CheckDefaultAllocator() {
    std::array<void*, 10> api{};
    ApplicationHeapRegister_nid_no_patch(api.data());
    u_cleanup_nid_postfix();
    UTextFuncs functions{};
    auto* text = static_cast<UText*>(uprv_malloc_nid_postfix(sizeof(UText)));
    Require(text != nullptr);
    *text = {};
    text->magic = Magic;
    text->flags = Heap | Extra | Open;
    text->pFuncs = &functions;
    text->pExtra = uprv_malloc_nid_postfix(23);
    text->extraSize = 23;
    Require(text->pExtra != nullptr);
    Require(utext_close(text) == nullptr);
}

void Run() {
    CheckEarlyReturns();
    CheckOwnership();
    CheckProviderChanges();
    CheckExtraPointers();
    CheckDefaultAllocator();
    std::puts("ICU UText close ownership and guest callback checks passed");
}

} // namespace UtextClose

void CheckIcuUText() {
    UtextClose::Run();
}
