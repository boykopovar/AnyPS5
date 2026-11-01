#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libSceFont/libSceFont.hpp"
#include "prx/libSceFontGs.native/libSceFontGs.native.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int Fatal = static_cast<int>(0x80460001);
constexpr int InvalidParameter = static_cast<int>(0x80460002);
constexpr int InvalidSequence = static_cast<int>(0x8046009a);
constexpr int SequenceLocked = static_cast<int>(0x804600b8);
constexpr int InvalidObject = static_cast<int>(0x80460084);

int g_errorCount = 0;
int g_lastError = 0;

void APS5_VABI RecordError(int error) {
    g_lastError = error;
    ++g_errorCount;
}

void Require(bool value) { if (!value) std::abort(); }

struct TestDrawupRecord {
    std::uint8_t type;
    std::uint8_t sizeTag;
    std::uint16_t flags;
    std::uint32_t sequence;
    TestDrawupRecord* next;
    std::uint32_t state;
    std::uint32_t mode;
    void* payload;
    std::uint32_t params[8];
    std::uint8_t reserved[0x40];
};

struct TestDrawupEntry {
    std::uint8_t* memory;
    std::uint32_t reserved8;
    std::uint32_t slotSize;
    std::uint32_t total;
    std::uint32_t used;
    std::uint16_t* nodes;
    std::uint16_t* scan;
    std::uint64_t reserved28;
};

struct TestDrawupPool {
    PthreadMutex* mutex;
    std::uint32_t reserved8;
    std::uint32_t entryCount;
    std::uint32_t used;
    std::uint32_t maximum;
    TestDrawupEntry entries[1];
};

struct TestDrawupOwner {
    std::uint8_t reserved[0x160];
    TestDrawupPool* pool;
};

struct TestDrawupObject {
    std::uint64_t reserved0;
    TestDrawupOwner* owner;
    void* resource;
    std::uint64_t reserved18[2];
    TestDrawupRecord* head;
    TestDrawupRecord* tail;
};

struct TestDrawupFrame {
    std::uint16_t tag;
    std::uint16_t reserved2;
    int error;
    TestDrawupObject* object;
    std::uint32_t unitSize;
    std::uint32_t unitUsed;
    void* unitHead;
    std::uint8_t* unit;
    std::uint32_t sequence;
};

static_assert(sizeof(TestDrawupRecord) == 0x80);
static_assert(offsetof(TestDrawupRecord, state) == 0x10);
static_assert(offsetof(TestDrawupOwner, pool) == 0x160);
static_assert(offsetof(TestDrawupFrame, object) == 0x8 && offsetof(TestDrawupFrame, sequence) == 0x28);

struct TestAgcSurface {
    void* head;
    std::uint8_t reserved8[5];
    std::uint8_t tag;
    std::uint8_t reserved0e;
    std::uint8_t mode;
    std::uint8_t reserved10[0xc];
    std::uint32_t busy;
    std::uint8_t reserved20[8];
    const void* fillObject;
    std::uint8_t reserved30[8];
    std::uint32_t initialized;
};

static_assert(offsetof(TestAgcSurface, tag) == 0x0d);
static_assert(offsetof(TestAgcSurface, mode) == 0x0f);
static_assert(offsetof(TestAgcSurface, busy) == 0x1c);
static_assert(offsetof(TestAgcSurface, fillObject) == 0x28);
static_assert(offsetof(TestAgcSurface, initialized) == 0x38);

struct TestAgcCommands {
    std::uint8_t reserved[0x28];
    const void* first;
    std::uint32_t firstSize;
    std::uint32_t secondCount;
    const void* second;
};

static_assert(offsetof(TestAgcCommands, first) == 0x28);
static_assert(offsetof(TestAgcCommands, firstSize) == 0x30);
static_assert(offsetof(TestAgcCommands, secondCount) == 0x34);
static_assert(offsetof(TestAgcCommands, second) == 0x38);

struct TestAgcSequence {
    std::uint16_t tag;
    std::uint8_t reserved2[0x19];
    std::uint8_t lock;
    std::uint8_t reserved1c[0xc];
    TestAgcCommands* commands;
};

static_assert(offsetof(TestAgcSequence, lock) == 0x1b);
static_assert(offsetof(TestAgcSequence, commands) == 0x28);

constexpr std::uint32_t TotalSlots = 8;

struct DrawupFixture {
    PthreadMutex mutex{};
    std::uint8_t memory[TotalSlots * 0x80]{};
    std::uint16_t nodes[TotalSlots]{};
    TestDrawupPool pool{};
    TestDrawupOwner owner{};
    TestDrawupObject object{};
    TestDrawupFrame frame{};
    std::uint8_t descriptor[0x200]{};
    TestAgcSurface surface{};

    DrawupFixture() {
        scePthreadMutexInit(&mutex, nullptr, "agc-drawup");
        nodes[0] = TotalSlots << 1;
        pool.mutex = &mutex;
        pool.entryCount = 1;
        pool.entries[0] = {memory, 0, 0x80, TotalSlots, 0, nodes, nodes, 0};
        owner.pool = &pool;
        object.owner = &owner;
        frame.tag = 0xf53;
        frame.object = &object;
        surface.head = &object;
        surface.tag = 0x58;
        surface.mode = 0x22;
        surface.fillObject = descriptor;
        surface.initialized = 1;
        for (std::size_t i = 0; i < sizeof(descriptor); ++i) descriptor[i] = static_cast<std::uint8_t>(i);
    }

    ~DrawupFixture() { scePthreadMutexDestroy(&mutex); }
};

void AgcSelect() {
    Require(sceFontSelectGraphicsAgc(1) == nullptr);
    Require(sceFontSelectGraphicsAgc(0xffffffff) == nullptr);
}

void AgcDrawupImage() {
    {
        void* out = reinterpret_cast<void*>(0xdead);
        const int before = g_errorCount;
        Require(sceFontGraphicsAgcDrawupFillTextureImage(nullptr, nullptr, &out, 0.0f, 0.0f, 8.0f, 8.0f) == InvalidParameter);
        Require(out == nullptr);
        Require(g_errorCount == before);
    }
    {
        DrawupFixture f;
        auto* frame = reinterpret_cast<FontGraphicsFrame*>(&f.frame);
        const int before = g_errorCount;
        Require(sceFontGraphicsAgcDrawupFillTextureImage(frame, &f.surface, nullptr, 0.0f, 0.0f, 8.0f, 8.0f) == InvalidParameter);
        Require(g_errorCount == before);
    }
    {
        DrawupFixture f;
        auto* frame = reinterpret_cast<FontGraphicsFrame*>(&f.frame);
        void* out = reinterpret_cast<void*>(0xdead);
        const int before = g_errorCount;
        f.surface.tag = 0x57;
        Require(sceFontGraphicsAgcDrawupFillTextureImage(frame, &f.surface, &out, 0.0f, 0.0f, 8.0f, 8.0f) == InvalidParameter);
        Require(out == nullptr);
        f.surface.tag = 0x58;
        f.surface.head = nullptr;
        Require(sceFontGraphicsAgcDrawupFillTextureImage(frame, &f.surface, &out, 0.0f, 0.0f, 8.0f, 8.0f) == InvalidParameter);
        f.surface.head = &f.object;
        f.surface.initialized = 0;
        Require(sceFontGraphicsAgcDrawupFillTextureImage(frame, &f.surface, &out, 0.0f, 0.0f, 8.0f, 8.0f) == InvalidParameter);
        f.surface.initialized = 1;
        f.surface.busy = 1;
        Require(sceFontGraphicsAgcDrawupFillTextureImage(frame, &f.surface, &out, 0.0f, 0.0f, 8.0f, 8.0f) == InvalidParameter);
        f.surface.busy = 0;
        Require(sceFontGraphicsAgcDrawupFillTextureImage(frame, &f.surface, &out, -1.0f, 0.0f, 8.0f, 8.0f) == InvalidParameter);
        Require(sceFontGraphicsAgcDrawupFillTextureImage(frame, &f.surface, &out, 0.0f, -1.0f, 8.0f, 8.0f) == InvalidParameter);
        const float nan = std::strtof("nan", nullptr);
        Require(!(nan >= 0.0f));
        Require(sceFontGraphicsAgcDrawupFillTextureImage(frame, &f.surface, &out, nan, 0.0f, 8.0f, 8.0f) == InvalidParameter);
        Require(sceFontGraphicsAgcDrawupFillTextureImage(frame, &f.surface, &out, 0.0f, nan, 8.0f, 8.0f) == InvalidParameter);
        Require(sceFontGraphicsAgcDrawupFillTextureImage(frame, &f.surface, &out, 0.0f, 0.0f, nan, 8.0f) == InvalidParameter);
        Require(sceFontGraphicsAgcDrawupFillTextureImage(frame, &f.surface, &out, 0.0f, 0.0f, 8.0f, nan) == InvalidParameter);
        Require(sceFontGraphicsAgcDrawupFillTextureImage(frame, &f.surface, &out, 16830.0f, 0.0f, 8.0f, 8.0f) == InvalidParameter);
        Require(sceFontGraphicsAgcDrawupFillTextureImage(frame, &f.surface, &out, 0.0f, 0.0f, -4.0f, 8.0f) == InvalidParameter);
        Require(sceFontGraphicsAgcDrawupFillTextureImage(frame, &f.surface, &out, 0.0f, 0.0f, 8.0f, -4.0f) == InvalidParameter);
        Require(sceFontGraphicsAgcDrawupFillTextureImage(frame, &f.surface, &out, 0.0f, 0.0f, 16834.0f, 16834.0f) == 0);
        Require(out != nullptr);
        const auto* firstRecord = static_cast<const TestDrawupRecord*>(out);
        Require(f.object.head == firstRecord && f.object.tail == firstRecord);
        out = reinterpret_cast<void*>(0xdead);
        Require(sceFontGraphicsAgcDrawupFillTextureImage(frame, &f.surface, &out, 0.0f, 0.0f, 8.0f, 8.0f) == 0);
        Require(out != nullptr);
        const auto* record = static_cast<const TestDrawupRecord*>(out);
        Require(record->state == 1);
        Require(record->mode == 0x22);
        Require(f.object.head == firstRecord && f.object.tail == record);
        Require(g_errorCount == before);
    }
    {
        DrawupFixture f;
        auto* frame = reinterpret_cast<FontGraphicsFrame*>(&f.frame);
        void* out = reinterpret_cast<void*>(0xdead);
        f.frame.tag = 0;
        Require(sceFontGraphicsAgcDrawupFillTextureImage(frame, &f.surface, &out, 0.0f, 0.0f, 8.0f, 8.0f) == InvalidObject);
        Require(out == nullptr);
        Require(f.frame.error == InvalidObject);
        Require(g_lastError == InvalidObject);
        g_lastError = 0;
    }
}

void AgcDrawupPattern() {
    {
        DrawupFixture f;
        auto* frame = reinterpret_cast<FontGraphicsFrame*>(&f.frame);
        const int before = g_errorCount;
        Require(sceFontGraphicsAgcDrawupFillTexturePattern(frame, &f.surface, 1, 2, 3, 4, 5, 6, nullptr) == InvalidParameter);
        f.surface.initialized = 0;
        void* out = reinterpret_cast<void*>(0xdead);
        Require(sceFontGraphicsAgcDrawupFillTexturePattern(frame, &f.surface, 1, 2, 3, 4, 5, 6, &out) == InvalidParameter);
        Require(out == nullptr);
        Require(g_errorCount == before);
        f.surface.initialized = 1;
        Require(sceFontGraphicsAgcDrawupFillTexturePattern(frame, &f.surface, 1, 2, 3, 4, 5, 6, &out) == 0);
        Require(out != nullptr);
        const auto* record = static_cast<const TestDrawupRecord*>(out);
        Require(record->state == 1);
        Require(record->mode == 0x22);
    }
}

void AgcSequences() {
    std::uint32_t firstData[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    std::uint32_t secondData[3] = {9, 10, 11};
    TestAgcCommands commands{};
    commands.first = firstData;
    commands.firstSize = 0x20;
    commands.secondCount = 3;
    commands.second = secondData;
    TestAgcSequence sequence{};
    sequence.tag = 0xf80;
    sequence.commands = &commands;
    auto* seq = reinterpret_cast<FontGraphicsCanvasSequence*>(&sequence);
    {
        void* out = reinterpret_cast<void*>(0xdead);
        void* out2 = reinterpret_cast<void*>(0xbeef);
        std::uint32_t count = 0xcccc;
        std::uint32_t count2 = 0xdddd;
        const int before = g_errorCount;
        Require(sceFontGraphicsSequenceGetAgcCommandsForJump(nullptr, &out, &count, &out2, &count2) == InvalidSequence);
        Require(out == nullptr && count == 0 && out2 == nullptr && count2 == 0);
        Require(sceFontGraphicsSequenceGetAgcCommandsForJump(seq, &out, nullptr, &out2, &count2) == InvalidParameter);
        Require(out == nullptr && out2 == nullptr && count2 == 0);
        Require(g_errorCount == before);
        sequence.tag = 0xf81;
        Require(sceFontGraphicsSequenceGetAgcCommandsForJump(seq, &out, &count, &out2, &count2) == InvalidSequence);
        Require(sceFontGraphicsSequenceGetAgcCommandsForJump(seq, nullptr, nullptr, nullptr, nullptr) == InvalidSequence);
        sequence.tag = 0xf80;
        sequence.lock = 1;
        Require(sceFontGraphicsSequenceGetAgcCommandsForJump(seq, &out, &count, &out2, &count2) == SequenceLocked);
        sequence.lock = 0;
        auto* saved = sequence.commands;
        sequence.commands = nullptr;
        Require(sceFontGraphicsSequenceGetAgcCommandsForJump(seq, &out, &count, &out2, &count2) == Fatal);
        sequence.commands = saved;
        Require(sceFontGraphicsSequenceGetAgcCommandsForJump(seq, &out, &count, &out2, &count2) == 0);
        Require(out == firstData && count == 8 && out2 == secondData && count2 == 3);
        Require(g_errorCount == before);
    }
    {
        void* out = reinterpret_cast<void*>(0xdead);
        std::uint32_t count = 0xcccc;
        const int before = g_errorCount;
        Require(sceFontGraphicsSequenceGetAgcCommandsForSubmit(nullptr, &out, &count) == InvalidSequence);
        Require(out == nullptr && count == 0);
        Require(sceFontGraphicsSequenceGetAgcCommandsForSubmit(seq, nullptr, &count) == InvalidParameter);
        Require(count == 0);
        sequence.tag = 0xf81;
        Require(sceFontGraphicsSequenceGetAgcCommandsForSubmit(seq, nullptr, nullptr) == InvalidSequence);
        sequence.tag = 0xf80;
        sequence.lock = 1;
        Require(sceFontGraphicsSequenceGetAgcCommandsForSubmit(seq, &out, &count) == SequenceLocked);
        sequence.lock = 0;
        Require(sceFontGraphicsSequenceGetAgcCommandsForSubmit(seq, &out, &count) == 0);
        Require(out == firstData && count == 0x20);
        Require(g_errorCount == before);
    }
}

}

int main() {
    sceFontSetGraphicsErrorCallback(RecordError);
    AgcSelect();
    AgcDrawupImage();
    AgcDrawupPattern();
    AgcSequences();
    sceFontSetGraphicsErrorCallback(nullptr);
}
