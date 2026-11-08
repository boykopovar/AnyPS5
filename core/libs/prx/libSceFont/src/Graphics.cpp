#include <atomic>
#include <bit>
#include <cstdint>
#include <cstring>

#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceFont/include/FontGraphics.hpp"
#include "prx/libSceFont/include/FontTypes.hpp"

namespace {

constexpr int FONT_GRAPHICS_ERROR_UNINITIALIZED = static_cast<int>(0x80460001);
constexpr int FONT_GRAPHICS_ERROR_SEALED = static_cast<int>(0x8046005c);
constexpr int FONT_GRAPHICS_ERROR_INVALID_OBJECT = static_cast<int>(0x80460084);
constexpr int FONT_GRAPHICS_ERROR_INVALID_GRADIENT = static_cast<int>(0x80460020);
constexpr int FONT_GRAPHICS_ERROR_INVALID_FILL = static_cast<int>(0x80460086);
constexpr int FONT_GRAPHICS_ERROR_INVALID_REGION = static_cast<int>(0x80460087);
constexpr int FONT_GRAPHICS_ERROR_INVALID_FILL_PLOT = static_cast<int>(0x80460088);
constexpr int FONT_GRAPHICS_ERROR_INVALID_FILL_RATES = static_cast<int>(0x80460089);
constexpr int FONT_GRAPHICS_ERROR_NO_SEGMENT_SPACE = static_cast<int>(0x804600a0);
constexpr int FONT_GRAPHICS_ERROR_NO_RECORD_SPACE = static_cast<int>(0x804600a2);
constexpr int FONT_GRAPHICS_ERROR_STICKY = static_cast<int>(0x804600a9);

constexpr std::uint32_t RegionTag = 0xf60;
constexpr std::uint32_t RectangleRegionFlags = 0xf;
constexpr std::uint32_t CircularRegionFlags = 0xff;
constexpr std::uint32_t FillMethodTag = 0xf01b;
constexpr std::uint32_t FillMethodFlags = 0xf;
constexpr std::uint16_t FillPlotTag = 0xf01c;
constexpr std::uint16_t FillRatesTag = 0xf01a;
constexpr std::uint16_t DefaultLayout = 0xf77;
constexpr std::uint32_t FillEffects = 0x1033f;

std::atomic<Font::GraphicsErrorCallback> g_errorCallback{nullptr};

constexpr std::uint16_t DrawupFrameTag = 0xf53;
constexpr std::uint8_t DrawupTypeFlatColor = 0x10;
constexpr std::uint8_t DrawupTypeLinearGradient = 0x20;
constexpr std::uint8_t DrawupTypeRadialGradient = 0x30;
constexpr std::uint8_t DrawupTypeTextureImage = 0x40;
constexpr std::uint8_t DrawupTypeTexturePattern = 0x50;
constexpr std::uint32_t DrawupRecordHeader = 0x80;
constexpr std::uint32_t DrawupPoolSealed = 0xffffffff;

struct FontDrawupRecord {
    std::uint8_t type;
    std::uint8_t sizeTag;
    std::uint16_t flags;
    std::uint32_t sequence;
    FontDrawupRecord* next;
    union {
        struct {
            std::uint32_t state;
            std::uint32_t mode;
            void* payload;
            float quad[4];
        } texture;
        struct {
            std::uint32_t color;
        } flat;
        struct {
            std::uint32_t color1;
            std::uint32_t color2;
            float extent0;
            float extent1;
            std::uint32_t mode;
            std::uint32_t unused;
            float values[4];
            std::uint64_t tail;
        } gradient;
        std::uint8_t raw[0x70];
    };
};

struct FontDrawupPoolEntry {
    std::uint8_t* memory;
    std::uint32_t reserved8;
    std::uint32_t slotSize;
    std::uint32_t total;
    std::uint32_t used;
    std::uint16_t* nodes;
    std::uint16_t* scan;
    std::uint64_t reserved28;
};

struct FontDrawupPool {
    PthreadMutex* mutex;
    std::uint32_t span;
    std::uint32_t entryCount;
    std::uint32_t used;
    std::uint32_t maximum;
    std::uint8_t* limit;
    FontDrawupPoolEntry entries[];
};

struct FontDrawupPoolOwner {
    std::uint8_t reserved[0x160];
    FontDrawupPool* pool;
};

struct FontSegmentResource;

struct FontDrawupObject {
    std::uint64_t reserved0;
    FontDrawupPoolOwner* owner;
    FontSegmentResource* resource;
    std::uint64_t reserved18[2];
    FontDrawupRecord* head;
    FontDrawupRecord* tail;
};

struct FontDrawupFrame {
    std::uint16_t tag;
    std::uint16_t reserved2;
    int error;
    FontDrawupObject* object;
    std::uint32_t unitSize;
    std::uint32_t unitUsed;
    void* unitHead;
    std::uint8_t* unit;
    std::uint32_t sequence;
};

struct FontSegmentNode {
    std::uint32_t head;
    std::uint32_t next;
    std::uint32_t end;
};

struct FontSegmentArena {
    std::uint8_t* memory;
    std::uint32_t size;
    std::uint32_t slotSize;
    std::uint32_t total;
    std::uint32_t used;
    FontSegmentNode* nodes;
    FontSegmentNode* sentinel;
    FontSegmentNode* scan;
    FontSegmentNode spare;
    std::uint32_t reserved3c;
    PthreadMutex* mutex;
    std::uint32_t statUsed;
    std::uint32_t statMaximum;
};

struct FontSegmentResource {
    std::uint8_t reserved[0xc0];
    FontSegmentArena arena;
};

struct FontSegmentLink {
    std::uint8_t type;
    std::uint8_t reserved1[3];
    std::uint32_t epoch;
    FontSegmentLink* next;
};

struct FontSegmentStop {
    std::uint8_t type;
    std::uint8_t reserved1;
    std::uint16_t reserved2;
    std::uint32_t epoch;
    FontSegmentStop* next;
    float position;
    std::uint32_t color;
};

static_assert(sizeof(FontDrawupRecord) == DrawupRecordHeader);
static_assert(sizeof(FontDrawupPoolEntry) == 0x30);
static_assert(offsetof(FontDrawupPoolEntry, slotSize) == 0x0c);
static_assert(offsetof(FontDrawupPool, limit) == 0x18 && offsetof(FontDrawupPool, entries) == 0x20);
static_assert(offsetof(FontDrawupPoolOwner, pool) == 0x160);
static_assert(offsetof(FontDrawupObject, resource) == 0x10);
static_assert(offsetof(FontDrawupObject, head) == 0x28 && offsetof(FontDrawupObject, tail) == 0x30);
static_assert(offsetof(FontDrawupFrame, unitSize) == 0x10 && offsetof(FontDrawupFrame, unitUsed) == 0x14);
static_assert(offsetof(FontDrawupFrame, unitHead) == 0x18 && offsetof(FontDrawupFrame, unit) == 0x20);
static_assert(sizeof(FontSegmentNode) == 0x0c);
static_assert(sizeof(FontSegmentArena) == 0x50);
static_assert(offsetof(FontSegmentArena, slotSize) == 0x0c && offsetof(FontSegmentArena, total) == 0x10);
static_assert(offsetof(FontSegmentArena, used) == 0x14 && offsetof(FontSegmentArena, nodes) == 0x18);
static_assert(offsetof(FontSegmentArena, sentinel) == 0x20 && offsetof(FontSegmentArena, scan) == 0x28);
static_assert(offsetof(FontSegmentArena, mutex) == 0x40 && offsetof(FontSegmentArena, statUsed) == 0x48);
static_assert(offsetof(FontSegmentResource, arena) == 0xc0);
static_assert(offsetof(FontSegmentStop, position) == 0x10);
static_assert(sizeof(FontSegmentStop) == 0x18);
static_assert(offsetof(FontDrawupFrame, object) == 0x8 && offsetof(FontDrawupFrame, sequence) == 0x28);

FontDrawupRecord* DrawupPoolAllocate(FontDrawupPoolEntry* entry, std::uint32_t slots) {
    if (!entry) return nullptr;
    const std::uint32_t total = entry->total;
    if (total - entry->used < slots || static_cast<std::int32_t>(total) <= 0) return nullptr;
    std::uint16_t* node = entry->scan;
    std::uint16_t* const nodes = entry->nodes;
    if (!nodes || node < nodes || node >= nodes + total) return nullptr;
    std::uint16_t* previous = node;
    std::uint32_t run = 0;
    std::uint32_t index = static_cast<std::uint32_t>(node - nodes);
    std::uint32_t start = 0;
    std::uint16_t stopWord = 0;
    for (;;) {
        const std::uint16_t word = *node;
        if (word & 1) {
            previous = node;
            run = 0;
        } else {
            if (run == 0) start = index;
            std::uint32_t next = word >> 1;
            if (next == 0) next = total;
            run += next - index;
            if (run >= slots) {
                stopWord = word;
                break;
            }
        }
        const std::uint32_t nextIndex = word >> 1;
        if (nextIndex == 0) return nullptr;
        node = nodes + nextIndex;
        index = nextIndex;
        if (static_cast<std::int32_t>(total) <= static_cast<std::int32_t>(nextIndex)) return nullptr;
    }
    const std::uint32_t end = start + slots;
    for (std::uint32_t i = start; i < end; ++i) nodes[i] |= 1;
    if (run == slots) {
        *previous = end < total
            ? static_cast<std::uint16_t>((*previous & 1) | (nodes[end] & ~1))
            : static_cast<std::uint16_t>((total << 1) | (*previous & 1));
    } else {
        *previous = static_cast<std::uint16_t>((end << 1) | (*previous & 1));
        nodes[end] = stopWord & ~1;
    }
    entry->used += slots;
    return reinterpret_cast<FontDrawupRecord*>(entry->memory + index * entry->slotSize);
}

void DrawupEnqueue(FontDrawupFrame* frame, FontDrawupRecord* record) {
    FontDrawupObject* const object = frame->object;
    FontDrawupRecord* tail = object->tail;
    if (!tail) {
        record->sequence = 0;
        object->head = record;
        object->tail = record;
        ++frame->sequence;
        return;
    }
    const std::uint32_t sequence = tail->sequence;
    FontDrawupRecord* next = tail->next;
    if (!next) {
        tail->next = record;
        record->sequence = sequence + 1;
        object->tail = record;
        ++frame->sequence;
        return;
    }
    while (next && next->sequence == sequence) {
        tail = next;
        next = tail->next;
    }
    if (next) return;
    tail->next = record;
    record->sequence = sequence + 1;
    object->tail = record;
    ++frame->sequence;
}

int Fail(int error) {
    if (const auto callback = g_errorCallback.load()) callback(error);
    return error;
}

bool IsObject(const void* pointer) {
    return reinterpret_cast<std::uintptr_t>(pointer) + 1 >= 2;
}

bool IsValidFill(const FontGraphicsFill* fill) {
    const auto value = reinterpret_cast<std::intptr_t>(fill);
    if (value >= -1 && value < 3) return true;
    const std::uint8_t mode = *reinterpret_cast<const std::uint8_t*>(fill);
    return (mode & 0xf) == 0 && mode >= 0x10 && mode <= 0x50;
}

bool IsValidEffect(std::uint32_t effect) {
    return effect < 17 && ((FillEffects >> effect) & 1) != 0;
}

bool IsValidLayoutNibble(std::uint32_t nibble) {
    return nibble <= 4 && nibble != 3;
}

bool IsValidLayout(std::int32_t first, std::int32_t second) {
    const bool firstValid = first == 0 || first == 0x170 || first == 0x207 || first == 0x400 || first == 0x800 || first == 0xf77;
    const auto bits = static_cast<std::uint32_t>(second);
    return firstValid && IsValidLayoutNibble((bits >> 4) & 0xf) && IsValidLayoutNibble(bits & 0xf);
}

bool IsUsableRegion(const FontGraphicsRegion& region) {
    if (region.tag != RegionTag) return false;
    if (!(region.outerRadius >= 0.0f) || !(region.innerRadius >= 0.0f)) return false;
    const float half = (region.width < region.height ? region.width : region.height) * 0.5f;
    if (region.outerRadius == 0.0f) return half >= region.innerRadius;
    return half >= region.outerRadius && region.outerRadius >= region.innerRadius;
}

std::uint8_t MappedFlags(std::uint8_t flags) {
    return static_cast<std::uint8_t>((flags & 0xfc) | 1);
}

}

namespace Font {

int OutlineVertexesInterfaceInit(void* interface, const void* first, const void* second, std::uint64_t count) {
    if (!interface) return SCE_FONT_ERROR_INVALID_PARAMETER;
    struct OutlineInterface {
        std::uint64_t tag;
        const void* first;
        const void* second;
        std::uint64_t count;
        std::uint64_t reserved[2];
    };
    static_assert(sizeof(OutlineInterface) == 0x30);
    const bool valid = first != nullptr && count >= 1 && count <= 3;
    const OutlineInterface value = valid ? OutlineInterface{0xf08, first, second, count, {}} : OutlineInterface{};
    std::memcpy(interface, &value, sizeof(value));
    return valid ? SCE_FONT_OK : SCE_FONT_ERROR_INVALID_PARAMETER;
}

void SetGraphicsErrorCallback(GraphicsErrorCallback callback) {
    g_errorCallback.store(callback);
}

int ReportGraphicsError(int error) {
    const auto callback = g_errorCallback.load();
    if (callback == nullptr) return static_cast<int>(0x80460020);
    callback(error);
    return 0;
}

int GraphicsRegionInit(FontGraphicsRegion* region, float x, float y, float width, float height) {
    if (!region || 0.0f > width || 0.0f > height) return Fail(SCE_FONT_ERROR_INVALID_PARAMETER);
    *region = {x, y, width, height, RectangleRegionFlags, RegionTag, 0.0f, 0.0f};
    return SCE_FONT_OK;
}

int GraphicsRegionInitCircular(FontGraphicsRegion* region, float centerX, float centerY, float outerRadius, float innerRadius) {
    if (innerRadius > outerRadius || !region || 0.0f >= outerRadius || 0.0f >= innerRadius) return Fail(SCE_FONT_ERROR_INVALID_PARAMETER);
    const float diameter = outerRadius + outerRadius;
    *region = {centerX - outerRadius, centerY - outerRadius, diameter, diameter, CircularRegionFlags, RegionTag, outerRadius, innerRadius};
    return SCE_FONT_OK;
}

int GraphicsRegionInitRoundish(FontGraphicsRegion* region, float x, float y, float width, float height, float outerRadius, float innerRadius, std::uint32_t flags) {
    if (!region || 0.0f > innerRadius || 0.0f > outerRadius || 0.0f >= height || 0.0f >= width || flags > 0xff) return Fail(SCE_FONT_ERROR_INVALID_PARAMETER);
    if (outerRadius > height * 0.5f || outerRadius > width * 0.5f) return Fail(SCE_FONT_ERROR_INVALID_PARAMETER);
    if (outerRadius < innerRadius && outerRadius != 0.0f) return Fail(SCE_FONT_ERROR_INVALID_PARAMETER);
    *region = {x, y, width, height, flags, RegionTag, outerRadius, innerRadius};
    return SCE_FONT_OK;
}

int GraphicsFillMethodInit(FontGraphicsFillMethod* method, std::int32_t mode, std::int32_t type) {
    const auto modeBits = static_cast<std::uint32_t>(mode);
    const bool modeValid = (modeBits & 0xf) == 0 && (modeBits >> 4) < 6;
    const bool typeValid = static_cast<std::uint32_t>(type) - 1 < 7 || type == 12;
    if (!method || !modeValid || !typeValid) return Fail(SCE_FONT_ERROR_INVALID_PARAMETER);
    *method = {};
    method->mode = mode;
    method->type = type;
    method->flags = FillMethodFlags;
    method->tag = FillMethodTag;
    return SCE_FONT_OK;
}

int GraphicsFillPlotInit(FontGraphicsFillPlot* plot, const FontGraphicsRegion* region) {
    if (!IsObject(plot)) return Fail(FONT_GRAPHICS_ERROR_INVALID_FILL_PLOT);
    if (IsObject(region) && !IsUsableRegion(*region)) {
        plot->tag = 0;
        return Fail(FONT_GRAPHICS_ERROR_INVALID_REGION);
    }
    *plot = {};
    if (!region) {
        plot->flags = 2;
    } else if (!IsObject(region)) {
        plot->flags = 6;
        plot->layout = DefaultLayout;
    } else {
        plot->mapping[0] = region->x;
        plot->mapping[1] = region->y;
        plot->mapping[2] = region->x + region->width;
        plot->mapping[3] = region->y + region->height;
        plot->flags = 1;
        plot->layout = DefaultLayout;
    }
    plot->region = region;
    plot->tag = FillPlotTag;
    return SCE_FONT_OK;
}

int GraphicsFillPlotSetLayout(FontGraphicsFillPlot* plot, std::int32_t first, std::int32_t second) {
    if (!IsObject(plot) || plot->tag != FillPlotTag) return Fail(FONT_GRAPHICS_ERROR_INVALID_FILL_PLOT);
    if (!IsValidLayout(first, second)) return Fail(SCE_FONT_ERROR_INVALID_PARAMETER);
    const auto combined = static_cast<std::uint32_t>(first) | static_cast<std::uint32_t>(second);
    plot->layout = static_cast<std::uint16_t>(combined);
    plot->flags = static_cast<std::uint8_t>((plot->flags & 0xf6) | (combined != 0 ? 1 : 0) | 8);
    return SCE_FONT_OK;
}

int GraphicsFillPlotSetMapping(FontGraphicsFillPlot* plot, float first, float second, float third, float fourth) {
    if (!IsObject(plot) || plot->tag != FillPlotTag) return Fail(FONT_GRAPHICS_ERROR_INVALID_FILL_PLOT);
    plot->mapping[0] = first;
    plot->mapping[1] = second;
    plot->mapping[2] = third;
    plot->mapping[3] = fourth;
    plot->flags = MappedFlags(plot->flags);
    return SCE_FONT_OK;
}

int GraphicsFillRatesInit(FontGraphicsFillRates* rates, const FontGraphicsFill* fill, std::uint32_t effect) {
    if (!IsObject(rates)) return Fail(SCE_FONT_ERROR_INVALID_PARAMETER);
    *rates = {};
    if (!IsValidFill(fill)) return Fail(FONT_GRAPHICS_ERROR_INVALID_FILL);
    if (!IsValidEffect(effect)) return Fail(SCE_FONT_ERROR_INVALID_PARAMETER);
    rates->fill = fill;
    rates->tag = FillRatesTag;
    rates->effect = static_cast<std::uint16_t>(effect);
    rates->layout = DefaultLayout;
    rates->flags = 2;
    return SCE_FONT_OK;
}

int GraphicsFillRatesSetFillEffect(FontGraphicsFillRates* rates, const FontGraphicsFill* fill, std::uint32_t effect) {
    if (!IsObject(rates) || rates->tag != FillRatesTag) return Fail(FONT_GRAPHICS_ERROR_INVALID_FILL_RATES);
    if (!IsValidFill(fill)) return Fail(FONT_GRAPHICS_ERROR_INVALID_FILL);
    if (!IsValidEffect(effect)) return Fail(SCE_FONT_ERROR_INVALID_PARAMETER);
    rates->effect = static_cast<std::uint16_t>(effect);
    rates->fill = fill;
    return SCE_FONT_OK;
}

int GraphicsFillRatesSetLayout(FontGraphicsFillRates* rates, std::int32_t first, std::int32_t second) {
    if (!IsObject(rates) || rates->tag != FillRatesTag) return Fail(FONT_GRAPHICS_ERROR_INVALID_FILL_RATES);
    if (!IsValidLayout(first, second)) return Fail(SCE_FONT_ERROR_INVALID_PARAMETER);
    const auto combined = static_cast<std::uint32_t>(first) | static_cast<std::uint32_t>(second);
    rates->flags = static_cast<std::uint8_t>((rates->flags & 0xfe) | (combined != 0 ? 1 : 0));
    rates->layout = 0;
    return SCE_FONT_OK;
}

int GraphicsFillRatesSetMapping(FontGraphicsFillRates* rates, float first, float second, float third, float fourth) {
    if (!IsObject(rates) || rates->tag != FillRatesTag) return Fail(FONT_GRAPHICS_ERROR_INVALID_FILL_RATES);
    rates->mapping[0] = first;
    rates->mapping[1] = second;
    rates->mapping[2] = third;
    rates->mapping[3] = fourth;
    rates->flags = MappedFlags(rates->flags);
    return SCE_FONT_OK;
}

int DrawupRecordError(FontDrawupFrame* frame, int error) {
    frame->error = error;
    return Fail(error);
}

FontDrawupRecord* DrawupAllocate(FontDrawupFrame* frame, std::uint32_t mode, int* error) {
    FontDrawupPool* const pool = frame->object->owner->pool;
    if (!pool) {
        *error = DrawupRecordError(frame, FONT_GRAPHICS_ERROR_NO_RECORD_SPACE);
        return nullptr;
    }
    if (scePthreadMutexLock(pool->mutex) != 0) {
        *error = DrawupRecordError(frame, FONT_GRAPHICS_ERROR_NO_RECORD_SPACE);
        return nullptr;
    }
    FontDrawupRecord* record = nullptr;
    FontDrawupPoolEntry* const end = pool->entries + pool->entryCount;
    for (FontDrawupPoolEntry* entry = pool->entries; entry != end && !record; ++entry) {
        const std::uint32_t slots = static_cast<std::uint32_t>((static_cast<std::uint64_t>(mode) + DrawupRecordHeader - 1 + entry->slotSize) / entry->slotSize);
        record = DrawupPoolAllocate(entry, slots);
        if (record) {
            pool->used += slots;
            if (pool->maximum < pool->used) pool->maximum = pool->used;
        }
    }
    scePthreadMutexUnlock(pool->mutex);
    if (!record) {
        *error = DrawupRecordError(frame, FONT_GRAPHICS_ERROR_NO_RECORD_SPACE);
        return nullptr;
    }
    record->sizeTag = static_cast<std::uint8_t>(mode + DrawupRecordHeader);
    record->flags = 0;
    record->sequence = frame->sequence;
    record->next = nullptr;
    return record;
}

std::uint32_t DrawupPackColor(const float* color) {
    const auto channel = [](float value) {
        const float scaled = value * 255.0f + 0.5f;
        if (!(scaled >= -0x1p63f && scaled < 0x1p63f)) return std::uint32_t{0};
        return static_cast<std::uint32_t>(static_cast<std::int64_t>(scaled));
    };
    return channel(color[0]) | (channel(color[1]) << 8) | (channel(color[2]) << 16) | (channel(color[3]) << 24);
}

bool DrawupColorValid(const float* color) {
    for (int i = 0; i < 4; ++i) {
        if (0.0f > color[i] || color[i] > 1.0f) return false;
    }
    return true;
}

std::uint8_t* SegmentArenaAllocate(FontSegmentArena* arena, std::uint32_t slots) {
    if (!arena || !slots || slots > arena->total) return nullptr;
    FontSegmentNode* const nodes = arena->nodes;
    FontSegmentNode* node = arena->scan;
    FontSegmentNode* const sentinel = arena->sentinel;
    const std::uint32_t total = arena->total;
    std::uint32_t index = node == sentinel ? 0 : static_cast<std::uint32_t>(node - nodes);
    std::uint32_t spanEnd = 0;
    for (;;) {
        const std::uint32_t head = node->head;
        if (!(head & 0x3fffff)) {
            spanEnd = node->end & 0x1fffff;
            if (!spanEnd) spanEnd = total;
            if (static_cast<std::int32_t>(spanEnd - index) >= static_cast<std::int32_t>(slots)) break;
        }
        if (!index) return nullptr;
        index = node->next & 0x1fffff;
        node = index ? nodes + index : sentinel;
    }
    const std::uint32_t chain = node->next & 0x1fffff;
    FontSegmentNode* const next = chain ? nodes + chain : sentinel;
    std::uint32_t result = index;
    if (spanEnd - index == slots) {
        node->head = (node->head & 0xffc00000) | (slots & 0x3fffff);
        const std::uint32_t end = index + slots;
        if (end < total) {
            const std::uint32_t after = nodes[end].end & 0x1fffff;
            next->end = (next->end & 0xffe00000) | after;
            if (after >= total || !after) {
                arena->scan = next;
            } else {
                nodes[after].next = (nodes[after].next & 0xffe00000) | chain;
            }
        } else {
            next->end = (next->end & 0xffe00000) | (total & 0x1fffff);
        }
    } else {
        const std::uint32_t carved = spanEnd - slots;
        node->end = (node->end & 0xffe00000) | (carved & 0x1fffff);
        FontSegmentNode* const fresh = nodes + carved;
        fresh->head = (fresh->head & 0xffc00000) | (slots & 0x3fffff);
        fresh->next = (fresh->next & 0xffe00000) | (index & 0x1fffff);
        std::uint32_t follow = spanEnd;
        if (spanEnd < total) follow = nodes[spanEnd].end & 0x1fffff;
        fresh->end = (fresh->end & 0xffe00000) | (follow & 0x1fffff);
        if (follow < total && follow) {
            nodes[follow].next = (nodes[follow].next & 0xffe00000) | carved;
        } else {
            arena->scan = fresh;
        }
        result = carved;
    }
    arena->used += slots;
    return arena->memory + static_cast<std::size_t>(result) * arena->slotSize;
}

bool SegmentWrite(FontDrawupFrame* frame, FontSegmentArena* arena,
        const std::uint8_t* source, std::uint32_t bytes, std::uint8_t** first) {
    for (std::uint32_t remaining = bytes; remaining;) {
        const std::uint32_t offset = frame->unitUsed % frame->unitSize;
        std::uint8_t* unit = frame->unit;
        if (!unit || !offset) {
            if (scePthreadMutexLock(arena->mutex) != 0) return false;
            const std::uint32_t slots = (frame->unitSize + 7 + arena->slotSize) / arena->slotSize;
            std::uint8_t* const memory = SegmentArenaAllocate(arena, slots);
            if (memory) {
                arena->statUsed = arena->used;
                if (arena->used > arena->statMaximum) arena->statMaximum = arena->used;
            }
            scePthreadMutexUnlock(arena->mutex);
            if (!memory) return false;
            *reinterpret_cast<void**>(memory) = nullptr;
            if (unit) *reinterpret_cast<void**>(unit) = memory;
            else frame->unitHead = memory;
            unit = memory;
            frame->unit = memory;
        }
        std::uint32_t chunk = frame->unitSize - offset;
        if (chunk > remaining) chunk = remaining;
        std::uint8_t* const target = unit + offset + 8;
        std::uint8_t* const limit = arena->memory + static_cast<std::size_t>(arena->total) * arena->slotSize;
        const std::size_t room = target < limit ? static_cast<std::size_t>(limit - target) : 0;
        if (room < chunk) return false;
        std::memcpy(target, source, chunk);
        if (first && !*first) *first = target;
        source += chunk;
        frame->unitUsed += chunk;
        remaining -= chunk;
    }
    return true;
}

int DrawupFrameError(FontDrawupFrame* frame, int error) {
    return frame ? DrawupRecordError(frame, error) : Fail(error);
}

int DrawupGradientRecord(FontGraphicsFrame* graphicsObject, std::uint32_t mode, const void* color1,
        const void* color2, void** output, float extent0, float extent1, float x0, float y0,
        float x1, float y1, std::uint8_t type) {
    if (output) *output = nullptr;
    auto* const frame = reinterpret_cast<FontDrawupFrame*>(graphicsObject);
    if (!frame || frame->tag != DrawupFrameTag)
        return DrawupFrameError(frame, FONT_GRAPHICS_ERROR_INVALID_OBJECT);
    if (frame->error)
        return DrawupRecordError(frame, FONT_GRAPHICS_ERROR_STICKY);
    const auto* const c1 = static_cast<const float*>(color1);
    const auto* const c2 = static_cast<const float*>(color2);
    if (0.0f >= extent0 || 0.0f >= extent1 || mode > 2 || !c1 || !c2 || !DrawupColorValid(c1) || !DrawupColorValid(c2) || !output)
        return DrawupRecordError(frame, SCE_FONT_ERROR_INVALID_PARAMETER);
    if (!frame->object)
        return DrawupRecordError(frame, FONT_GRAPHICS_ERROR_UNINITIALIZED);
    if (frame->sequence == DrawupPoolSealed)
        return DrawupRecordError(frame, FONT_GRAPHICS_ERROR_SEALED);
    int error = 0;
    FontDrawupRecord* const record = DrawupAllocate(frame, 0, &error);
    if (!record) return error;
    record->type = type;
    record->gradient.color1 = DrawupPackColor(c1);
    record->gradient.color2 = DrawupPackColor(c2);
    record->gradient.extent0 = extent0;
    record->gradient.extent1 = extent1;
    record->gradient.mode = mode;
    record->gradient.unused = 0;
    record->gradient.values[0] = x0;
    record->gradient.values[1] = y0;
    record->gradient.values[2] = x1;
    record->gradient.values[3] = y1;
    if (type == DrawupTypeRadialGradient) record->gradient.tail = 0;
    DrawupEnqueue(frame, record);
    *output = record;
    return SCE_FONT_OK;
}

int GraphicsDrawupFillFlatColor(FontGraphicsFrame* graphicsObject, const void* color, void** output) {
    if (output) *output = nullptr;
    auto* const frame = reinterpret_cast<FontDrawupFrame*>(graphicsObject);
    if (!frame || frame->tag != DrawupFrameTag)
        return DrawupFrameError(frame, FONT_GRAPHICS_ERROR_INVALID_OBJECT);
    if (frame->error)
        return DrawupRecordError(frame, FONT_GRAPHICS_ERROR_STICKY);
    const auto* const channels = static_cast<const float*>(color);
    if (!channels || !DrawupColorValid(channels) || !output)
        return DrawupRecordError(frame, SCE_FONT_ERROR_INVALID_PARAMETER);
    if (!frame->object)
        return DrawupRecordError(frame, FONT_GRAPHICS_ERROR_UNINITIALIZED);
    if (frame->sequence == DrawupPoolSealed)
        return DrawupRecordError(frame, FONT_GRAPHICS_ERROR_SEALED);
    int error = 0;
    FontDrawupRecord* const record = DrawupAllocate(frame, 0, &error);
    if (!record) return error;
    record->type = DrawupTypeFlatColor;
    record->flat.color = DrawupPackColor(channels);
    DrawupEnqueue(frame, record);
    *output = record;
    return SCE_FONT_OK;
}

int GraphicsDrawupFillLinearGradient(FontGraphicsFrame* graphicsObject, std::uint32_t mode,
        const void* color1, const void* color2, void** output,
        float extent0, float extent1, float x0, float y0, float x1, float y1) {
    return DrawupGradientRecord(graphicsObject, mode, color1, color2, output,
            extent0, extent1, x0, y0, x1, y1, DrawupTypeLinearGradient);
}

int GraphicsDrawupFillRadialGradient(FontGraphicsFrame* graphicsObject, std::uint32_t mode,
        const void* color1, const void* color2, void** output,
        float extent0, float extent1, float x0, float y0, float x1, float y1) {
    return DrawupGradientRecord(graphicsObject, mode, color1, color2, output,
            extent0, extent1, x0, y0, x1, y1, DrawupTypeRadialGradient);
}

int GraphicsDrawupFillTextureImageObject(FontGraphicsFrame* graphicsObject, const void* fillObject,
        std::uint32_t mode, void** output, float x, float y, float width, float height) {
    if (output) *output = nullptr;
    auto* const frame = reinterpret_cast<FontDrawupFrame*>(graphicsObject);
    if (!frame || frame->tag != DrawupFrameTag)
        return DrawupFrameError(frame, FONT_GRAPHICS_ERROR_INVALID_OBJECT);
    if (frame->error)
        return DrawupRecordError(frame, FONT_GRAPHICS_ERROR_STICKY);
    if (!output || !fillObject || !mode || width <= 0.0f || height <= 0.0f)
        return DrawupRecordError(frame, SCE_FONT_ERROR_INVALID_PARAMETER);
    if (!frame->object)
        return DrawupRecordError(frame, FONT_GRAPHICS_ERROR_UNINITIALIZED);
    if (frame->sequence == DrawupPoolSealed)
        return DrawupRecordError(frame, FONT_GRAPHICS_ERROR_SEALED);
    int error = 0;
    FontDrawupRecord* const record = DrawupAllocate(frame, mode, &error);
    if (!record) return error;
    record->type = DrawupTypeTextureImage;
    record->texture.state = 0;
    record->texture.mode = mode;
    record->texture.payload = reinterpret_cast<std::uint8_t*>(record) + DrawupRecordHeader;
    record->texture.quad[0] = x;
    record->texture.quad[1] = y;
    record->texture.quad[2] = x + width;
    record->texture.quad[3] = y + height;
    std::memcpy(record->texture.payload, fillObject, mode);
    DrawupEnqueue(frame, record);
    *output = record;
    return SCE_FONT_OK;
}

int GraphicsDrawupFillTexturePatternObject(FontGraphicsFrame* graphicsObject, const void* fillObject,
        std::uint32_t mode, std::uint32_t format, std::uint32_t select, std::uint32_t dimension1,
        std::uint32_t option1, std::uint32_t option2, std::uint32_t dimension2, void** output) {
    constexpr std::uint32_t FormatBase[] = {0x100, 0x300, 0x400, 0x600, 0x200};
    constexpr std::uint32_t SelectBase[] = {4, 1, 2, 3};
    constexpr std::uint32_t Option1Flags[] = {0x1000, 0x3000, 0x4000, 0x6000, 0x2000};
    constexpr std::uint32_t Option2Flags[] = {0x40, 0x10, 0x20, 0x30};
    std::uint32_t formatWord = format < 5 ? FormatBase[format] : 0;
    if (option1 < 5) formatWord |= Option1Flags[option1];
    std::uint32_t selectWord = select < 4 ? SelectBase[select] : 0;
    if (option2 < 4) selectWord |= Option2Flags[option2];
    if (output) *output = nullptr;
    auto* const frame = reinterpret_cast<FontDrawupFrame*>(graphicsObject);
    if (!frame || frame->tag != DrawupFrameTag)
        return DrawupFrameError(frame, FONT_GRAPHICS_ERROR_INVALID_OBJECT);
    if (frame->error)
        return DrawupRecordError(frame, FONT_GRAPHICS_ERROR_STICKY);
    if (!output || ((dimension2 - 1u) | (dimension1 - 1u)) > 0xff || !formatWord || !selectWord)
        return DrawupRecordError(frame, SCE_FONT_ERROR_INVALID_PARAMETER);
    if (!frame->object)
        return DrawupRecordError(frame, FONT_GRAPHICS_ERROR_UNINITIALIZED);
    if (frame->sequence == DrawupPoolSealed)
        return DrawupRecordError(frame, FONT_GRAPHICS_ERROR_SEALED);
    int error = 0;
    FontDrawupRecord* const record = DrawupAllocate(frame, mode, &error);
    if (!record) return error;
    record->type = DrawupTypeTexturePattern;
    record->texture.state = 0;
    record->texture.mode = mode;
    record->texture.payload = reinterpret_cast<std::uint8_t*>(record) + DrawupRecordHeader;
    auto* const params = reinterpret_cast<std::uint32_t*>(record->texture.quad);
    params[0] = formatWord;
    params[1] = selectWord;
    auto* const dims = reinterpret_cast<std::uint8_t*>(&params[2]);
    dims[0] = static_cast<std::uint8_t>(dimension1 - 1);
    dims[1] = static_cast<std::uint8_t>(dimension2 - 1);
    std::memcpy(record->texture.payload, fillObject, mode);
    DrawupEnqueue(frame, record);
    *output = record;
    return SCE_FONT_OK;
}

int GraphicsAppendGradientSegment(FontGraphicsFrame* graphicsObject, void* gradient,
        float position, const void* color) {
    auto* const frame = reinterpret_cast<FontDrawupFrame*>(graphicsObject);
    if (!frame || frame->tag != DrawupFrameTag)
        return DrawupFrameError(frame, FONT_GRAPHICS_ERROR_INVALID_OBJECT);
    if (frame->error)
        return DrawupRecordError(frame, FONT_GRAPHICS_ERROR_STICKY);
    if (reinterpret_cast<std::uintptr_t>(gradient) + 1 < 2)
        return DrawupRecordError(frame, FONT_GRAPHICS_ERROR_INVALID_GRADIENT);
    auto* const record = reinterpret_cast<FontSegmentLink*>(gradient);
    if (record->type != DrawupTypeLinearGradient && record->type != DrawupTypeRadialGradient)
        return DrawupRecordError(frame, FONT_GRAPHICS_ERROR_INVALID_FILL);
    if (0.0f > position || position > 1.0f)
        return DrawupRecordError(frame, SCE_FONT_ERROR_INVALID_PARAMETER);
    const auto* const rgba = static_cast<const float*>(color);
    if (!rgba || !DrawupColorValid(rgba))
        return DrawupRecordError(frame, SCE_FONT_ERROR_INVALID_PARAMETER);
    FontSegmentLink* previous = record;
    FontSegmentLink* next = record->next;
    while (next && next->epoch == record->epoch
            && position >= reinterpret_cast<FontSegmentStop*>(next)->position) {
        previous = next;
        next = next->next;
    }
    FontSegmentStop stop{};
    stop.type = record->type;
    stop.epoch = previous->epoch;
    stop.next = reinterpret_cast<FontSegmentStop*>(next);
    stop.position = position;
    stop.color = DrawupPackColor(rgba);
    auto* const object = frame->object;
    auto* const arena = &object->resource->arena;
    if (!frame->unitSize)
        return DrawupRecordError(frame, FONT_GRAPHICS_ERROR_NO_SEGMENT_SPACE);
    std::uint32_t offset = frame->unitUsed % frame->unitSize;
    if (!frame->unit) offset = 0;
    const std::uint32_t space = frame->unitSize - offset;
    if (space <= 0x17) {
        const std::uint8_t pad[] = {0xff, static_cast<std::uint8_t>(space), 0, 0};
        SegmentWrite(frame, arena, pad, sizeof(pad), nullptr);
        offset = frame->unitUsed % frame->unitSize;
        if (frame->unit && offset) {
            const std::uint32_t tail = frame->unitSize - offset;
            std::uint8_t* const tailTarget = frame->unit + offset + 8;
            std::uint8_t* const limit = arena->memory + static_cast<std::size_t>(arena->total) * arena->slotSize;
            const std::size_t tailRoom = tailTarget < limit ? static_cast<std::size_t>(limit - tailTarget) : 0;
            std::memset(tailTarget, 0, tail < tailRoom ? tail : tailRoom);
            frame->unitUsed += tail;
        }
    }
    std::uint8_t* first = nullptr;
    if (!SegmentWrite(frame, arena, reinterpret_cast<const std::uint8_t*>(&stop),
            sizeof(stop), &first))
        return DrawupRecordError(frame, FONT_GRAPHICS_ERROR_NO_SEGMENT_SPACE);
    previous->next = reinterpret_cast<FontSegmentLink*>(first);
    return SCE_FONT_OK;
}

}

namespace {

constexpr int FONT_GRAPHICS_ERROR_INVALID_PARAMETER = static_cast<int>(0x80460002);
constexpr int FONT_GRAPHICS_ERROR_ALLOCATION_FAILED = static_cast<int>(0x80460010);
constexpr int FONT_GRAPHICS_ERROR_INVALID_SERVICE = static_cast<int>(0x80460080);
constexpr std::uint16_t ServiceMemoryTag = 0xf00;
constexpr std::uint16_t ServiceTag = 0xf0e;
constexpr std::uint16_t BackendTag = 0xe08;
constexpr std::uint32_t BackendLevel = 0x100;
constexpr std::uint32_t BackendRotatedOpcode = 0x47000004;
constexpr std::uint32_t InstanceReferencesExhausted = 0xffffffff;
constexpr std::uint64_t BlockLargeAlignment = 0x200000;
constexpr std::uint64_t BlockSmallAlignment = 0x10000;
constexpr std::uintptr_t BlockAddressHint = 0x200000000;
constexpr int BlockProtection = 0x33;
constexpr KernelUseconds SlotRetryMicroseconds = 30;

struct FontGraphicsBlock {
    void* address;
    std::uint64_t size;
    std::int64_t physical;
    PthreadMutex mutex;
};

struct FontGraphicsInstance {
    std::uint32_t reserved0;
    std::uint32_t references;
    std::uint8_t reserved8[0xc];
    std::uint8_t active;
    std::uint8_t reserved15[3];
    FontGraphicsBlock block;
    std::uint64_t dispatch[0x1c];
};

static_assert(offsetof(FontGraphicsInstance, references) == 0x04);
static_assert(offsetof(FontGraphicsInstance, active) == 0x14);
static_assert(offsetof(FontGraphicsInstance, block) == 0x18);
static_assert(offsetof(FontGraphicsInstance, dispatch) == 0x38);
static_assert(sizeof(FontGraphicsInstance) == 0x118);

struct FontGraphicsServiceData {
    std::uint16_t tag;
    std::uint16_t revision;
    std::uint8_t reserved4[0xc];
    FontMemory memory;
    FontMemoryInterface iface;
    void** instanceSlot;
    std::uint8_t reserved88[0x78];
};

static_assert(offsetof(FontGraphicsServiceData, memory) == 0x10);
static_assert(offsetof(FontGraphicsServiceData, iface) == 0x50);
static_assert(offsetof(FontGraphicsServiceData, instanceSlot) == 0x80);
static_assert(sizeof(FontGraphicsServiceData) == 0x100);

using BackendInitFunction = int (APS5_VABI*)(void* instance, void* data, std::uint64_t size);
using BackendTerminateFunction = void (APS5_VABI*)(void* instance);

struct BackendDispatchSlot {
    std::uint32_t opcode;
    std::uint32_t index;
};

constexpr BackendDispatchSlot BackendDispatchSlots[] = {
    {0x47000001, 0}, {0x47000002, 1}, {0x47000003, 2}, {0x47000004, 3}, {0x47000005, 4}, {0x4700000b, 5}, {0x4700002d, 6},
    {0x4700002e, 7}, {0x470000c0, 8}, {0x470000cb, 9}, {0x470000ce, 10}, {0x470000d1, 11}, {0x470000d2, 12}, {0x470000d4, 13},
    {0x470000e0, 14}, {0x470000e1, 15}, {0x470000e2, 16}, {0x470000ec, 17}, {0x470000d0, 18}, {0x470000e3, 19}, {0x470000f0, 20}};

int APS5_VABI FillBackendDispatch(void* dispatch, const FontGraphicsBackendList* list) {
    auto* const slots = static_cast<std::uint64_t*>(dispatch);
    std::memset(slots, 0, sizeof(FontGraphicsInstance::dispatch));
    for (std::int32_t i = 0; i < list->count; ++i) {
        const FontGraphicsBackendEntry& entry = list->entries[i];
        if (entry.value == 0) continue;
        for (const BackendDispatchSlot& slot : BackendDispatchSlots)
            if (slot.opcode == entry.opcode)
                slots[slot.index] = entry.opcode == BackendRotatedOpcode ? std::rotr(entry.value, 32) : entry.value;
    }
    return slots[0] != 0 && slots[1] != 0 ? SCE_FONT_OK : FONT_GRAPHICS_ERROR_UNINITIALIZED;
}

FontGraphicsInstance* LockInstanceSlot(void** slot) {
    void* const locked = reinterpret_cast<void*>(~std::uintptr_t{0});
    std::atomic_ref<void*> value(*slot);
    for (;;) {
        void* current = value.load();
        if (current != locked && value.compare_exchange_strong(current, locked)) return static_cast<FontGraphicsInstance*>(current);
        sceKernelUsleep_nid_postfix(SlotRetryMicroseconds);
    }
}

void UnlockInstanceSlot(void** slot, FontGraphicsInstance* instance) {
    std::atomic_ref<void*>(*slot).store(instance);
}

PthreadMutex CreateBlockMutex() {
    PthreadMutexattr attr = nullptr;
    if (scePthreadMutexattrInit(&attr) != 0) return nullptr;
    PthreadMutex mutex = nullptr;
    int error = scePthreadMutexattrSetprotocol(&attr, 1);
    if (error == 0) error = scePthreadMutexattrSettype(&attr, 2);
    if (error == 0) error = scePthreadMutexInit(&mutex, &attr, "SceFontMutex");
    scePthreadMutexattrDestroy(&attr);
    return error == 0 ? mutex : nullptr;
}

int AllocateBlock(FontGraphicsBlock& block, std::uint32_t size) {
    const std::uint64_t alignment =
        (size & (BlockLargeAlignment - 1)) == 0 && size >= BlockLargeAlignment ? BlockLargeAlignment : BlockSmallAlignment;
    const std::uint64_t length = (size + alignment - 1) & ~(alignment - 1);
    std::int64_t physical = 0;
    if (sceKernelAllocateDirectMemory(0, static_cast<std::int64_t>(sceKernelGetDirectMemorySize()), length, alignment, 0, &physical) != 0)
        return FONT_GRAPHICS_ERROR_ALLOCATION_FAILED;
    void* address = reinterpret_cast<void*>(BlockAddressHint);
    if (sceKernelMapDirectMemory(&address, length, BlockProtection, 0, physical, alignment) != 0) {
        if (length != 0) sceKernelReleaseDirectMemory(physical, length);
        return FONT_GRAPHICS_ERROR_ALLOCATION_FAILED;
    }
    if (address == nullptr) return FONT_GRAPHICS_ERROR_ALLOCATION_FAILED;
    block = {address, length, physical, CreateBlockMutex()};
    return SCE_FONT_OK;
}

void FreeBlock(FontGraphicsBlock& block) {
    scePthreadMutexLock(&block.mutex);
    if (block.size != 0) {
        sceKernelReleaseDirectMemory(block.physical, block.size);
        block.address = nullptr;
        block.physical = 0;
    }
    block.size = 0;
    scePthreadMutexUnlock(&block.mutex);
    scePthreadMutexDestroy(&block.mutex);
    block.mutex = nullptr;
}

int CreateInstance(const FontGraphicsServiceParam* param, FontGraphicsInstance*& instance) {
    FontGraphicsBlock block{};
    if (const int error = AllocateBlock(block, param->instanceSize); error != SCE_FONT_OK) return error;
    auto* const created = static_cast<FontGraphicsInstance*>(block.address);
    std::memset(created, 0, sizeof(*created));
    created->active = 1;
    created->block = block;
    param->enumerate(created->dispatch, FillBackendDispatch);
    const auto init = reinterpret_cast<BackendInitFunction>(created->dispatch[0]);
    int initialized = 0;
    try {
        initialized = init(created, created + 1, param->instanceSize - sizeof(FontGraphicsInstance));
    } catch (...) {
        FreeBlock(block);
        throw;
    }
    if (initialized != 0) {
        FreeBlock(block);
        return FONT_GRAPHICS_ERROR_UNINITIALIZED;
    }
    ++created->references;
    instance = created;
    return SCE_FONT_OK;
}

int AttachInstance(const FontGraphicsServiceParam* param) {
    void** const slot = param->instanceSlot;
    if (slot == nullptr || param->instanceSize <= sizeof(FontGraphicsInstance)) return FONT_GRAPHICS_ERROR_INVALID_PARAMETER;
    FontGraphicsInstance* instance = LockInstanceSlot(slot);
    struct SlotRelease {
        void** slot;
        FontGraphicsInstance*& instance;
        ~SlotRelease() { UnlockInstanceSlot(slot, instance); }
    } release{slot, instance};
    int error = SCE_FONT_OK;
    if (instance == nullptr) error = CreateInstance(param, instance);
    else if (instance->references == InstanceReferencesExhausted) error = FONT_GRAPHICS_ERROR_UNINITIALIZED;
    else ++instance->references;
    return error;
}

void DetachInstance(void** slot) {
    FontGraphicsInstance* instance = LockInstanceSlot(slot);
    if (instance != nullptr && instance->references != 0) {
        if (--instance->references == 0) {
            FontGraphicsBlock block = instance->block;
            reinterpret_cast<BackendTerminateFunction>(instance->dispatch[1])(instance);
            FreeBlock(block);
        }
        instance = nullptr;
    }
    UnlockInstanceSlot(slot, instance);
}

constexpr int FONT_GRAPHICS_ERROR_INVALID_DEVICE = static_cast<int>(0x80460081);
constexpr int FONT_GRAPHICS_ERROR_INVALID_SURFACE = static_cast<int>(0x80460092);
constexpr std::uint16_t DeviceTag = 0xf50;
constexpr std::uint16_t SurfaceTextureTag = 0xf82;
constexpr std::uint16_t SurfaceTag = 0x47e0;
constexpr std::uint32_t SurfaceSize = 0x100;
constexpr std::uint32_t SurfaceTextureHeader = 0x40;
constexpr std::uint8_t SurfaceOwnsMemory = 2;
constexpr std::size_t DrawupRecordPayload = 0x50;

struct FontGraphicsDeviceData {
    std::uint16_t tag;
    std::uint8_t reserved2[0x76];
    FontGraphicsServiceData* service;
    std::uint8_t reserved80[0xd0];
    PthreadMutex* surfaceMutex;
    std::uint8_t reserved158[8];
    FontDrawupPool* pool;
    FontDrawupPool* payloadPool;
};

static_assert(offsetof(FontGraphicsDeviceData, service) == 0x78);
static_assert(offsetof(FontGraphicsDeviceData, surfaceMutex) == 0x150);
static_assert(offsetof(FontGraphicsDeviceData, pool) == 0x160 && offsetof(FontGraphicsDeviceData, payloadPool) == 0x168);

struct FontSurfaceTexture {
    std::uint16_t tag;
    std::uint16_t reserved2;
    std::uint32_t state;
    FontGraphicsDeviceData* device;
    FontDrawupRecord* records;
    std::uint8_t* surface;
    std::uint8_t reserved20[0x20];
};

static_assert(offsetof(FontSurfaceTexture, device) == 0x08 && offsetof(FontSurfaceTexture, records) == 0x10);
static_assert(offsetof(FontSurfaceTexture, surface) == 0x18 && sizeof(FontSurfaceTexture) == SurfaceTextureHeader);

using BackendStructureTextureFunction = int (APS5_VABI*)(void* texture, void* surface, std::uint64_t base);

std::uint32_t BackendTextureSize(const FontGraphicsInstance* instance) {
    return static_cast<std::uint32_t>(instance->dispatch[3] >> 32) + SurfaceTextureHeader;
}

int DeviceInstance(const FontGraphicsDeviceData* device, int invalid, FontGraphicsInstance*& instance) {
    if (!IsObject(device) || device->tag != DeviceTag || device->service->tag != ServiceTag) return invalid;
    instance = static_cast<FontGraphicsInstance*>(*device->service->instanceSlot);
    return instance == nullptr ? FONT_GRAPHICS_ERROR_UNINITIALIZED : SCE_FONT_OK;
}

void* PoolAllocate(FontDrawupPool* pool, std::uint32_t size) {
    if (scePthreadMutexLock(pool->mutex) != 0) return nullptr;
    FontDrawupPoolEntry* entry = pool->entries;
    FontDrawupPoolEntry* const end = pool->entries + pool->entryCount;
    FontDrawupRecord* block = nullptr;
    std::uint32_t slots = 0;
    do {
        slots = static_cast<std::uint32_t>((std::uint64_t{size} - 1 + entry->slotSize) / entry->slotSize);
        block = DrawupPoolAllocate(entry, slots);
        ++entry;
    } while (block == nullptr && entry < end);
    if (block != nullptr) {
        pool->used += slots;
        if (pool->used > pool->maximum) pool->maximum = pool->used;
    }
    scePthreadMutexUnlock(pool->mutex);
    return block;
}

std::uint32_t DrawupPoolFree(FontDrawupPoolEntry* entry, const std::uint8_t* pointer, std::uint32_t slots) {
    if (entry == nullptr || entry->memory > pointer) return 0;
    const auto index = static_cast<std::uint32_t>(
        (reinterpret_cast<std::uintptr_t>(pointer) - reinterpret_cast<std::uintptr_t>(entry->memory)) / entry->slotSize);
    const std::uint32_t total = entry->total;
    if (total <= index) return 0;
    std::uint16_t* const nodes = entry->nodes;
    std::uint16_t* marker = entry->scan;
    std::uint16_t* node = marker;
    std::uint16_t* previous = marker;
    std::uint32_t nodeIndex = 0;
    std::uint32_t word = *node;
    std::uint32_t next = word >> 1;
    if (word >= 2 && index >= next) {
        for (std::uint32_t remaining = total;;) {
            if (--remaining == 0) return 0;
            previous = node;
            nodeIndex = next;
            node = nodes + (next & 0xffff);
            word = *node;
            next = word >> 1;
            if (word < 2 || index < next) break;
        }
    }
    const std::uint32_t allocated = word & 1;
    if (allocated == 0) return 0;
    const auto end = static_cast<std::int32_t>(index + slots);
    const auto signedTotal = static_cast<std::int32_t>(total);
    std::uint16_t* target = previous;
    std::uint32_t value = 0;
    bool link = true;
    if (nodeIndex == index) {
        if (end < static_cast<std::int32_t>(next)) {
            if (index != 0) marker = nodes + index;
            nodes[end] = static_cast<std::uint16_t>((nodes[end] & 1) | (word & ~1u));
            value = static_cast<std::uint32_t>(end) * 2;
        } else if (index != 0) {
            const std::uint32_t following = end < signedTotal ? nodes[end] >> 1 : static_cast<std::uint32_t>(end);
            marker = nodes + index;
            value = following * 2;
        } else if (end < signedTotal) {
            value = nodes[end] & ~1u;
            target = marker;
        } else {
            link = false;
        }
    } else {
        marker = nodes + index;
        *node = static_cast<std::uint16_t>(allocated | index * 2);
        if (end < static_cast<std::int32_t>(next)) {
            *marker = static_cast<std::uint16_t>((*marker & 1) + static_cast<std::uint32_t>(end) * 2);
            value = word & 0xfffe;
            target = nodes + end;
        } else {
            value = end < signedTotal ? nodes[end] & ~1u : static_cast<std::uint32_t>(end) * 2;
            target = marker;
        }
    }
    if (link) *target = static_cast<std::uint16_t>((*target & 1) | value);
    entry->used -= slots;
    *reinterpret_cast<std::uint8_t*>(marker) &= 0xfe;
    return slots;
}

int PoolFree(FontDrawupPool* pool, const void* pointer, std::uint32_t size) {
    if (scePthreadMutexLock(pool->mutex) != 0) return FONT_GRAPHICS_ERROR_UNINITIALIZED;
    int result = FONT_GRAPHICS_ERROR_UNINITIALIZED;
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    const auto first = reinterpret_cast<std::uintptr_t>(pool->entries[0].memory);
    if (first <= address && address < reinterpret_cast<std::uintptr_t>(pool->limit)) {
        FontDrawupPoolEntry* const entry = &pool->entries[(address - first) / pool->span];
        const std::uint64_t slotSize = entry->slotSize;
        const auto slots = static_cast<std::uint32_t>((std::uint64_t{size} + slotSize - 1) / slotSize);
        const auto memory = reinterpret_cast<std::uintptr_t>(entry->memory);
        if ((address - memory) % slotSize == 0 && memory + std::uint64_t{entry->total - slots} * slotSize > address) {
            const std::uint32_t freed = DrawupPoolFree(entry, static_cast<const std::uint8_t*>(pointer), slots);
            if (freed != 0) result = SCE_FONT_OK;
            pool->used -= freed;
        }
    }
    scePthreadMutexUnlock(pool->mutex);
    return result;
}

int ReleaseSurfaceTexture(void** object) {
    auto* const texture = static_cast<FontSurfaceTexture*>(*object);
    FontGraphicsDeviceData* const device = texture->device;
    FontGraphicsInstance* instance = nullptr;
    if (const int error = DeviceInstance(device, FONT_GRAPHICS_ERROR_UNINITIALIZED, instance); error != SCE_FONT_OK) return Fail(error);
    FontDrawupRecord* record = texture->records;
    std::uint8_t* const surface = texture->surface;
    texture->tag = 0;
    while (record != nullptr) {
        FontDrawupRecord* const next = record->next;
        void* payload;
        std::memcpy(&payload, reinterpret_cast<const std::uint8_t*>(record) + DrawupRecordPayload, sizeof(payload));
        if ((record->type | 0x10) == DrawupTypeTexturePattern && payload != nullptr && device->payloadPool != nullptr)
            PoolFree(device->payloadPool, payload, record->flags);
        if (device->pool != nullptr) PoolFree(device->pool, record, record->sizeTag);
        record = next;
    }
    texture->records = nullptr;
    if (surface != nullptr) {
        std::uint64_t memory;
        std::memcpy(&memory, surface, sizeof(memory));
        if ((surface[0x0e] & SurfaceOwnsMemory) != 0 && memory != 0) NotImplemented_nid_no_patch("sceFontGraphicsRelease");
        if (device->pool == nullptr) {
            *object = nullptr;
            return SCE_FONT_OK;
        }
        PoolFree(device->pool, surface, SurfaceSize);
    }
    if (device->pool != nullptr) PoolFree(device->pool, texture, BackendTextureSize(instance));
    *object = nullptr;
    return SCE_FONT_OK;
}

bool IsUsableMemory(const FontMemory* memory) {
    return memory != nullptr && memory->mem_kind == ServiceMemoryTag && memory->iface != nullptr && memory->iface->alloc != nullptr &&
           memory->iface->dealloc != nullptr;
}

}

namespace Font {

int CreateGraphicsService(const FontMemory* memory, const FontGraphicsServiceParam* param, FontGraphicsService** service) {
    if (service != nullptr) *service = nullptr;
    if (!IsUsableMemory(memory) || param == nullptr || param->tag != BackendTag || service == nullptr)
        return Fail(FONT_GRAPHICS_ERROR_INVALID_PARAMETER);
    if (param->level < BackendLevel) return Fail(FONT_GRAPHICS_ERROR_UNINITIALIZED);
    auto* const data = static_cast<FontGraphicsServiceData*>(memory->iface->alloc(memory->mspace_handle, sizeof(FontGraphicsServiceData)));
    if (data == nullptr) return Fail(FONT_GRAPHICS_ERROR_ALLOCATION_FAILED);
    std::memset(data, 0, sizeof(*data));
    if (const int error = AttachInstance(param); error != SCE_FONT_OK) {
        memory->iface->dealloc(memory->mspace_handle, data);
        return Fail(error);
    }
    data->tag = ServiceTag;
    data->revision = param->revision;
    data->memory = *memory;
    data->iface = *memory->iface;
    data->memory.iface = &data->iface;
    data->instanceSlot = param->instanceSlot;
    *service = reinterpret_cast<FontGraphicsService*>(data);
    return SCE_FONT_OK;
}

int DestroyGraphicsService(FontGraphicsService** service) {
    if (service == nullptr) return Fail(FONT_GRAPHICS_ERROR_INVALID_PARAMETER);
    auto* const data = reinterpret_cast<FontGraphicsServiceData*>(*service);
    if (data == nullptr || !IsUsableMemory(&data->memory)) return Fail(FONT_GRAPHICS_ERROR_INVALID_SERVICE);
    if (data->instanceSlot != nullptr) DetachInstance(data->instanceSlot);
    data->memory.iface->dealloc(data->memory.mspace_handle, data);
    *service = nullptr;
    return SCE_FONT_OK;
}

int GraphicsStructureSurfaceTexture(FontGraphicsDevice* device, const FontGraphicsSurface* surface, FontGraphicsSurfaceTexture** texture) {
    if (texture != nullptr) *texture = nullptr;
    auto* const data = reinterpret_cast<FontGraphicsDeviceData*>(device);
    FontGraphicsInstance* instance = nullptr;
    if (const int error = DeviceInstance(data, FONT_GRAPHICS_ERROR_INVALID_DEVICE, instance); error != SCE_FONT_OK) return Fail(error);
    const auto* const source = reinterpret_cast<const std::uint8_t*>(surface);
    if (source == nullptr) return Fail(FONT_GRAPHICS_ERROR_INVALID_SURFACE);
    std::uint16_t surfaceTag;
    std::uint32_t fillObject;
    std::memcpy(&surfaceTag, source + 0x0e, sizeof(surfaceTag));
    std::memcpy(&fillObject, source + 0x28, sizeof(fillObject));
    if ((surfaceTag & 0xfff0) != SurfaceTag || fillObject == 0) return Fail(FONT_GRAPHICS_ERROR_INVALID_SURFACE);
    if (texture == nullptr) return Fail(FONT_GRAPHICS_ERROR_INVALID_PARAMETER);
    if (data->pool == nullptr) return Fail(FONT_GRAPHICS_ERROR_NO_RECORD_SPACE);
    std::uint64_t base;
    std::memcpy(&base, source, sizeof(base));
    const std::uint32_t size = BackendTextureSize(instance);
    auto* const copy = static_cast<std::uint8_t*>(PoolAllocate(data->pool, SurfaceSize));
    if (copy == nullptr || data->pool == nullptr) return Fail(FONT_GRAPHICS_ERROR_NO_RECORD_SPACE);
    auto* const object = static_cast<FontSurfaceTexture*>(PoolAllocate(data->pool, size));
    if (object == nullptr) {
        if (data->pool != nullptr) PoolFree(data->pool, copy, SurfaceSize);
        return Fail(FONT_GRAPHICS_ERROR_NO_RECORD_SPACE);
    }
    if (base == 0) NotImplemented_nid_no_patch("sceFontGraphicsStructureSurfaceTexture");
    std::memcpy(copy, source, SurfaceSize);
    std::memset(object, 0, sizeof(*object));
    object->tag = SurfaceTextureTag;
    object->device = data;
    object->records = nullptr;
    object->surface = copy;
    const auto structure = reinterpret_cast<BackendStructureTextureFunction>(instance->dispatch[4]);
    object->state = static_cast<std::uint32_t>(structure(object, copy + SurfaceTextureHeader, base));
    *texture = reinterpret_cast<FontGraphicsSurfaceTexture*>(object);
    return SCE_FONT_OK;
}

FontGraphicsSurface* GraphicsTextureRefersSurface(const FontGraphicsSurfaceTexture* texture) {
    const auto* const object = reinterpret_cast<const FontSurfaceTexture*>(texture);
    if (object == nullptr || object->tag != SurfaceTextureTag) return nullptr;
    return reinterpret_cast<FontGraphicsSurface*>(object->surface);
}

int GraphicsRelease(void** object) {
    if (object == nullptr || *object == nullptr) return Fail(FONT_GRAPHICS_ERROR_INVALID_PARAMETER);
    switch (*static_cast<const std::uint16_t*>(*object)) {
    case 0xf51:
    case 0xf52:
    case 0xf70:
    case 0xf80:
    case 0xf81:
        NotImplemented_nid_no_patch("sceFontGraphicsRelease");
    case SurfaceTextureTag:
        return ReleaseSurfaceTexture(object);
    default:
        return Fail(FONT_GRAPHICS_ERROR_INVALID_PARAMETER);
    }
}

}
