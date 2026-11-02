#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libSceFont/libSceFont.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <type_traits>

namespace {

using FontDrawupSignature = int (APS5_VABI *)(FontGraphicsFrame*, const void*, std::uint32_t, void**, float, float, float, float);
using FontDrawupPatternSignature = int (APS5_VABI *)(FontGraphicsFrame*, const void*, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, void**);
using FontDrawupFlatSignature = int (APS5_VABI *)(FontGraphicsFrame*, const void*, void**);
using FontDrawupGradientSignature = int (APS5_VABI *)(FontGraphicsFrame*, std::uint32_t, const void*, const void*, void**, float, float, float, float, float, float);
using FontGradientSegmentSignature = int (APS5_VABI *)(FontGraphicsFrame*, void*, float, const void*);
static_assert(std::is_same_v<decltype(&sceFontGraphicsDrawupFillTextureImageObject), FontDrawupSignature>);
static_assert(std::is_same_v<decltype(&sceFontGraphicsDrawupFillTexturePatternObject), FontDrawupPatternSignature>);
static_assert(std::is_same_v<decltype(&sceFontGraphicsDrawupFillFlatColor), FontDrawupFlatSignature>);
static_assert(std::is_same_v<decltype(&sceFontGraphicsDrawupFillLinearGradient), FontDrawupGradientSignature>);
static_assert(std::is_same_v<decltype(&sceFontGraphicsDrawupFillRadialGradient), FontDrawupGradientSignature>);
static_assert(std::is_same_v<decltype(&sceFontGraphicsAppendGradientSegment), FontGradientSegmentSignature>);

constexpr int Uninitialized = static_cast<int>(0x80460001);
constexpr int InvalidParameter = static_cast<int>(0x80460002);
constexpr int InvalidGradient = static_cast<int>(0x80460020);
constexpr int Sealed = static_cast<int>(0x8046005c);
constexpr int InvalidObject = static_cast<int>(0x80460084);
constexpr int NoSegmentSpace = static_cast<int>(0x804600a0);
constexpr int NoRecordSpace = static_cast<int>(0x804600a2);
constexpr int Sticky = static_cast<int>(0x804600a9);
constexpr int InvalidFill = static_cast<int>(0x80460086);
constexpr int InvalidRegion = static_cast<int>(0x80460087);
constexpr int InvalidFillPlot = static_cast<int>(0x80460088);
constexpr int InvalidFillRates = static_cast<int>(0x80460089);
constexpr float NaN = std::numeric_limits<float>::quiet_NaN();

int g_lastError = 0;
int g_errorCount = 0;

void APS5_VABI RecordError(int error) {
    g_lastError = error;
    ++g_errorCount;
}

void Require(bool value) { if (!value) std::abort(); }

template <typename T>
void Fill(T& object, std::uint8_t value) {
    std::memset(&object, value, sizeof(object));
}

template <typename T>
bool AllBytes(const T& object, std::size_t from, std::size_t to, std::uint8_t value) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&object);
    for (std::size_t i = from; i < to; ++i) if (bytes[i] != value) return false;
    return true;
}

int g_expectedErrors = 0;

void RequireError(int result, int error) {
    Require(result == error);
    Require(g_lastError == error);
    g_lastError = 0;
    ++g_expectedErrors;
}

const FontGraphicsFill* Sentinel(std::intptr_t value) {
    return reinterpret_cast<const FontGraphicsFill*>(value);
}

void Regions() {
    FontGraphicsRegion region;
    Fill(region, 0xa5);
    Require(sceFontGraphicsRegionInit(&region, 1.0f, 2.0f, 3.0f, 4.0f) == 0);
    Require(region.x == 1.0f && region.y == 2.0f && region.width == 3.0f && region.height == 4.0f);
    Require(region.flags == 0xf && region.tag == 0xf60 && region.outerRadius == 0.0f && region.innerRadius == 0.0f);
    Require(sceFontGraphicsRegionInit(&region, 0.0f, 0.0f, 0.0f, 0.0f) == 0);
    Require(sceFontGraphicsRegionInit(&region, 0.0f, 0.0f, NaN, NaN) == 0);
    Fill(region, 0xa5);
    RequireError(sceFontGraphicsRegionInit(&region, 0.0f, 0.0f, -1.0f, 1.0f), InvalidParameter);
    RequireError(sceFontGraphicsRegionInit(&region, 0.0f, 0.0f, 1.0f, -1.0f), InvalidParameter);
    RequireError(sceFontGraphicsRegionInit(nullptr, 0.0f, 0.0f, 1.0f, 1.0f), InvalidParameter);
    Require(AllBytes(region, 0, sizeof(region), 0xa5));

    Require(sceFontGraphicsRegionInitCircular(&region, 10.0f, 20.0f, 4.0f, 1.0f) == 0);
    Require(region.x == 6.0f && region.y == 16.0f && region.width == 8.0f && region.height == 8.0f);
    Require(region.flags == 0xff && region.tag == 0xf60 && region.outerRadius == 4.0f && region.innerRadius == 1.0f);
    Require(sceFontGraphicsRegionInitCircular(&region, 0.0f, 0.0f, 2.0f, 2.0f) == 0);
    RequireError(sceFontGraphicsRegionInitCircular(&region, 0.0f, 0.0f, 1.0f, 2.0f), InvalidParameter);
    RequireError(sceFontGraphicsRegionInitCircular(&region, 0.0f, 0.0f, 0.0f, 0.0f), InvalidParameter);
    RequireError(sceFontGraphicsRegionInitCircular(&region, 0.0f, 0.0f, 1.0f, 0.0f), InvalidParameter);
    RequireError(sceFontGraphicsRegionInitCircular(nullptr, 0.0f, 0.0f, 2.0f, 1.0f), InvalidParameter);

    Require(sceFontGraphicsRegionInitRoundish(&region, 1.0f, 2.0f, 10.0f, 8.0f, 4.0f, 2.0f, 0x5) == 0);
    Require(region.width == 10.0f && region.height == 8.0f && region.flags == 0x5 && region.tag == 0xf60);
    Require(region.outerRadius == 4.0f && region.innerRadius == 2.0f);
    Require(sceFontGraphicsRegionInitRoundish(&region, 0.0f, 0.0f, 10.0f, 8.0f, 0.0f, 9.0f, 0xff) == 0);
    RequireError(sceFontGraphicsRegionInitRoundish(&region, 0.0f, 0.0f, 10.0f, 8.0f, 4.5f, 0.0f, 0), InvalidParameter);
    RequireError(sceFontGraphicsRegionInitRoundish(&region, 0.0f, 0.0f, 10.0f, 8.0f, 2.0f, 3.0f, 0), InvalidParameter);
    RequireError(sceFontGraphicsRegionInitRoundish(&region, 0.0f, 0.0f, 10.0f, 8.0f, 2.0f, 1.0f, 0x100), InvalidParameter);
    RequireError(sceFontGraphicsRegionInitRoundish(&region, 0.0f, 0.0f, 0.0f, 8.0f, 0.0f, 0.0f, 0), InvalidParameter);
    RequireError(sceFontGraphicsRegionInitRoundish(&region, 0.0f, 0.0f, 10.0f, 8.0f, -1.0f, 0.0f, 0), InvalidParameter);
}

void FillMethods() {
    FontGraphicsFillMethod method;
    Fill(method, 0xa5);
    Require(sceFontGraphicsFillMethodInit(&method, 0x50, 12) == 0);
    Require(method.mode == 0x50 && method.type == 12 && method.flags == 0xf && method.tag == 0xf01b);
    Require(AllBytes(method, 16, 32, 0));
    for (std::int32_t mode : {0x00, 0x10, 0x20, 0x30, 0x40}) Require(sceFontGraphicsFillMethodInit(&method, mode, 1) == 0);
    for (std::int32_t type = 1; type <= 7; ++type) Require(sceFontGraphicsFillMethodInit(&method, 0, type) == 0);
    for (std::int32_t mode : {0x01, 0x08, 0x60, -0x10}) RequireError(sceFontGraphicsFillMethodInit(&method, mode, 1), InvalidParameter);
    for (std::int32_t type : {0, 8, 11, 13, -1}) RequireError(sceFontGraphicsFillMethodInit(&method, 0, type), InvalidParameter);
    RequireError(sceFontGraphicsFillMethodInit(nullptr, 0, 1), InvalidParameter);
}

void FillPlots() {
    FontGraphicsRegion region;
    Require(sceFontGraphicsRegionInitRoundish(&region, 1.0f, 2.0f, 10.0f, 8.0f, 4.0f, 2.0f, 0x5) == 0);
    FontGraphicsFillPlot plot;
    Fill(plot, 0xa5);
    Require(sceFontGraphicsFillPlotInit(&plot, &region) == 0);
    Require(plot.mapping[0] == 1.0f && plot.mapping[1] == 2.0f && plot.mapping[2] == 11.0f && plot.mapping[3] == 10.0f);
    Require(plot.region == &region && plot.tag == 0xf01c && plot.reserved26 == 0 && plot.layout == 0xf77 && plot.flags == 1);
    Require(AllBytes(plot, 31, 64, 0));

    Fill(plot, 0xa5);
    Require(sceFontGraphicsFillPlotInit(&plot, nullptr) == 0);
    Require(plot.region == nullptr && plot.tag == 0xf01c && plot.layout == 0 && plot.flags == 2);
    Require(AllBytes(plot, 0, 16, 0));
    const auto* whole = reinterpret_cast<const FontGraphicsRegion*>(std::intptr_t{-1});
    Require(sceFontGraphicsFillPlotInit(&plot, whole) == 0);
    Require(plot.region == whole && plot.layout == 0xf77 && plot.flags == 6);

    FontGraphicsRegion bad = region;
    bad.tag = 0;
    Fill(plot, 0xa5);
    RequireError(sceFontGraphicsFillPlotInit(&plot, &bad), InvalidRegion);
    Require(plot.tag == 0 && AllBytes(plot, 0, 24, 0xa5) && AllBytes(plot, 26, 64, 0xa5));
    bad = region;
    bad.outerRadius = NaN;
    RequireError(sceFontGraphicsFillPlotInit(&plot, &bad), InvalidRegion);
    bad = region;
    bad.outerRadius = 4.5f;
    RequireError(sceFontGraphicsFillPlotInit(&plot, &bad), InvalidRegion);
    bad = region;
    bad.outerRadius = 0.0f;
    bad.innerRadius = 4.0f;
    Require(sceFontGraphicsFillPlotInit(&plot, &bad) == 0);
    bad.innerRadius = 4.5f;
    RequireError(sceFontGraphicsFillPlotInit(&plot, &bad), InvalidRegion);
    RequireError(sceFontGraphicsFillPlotInit(nullptr, &region), InvalidFillPlot);

    Require(sceFontGraphicsFillPlotInit(&plot, &region) == 0);
    Require(sceFontGraphicsFillPlotSetLayout(&plot, 0x400, 0x24) == 0);
    Require(plot.layout == 0x424 && plot.flags == 9);
    Require(sceFontGraphicsFillPlotSetLayout(&plot, 0, 0) == 0);
    Require(plot.layout == 0 && plot.flags == 8);
    Require(sceFontGraphicsFillPlotSetLayout(&plot, 0x170, 0x1100) == 0);
    Require(plot.layout == 0x1170);
    RequireError(sceFontGraphicsFillPlotSetLayout(&plot, 0x171, 0), InvalidParameter);
    RequireError(sceFontGraphicsFillPlotSetLayout(&plot, 0, 0x3), InvalidParameter);
    RequireError(sceFontGraphicsFillPlotSetLayout(&plot, 0, 0x50), InvalidParameter);
    Require(sceFontGraphicsFillPlotSetMapping(&plot, 5.0f, 6.0f, 7.0f, NaN) == 0);
    Require(plot.mapping[0] == 5.0f && plot.mapping[2] == 7.0f && std::isnan(plot.mapping[3]) && plot.flags == 9);

    FontGraphicsFillPlot untagged{};
    RequireError(sceFontGraphicsFillPlotSetLayout(&untagged, 0, 0), InvalidFillPlot);
    RequireError(sceFontGraphicsFillPlotSetMapping(&untagged, 0.0f, 0.0f, 0.0f, 0.0f), InvalidFillPlot);
}

void FillRates() {
    FontGraphicsFillRates rates;
    Fill(rates, 0xa5);
    std::array<std::uint8_t, 8> fill{0x30};
    const auto* fillPointer = reinterpret_cast<const FontGraphicsFill*>(fill.data());
    Require(sceFontGraphicsFillRatesInit(&rates, fillPointer, 9) == 0);
    Require(rates.fill == fillPointer && rates.tag == 0xf01a && rates.effect == 9 && rates.layout == 0xf77 && rates.flags == 2);
    Require(AllBytes(rates, 0, 16, 0) && AllBytes(rates, 31, 64, 0));
    for (std::intptr_t value : {-1, 0, 1, 2}) Require(sceFontGraphicsFillRatesInit(&rates, Sentinel(value), 0) == 0);
    for (std::uint32_t effect : {0u, 1u, 2u, 3u, 4u, 5u, 8u, 9u, 16u}) {
        Require(sceFontGraphicsFillRatesInit(&rates, nullptr, effect) == 0);
        Require(rates.effect == effect);
    }

    Fill(rates, 0xa5);
    RequireError(sceFontGraphicsFillRatesInit(&rates, nullptr, 6), InvalidParameter);
    Require(AllBytes(rates, 0, sizeof(rates), 0));
    fill[0] = 0x31;
    RequireError(sceFontGraphicsFillRatesInit(&rates, fillPointer, 0), InvalidFill);
    fill[0] = 0x60;
    RequireError(sceFontGraphicsFillRatesInit(&rates, fillPointer, 0), InvalidFill);
    RequireError(sceFontGraphicsFillRatesInit(nullptr, nullptr, 0), InvalidParameter);

    fill[0] = 0x10;
    Require(sceFontGraphicsFillRatesInit(&rates, nullptr, 0) == 0);
    Require(sceFontGraphicsFillRatesSetFillEffect(&rates, fillPointer, 16) == 0);
    Require(rates.fill == fillPointer && rates.effect == 16 && rates.flags == 2 && rates.layout == 0xf77);
    RequireError(sceFontGraphicsFillRatesSetFillEffect(&rates, nullptr, 17), InvalidParameter);
    Require(rates.fill == fillPointer && rates.effect == 16);

    Require(sceFontGraphicsFillRatesSetLayout(&rates, 0xf77, 0x44) == 0);
    Require(rates.layout == 0 && rates.flags == 3);
    Require(sceFontGraphicsFillRatesSetLayout(&rates, 0, 0) == 0);
    Require(rates.layout == 0 && rates.flags == 2);
    RequireError(sceFontGraphicsFillRatesSetLayout(&rates, 0x801, 0), InvalidParameter);
    Require(sceFontGraphicsFillRatesSetMapping(&rates, 1.0f, 2.0f, 3.0f, 4.0f) == 0);
    Require(rates.mapping[0] == 1.0f && rates.mapping[3] == 4.0f && rates.flags == 1);

    FontGraphicsFillRates untagged{};
    RequireError(sceFontGraphicsFillRatesSetFillEffect(&untagged, nullptr, 0), InvalidFillRates);
    RequireError(sceFontGraphicsFillRatesSetLayout(&untagged, 0, 0), InvalidFillRates);
    RequireError(sceFontGraphicsFillRatesSetMapping(&untagged, 0.0f, 0.0f, 0.0f, 0.0f), InvalidFillRates);
}

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

struct TestSegmentResource;

struct TestDrawupObject {
    std::uint64_t reserved0;
    TestDrawupOwner* owner;
    TestSegmentResource* resource;
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

struct TestSegmentNode {
    std::uint32_t head;
    std::uint32_t next;
    std::uint32_t end;
};

struct TestSegmentArena {
    std::uint8_t* memory;
    std::uint32_t size;
    std::uint32_t slotSize;
    std::uint32_t total;
    std::uint32_t used;
    TestSegmentNode* nodes;
    TestSegmentNode* sentinel;
    TestSegmentNode* scan;
    TestSegmentNode spare;
    std::uint32_t reserved3c;
    PthreadMutex* mutex;
    std::uint32_t statUsed;
    std::uint32_t statMaximum;
};

struct TestSegmentResource {
    std::uint8_t reserved[0xc0];
    TestSegmentArena arena;
};

struct TestSegmentStop {
    std::uint8_t type;
    std::uint8_t reserved1;
    std::uint16_t reserved2;
    std::uint32_t epoch;
    TestSegmentStop* next;
    float position;
    std::uint32_t color;
};

static_assert(sizeof(TestDrawupRecord) == 0x80);
static_assert(sizeof(TestDrawupEntry) == 0x30);
static_assert(offsetof(TestDrawupPool, entries) == 0x18);
static_assert(offsetof(TestDrawupOwner, pool) == 0x160);
static_assert(offsetof(TestDrawupObject, head) == 0x28 && offsetof(TestDrawupObject, tail) == 0x30);
static_assert(offsetof(TestDrawupFrame, object) == 0x8 && offsetof(TestDrawupFrame, sequence) == 0x28);
static_assert(offsetof(TestDrawupFrame, unitSize) == 0x10 && offsetof(TestDrawupFrame, unitUsed) == 0x14);
static_assert(offsetof(TestDrawupFrame, unitHead) == 0x18 && offsetof(TestDrawupFrame, unit) == 0x20);
static_assert(sizeof(TestSegmentArena) == 0x50);
static_assert(offsetof(TestSegmentResource, arena) == 0xc0);
static_assert(offsetof(TestSegmentArena, mutex) == 0x40);
static_assert(sizeof(TestSegmentStop) == 0x18);

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

    DrawupFixture() {
        scePthreadMutexInit(&mutex, nullptr, "drawup");
        nodes[0] = TotalSlots << 1;
        pool.mutex = &mutex;
        pool.entryCount = 1;
        pool.entries[0] = {memory, 0, 0x80, TotalSlots, 0, nodes, nodes, 0};
        owner.pool = &pool;
        object.owner = &owner;
        frame.tag = 0xf53;
        frame.object = &object;
        for (std::size_t i = 0; i < sizeof(descriptor); ++i) descriptor[i] = static_cast<std::uint8_t>(i);
    }

    ~DrawupFixture() { scePthreadMutexDestroy(&mutex); }
};

void DrawupTextureRecords() {
    {
        DrawupFixture f;
        auto* frame = reinterpret_cast<FontGraphicsFrame*>(&f.frame);
        void* out = reinterpret_cast<void*>(0xdead);
        RequireError(sceFontGraphicsDrawupFillTextureImageObject(nullptr, f.descriptor, 0x100, &out, 0.0f, 0.0f, 8.0f, 8.0f), InvalidObject);
        Require(out == nullptr);
        f.frame.tag = 0;
        RequireError(sceFontGraphicsDrawupFillTextureImageObject(frame, f.descriptor, 0x100, &out, 0.0f, 0.0f, 8.0f, 8.0f), InvalidObject);
        Require(f.frame.error == InvalidObject);
        f.frame.tag = 0xf53;
        f.frame.error = -7;
        RequireError(sceFontGraphicsDrawupFillTextureImageObject(frame, f.descriptor, 0x100, &out, 0.0f, 0.0f, 8.0f, 8.0f), Sticky);
        Require(f.frame.error == Sticky);
        f.frame.error = 0;
        RequireError(sceFontGraphicsDrawupFillTextureImageObject(frame, f.descriptor, 0x100, nullptr, 0.0f, 0.0f, 8.0f, 8.0f), InvalidParameter);
        Require(f.frame.error == InvalidParameter);
        f.frame.error = 0;
        RequireError(sceFontGraphicsDrawupFillTextureImageObject(frame, f.descriptor, 0x100, &out, 0.0f, 0.0f, 0.0f, 8.0f), InvalidParameter);
        f.frame.error = 0;
        Require(sceFontGraphicsDrawupFillTextureImageObject(frame, f.descriptor, 0x100, &out, NaN, NaN, NaN, NaN) == 0);
        const auto* nanRecord = static_cast<TestDrawupRecord*>(out);
        const auto* nanQuad = reinterpret_cast<const float*>(nanRecord->params);
        Require(std::isnan(nanQuad[2]) && std::isnan(nanQuad[3]));
        f.frame.error = 0;
        f.frame.object = nullptr;
        RequireError(sceFontGraphicsDrawupFillTextureImageObject(frame, f.descriptor, 0x100, &out, 0.0f, 0.0f, 8.0f, 8.0f), Uninitialized);
        f.frame.error = 0;
        f.frame.object = &f.object;
        f.frame.sequence = 0xffffffffu;
        RequireError(sceFontGraphicsDrawupFillTextureImageObject(frame, f.descriptor, 0x100, &out, 0.0f, 0.0f, 8.0f, 8.0f), Sealed);
        Require(f.frame.error == Sealed);
        f.frame.error = 0;
        f.frame.sequence = 0;
        f.owner.pool = nullptr;
        RequireError(sceFontGraphicsDrawupFillTextureImageObject(frame, f.descriptor, 0x100, &out, 0.0f, 0.0f, 8.0f, 8.0f), NoRecordSpace);
        f.frame.error = 0;
        f.owner.pool = &f.pool;
        f.nodes[0] = 1;
        RequireError(sceFontGraphicsDrawupFillTextureImageObject(frame, f.descriptor, 0x100, &out, 0.0f, 0.0f, 8.0f, 8.0f), NoRecordSpace);
        f.nodes[0] = TotalSlots << 1;
        f.frame.error = 0;
    }
    {
        DrawupFixture f;
        auto* frame = reinterpret_cast<FontGraphicsFrame*>(&f.frame);
        void* out = nullptr;
        Require(sceFontGraphicsDrawupFillTextureImageObject(frame, f.descriptor, 0x100, &out, 1.5f, 2.5f, 8.0f, 4.0f) == 0);
        auto* record = static_cast<TestDrawupRecord*>(out);
        Require(record == reinterpret_cast<TestDrawupRecord*>(f.memory));
        Require(record->type == 0x40 && record->sizeTag == 0x80 && record->flags == 0);
        Require(record->sequence == 0 && record->next == nullptr && record->state == 0);
        Require(record->mode == 0x100 && record->payload == reinterpret_cast<std::uint8_t*>(record) + 0x80);
        const auto* quad = reinterpret_cast<const float*>(record->params);
        Require(quad[0] == 1.5f && quad[1] == 2.5f && quad[2] == 9.5f && quad[3] == 6.5f);
        Require(std::memcmp(record->payload, f.descriptor, 0x100) == 0);
        Require(f.object.head == record && f.object.tail == record);
        Require(f.frame.sequence == 1);
        Require(f.pool.used == 3 && f.pool.maximum == 3);
        Require((f.nodes[0] & 1) && (f.nodes[1] & 1) && (f.nodes[2] & 1) && !(f.nodes[3] & 1));
        Require(f.pool.entries[0].used == 3);
        void* out2 = nullptr;
        Require(sceFontGraphicsDrawupFillTextureImageObject(frame, f.descriptor, 0x80, &out2, -1.0f, -2.0f, 2.0f, 2.0f) == 0);
        auto* record2 = static_cast<TestDrawupRecord*>(out2);
        Require(record2 == reinterpret_cast<TestDrawupRecord*>(f.memory + 3 * 0x80));
        Require(record2->type == 0x40 && record2->sequence == 1 && record2->next == nullptr);
        Require(record->next == record2 && f.object.tail == record2 && f.object.head == record);
        Require(f.frame.sequence == 2 && f.pool.used == 5 && f.pool.maximum == 5);
        const auto* quad2 = reinterpret_cast<const float*>(record2->params);
        Require(quad2[0] == -1.0f && quad2[1] == -2.0f && quad2[2] == 1.0f && quad2[3] == 0.0f);
    }
    {
        DrawupFixture f;
        auto* frame = reinterpret_cast<FontGraphicsFrame*>(&f.frame);
        void* out = nullptr;
        Require(sceFontGraphicsDrawupFillTexturePatternObject(frame, f.descriptor, 0x200, 2, 0, 4, 1, 2, 8, &out) == 0);
        auto* record = static_cast<TestDrawupRecord*>(out);
        Require(record->type == 0x50 && record->sizeTag == 0x80 && record->mode == 0x200);
        Require(record->sequence == 0 && record->next == nullptr && record->state == 0);
        Require(record->params[0] == (0x400 | 0x3000) && record->params[1] == (4 | 0x20));
        auto* dims = reinterpret_cast<const std::uint8_t*>(&record->params[2]);
        Require(dims[0] == 3 && dims[1] == 7);
        Require(std::memcmp(record->payload, f.descriptor, 0x200) == 0);
        Require(f.object.head == record && f.object.tail == record && f.frame.sequence == 1);
        Require(f.pool.used == 5 && f.pool.entries[0].used == 5);
        RequireError(sceFontGraphicsDrawupFillTexturePatternObject(frame, f.descriptor, 0x100, 5, 0, 4, 5, 2, 8, &out), InvalidParameter);
        f.frame.error = 0;
        RequireError(sceFontGraphicsDrawupFillTexturePatternObject(frame, f.descriptor, 0x100, 0, 4, 4, 1, 4, 8, &out), InvalidParameter);
        f.frame.error = 0;
        RequireError(sceFontGraphicsDrawupFillTexturePatternObject(frame, f.descriptor, 0x100, 0, 0, 0, 1, 2, 8, &out), InvalidParameter);
        f.frame.error = 0;
        RequireError(sceFontGraphicsDrawupFillTexturePatternObject(frame, f.descriptor, 0x100, 0, 0, 4, 1, 2, 0x101, &out), InvalidParameter);
        f.frame.error = 0;
        f.frame.sequence = 0xffffffffu;
        RequireError(sceFontGraphicsDrawupFillTexturePatternObject(frame, f.descriptor, 0x100, 0, 0, 4, 1, 2, 8, &out), Sealed);
        Require(f.frame.error == Sealed);
        f.frame.error = 0;
        f.frame.sequence = 0;
        Require(sceFontGraphicsDrawupFillTexturePatternObject(frame, f.descriptor, 0, 5, 4, 4, 1, 2, 8, &out) == 0);
        record = static_cast<TestDrawupRecord*>(out);
        Require(record->params[0] == 0x3000 && record->params[1] == 0x20);
        Require(sceFontGraphicsDrawupFillTexturePatternObject(frame, f.descriptor, 0, 4, 3, 256, 4, 3, 256, &out) == 0);
        record = static_cast<TestDrawupRecord*>(out);
        Require(record->params[0] == (0x200 | 0x2000) && record->params[1] == (3 | 0x30));
        dims = reinterpret_cast<const std::uint8_t*>(&record->params[2]);
        Require(dims[0] == 255 && dims[1] == 255);
    }
    {
        DrawupFixture f;
        auto* frame = reinterpret_cast<FontGraphicsFrame*>(&f.frame);
        void* out = nullptr;
        Require(sceFontGraphicsDrawupFillTextureImageObject(frame, f.descriptor, 0x180, &out, 0.0f, 0.0f, 1.0f, 1.0f) == 0);
        Require(sceFontGraphicsDrawupFillTextureImageObject(frame, f.descriptor, 0x180, &out, 0.0f, 0.0f, 1.0f, 1.0f) == 0);
        auto* record2 = static_cast<TestDrawupRecord*>(out);
        Require(f.pool.used == 8 && f.pool.entries[0].used == 8);
        Require(record2->sequence == 1 && f.frame.sequence == 2);
        RequireError(sceFontGraphicsDrawupFillTextureImageObject(frame, f.descriptor, 0x80, &out, 0.0f, 0.0f, 1.0f, 1.0f), NoRecordSpace);
    }
    {
        DrawupFixture f;
        auto* frame = reinterpret_cast<FontGraphicsFrame*>(&f.frame);
        void* out = nullptr;
        std::memset(f.memory, 0xa5, 2 * 0x80);
        f.nodes[0] = (2 << 1) | 1;
        f.nodes[1] = 1;
        f.nodes[2] = TotalSlots << 1;
        f.pool.entries[0].used = 2;
        f.pool.used = 2;
        f.pool.entries[0].scan = f.nodes + 2;
        const float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        Require(sceFontGraphicsDrawupFillFlatColor(frame, color, &out) == 0);
        auto* record = static_cast<TestDrawupRecord*>(out);
        Require(record == reinterpret_cast<TestDrawupRecord*>(f.memory + 2 * 0x80));
        Require(record->type == 0x10 && record->state == 0xffffffff);
        Require(AllBytes(*record, 0x14, 0x80, 0));
        Require(f.memory[0] == 0xa5 && f.memory[0x7f] == 0xa5 && f.memory[0x80] == 0xa5 && f.memory[0xff] == 0xa5);
        Require(f.nodes[0] == ((2 << 1) | 1) && f.nodes[1] == 1);
        Require(f.nodes[2] == ((3 << 1) | 1) && f.nodes[3] == (TotalSlots << 1));
        Require(f.pool.used == 3 && f.pool.entries[0].used == 3);
        f.pool.entries[0].scan = f.nodes + TotalSlots;
        f.frame.error = 0;
        RequireError(sceFontGraphicsDrawupFillFlatColor(frame, color, &out), NoRecordSpace);
    }
}

void DrawupFillRecords() {
    {
        DrawupFixture f;
        auto* frame = reinterpret_cast<FontGraphicsFrame*>(&f.frame);
        void* out = reinterpret_cast<void*>(0xdead);
        std::memset(f.memory, 0xa5, sizeof(f.memory));
        const float color[4] = {1.0f, 0.5f, 0.0f, 1.0f};
        Require(sceFontGraphicsDrawupFillFlatColor(frame, color, &out) == 0);
        auto* record = static_cast<TestDrawupRecord*>(out);
        Require(record == reinterpret_cast<TestDrawupRecord*>(f.memory));
        Require(record->type == 0x10 && record->sizeTag == 0x80 && record->flags == 0);
        Require(record->sequence == 0 && record->next == nullptr);
        Require(record->state == 0xff0080ff);
        Require(AllBytes(*record, 0x14, 0x80, 0xa5));
        Require(f.object.head == record && f.object.tail == record && f.frame.sequence == 1);
        Require(f.pool.used == 1 && f.pool.entries[0].used == 1);
        const float nanColor[4] = {NaN, 0.0f, 0.0f, 0.0f};
        Require(sceFontGraphicsDrawupFillFlatColor(frame, nanColor, &out) == 0);
        Require(static_cast<TestDrawupRecord*>(out)->state == 0x00000000);
        f.frame.error = 0;
        RequireError(sceFontGraphicsDrawupFillFlatColor(nullptr, color, &out), InvalidObject);
        f.frame.error = 0;
        RequireError(sceFontGraphicsDrawupFillFlatColor(frame, nullptr, &out), InvalidParameter);
        f.frame.error = 0;
        const float over[4] = {0.0f, 0.0f, 0.0f, 1.5f};
        RequireError(sceFontGraphicsDrawupFillFlatColor(frame, over, &out), InvalidParameter);
        f.frame.error = 0;
        const float under[4] = {0.0f, -0.5f, 0.0f, 0.0f};
        RequireError(sceFontGraphicsDrawupFillFlatColor(frame, under, &out), InvalidParameter);
        f.frame.error = 0;
        RequireError(sceFontGraphicsDrawupFillFlatColor(frame, color, nullptr), InvalidParameter);
        f.frame.error = 0;
        f.frame.object = nullptr;
        RequireError(sceFontGraphicsDrawupFillFlatColor(frame, color, &out), Uninitialized);
        f.frame.object = &f.object;
        f.frame.error = 0;
        f.frame.sequence = 0xffffffff;
        RequireError(sceFontGraphicsDrawupFillFlatColor(frame, color, &out), Sealed);
        Require(f.frame.error == Sealed);
        f.frame.error = 0;
        f.frame.sequence = 1;
    }
    {
        DrawupFixture f;
        auto* frame = reinterpret_cast<FontGraphicsFrame*>(&f.frame);
        void* out = nullptr;
        std::memset(f.memory, 0xa5, sizeof(f.memory));
        const float c1[4] = {0.0f, 0.25f, 0.5f, 1.0f};
        const float c2[4] = {1.0f, 0.75f, 0.5f, 0.25f};
        Require(sceFontGraphicsDrawupFillLinearGradient(frame, 2, c1, c2, &out, 2.0f, 4.0f, 1.0f, 2.0f, 3.0f, 4.0f) == 0);
        auto* record = static_cast<TestDrawupRecord*>(out);
        Require(record->type == 0x20 && record->sizeTag == 0x80 && record->flags == 0);
        Require(record->sequence == 0 && record->next == nullptr);
        Require(record->state == 0xff804000 && record->mode == 0x4080bfff);
        const auto* extents = reinterpret_cast<const float*>(&record->payload);
        Require(extents[0] == 2.0f && extents[1] == 4.0f);
        Require(record->params[0] == 2 && record->params[1] == 0);
        const auto* values = reinterpret_cast<const float*>(&record->params[2]);
        Require(values[0] == 1.0f && values[1] == 2.0f && values[2] == 3.0f && values[3] == 4.0f);
        Require(AllBytes(*record, 0x38, 0x80, 0xa5));
        f.frame.error = 0;
        RequireError(sceFontGraphicsDrawupFillLinearGradient(frame, 3, c1, c2, &out, 2.0f, 4.0f, 1.0f, 2.0f, 3.0f, 4.0f), InvalidParameter);
        f.frame.error = 0;
        RequireError(sceFontGraphicsDrawupFillLinearGradient(frame, 2, c1, c2, &out, 0.0f, 4.0f, 1.0f, 2.0f, 3.0f, 4.0f), InvalidParameter);
        f.frame.error = 0;
        RequireError(sceFontGraphicsDrawupFillLinearGradient(frame, 2, c1, c2, &out, 2.0f, -1.0f, 1.0f, 2.0f, 3.0f, 4.0f), InvalidParameter);
        f.frame.error = 0;
        Require(sceFontGraphicsDrawupFillLinearGradient(frame, 2, c1, c2, &out, NaN, 4.0f, 1.0f, 2.0f, 3.0f, 4.0f) == 0);
        record = static_cast<TestDrawupRecord*>(out);
        extents = reinterpret_cast<const float*>(&record->payload);
        Require(std::isnan(extents[0]) && extents[1] == 4.0f);
        f.frame.error = 0;
        RequireError(sceFontGraphicsDrawupFillLinearGradient(frame, 2, nullptr, c2, &out, 2.0f, 4.0f, 1.0f, 2.0f, 3.0f, 4.0f), InvalidParameter);
        f.frame.error = 0;
        const float bad[4] = {0.0f, 0.0f, 0.0f, 2.0f};
        RequireError(sceFontGraphicsDrawupFillLinearGradient(frame, 2, bad, c2, &out, 2.0f, 4.0f, 1.0f, 2.0f, 3.0f, 4.0f), InvalidParameter);
        f.frame.error = 0;
        RequireError(sceFontGraphicsDrawupFillLinearGradient(frame, 2, c1, c2, nullptr, 2.0f, 4.0f, 1.0f, 2.0f, 3.0f, 4.0f), InvalidParameter);
        f.frame.error = 0;
        f.frame.sequence = 0xffffffff;
        RequireError(sceFontGraphicsDrawupFillLinearGradient(frame, 2, c1, c2, &out, 2.0f, 4.0f, 1.0f, 2.0f, 3.0f, 4.0f), Sealed);
        Require(f.frame.error == Sealed);
        f.frame.error = 0;
        f.frame.sequence = 2;
    }
    {
        DrawupFixture f;
        auto* frame = reinterpret_cast<FontGraphicsFrame*>(&f.frame);
        void* out = nullptr;
        std::memset(f.memory, 0xa5, sizeof(f.memory));
        const float c1[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        const float c2[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        Require(sceFontGraphicsDrawupFillRadialGradient(frame, 0, c1, c2, &out, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f) == 0);
        auto* record = static_cast<TestDrawupRecord*>(out);
        Require(record->type == 0x30 && record->sizeTag == 0x80 && record->flags == 0);
        Require(record->state == 0xffffffff && record->mode == 0);
        const auto* values = reinterpret_cast<const float*>(&record->params[2]);
        Require(values[0] == 0.0f && values[1] == 0.0f && values[2] == 1.0f && values[3] == 1.0f);
        Require(record->params[6] == 0 && record->params[7] == 0);
        Require(AllBytes(*record, 0x40, 0x80, 0xa5));
    }
    {
        DrawupFixture f;
        auto* frame = reinterpret_cast<FontGraphicsFrame*>(&f.frame);
        void* out = nullptr;
        f.nodes[0] = (2 << 1) | 1;
        f.nodes[1] = 1;
        f.nodes[2] = 4 << 1;
        f.nodes[4] = TotalSlots << 1;
        f.pool.entries[0].used = 2;
        f.pool.used = 2;
        f.pool.entries[0].scan = f.nodes + 2;
        Require(sceFontGraphicsDrawupFillTexturePatternObject(frame, f.descriptor, 0x180, 0, 0, 1, 0, 0, 1, &out) == 0);
        auto* record = static_cast<TestDrawupRecord*>(out);
        Require(record == reinterpret_cast<TestDrawupRecord*>(f.memory + 4 * 0x80));
        Require(f.nodes[2] == ((6 << 1) | 1) && f.nodes[3] == 1);
        Require(f.nodes[4] == ((TotalSlots << 1) | 1) && f.nodes[5] == 1);
        Require(f.nodes[6] == (TotalSlots << 1));
        Require(f.pool.used == 6 && f.pool.entries[0].used == 6);
    }
}

struct SegmentFixture {
    static constexpr std::uint32_t Slots = 64;
    static constexpr std::uint32_t SlotSize = 4;
    static constexpr std::uint32_t UnitSize = 0x40;
    static constexpr std::uint32_t UnitSlots = (UnitSize + 7 + SlotSize) / SlotSize;
    PthreadMutex segMutex{};
    std::uint8_t memory[Slots * SlotSize]{};
    TestSegmentNode nodes[Slots]{};
    TestSegmentResource resource{};
    TestDrawupObject object{};
    TestDrawupFrame frame{};
    TestDrawupRecord record{};

    SegmentFixture() {
        scePthreadMutexInit(&segMutex, nullptr, "segment");
        auto& arena = resource.arena;
        arena.memory = memory;
        arena.size = sizeof(memory);
        arena.slotSize = SlotSize;
        arena.total = Slots;
        arena.nodes = nodes;
        arena.sentinel = nodes;
        arena.scan = nodes;
        arena.mutex = &segMutex;
        object.resource = &resource;
        frame.tag = 0xf53;
        frame.object = &object;
        frame.unitSize = UnitSize;
        record.type = 0x20;
        record.sequence = 7;
    }

    ~SegmentFixture() { scePthreadMutexDestroy(&segMutex); }

    TestSegmentStop* Chain() {
        return reinterpret_cast<TestSegmentStop*>(record.next);
    }
};

void DrawupGradientSegments() {
    const float color[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    {
        SegmentFixture f;
        auto* frame = reinterpret_cast<FontGraphicsFrame*>(&f.frame);
        RequireError(sceFontGraphicsAppendGradientSegment(nullptr, &f.record, 0.5f, color), InvalidObject);
        f.frame.tag = 0;
        RequireError(sceFontGraphicsAppendGradientSegment(frame, &f.record, 0.5f, color), InvalidObject);
        Require(f.frame.error == InvalidObject);
        f.frame.tag = 0xf53;
        f.frame.error = -3;
        RequireError(sceFontGraphicsAppendGradientSegment(frame, &f.record, 0.5f, color), Sticky);
        f.frame.error = 0;
        RequireError(sceFontGraphicsAppendGradientSegment(frame, nullptr, 0.5f, color), InvalidGradient);
        f.frame.error = 0;
        RequireError(sceFontGraphicsAppendGradientSegment(frame, reinterpret_cast<void*>(~std::uintptr_t{0}), 0.5f, color), InvalidGradient);
        f.frame.error = 0;
        f.record.type = 0x10;
        RequireError(sceFontGraphicsAppendGradientSegment(frame, &f.record, 0.5f, color), InvalidFill);
        f.frame.error = 0;
        f.record.type = 0x40;
        RequireError(sceFontGraphicsAppendGradientSegment(frame, &f.record, 0.5f, color), InvalidFill);
        f.record.type = 0x20;
        f.frame.error = 0;
        RequireError(sceFontGraphicsAppendGradientSegment(frame, &f.record, -0.01f, color), InvalidParameter);
        f.frame.error = 0;
        RequireError(sceFontGraphicsAppendGradientSegment(frame, &f.record, 1.01f, color), InvalidParameter);
        f.frame.error = 0;
        RequireError(sceFontGraphicsAppendGradientSegment(frame, &f.record, 0.5f, nullptr), InvalidParameter);
        f.frame.error = 0;
        const float bad[4] = {0.0f, 0.0f, 0.0f, 1.5f};
        RequireError(sceFontGraphicsAppendGradientSegment(frame, &f.record, 0.5f, bad), InvalidParameter);
        f.frame.error = 0;
        Require(f.frame.unitHead == nullptr && f.frame.unitUsed == 0);
    }
    {
        SegmentFixture f;
        auto* frame = reinterpret_cast<FontGraphicsFrame*>(&f.frame);
        Require(sceFontGraphicsAppendGradientSegment(frame, &f.record, 0.5f, color) == 0);
        Require(f.frame.error == 0);
        Require(f.frame.unitUsed == 0x18 && f.frame.unit != nullptr && f.frame.unitHead == f.frame.unit);
        Require(*reinterpret_cast<void**>(f.frame.unit) == nullptr);
        auto* unit = static_cast<std::uint8_t*>(f.frame.unit);
        Require(unit == f.memory + (SegmentFixture::Slots - SegmentFixture::UnitSlots) * SegmentFixture::SlotSize);
        Require(f.resource.arena.used == SegmentFixture::UnitSlots);
        Require(f.resource.arena.statUsed == SegmentFixture::UnitSlots);
        Require(f.resource.arena.statMaximum == SegmentFixture::UnitSlots);
        Require((f.nodes[0].end & 0x1fffff) == SegmentFixture::Slots - SegmentFixture::UnitSlots);
        Require((f.nodes[SegmentFixture::Slots - SegmentFixture::UnitSlots].head & 0x3fffff) == SegmentFixture::UnitSlots);
        auto* stop = reinterpret_cast<TestSegmentStop*>(f.record.next);
        Require(stop == reinterpret_cast<TestSegmentStop*>(unit + 8));
        Require(stop->type == 0x20 && stop->epoch == 7 && stop->next == nullptr);
        Require(stop->position == 0.5f && stop->color == 0x80808080);
        Require(reinterpret_cast<std::uint8_t*>(stop)[1] == 0 && *reinterpret_cast<std::uint16_t*>(reinterpret_cast<std::uint8_t*>(stop) + 2) == 0);
        Require(sceFontGraphicsAppendGradientSegment(frame, &f.record, 0.2f, color) == 0);
        Require(f.frame.unitUsed == 0x30);
        auto* second = reinterpret_cast<TestSegmentStop*>(f.record.next);
        Require(second->position == 0.2f && second->next == stop);
        Require(sceFontGraphicsAppendGradientSegment(frame, &f.record, 0.8f, color) == 0);
        Require(f.frame.unitUsed == 0x58);
        auto* unit2 = static_cast<std::uint8_t*>(f.frame.unit);
        Require(unit2 != unit && *reinterpret_cast<void**>(unit) == unit2 && *reinterpret_cast<void**>(unit2) == nullptr);
        auto* third = reinterpret_cast<TestSegmentStop*>(unit2 + 8);
        Require(third->position == 0.8f && third->next == nullptr && stop->next == third && second->next == stop);
        const auto* pad = unit + 8 + 0x30;
        Require(pad[0] == 0xff && pad[1] == 0x10 && pad[2] == 0 && pad[3] == 0);
        Require(std::all_of(pad + 4, pad + 0x10, [](std::uint8_t v) { return v == 0; }));
        const auto* stopChain = reinterpret_cast<const TestSegmentStop*>(f.record.next);
        Require(stopChain->position == 0.2f && stopChain->next->position == 0.5f && stopChain->next->next->position == 0.8f && stopChain->next->next->next == nullptr);
        Require(f.resource.arena.used == 2 * SegmentFixture::UnitSlots);
    }
    {
        SegmentFixture f;
        auto* frame = reinterpret_cast<FontGraphicsFrame*>(&f.frame);
        f.record.type = 0x30;
        Require(sceFontGraphicsAppendGradientSegment(frame, &f.record, 0.5f, color) == 0);
        auto* stop = reinterpret_cast<TestSegmentStop*>(f.record.next);
        Require(stop->type == 0x30 && stop->epoch == 7 && stop->position == 0.5f);
    }
    {
        SegmentFixture f;
        auto* frame = reinterpret_cast<FontGraphicsFrame*>(&f.frame);
        TestSegmentStop stale{0x20, 0, 0, 99, nullptr, 0.3f, 0};
        f.record.next = reinterpret_cast<TestDrawupRecord*>(&stale);
        Require(sceFontGraphicsAppendGradientSegment(frame, &f.record, 0.5f, color) == 0);
        auto* stop = reinterpret_cast<TestSegmentStop*>(f.record.next);
        Require(stop->position == 0.5f && stop->epoch == 7);
        Require(reinterpret_cast<TestSegmentStop*>(stop->next) == &stale);
    }
    {
        SegmentFixture f;
        auto* frame = reinterpret_cast<FontGraphicsFrame*>(&f.frame);
        f.frame.unitSize = 0x10;
        Require(sceFontGraphicsAppendGradientSegment(frame, &f.record, 0.5f, color) == 0);
        Require(f.frame.error == 0);
        auto* padUnit = static_cast<std::uint8_t*>(f.frame.unitHead);
        Require(padUnit != nullptr && f.frame.unitUsed == 0x28);
        const auto* pad = padUnit + 8;
        Require(pad[0] == 0xff && pad[1] == 0x10 && pad[2] == 0 && pad[3] == 0);
        Require(std::all_of(pad + 4, pad + 0x10, [](std::uint8_t v) { return v == 0; }));
        auto* unit2 = static_cast<std::uint8_t*>(*reinterpret_cast<void**>(padUnit));
        auto* unit3 = static_cast<std::uint8_t*>(*reinterpret_cast<void**>(unit2));
        Require(unit2 != nullptr && unit3 != nullptr && f.frame.unit == unit3);
        auto* stop = reinterpret_cast<TestSegmentStop*>(f.record.next);
        Require(reinterpret_cast<std::uint8_t*>(stop) == unit2 + 8);
        Require(stop->type == 0x20 && stop->epoch == 7 && stop->next == nullptr);
        Require(*reinterpret_cast<float*>(unit3 + 8) == 0.5f && *reinterpret_cast<std::uint32_t*>(unit3 + 12) == 0x80808080);
        Require(f.resource.arena.used == 18 && f.resource.arena.statMaximum == 18);
        Require(sceFontGraphicsAppendGradientSegment(frame, &f.record, 0.2f, color) == 0);
        Require(f.frame.error == 0);
    }
    {
        SegmentFixture f;
        auto* frame = reinterpret_cast<FontGraphicsFrame*>(&f.frame);
        f.frame.unitSize = 0xffffffff;
        std::memset(f.memory, 0xa5, sizeof(f.memory));
        RequireError(sceFontGraphicsAppendGradientSegment(frame, &f.record, 0.5f, color), NoSegmentSpace);
        const std::uint32_t slots = (0xffffffffu + 7 + SegmentFixture::SlotSize) / SegmentFixture::SlotSize;
        Require(slots == 2 && f.resource.arena.used == slots);
        Require(f.frame.error == NoSegmentSpace);
        Require(f.frame.unitUsed == 0 && f.frame.unit == f.memory + 248 && f.frame.unitHead == f.frame.unit);
        Require(f.record.next == nullptr);
        Require(AllBytes(f.memory, 0, 248, 0xa5));
        Require(AllBytes(f.memory, 248, 256, 0));
    }
    {
        SegmentFixture f;
        auto* frame = reinterpret_cast<FontGraphicsFrame*>(&f.frame);
        f.resource.arena.total = 2 * SegmentFixture::UnitSlots;
        Require(sceFontGraphicsAppendGradientSegment(frame, &f.record, 0.5f, color) == 0);
        Require(sceFontGraphicsAppendGradientSegment(frame, &f.record, 0.4f, color) == 0);
        Require(sceFontGraphicsAppendGradientSegment(frame, &f.record, 0.6f, color) == 0);
        Require(sceFontGraphicsAppendGradientSegment(frame, &f.record, 0.3f, color) == 0);
        RequireError(sceFontGraphicsAppendGradientSegment(frame, &f.record, 0.7f, color), NoSegmentSpace);
        Require(f.frame.error == NoSegmentSpace);
    }
    {
        SegmentFixture f;
        auto* frame = reinterpret_cast<FontGraphicsFrame*>(&f.frame);
        f.frame.unitSize = 0;
        RequireError(sceFontGraphicsAppendGradientSegment(frame, &f.record, 0.5f, color), NoSegmentSpace);
        Require(f.frame.error == NoSegmentSpace && f.frame.unit == nullptr && f.frame.unitHead == nullptr);
    }
    {
        SegmentFixture f;
        auto* frame = reinterpret_cast<FontGraphicsFrame*>(&f.frame);
        const float nan[4] = {0.5f, NaN, 0.5f, 0.5f};
        Require(sceFontGraphicsAppendGradientSegment(frame, &f.record, NaN, color) == 0);
        Require(sceFontGraphicsAppendGradientSegment(frame, &f.record, 0.75f, nan) == 0);
        Require(reinterpret_cast<TestSegmentStop*>(f.record.next)->color == 0x80800080);
    }
    {
        SegmentFixture f;
        auto* frame = reinterpret_cast<FontGraphicsFrame*>(&f.frame);
        f.frame.unitUsed = 0x30;
        f.frame.unitHead = f.memory + 0;
        f.frame.unit = f.memory + 0;
        *reinterpret_cast<void**>(f.frame.unit) = nullptr;
        f.nodes[0].end = SegmentFixture::Slots - SegmentFixture::UnitSlots;
        f.resource.arena.used = SegmentFixture::UnitSlots;
        Require(sceFontGraphicsAppendGradientSegment(frame, &f.record, 0.5f, color) == 0);
        auto* unit = static_cast<std::uint8_t*>(f.frame.unitHead);
        auto* unit2 = static_cast<std::uint8_t*>(*reinterpret_cast<void**>(unit));
        Require(unit2 != nullptr);
        const auto* pad = unit + 8 + 0x30;
        Require(pad[0] == 0xff && pad[1] == 0x10 && pad[2] == 0 && pad[3] == 0);
        Require(std::all_of(pad + 4, pad + 0x10, [](std::uint8_t v) { return v == 0; }));
        auto* stop = reinterpret_cast<TestSegmentStop*>(f.record.next);
        Require(reinterpret_cast<std::uint8_t*>(stop) == unit2 + 8);
        Require(f.frame.unitUsed == 0x58);
        Require(stop->position == 0.5f && stop->next == nullptr);
    }
}

}

int main() {
    sceFontSetGraphicsErrorCallback(RecordError);
    Regions();
    FillMethods();
    FillPlots();
    FillRates();
    DrawupTextureRecords();
    DrawupFillRecords();
    DrawupGradientSegments();
    Require(g_errorCount == g_expectedErrors);
    sceFontSetGraphicsErrorCallback(nullptr);
    const int before = g_errorCount;
    FontGraphicsRegion region;
    Require(sceFontGraphicsRegionInit(&region, 0.0f, 0.0f, -1.0f, 0.0f) == InvalidParameter);
    Require(g_errorCount == before);
}
