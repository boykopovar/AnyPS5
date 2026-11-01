#include <atomic>
#include <cstdint>
#include <cstring>

#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libSceFont/include/FontGraphics.hpp"
#include "prx/libSceFont/include/FontTypes.hpp"

namespace {

constexpr int FONT_GRAPHICS_ERROR_UNINITIALIZED = static_cast<int>(0x80460001);
constexpr int FONT_GRAPHICS_ERROR_SEALED = static_cast<int>(0x8046005c);
constexpr int FONT_GRAPHICS_ERROR_INVALID_OBJECT = static_cast<int>(0x80460084);
constexpr int FONT_GRAPHICS_ERROR_INVALID_FILL = static_cast<int>(0x80460086);
constexpr int FONT_GRAPHICS_ERROR_INVALID_REGION = static_cast<int>(0x80460087);
constexpr int FONT_GRAPHICS_ERROR_INVALID_FILL_PLOT = static_cast<int>(0x80460088);
constexpr int FONT_GRAPHICS_ERROR_INVALID_FILL_RATES = static_cast<int>(0x80460089);
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
    std::uint32_t state;
    std::uint32_t mode;
    void* payload;
    float quad[4];
    std::uint8_t reserved[0x50];
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
    std::uint32_t reserved8;
    std::uint32_t entryCount;
    std::uint32_t used;
    std::uint32_t maximum;
    FontDrawupPoolEntry entries[];
};

struct FontDrawupPoolOwner {
    std::uint8_t reserved[0x160];
    FontDrawupPool* pool;
};

struct FontDrawupObject {
    std::uint64_t reserved0;
    FontDrawupPoolOwner* owner;
    std::uint64_t reserved10[3];
    FontDrawupRecord* head;
    FontDrawupRecord* tail;
};

struct FontDrawupFrame {
    std::uint16_t tag;
    std::uint16_t reserved2;
    int error;
    FontDrawupObject* object;
    std::uint64_t reserved10[3];
    std::uint32_t sequence;
};

static_assert(sizeof(FontDrawupRecord) == DrawupRecordHeader);
static_assert(sizeof(FontDrawupPoolEntry) == 0x30);
static_assert(offsetof(FontDrawupPoolEntry, slotSize) == 0x0c);
static_assert(offsetof(FontDrawupPool, entries) == 0x18);
static_assert(offsetof(FontDrawupPoolOwner, pool) == 0x160);
static_assert(offsetof(FontDrawupObject, head) == 0x28 && offsetof(FontDrawupObject, tail) == 0x30);
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

void SetGraphicsErrorCallback(GraphicsErrorCallback callback) {
    g_errorCallback.store(callback);
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
    record->state = 0;
    record->mode = mode;
    record->payload = reinterpret_cast<std::uint8_t*>(record) + DrawupRecordHeader;
    return record;
}

int DrawupFrameError(FontDrawupFrame* frame, int error) {
    return frame ? DrawupRecordError(frame, error) : Fail(error);
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
    record->quad[0] = x;
    record->quad[1] = y;
    record->quad[2] = x + width;
    record->quad[3] = y + height;
    std::memcpy(record->payload, fillObject, mode);
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
    auto* const params = reinterpret_cast<std::uint32_t*>(record->quad);
    params[0] = formatWord;
    params[1] = selectWord;
    auto* const dims = reinterpret_cast<std::uint8_t*>(&params[2]);
    dims[0] = static_cast<std::uint8_t>(dimension1 - 1);
    dims[1] = static_cast<std::uint8_t>(dimension2 - 1);
    std::memcpy(record->payload, fillObject, mode);
    DrawupEnqueue(frame, record);
    *output = record;
    return SCE_FONT_OK;
}

}
