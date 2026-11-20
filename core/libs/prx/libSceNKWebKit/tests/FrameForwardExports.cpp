#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FrameForwardExports check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

constexpr std::uint64_t ForwardedResult = 0x5151515151515151ULL;

struct Seen {
    void* self;
    std::uint64_t arg;
    int calls;
};

static Seen seen;

static std::uint64_t APS5_VABI Target(void* self, std::uint64_t a) {
    seen.self = self;
    seen.arg = a;
    ++seen.calls;
    return ForwardedResult;
}

static std::uint64_t APS5_VABI Wrong(void*, std::uint64_t) {
    std::abort();
}

struct Object {
    alignas(16) unsigned char bytes[0x600];
};

static void Set(Object& object, std::size_t offset, const void* pointer) {
    std::memcpy(object.bytes + offset, &pointer, sizeof(pointer));
}

template<typename F> static bool Throws(F f) {
    try {
        f();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

using Call = std::uint64_t (APS5_VABI*)(void*, std::uint64_t);

static void* vtable[0x400];

static void Arm(std::size_t slot) {
    for (auto& entry : vtable) entry = reinterpret_cast<void*>(&Wrong);
    vtable[slot / sizeof(void*)] = reinterpret_cast<void*>(&Target);
}

static void TestFrame(Call call, std::size_t slot) {
    Object client{};
    Set(client, 0, vtable);
    Object loader{};
    Set(loader, 0x8, &client);
    Object frame{};
    Set(frame, 0xa0, &loader);
    Arm(slot);
    seen = {};
    Require(call(&frame, 0x1234) == ForwardedResult);
    Require(seen.calls == 1 && seen.self == &client && seen.arg == 0x1234);
    Require(Throws([&] { call(nullptr, 0); }));
    Set(frame, 0xa0, nullptr);
    Require(Throws([&] { call(&frame, 0); }));
}

static void TestDocument() {
    Object client{};
    Set(client, 0, vtable);
    Object loader{};
    Set(loader, 0x8, &client);
    Object frame{};
    Set(frame, 0xa0, &loader);
    Object weak{};
    Set(weak, 0x8, &frame);
    Object document{};
    Set(document, 0x188, &weak);
    const auto call = reinterpret_cast<Call>(&_ZNK7WebCore8Document6pageIDEv);
    Arm(0x28);
    seen = {};
    Require(call(&document, 0x4321) == ForwardedResult);
    Require(seen.calls == 1 && seen.self == &client && seen.arg == 0x4321);
    Require(Throws([&] { call(nullptr, 0); }));
    Set(document, 0x188, nullptr);
    Require(Throws([&] { call(&document, 0); }));
    Set(document, 0x188, &weak);
    Set(weak, 0x8, nullptr);
    Require(Throws([&] { call(&document, 0); }));
    Set(weak, 0x8, &frame);
    Set(frame, 0xa0, nullptr);
    Require(Throws([&] { call(&document, 0); }));
}

static void TestTiledBacking() {
    Object graphicsLayer{};
    Set(graphicsLayer, 0, vtable);
    Object backing{};
    Set(backing, 0x38, &graphicsLayer);
    Object layer{};
    Set(layer, 0xe8, &backing);
    Object renderView{};
    Set(renderView, 0xa0, &layer);
    Object document{};
    Set(document, 0x578, &renderView);
    Object frame{};
    Set(frame, 0xc0, &document);
    Object view{};
    Set(view, 0x128, &frame);
    const auto call = reinterpret_cast<Call>(&_ZNK7WebCore9FrameView12tiledBackingEv);
    Arm(0x2e0);
    seen = {};
    Require(call(&view, 0x777) == ForwardedResult);
    Require(seen.calls == 1 && seen.self == &graphicsLayer && seen.arg == 0x777);
    for (const std::size_t level : {0u, 1u, 2u}) {
        Object* const owners[] = {&frame, &document, &layer};
        const std::size_t offsets[] = {0xc0, 0x578, 0xe8};
        const void* const saved = level == 0 ? static_cast<void*>(&document) : level == 1 ? static_cast<void*>(&renderView) : static_cast<void*>(&backing);
        Set(*owners[level], offsets[level], nullptr);
        seen = {};
        Require(call(&view, 1) == 0 && seen.calls == 0);
        Set(*owners[level], offsets[level], saved);
    }
    Require(Throws([&] { call(nullptr, 0); }));
    Set(view, 0x128, nullptr);
    Require(Throws([&] { call(&view, 0); }));
    Set(view, 0x128, &frame);
    Set(renderView, 0xa0, nullptr);
    Require(Throws([&] { call(&view, 0); }));
}

int main() {
    const auto before = NotImplementedCalls_nid_no_patch();
    TestFrame(reinterpret_cast<Call>(&_ZNK7WebCore5Frame6pageIDEv), 0x28);
    TestFrame(reinterpret_cast<Call>(&_ZNK7WebCore5Frame7frameIDEv), 0x30);
    TestDocument();
    TestTiledBacking();
    Require(NotImplementedCalls_nid_no_patch() == before);
    return 0;
}
