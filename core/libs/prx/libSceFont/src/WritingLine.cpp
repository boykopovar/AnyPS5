#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_set>

#include "prx/libSceFont/include/FontTypes.hpp"

namespace {

constexpr std::int32_t WRITING_FORM_HORIZONTAL_RTL = 0x11;
constexpr std::int32_t WRITING_FORM_HORIZONTAL_LTR = 0x12;
constexpr std::uint64_t WRITING_ATTRIBUTE_ID_MASK = 0xFF;
constexpr std::uint64_t WRITING_ATTRIBUTE_ID = 0xFA;
constexpr std::uint64_t WRITING_ATTRIBUTE_SPACE_BEFORE = std::uint64_t{1} << 30;
constexpr std::uint64_t WRITING_ATTRIBUTE_SPACE_AFTER = std::uint64_t{1} << 31;
constexpr std::size_t WRITING_LEVEL_COUNT = 0x7F;

struct WritingOrder {
    void* orderer = nullptr;
    WritingOrder* prev = nullptr;
    WritingOrder* next = nullptr;
    std::uint64_t reserved = 0;
    std::uint64_t attribute = 0;
    FontWritingMetrics metrics{};
};

struct WritingLevel {
    WritingOrder* first = nullptr;
    WritingOrder* last = nullptr;
};

struct WritingLine {
    std::uint16_t kind = 0xF0A;
    std::uint16_t reserved = 0;
    std::int32_t form = 0;
    const FontMemory* memory = nullptr;
    FontWritingMetrics renderMetrics{};
    FontWritingLineStep step{};
    float advanceX = 0.0f;
    float advanceY = 0.0f;
    float orderingSpace = 0.0f;
    float steppedSpace = 0.0f;
    std::uint32_t lastLevel = 0;
    std::uint32_t reservedLevel = 0;
    WritingOrder* first = nullptr;
    WritingOrder* last = nullptr;
    WritingOrder* head = nullptr;
    WritingOrder* tail = nullptr;
    WritingOrder* cursor = nullptr;
    std::uint64_t reservedCursor = 0;
    std::array<WritingLevel, WRITING_LEVEL_COUNT> levels{};
};

static_assert(sizeof(WritingOrder) == 64);
static_assert(offsetof(WritingOrder, attribute) == 32);
static_assert(offsetof(WritingOrder, metrics) == 40);
static_assert(sizeof(WritingLine) == 2208);
static_assert(offsetof(WritingLine, step) == 40);
static_assert(offsetof(WritingLine, advanceX) == 104);
static_assert(offsetof(WritingLine, last) == 136);
static_assert(offsetof(WritingLine, head) == 144);
static_assert(offsetof(WritingLine, cursor) == 160);
static_assert(offsetof(WritingLine, levels) == 176);

bool IsMemoryValid(const FontMemory* memory) {
    return memory && memory->mem_kind == 0xF00 && memory->iface && memory->iface->alloc && memory->iface->dealloc;
}

std::mutex linesMutex;
std::unordered_set<WritingLine*> lines;

[[noreturn]] void Unsupported(const char* function, const std::string& what) {
    throw std::runtime_error(std::string(function) + ": " + what);
}

bool IsRightToLeft(const WritingLine& line) {
    return line.form == WRITING_FORM_HORIZONTAL_RTL;
}

bool SpaceAfter(const WritingOrder& order) {
    return (order.attribute & WRITING_ATTRIBUTE_SPACE_AFTER) != 0;
}

bool SpaceBefore(const WritingOrder& order) {
    return (order.attribute & WRITING_ATTRIBUTE_SPACE_BEFORE) != 0;
}

bool IsSpaced(const WritingOrder& before, const WritingOrder& after) {
    return SpaceAfter(before) || SpaceBefore(after);
}

float HeadSpace(const WritingLine& line, const WritingOrder& order) {
    return IsRightToLeft(line) ? order.metrics.advanceX - order.metrics.Extent.left : order.metrics.Extent.left;
}

float TailSpace(const WritingLine& line, const WritingOrder& order) {
    return IsRightToLeft(line) ? order.metrics.Extent.right : order.metrics.advanceX - order.metrics.Extent.right;
}

void ResetOrders(WritingLine& line) {
    for (WritingOrder* order = line.head; order;) {
        WritingOrder* next = order->next;
        std::destroy_at(order);
        line.memory->iface->dealloc(line.memory->mspace_handle, order);
        order = next;
    }
    line.first = nullptr;
    line.levels.fill({});
    line.advanceX = 0.0f;
    line.advanceY = 0.0f;
    line.orderingSpace = 0.0f;
    line.last = nullptr;
    line.head = nullptr;
    line.tail = nullptr;
    line.cursor = nullptr;
}

void InsertAfter(WritingOrder* anchor, WritingOrder* order) {
    order->prev = anchor;
    order->next = anchor->next;
    if (anchor->next) anchor->next->prev = order;
    anchor->next = order;
}

void InsertBefore(WritingOrder* anchor, WritingOrder* order) {
    order->next = anchor;
    order->prev = anchor->prev;
    if (anchor->prev) anchor->prev->next = order;
    anchor->prev = order;
}

void Link(WritingLine& line, WritingOrder* order, std::size_t level) {
    WritingLevel& slot = line.levels[level];
    if (!line.last) {
        slot = {order, order};
        return;
    }
    if (level >= line.lastLevel) {
        if (line.lastLevel % 2 == 0) {
            InsertAfter(line.last, order);
        } else {
            InsertBefore(line.last, order);
        }
        if (!slot.last || order->prev == slot.last) slot.last = order;
        if (!slot.first || order->next == slot.first) slot.first = order;
        return;
    }
    WritingOrder* first = line.levels[line.lastLevel].first;
    WritingOrder* last = line.levels[line.lastLevel].last;
    line.levels[line.lastLevel] = {};
    for (std::size_t closing = line.lastLevel - 1; closing > level; --closing) {
        WritingLevel& closed = line.levels[closing];
        if (closing % 2 == 0) {
            if (closed.first) first = closed.first;
        } else {
            if (closed.last) last = closed.last;
        }
        closed = {};
    }
    if (level % 2 == 0) {
        if (!slot.first) slot.first = first;
        InsertAfter(last, order);
        slot.last = order;
    } else {
        if (!slot.last) slot.last = last;
        InsertBefore(first, order);
        slot.first = order;
    }
}

float OrderingSpaceDelta(const WritingLine& line, const WritingOrder& order) {
    const WritingOrder* before = order.prev;
    const WritingOrder* after = order.next;
    const bool split = before && after && IsSpaced(*before, *after);
    const float sign = split ? -1.0f : 1.0f;
    float delta = 0.0f;
    if (before) {
        const bool spaced = IsSpaced(*before, order);
        if (spaced) delta = HeadSpace(line, order);
        if (spaced != split) delta += sign * TailSpace(line, *before);
    }
    if (after) {
        const bool spaced = IsSpaced(order, *after);
        if (spaced) delta += TailSpace(line, order);
        if (spaced != split) delta += sign * HeadSpace(line, *after);
    }
    return delta;
}

void ExtendRenderMetrics(WritingLine& line) {
    const FontWritingMetrics& run = line.step.Metrics;
    const float x = line.step.x + line.step.Adjusting.x;
    const float y = line.step.y + line.step.Adjusting.y;
    line.step.x += run.advanceX;
    line.step.y += run.advanceY;
    FontWritingMetrics& total = line.renderMetrics;
    total.advanceX = x + run.advanceX;
    total.advanceY = y + run.advanceY;
    if (!(run.Extent.left == run.Extent.right)) {
        const float left = x + run.Extent.left;
        const float right = x + run.Extent.right;
        if (total.Extent.left == 0.0f && total.Extent.right == 0.0f) {
            total.Extent.left = left;
            total.Extent.right = right;
        } else {
            if (total.Extent.left > left) total.Extent.left = left;
            if (right > total.Extent.right) total.Extent.right = right;
        }
    }
    if (!(run.Extent.top == run.Extent.bottom)) {
        const float top = y + run.Extent.top;
        const float bottom = y + run.Extent.bottom;
        if (total.Extent.top == 0.0f && total.Extent.bottom == 0.0f) {
            total.Extent.top = top;
            total.Extent.bottom = bottom;
        } else {
            if (total.Extent.top > top) total.Extent.top = top;
            if (bottom > total.Extent.bottom) total.Extent.bottom = bottom;
        }
    }
}

}

#pragma GCC visibility push(default)

extern "C" {

int APS5_VABI sceFontCreateWritingLine(const FontMemory* memory, std::int32_t writingForm, const void* writingLineDetail, void** pWritingLine) {
    if (!memory) return SCE_FONT_ERROR_INVALID_PARAMETER;
    if (!IsMemoryValid(memory)) return SCE_FONT_ERROR_INVALID_MEMORY;
    if (!pWritingLine || writingLineDetail || (writingForm != WRITING_FORM_HORIZONTAL_RTL && writingForm != WRITING_FORM_HORIZONTAL_LTR)) return SCE_FONT_ERROR_INVALID_PARAMETER;
    void* allocation = memory->iface->alloc(memory->mspace_handle, sizeof(WritingLine));
    if (!allocation) return SCE_FONT_ERROR_ALLOCATION_FAILED;
    auto* line = std::construct_at(static_cast<WritingLine*>(allocation));
    line->memory = memory;
    line->form = writingForm;
    {
        std::lock_guard lock(linesMutex);
        try {
            lines.insert(line);
        } catch (...) {
            std::destroy_at(line);
            memory->iface->dealloc(memory->mspace_handle, allocation);
            throw;
        }
    }
    *pWritingLine = line;
    return SCE_FONT_OK;
}

int APS5_VABI sceFontDestroyWritingLine(void** pWritingLine) {
    if (!pWritingLine || !*pWritingLine) return SCE_FONT_ERROR_INVALID_PARAMETER;
    auto* line = static_cast<WritingLine*>(*pWritingLine);
    {
        std::lock_guard lock(linesMutex);
        if (!lines.contains(line) || !IsMemoryValid(line->memory)) return SCE_FONT_ERROR_INVALID_WRITING;
        lines.erase(line);
    }
    const FontMemory* memory = line->memory;
    ResetOrders(*line);
    line->kind = 0;
    std::destroy_at(line);
    memory->iface->dealloc(memory->mspace_handle, line);
    *pWritingLine = nullptr;
    return SCE_FONT_OK;
}

int APS5_VABI sceFontWritingLineClear(void* writingLine) {
    if (!writingLine) return SCE_FONT_ERROR_INVALID_PARAMETER;
    auto* line = static_cast<WritingLine*>(writingLine);
    {
        std::lock_guard lock(linesMutex);
        if (!lines.contains(line)) return SCE_FONT_ERROR_INVALID_WRITING;
    }
    if (!IsMemoryValid(line->memory)) return SCE_FONT_ERROR_INVALID_WRITING;
    ResetOrders(*line);
    return SCE_FONT_OK;
}

int APS5_VABI sceFontWritingLineWritesOrder(void* writingLine, std::uint64_t writingAttribute, const FontWritingMetrics* writingMetrics, void* writingOrderer) {
    const std::size_t level = (writingAttribute >> 8) & 0xFF;
    if (!writingLine || !writingOrderer || (writingAttribute & WRITING_ATTRIBUTE_ID_MASK) != WRITING_ATTRIBUTE_ID || level >= WRITING_LEVEL_COUNT) return SCE_FONT_ERROR_INVALID_PARAMETER;
    auto* line = static_cast<WritingLine*>(writingLine);
    {
        std::lock_guard lock(linesMutex);
        if (!lines.contains(line)) return SCE_FONT_ERROR_INVALID_WRITING;
    }
    if (!IsMemoryValid(line->memory)) return SCE_FONT_ERROR_INVALID_PARAMETER;
    if (line->cursor) ResetOrders(*line);
    void* allocation = line->memory->iface->alloc(line->memory->mspace_handle, sizeof(WritingOrder));
    if (!allocation) return SCE_FONT_ERROR_ALLOCATION_FAILED;
    WritingOrder* order = std::construct_at(static_cast<WritingOrder*>(allocation));
    if (!line->last) line->first = order;
    order->orderer = writingOrderer;
    order->attribute = writingAttribute;
    if (writingMetrics) order->metrics = *writingMetrics;
    Link(*line, order, level);
    if (!order->prev) line->head = order;
    if (!order->next) line->tail = order;
    line->orderingSpace += OrderingSpaceDelta(*line, *order);
    line->advanceX += order->metrics.advanceX;
    line->advanceY += order->metrics.advanceY;
    line->last = order;
    line->lastLevel = static_cast<std::uint32_t>(level);
    return SCE_FONT_OK;
}

const FontWritingLineStep* APS5_VABI sceFontWritingLineRefersRenderStep(void* writingLine) {
    if (!writingLine) return nullptr;
    auto* line = static_cast<WritingLine*>(writingLine);
    {
        std::lock_guard lock(linesMutex);
        if (!lines.contains(line)) return nullptr;
    }
    if (line->cursor) {
        ExtendRenderMetrics(*line);
    } else {
        line->steppedSpace = 0.0f;
        line->step.x = 0.0f;
        line->step.y = 0.0f;
        line->step.spacingProgress = 0.0f;
        line->step.Metrics = {};
        line->renderMetrics = {};
    }
    const bool rightToLeft = IsRightToLeft(*line);
    WritingOrder* next = nullptr;
    if (rightToLeft) {
        next = line->cursor ? line->cursor->prev : line->tail;
    } else {
        next = line->cursor ? line->cursor->next : line->head;
    }
    if (!next) {
        line->cursor = nullptr;
        line->step.Metrics = {};
        return nullptr;
    }
    if (rightToLeft) {
        const WritingOrder* stepped = next->next;
        if (stepped && IsSpaced(*next, *stepped))
            line->steppedSpace += stepped->metrics.advanceX - stepped->metrics.Extent.left + next->metrics.Extent.right;
    } else {
        const WritingOrder* stepped = next->prev;
        if (stepped && IsSpaced(*stepped, *next))
            line->steppedSpace += stepped->metrics.advanceX - stepped->metrics.Extent.right + next->metrics.Extent.left;
    }
    line->cursor = next;
    line->step.Metrics = next->metrics;
    line->step.writingOrderer = next->orderer;
    line->step.Adjusting = {};
    line->step.spacingProgress = 0.0f;
    if (!(line->steppedSpace == 0.0f) && !(line->orderingSpace == 0.0f)) line->step.spacingProgress = line->steppedSpace / line->orderingSpace;
    return &line->step;
}

int APS5_VABI sceFontWritingLineGetRenderMetrics(void* writingLine, FontWritingMetrics* writingMetrics) {
    if (!writingMetrics) return SCE_FONT_ERROR_INVALID_PARAMETER;
    *writingMetrics = {};
    if (!writingLine) return SCE_FONT_ERROR_INVALID_PARAMETER;
    auto* line = static_cast<WritingLine*>(writingLine);
    std::lock_guard lock(linesMutex);
    if (!lines.contains(line)) return SCE_FONT_ERROR_INVALID_WRITING;
    *writingMetrics = line->renderMetrics;
    return SCE_FONT_OK;
}

int APS5_VABI sceFontWritingLineGetOrderingSpace(void* writingLine, float* advanceSpace, float* headSpace, float* inlineSpace, float* tailSpace) {
    auto* line = static_cast<WritingLine*>(writingLine);
    int error = SCE_FONT_OK;
    {
        std::lock_guard lock(linesMutex);
        if (!line) error = SCE_FONT_ERROR_INVALID_PARAMETER;
        else if (!lines.contains(line)) error = SCE_FONT_ERROR_INVALID_WRITING;
        else if (line->form != WRITING_FORM_HORIZONTAL_RTL && line->form != WRITING_FORM_HORIZONTAL_LTR) error = SCE_FONT_ERROR_INVALID_PARAMETER;
    }
    if (error != SCE_FONT_OK) {
        if (advanceSpace) *advanceSpace = 0.0f;
        if (headSpace) *headSpace = 0.0f;
        if (inlineSpace) *inlineSpace = 0.0f;
        if (tailSpace) *tailSpace = 0.0f;
        return error;
    }
    if (headSpace && !tailSpace) Unsupported(__func__, "a head space pointer without a tail space pointer is not modelled");
    if (advanceSpace) *advanceSpace = line->advanceX;
    if (headSpace) {
        const WritingOrder* leading = IsRightToLeft(*line) ? line->tail : line->head;
        const WritingOrder* trailing = IsRightToLeft(*line) ? line->head : line->tail;
        if (IsRightToLeft(*line)) {
            *headSpace = leading ? leading->metrics.Extent.right : 0.0f;
            *tailSpace = trailing ? trailing->metrics.advanceX - trailing->metrics.Extent.left : 0.0f;
        } else {
            *headSpace = leading ? leading->metrics.Extent.left : 0.0f;
            *tailSpace = trailing ? trailing->metrics.advanceX - trailing->metrics.Extent.right : 0.0f;
        }
    }
    if (inlineSpace) *inlineSpace = line->orderingSpace;
    return SCE_FONT_OK;
}

}

#pragma GCC visibility pop
