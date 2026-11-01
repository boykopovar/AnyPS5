#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libSceFont/libSceFont.hpp"
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
static_assert(std::is_same_v<decltype(&sceFontGraphicsDrawupFillTextureImageObject), FontDrawupSignature>);
static_assert(std::is_same_v<decltype(&sceFontGraphicsDrawupFillTexturePatternObject), FontDrawupPatternSignature>);

constexpr int Uninitialized = static_cast<int>(0x80460001);
constexpr int InvalidParameter = static_cast<int>(0x80460002);
constexpr int Sealed = static_cast<int>(0x8046005c);
constexpr int InvalidObject = static_cast<int>(0x80460084);
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

struct TestDrawupObject {
    std::uint64_t reserved0;
    TestDrawupOwner* owner;
    std::uint64_t reserved10[3];
    TestDrawupRecord* head;
    TestDrawupRecord* tail;
};

struct TestDrawupFrame {
    std::uint16_t tag;
    std::uint16_t reserved2;
    int error;
    TestDrawupObject* object;
    std::uint64_t reserved10[3];
    std::uint32_t sequence;
};

static_assert(sizeof(TestDrawupRecord) == 0x80);
static_assert(sizeof(TestDrawupEntry) == 0x30);
static_assert(offsetof(TestDrawupPool, entries) == 0x18);
static_assert(offsetof(TestDrawupOwner, pool) == 0x160);
static_assert(offsetof(TestDrawupObject, head) == 0x28 && offsetof(TestDrawupObject, tail) == 0x30);
static_assert(offsetof(TestDrawupFrame, object) == 0x8 && offsetof(TestDrawupFrame, sequence) == 0x28);

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
        Require(sceFontGraphicsDrawupFillTexturePatternObject(frame, f.descriptor, 0, 0, 0, 1, 0, 0, 1, &out) == 0);
        auto* record = static_cast<TestDrawupRecord*>(out);
        Require(record == reinterpret_cast<TestDrawupRecord*>(f.memory + 2 * 0x80));
        Require(record->type == 0x50 && record->mode == 0);
        Require(record->params[0] == (0x100 | 0x1000) && record->params[1] == (4 | 0x40));
        auto* dims = reinterpret_cast<const std::uint8_t*>(&record->params[2]);
        Require(dims[0] == 0 && dims[1] == 0);
        Require(AllBytes(*record, 0x2a, 0x80, 0));
        Require(f.memory[0] == 0xa5 && f.memory[0x7f] == 0xa5 && f.memory[0x80] == 0xa5 && f.memory[0xff] == 0xa5);
        Require(f.nodes[0] == ((2 << 1) | 1) && f.nodes[1] == 1);
        Require(f.nodes[2] == ((3 << 1) | 1) && f.nodes[3] == (TotalSlots << 1));
        Require(f.pool.used == 3 && f.pool.entries[0].used == 3);
        f.pool.entries[0].scan = f.nodes + TotalSlots;
        f.frame.error = 0;
        RequireError(sceFontGraphicsDrawupFillTexturePatternObject(frame, f.descriptor, 0, 0, 0, 1, 0, 0, 1, &out), NoRecordSpace);
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

}

int main() {
    sceFontSetGraphicsErrorCallback(RecordError);
    Regions();
    FillMethods();
    FillPlots();
    FillRates();
    DrawupTextureRecords();
    Require(g_errorCount == g_expectedErrors);
    sceFontSetGraphicsErrorCallback(nullptr);
    const int before = g_errorCount;
    FontGraphicsRegion region;
    Require(sceFontGraphicsRegionInit(&region, 0.0f, 0.0f, -1.0f, 0.0f) == InvalidParameter);
    Require(g_errorCount == before);
}
