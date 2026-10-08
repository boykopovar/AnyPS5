#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#include "prx/libSceFont/include/FontTypes.hpp"

namespace {

constexpr std::int32_t WRITING_FORM_HORIZONTAL = 0x10;
constexpr std::int32_t WRITING_FORM_HORIZONTAL_LTR = 0x12;
constexpr std::uint16_t CREATE_WRITING_LINE_DETAIL_ID = 0x0FD5;

struct CreateWritingLineDetail {
    std::uint16_t detailId;
};

struct WritingLine {
    std::vector<FontWritingLineStep> steps;
    std::size_t cursor = 0;
    FontWritingMetrics metrics{};
};

std::mutex linesMutex;
std::unordered_set<WritingLine*> lines;

[[noreturn]] void Unsupported(const char* function, const std::string& what) {
    throw std::runtime_error(std::string(function) + ": " + what);
}

WritingLine* GetLine(const char* function, void* writingLine) {
    auto* line = static_cast<WritingLine*>(writingLine);
    std::lock_guard lock(linesMutex);
    if (!lines.contains(line)) Unsupported(function, "unknown writing line handle (error code not verified)");
    return line;
}

}

#pragma GCC visibility push(default)

extern "C" {

int APS5_VABI sceFontCreateWritingLine(const FontMemory* memory, std::int32_t writingForm, const void* writingLineDetail, void** pWritingLine) {
    (void)memory;
    if (!pWritingLine) Unsupported(__func__, "null writing line pointer (error code not verified)");
    if (writingForm != WRITING_FORM_HORIZONTAL && writingForm != WRITING_FORM_HORIZONTAL_LTR)
        Unsupported(__func__, "writing form " + std::to_string(writingForm) + " is not modelled");
    if (writingLineDetail && static_cast<const CreateWritingLineDetail*>(writingLineDetail)->detailId != CREATE_WRITING_LINE_DETAIL_ID)
        Unsupported(__func__, "unknown writing line detail id");
    auto* line = new WritingLine{};
    {
        std::lock_guard lock(linesMutex);
        lines.insert(line);
    }
    *pWritingLine = line;
    return SCE_FONT_OK;
}

int APS5_VABI sceFontDestroyWritingLine(void** pWritingLine) {
    if (!pWritingLine || !*pWritingLine) return SCE_FONT_OK;
    auto* line = GetLine(__func__, *pWritingLine);
    {
        std::lock_guard lock(linesMutex);
        lines.erase(line);
    }
    delete line;
    *pWritingLine = nullptr;
    return SCE_FONT_OK;
}

int APS5_VABI sceFontWritingLineClear(void* writingLine) {
    auto* line = GetLine(__func__, writingLine);
    line->steps.clear();
    line->cursor = 0;
    line->metrics = {};
    return SCE_FONT_OK;
}

int APS5_VABI sceFontWritingLineWritesOrder(void* writingLine, std::uint64_t writingAttribute, const FontWritingMetrics* writingMetrics, void* writingOrderer) {
    auto* line = GetLine(__func__, writingLine);
    if (writingAttribute != 0) Unsupported(__func__, "writing attribute " + std::to_string(writingAttribute) + " is not modelled");
    if (!writingMetrics) Unsupported(__func__, "null writing metrics are not modelled");
    const FontWritingMetrics& order = *writingMetrics;
    const float pen = line->metrics.advanceX;
    FontWritingLineStep step{};
    step.x = pen;
    step.advanceX = order.advanceX;
    step.advanceY = order.advanceY;
    step.writingOrderer = writingOrderer;
    step.Metrics = order;
    const FontWritingExtent extent{order.Extent.top, order.Extent.bottom, pen + order.Extent.left, pen + order.Extent.right};
    if (line->steps.empty()) {
        line->metrics.Extent = extent;
    } else {
        line->metrics.Extent.top = std::min(line->metrics.Extent.top, extent.top);
        line->metrics.Extent.bottom = std::max(line->metrics.Extent.bottom, extent.bottom);
        line->metrics.Extent.left = std::min(line->metrics.Extent.left, extent.left);
        line->metrics.Extent.right = std::max(line->metrics.Extent.right, extent.right);
    }
    line->metrics.advanceX += order.advanceX;
    line->metrics.advanceY += order.advanceY;
    line->steps.push_back(step);
    line->cursor = 0;
    return SCE_FONT_OK;
}

const FontWritingLineStep* APS5_VABI sceFontWritingLineRefersRenderStep(void* writingLine) {
    if (!writingLine) return nullptr;
    auto* line = GetLine(__func__, writingLine);
    if (line->cursor >= line->steps.size()) return nullptr;
    return &line->steps[line->cursor++];
}

int APS5_VABI sceFontWritingLineGetRenderMetrics(void* writingLine, FontWritingMetrics* writingMetrics) {
    auto* line = GetLine(__func__, writingLine);
    if (!writingMetrics) Unsupported(__func__, "null metrics pointer (error code not verified)");
    if (line->steps.empty()) Unsupported(__func__, "metrics of an empty line are not modelled");
    *writingMetrics = line->metrics;
    return SCE_FONT_OK;
}

int APS5_VABI sceFontWritingLineGetOrderingSpace(void* writingLine, float* headSpace, float* inlineSpace, float* tailSpace, float* advanceSpace) {
    GetLine(__func__, writingLine);
    if (!headSpace || !inlineSpace || !tailSpace || !advanceSpace) Unsupported(__func__, "null space pointer (error code not verified)");
    *headSpace = 0.0f;
    *inlineSpace = 0.0f;
    *tailSpace = 0.0f;
    *advanceSpace = 0.0f;
    return SCE_FONT_OK;
}

}

#pragma GCC visibility pop
