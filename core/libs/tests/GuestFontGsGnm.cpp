#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libSceFont/libSceFont.hpp"
#include "prx/libSceFontGs/libSceFontGs.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

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

struct TestGnmSurface {
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

static_assert(offsetof(TestGnmSurface, tag) == 0x0d);
static_assert(offsetof(TestGnmSurface, mode) == 0x0f);
static_assert(offsetof(TestGnmSurface, busy) == 0x1c);
static_assert(offsetof(TestGnmSurface, fillObject) == 0x28);
static_assert(offsetof(TestGnmSurface, initialized) == 0x38);

struct TestGnmCommands {
    std::uint8_t reserved[0x28];
    const void* first;
    std::uint32_t firstSize;
    std::uint32_t secondCount;
    const void* second;
};

static_assert(offsetof(TestGnmCommands, first) == 0x28);
static_assert(offsetof(TestGnmCommands, firstSize) == 0x30);
static_assert(offsetof(TestGnmCommands, secondCount) == 0x34);
static_assert(offsetof(TestGnmCommands, second) == 0x38);

struct TestGnmSequence {
    std::uint16_t tag;
    std::uint8_t reserved2[0x19];
    std::uint8_t lock;
    std::uint8_t reserved1c[0xc];
    TestGnmCommands* commands;
};

static_assert(offsetof(TestGnmSequence, lock) == 0x1b);
static_assert(offsetof(TestGnmSequence, commands) == 0x28);

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
    TestGnmSurface surface{};

    DrawupFixture() {
        scePthreadMutexInit(&mutex, nullptr, "gnm-drawup");
        nodes[0] = TotalSlots << 1;
        pool.mutex = &mutex;
        pool.entryCount = 1;
        pool.entries[0] = {memory, 0, 0x80, TotalSlots, 0, nodes, nodes, 0};
        owner.pool = &pool;
        object.owner = &owner;
        frame.tag = 0xf53;
        frame.object = &object;
        surface.head = &object;
        surface.tag = 0x48;
        surface.mode = 0x22;
        surface.fillObject = descriptor;
        surface.initialized = 1;
        for (std::size_t i = 0; i < sizeof(descriptor); ++i) descriptor[i] = static_cast<std::uint8_t>(i);
    }

    ~DrawupFixture() { scePthreadMutexDestroy(&mutex); }
};

void GnmSelect() {
    Require(sceFontSelectGraphicsGnm(1) == nullptr);
    Require(sceFontSelectGraphicsGnm(0xffffffff) == nullptr);
}

void GnmDrawupImage() {
    {
        void* out = reinterpret_cast<void*>(0xdead);
        const int before = g_errorCount;
        Require(sceFontGraphicsGnmDrawupFillTextureImage(nullptr, nullptr, &out, 0.0f, 0.0f, 8.0f, 8.0f) == InvalidParameter);
        Require(out == nullptr);
        Require(g_errorCount == before);
    }
    {
        DrawupFixture f;
        auto* frame = reinterpret_cast<FontGraphicsFrame*>(&f.frame);
        const int before = g_errorCount;
        Require(sceFontGraphicsGnmDrawupFillTextureImage(frame, &f.surface, nullptr, 0.0f, 0.0f, 8.0f, 8.0f) == InvalidParameter);
        Require(g_errorCount == before);
    }
    {
        DrawupFixture f;
        auto* frame = reinterpret_cast<FontGraphicsFrame*>(&f.frame);
        void* out = reinterpret_cast<void*>(0xdead);
        const int before = g_errorCount;
        f.surface.tag = 0x57;
        Require(sceFontGraphicsGnmDrawupFillTextureImage(frame, &f.surface, &out, 0.0f, 0.0f, 8.0f, 8.0f) == InvalidParameter);
        Require(out == nullptr);
        f.surface.tag = 0x48;
        f.surface.head = nullptr;
        Require(sceFontGraphicsGnmDrawupFillTextureImage(frame, &f.surface, &out, 0.0f, 0.0f, 8.0f, 8.0f) == InvalidParameter);
        f.surface.head = &f.object;
        f.surface.initialized = 0;
        Require(sceFontGraphicsGnmDrawupFillTextureImage(frame, &f.surface, &out, 0.0f, 0.0f, 8.0f, 8.0f) == InvalidParameter);
        f.surface.initialized = 1;
        f.surface.busy = 1;
        Require(sceFontGraphicsGnmDrawupFillTextureImage(frame, &f.surface, &out, 0.0f, 0.0f, 8.0f, 8.0f) == InvalidParameter);
        f.surface.busy = 0;
        Require(sceFontGraphicsGnmDrawupFillTextureImage(frame, &f.surface, &out, -1.0f, 0.0f, 8.0f, 8.0f) == InvalidParameter);
        Require(sceFontGraphicsGnmDrawupFillTextureImage(frame, &f.surface, &out, 0.0f, -1.0f, 8.0f, 8.0f) == InvalidParameter);
        const float nan = std::strtof("nan", nullptr);
        Require(!(nan >= 0.0f));
        Require(sceFontGraphicsGnmDrawupFillTextureImage(frame, &f.surface, &out, nan, 0.0f, 8.0f, 8.0f) == InvalidParameter);
        Require(sceFontGraphicsGnmDrawupFillTextureImage(frame, &f.surface, &out, 0.0f, nan, 8.0f, 8.0f) == InvalidParameter);
        Require(sceFontGraphicsGnmDrawupFillTextureImage(frame, &f.surface, &out, 0.0f, 0.0f, nan, 8.0f) == InvalidParameter);
        Require(sceFontGraphicsGnmDrawupFillTextureImage(frame, &f.surface, &out, 0.0f, 0.0f, 8.0f, nan) == InvalidParameter);
        Require(sceFontGraphicsGnmDrawupFillTextureImage(frame, &f.surface, &out, 16830.0f, 0.0f, 8.0f, 8.0f) == InvalidParameter);
        Require(sceFontGraphicsGnmDrawupFillTextureImage(frame, &f.surface, &out, 0.0f, 0.0f, -4.0f, 8.0f) == InvalidParameter);
        Require(sceFontGraphicsGnmDrawupFillTextureImage(frame, &f.surface, &out, 0.0f, 0.0f, 8.0f, -4.0f) == InvalidParameter);
        Require(sceFontGraphicsGnmDrawupFillTextureImage(frame, &f.surface, &out, 0.0f, 0.0f, 16834.0f, 16834.0f) == 0);
        Require(out != nullptr);
        const auto* firstRecord = static_cast<const TestDrawupRecord*>(out);
        Require(f.object.head == firstRecord && f.object.tail == firstRecord);
        out = reinterpret_cast<void*>(0xdead);
        Require(sceFontGraphicsGnmDrawupFillTextureImage(frame, &f.surface, &out, 0.0f, 0.0f, 8.0f, 8.0f) == 0);
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
        Require(sceFontGraphicsGnmDrawupFillTextureImage(frame, &f.surface, &out, 0.0f, 0.0f, 8.0f, 8.0f) == InvalidObject);
        Require(out == nullptr);
        Require(f.frame.error == InvalidObject);
        Require(g_lastError == InvalidObject);
        g_lastError = 0;
    }
}

void GnmDrawupPattern() {
    {
        DrawupFixture f;
        auto* frame = reinterpret_cast<FontGraphicsFrame*>(&f.frame);
        const int before = g_errorCount;
        Require(sceFontGraphicsGnmDrawupFillTexturePattern(frame, &f.surface, 1, 2, 3, 4, 5, 6, nullptr) == InvalidParameter);
        f.surface.initialized = 0;
        void* out = reinterpret_cast<void*>(0xdead);
        Require(sceFontGraphicsGnmDrawupFillTexturePattern(frame, &f.surface, 1, 2, 3, 4, 5, 6, &out) == InvalidParameter);
        Require(out == nullptr);
        Require(g_errorCount == before);
        f.surface.initialized = 1;
        Require(sceFontGraphicsGnmDrawupFillTexturePattern(frame, &f.surface, 1, 2, 3, 4, 5, 6, &out) == 0);
        Require(out != nullptr);
        const auto* record = static_cast<const TestDrawupRecord*>(out);
        Require(record->state == 1);
        Require(record->mode == 0x22);
    }
}

void GnmSequences() {
    std::uint32_t firstData[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    std::uint32_t secondData[3] = {9, 10, 11};
    TestGnmCommands commands{};
    commands.first = firstData;
    commands.firstSize = 0x20;
    commands.secondCount = 3;
    commands.second = secondData;
    TestGnmSequence sequence{};
    sequence.tag = 0xf80;
    sequence.commands = &commands;
    auto* seq = reinterpret_cast<FontGraphicsCanvasSequence*>(&sequence);
    {
        void* out = reinterpret_cast<void*>(0xdead);
        void* out2 = reinterpret_cast<void*>(0xbeef);
        std::uint32_t count = 0xcccc;
        std::uint32_t count2 = 0xdddd;
        const int before = g_errorCount;
        Require(sceFontGraphicsSequenceGetGnmCommandsForChain(nullptr, &out, &count, &out2, &count2) == InvalidSequence);
        Require(out == nullptr && count == 0 && out2 == nullptr && count2 == 0);
        Require(sceFontGraphicsSequenceGetGnmCommandsForChain(seq, &out, nullptr, &out2, &count2) == InvalidParameter);
        Require(out == nullptr && out2 == nullptr && count2 == 0);
        Require(g_errorCount == before + 2 && g_lastError == InvalidParameter);
        sequence.tag = 0xf81;
        Require(sceFontGraphicsSequenceGetGnmCommandsForChain(seq, &out, &count, &out2, &count2) == InvalidSequence);
        Require(sceFontGraphicsSequenceGetGnmCommandsForChain(seq, nullptr, nullptr, nullptr, nullptr) == InvalidSequence);
        sequence.tag = 0xf80;
        sequence.lock = 1;
        Require(sceFontGraphicsSequenceGetGnmCommandsForChain(seq, &out, &count, &out2, &count2) == SequenceLocked);
        sequence.lock = 0;
        auto* saved = sequence.commands;
        sequence.commands = nullptr;
        Require(sceFontGraphicsSequenceGetGnmCommandsForChain(seq, &out, &count, &out2, &count2) == Fatal);
        sequence.commands = saved;
        Require(sceFontGraphicsSequenceGetGnmCommandsForChain(seq, &out, &count, &out2, &count2) == 0);
        Require(out == firstData && count == 8 && out2 == secondData && count2 == 3);
        Require(g_errorCount == before + 6 && g_lastError == Fatal);
    }
    {
        void* out = reinterpret_cast<void*>(0xdead);
        std::uint32_t count = 0xcccc;
        const int before = g_errorCount;
        Require(sceFontGraphicsSequenceGetGnmCommandsForSubmit(nullptr, &out, &count) == InvalidSequence);
        Require(out == nullptr && count == 0);
        Require(sceFontGraphicsSequenceGetGnmCommandsForSubmit(seq, nullptr, &count) == InvalidParameter);
        Require(count == 0);
        sequence.tag = 0xf81;
        Require(sceFontGraphicsSequenceGetGnmCommandsForSubmit(seq, nullptr, nullptr) == InvalidSequence);
        sequence.tag = 0xf80;
        sequence.lock = 1;
        Require(sceFontGraphicsSequenceGetGnmCommandsForSubmit(seq, &out, &count) == SequenceLocked);
        sequence.lock = 0;
        Require(sceFontGraphicsSequenceGetGnmCommandsForSubmit(seq, &out, &count) == 0);
        Require(out == firstData && count == 0x20);
        Require(g_errorCount == before + 4 && g_lastError == SequenceLocked);
    }
}

constexpr int SurfaceConfigInvalid = static_cast<int>(0x8046005c);

struct SurfaceCase {
    const char* name;
    std::uint64_t base;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t tileMode;
    std::uint32_t format[6];
    std::uint8_t configTag;
    std::uint8_t surfaceTag;
    int result;
    std::uint32_t texture[2][16];
};

const SurfaceCase SurfaceCases[] = {
    {"8-bit alpha, linear aligned", 0x100000, 64, 32, 8, {1, 0, 0, 0, 0, 4}, 0x44, 0x10, 0,
     {{0x00001000, 0x00700007, 0x0000001f, 0, 0x00009804, 0x00000108, 0, 0x01234567, 0, 0x09876543, 0, 0x55667788, 0x11223344, 0, 0, 0x00200040},
      {0x00001000, 0x00700007, 0x0000001f, 0, 0x80009804, 0x00000108, 8, 0x01234567, 0, 0x09876543, 0, 0x55667788, 0x11223344, 0, 0, 0x00200040}}},
    {"RGBA8, 1D thin", 0x2345600, 100, 50, 0xd, {10, 0, 4, 5, 6, 7}, 0x44, 0x40, 0,
     {{0x00023456, 0x00c0000c, 0x0000005a, 0, 0x00008028, 0x000001ad, 0, 0x01234567, 0, 0x09876543, 0, 0x55667788, 0x11223344, 0, 0, 0x00320064},
      {0x00023456, 0x00c0000c, 0x0000005a, 0, 0x80008028, 0x000001ad, 8, 0x01234567, 0, 0x09876543, 0, 0x55667788, 0x11223344, 0, 0, 0x00320064}}},
    {"RGBA32F, 2D thin", 0x7fffabcd00, 1920, 1080, 0xa, {14, 7, 4, 5, 6, 7}, 0x44, 0x4f, 0,
     {{0x7fffabc5, 0x0ef000ef, 0x00007f7f, 0, 0x00040738, 0x0000014a, 0, 0x01234567, 0, 0x09876543, 0, 0x55667788, 0x11223344, 0, 0, 0x04380780},
      {0x7fffab8d, 0x0ef000ef, 0x00007f7f, 0, 0x80040738, 0x0000014a, 8, 0x01234567, 0, 0x09876543, 0, 0x55667788, 0x11223344, 0, 0, 0x04380780}}},
    {"RG8 sRGB, 2D depth mode", 0x40000fff00, 300, 200, 2, {3, 9, 4, 1, 2, 5}, 0x44, 0x20, 0,
     {{0x40000f87, 0x02f0002f, 0x000005ff, 0, 0x0000980c, 0x00000042, 0, 0x01234567, 0, 0x09876543, 0, 0x55667788, 0x11223344, 0, 0, 0x00c8012c},
      {0x40000f8f, 0x03f0003f, 0x000007ff, 0, 0x8000980c, 0x00000042, 8, 0x01234567, 0, 0x09876543, 0, 0x55667788, 0x11223344, 0, 0, 0x00c8012c}}},
    {"RGBA16, PRT, config tag 0x40", 0x1234567800, 512, 512, 0x16, {12, 0, 6, 5, 4, 7}, 0x40, 0x4e, 0,
     {{0x12345600, 0x03f0003f, 0x00000fff, 0, 0x00008830, 0x00000210, 0, 0x01234567, 0, 0x09876543, 0, 0x55667788, 0x11223344, 0, 0, 0x02000200},
      {0x12345608, 0x03f0003f, 0x00000fff, 0, 0x80008830, 0x00000210, 8, 0x01234567, 0, 0x09876543, 0, 0x55667788, 0x11223344, 0, 0, 0x02000200}}},
    {"tile mode out of range", 0x100000, 64, 64, 0x20, {9, 0, 4, 5, 6, 7}, 0x44, 0, SurfaceConfigInvalid,
     {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0}}},
};

std::uint32_t Get32(const std::uint8_t* bytes, std::size_t offset) {
    std::uint32_t value;
    std::memcpy(&value, bytes + offset, sizeof(value));
    return value;
}

void Put(std::uint8_t* bytes, std::size_t offset, const void* value, std::size_t size) { std::memcpy(bytes + offset, value, size); }

void Put32(std::uint8_t* bytes, std::size_t offset, std::uint32_t value) { Put(bytes, offset, &value, 4); }

void Put64(std::uint8_t* bytes, std::size_t offset, std::uint64_t value) { Put(bytes, offset, &value, 8); }

void SurfaceConfig(std::uint8_t* config, const SurfaceCase& test) {
    std::memset(config, 0, 0x100);
    Put64(config, 0x00, test.base);
    Put32(config, 0x08, 0x40);
    config[0x0c] = 3;
    config[0x0d] = test.configTag;
    Put32(config, 0x10, test.width);
    Put32(config, 0x14, test.height);
    Put32(config, 0x20, test.tileMode);
    Put32(config, 0x24, 1);
    Put32(config, 0x30, 0x1234);
    Put32(config, 0x34, 0x55667788);
    Put(config, 0x40, test.format, sizeof(test.format));
    Put64(config, 0x58, 0xabcdef0123456700ULL);
    Put64(config, 0x68, 0x1122334455667788ULL);
    Put64(config, 0x78, 0x0000000987654300ULL);
}

void GnmSurfaceValidation() {
    std::uint8_t config[0x100];
    std::uint8_t surface[0x100];
    SurfaceConfig(config, SurfaceCases[0]);
    Require(sceFontGraphicsGnmSurfaceInit(nullptr, config) == InvalidParameter);
    const auto expectFailure = [&](std::size_t offset, std::uint32_t value, int result, bool cleared) {
        std::uint8_t changed[0x100];
        std::memcpy(changed, config, sizeof(changed));
        Put32(changed, offset, value);
        std::memset(surface, 0xcc, sizeof(surface));
        Require(sceFontGraphicsGnmSurfaceInit(surface, changed) == result);
        for (std::size_t i = 0; i < sizeof(surface); ++i)
            Require(surface[i] == (cleared || i == 0x0e || i == 0x0f ? 0 : 0xcc));
    };
    expectFailure(0x0c, 0x4103, InvalidParameter, false);
    expectFailure(0x0c, 0x0003, InvalidParameter, false);
    expectFailure(0x24, 0, SurfaceConfigInvalid, true);
    expectFailure(0x18, 1, SurfaceConfigInvalid, true);
    expectFailure(0x1c, 1, SurfaceConfigInvalid, true);
    expectFailure(0x08, 0x80000000, SurfaceConfigInvalid, true);
    expectFailure(0x10, 0x80000000, SurfaceConfigInvalid, true);
    expectFailure(0x14, 0xffffffff, SurfaceConfigInvalid, true);
    expectFailure(0x40, 2, SurfaceConfigInvalid, true);
    expectFailure(0x40, 0, SurfaceConfigInvalid, true);
    expectFailure(0x44, 1, SurfaceConfigInvalid, true);
    expectFailure(0x54, 5, SurfaceConfigInvalid, true);
    std::uint8_t tagged[0x100];
    std::memcpy(tagged, config, sizeof(tagged));
    tagged[0x0d] = 0x40;
    Put32(tagged, 0x24, 2);
    std::memset(surface, 0xcc, sizeof(surface));
    Require(sceFontGraphicsGnmSurfaceInit(surface, tagged) == SurfaceConfigInvalid);
    for (std::size_t i = 0; i < sizeof(surface); ++i) Require(surface[i] == (i == 0x0e || i == 0x0f ? 0 : 0xcc));
}

void GnmSurfaceInitCases(bool neo) {
    for (const auto& test : SurfaceCases) {
        std::uint8_t config[0x100];
        std::uint8_t surface[0x100];
        SurfaceConfig(config, test);
        std::memset(surface, 0xcc, sizeof(surface));
        Require(sceFontGraphicsGnmSurfaceInit(surface, config) == test.result);
        for (std::size_t i = 0; i < 16; ++i) Require(Get32(surface, 0x40 + 4 * i) == test.texture[neo][i]);
        const std::uint8_t rest = test.configTag == 0x44 ? 0 : 0xcc;
        for (std::size_t i = 0x80; i < sizeof(surface); ++i) Require(surface[i] == rest);
        if (test.result != 0) {
            for (std::size_t i = 0; i < 0x40; ++i) Require(surface[i] == 0);
            continue;
        }
        const std::uint32_t header[16] = {static_cast<std::uint32_t>(test.base), static_cast<std::uint32_t>(test.base >> 32), 0x40,
                                          0x47e00003u | static_cast<std::uint32_t>(test.surfaceTag) << 8, test.width, test.height, 0, 0,
                                          test.width, test.height, 0x55667788, 0x00011234, 0, 0x00440100, 0, test.height << 16 | test.width};
        for (std::size_t i = 0; i < 16; ++i) Require(Get32(surface, 4 * i) == header[i]);
    }
}

void WriteParamSfo(const std::filesystem::path& directory, std::uint32_t attribute) {
    const auto metadata = directory / "app0" / "sce_sys";
    std::filesystem::create_directories(metadata);
    std::uint8_t sfo[112] = {};
    std::memcpy(sfo, "\0PSF", 4);
    Put32(sfo, 4, 0x0101);
    Put32(sfo, 8, 68);
    Put32(sfo, 12, 96);
    Put32(sfo, 16, 3);
    const std::uint32_t entries[3][4] = {{0x04040000, 4, 4, 0}, {0x0204000a, 2, 2, 4}, {0x02040010, 10, 10, 6}};
    Put(sfo, 20, entries, sizeof(entries));
    std::memcpy(sfo + 68, "ATTRIBUTE\0TITLE\0TITLE_ID", 25);
    Put32(sfo, 96, attribute);
    std::memcpy(sfo + 100, "T\0CUSA00000", 11);
    std::ofstream file(metadata / "param.sfo", std::ios::binary);
    file.write(reinterpret_cast<const char*>(sfo), sizeof(sfo));
    Require(static_cast<bool>(file));
}

int RunSelf(const char* program, const char* mode, const std::filesystem::path& directory) {
    const std::string arguments = "\"" + std::string(program) + "\" " + mode + " \"" + directory.string() + "\"";
#ifdef _WIN32
    return std::system(("\"" + arguments + "\"").c_str());
#else
    return std::system(arguments.c_str());
#endif
}

void GnmSurfaceModes(const char* program) {
    const auto root = std::filesystem::temp_directory_path() / ("guest_font_gs_gnm_" + std::to_string(getpid_nid_postfix()));
    std::filesystem::remove_all(root);
    WriteParamSfo(root / "base", 0);
    WriteParamSfo(root / "neo", 0x00800000);
    const int base = RunSelf(program, "--surface-base", root / "base");
    const int neo = RunSelf(program, "--surface-neo", root / "neo");
    std::filesystem::remove_all(root);
    Require(base == 0 && neo == 0);
}

}

void ReportError() {
    sceFontSetGraphicsErrorCallback(nullptr);
    Require(sceFontGraphicsReportError(InvalidParameter) == static_cast<int>(0x80460020));
    sceFontSetGraphicsErrorCallback(RecordError);
    const int before = g_errorCount;
    Require(sceFontGraphicsReportError(SequenceLocked) == 0);
    Require(g_errorCount == before + 1 && g_lastError == SequenceLocked);
}

int main(int argc, char** argv) {
    if (argc == 3) {
        std::filesystem::current_path(argv[2]);
        GnmSurfaceInitCases(std::string(argv[1]) == "--surface-neo");
        return 0;
    }
    ReportError();
    GnmSelect();
    GnmDrawupImage();
    GnmDrawupPattern();
    GnmSequences();
    GnmSurfaceValidation();
    GnmSurfaceModes(argv[0]);
    sceFontSetGraphicsErrorCallback(nullptr);
}
