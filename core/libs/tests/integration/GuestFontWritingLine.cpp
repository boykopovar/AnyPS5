#include "prx/libSceFont/include/FontTypes.hpp"

#include <Testing/Test.hpp>

#include <cstdint>
#include <exception>
#include <string>

extern "C" {
int APS5_VABI sceFontCreateWritingLine(const FontMemory*, std::int32_t, const void*, void**);
int APS5_VABI sceFontDestroyWritingLine(void**);
int APS5_VABI sceFontWritingLineClear(void*);
int APS5_VABI sceFontWritingLineWritesOrder(void*, std::uint64_t, const FontWritingMetrics*, void*);
const FontWritingLineStep* APS5_VABI sceFontWritingLineRefersRenderStep(void*);
int APS5_VABI sceFontWritingLineGetRenderMetrics(void*, FontWritingMetrics*);
int APS5_VABI sceFontWritingLineGetOrderingSpace(void*, float*, float*, float*, float*);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

struct WritingLineDetail {
    std::uint16_t detailId;
    std::uint16_t reserved[3];
    void* reservedPointers[3];
};

template<typename TOperation>
void RequireRejected(const TOperation& operation, const std::string& message) {
    try {
        operation();
    } catch (const Testing::Failure&) {
        throw;
    } catch (const std::exception&) {
        return;
    }
    Testing::Fail(message + ": did not throw");
}

void RequireSame(const FontWritingMetrics& actual, const FontWritingMetrics& expected, const std::string& message) {
    RequireEqual(actual.advanceX, expected.advanceX, message + " advanceX");
    RequireEqual(actual.advanceY, expected.advanceY, message + " advanceY");
    RequireEqual(actual.Extent.top, expected.Extent.top, message + " top");
    RequireEqual(actual.Extent.bottom, expected.Extent.bottom, message + " bottom");
    RequireEqual(actual.Extent.left, expected.Extent.left, message + " left");
    RequireEqual(actual.Extent.right, expected.Extent.right, message + " right");
}

void RequireNoSpace(void* line) {
    float head = 1.0f;
    float inner = 1.0f;
    float tail = 1.0f;
    float advance = 1.0f;
    RequireEqual(sceFontWritingLineGetOrderingSpace(line, &head, &inner, &tail, &advance), SCE_FONT_OK, "ordering space");
    RequireEqual(head, 0.0f, "head space");
    RequireEqual(inner, 0.0f, "inner space");
    RequireEqual(tail, 0.0f, "tail space");
    RequireEqual(advance, 0.0f, "space advance");
}

void RequireStep(const FontWritingLineStep* step, float x, const FontWritingMetrics& run, void* orderer, const std::string& message) {
    Require(step != nullptr, message + " exists");
    RequireEqual(step->x, x, message + " x");
    RequireEqual(step->y, 0.0f, message + " y");
    RequireEqual(step->advanceX, run.advanceX, message + " advanceX");
    RequireEqual(step->advanceY, run.advanceY, message + " advanceY");
    RequireEqual(step->spacingProgress, 0.0f, message + " spacing progress");
    Require(step->writingOrderer == orderer, message + " orderer");
    RequireEqual(step->Adjusting.x, 0.0f, message + " adjusting x");
    RequireEqual(step->Adjusting.y, 0.0f, message + " adjusting y");
    RequireSame(step->Metrics, run, message + " metrics");
}

class WritingLine {
public:
    explicit WritingLine(std::int32_t mode = 0x10) {
        WritingLineDetail detail{0x0FD5, {}, {}};
        RequireEqual(sceFontCreateWritingLine(&memory, mode, mode == 0x10 ? &detail : nullptr, &line), SCE_FONT_OK, "create the line");
        Require(line != nullptr, "line handle is set");
    }

    ~WritingLine() {
        if (line != nullptr) sceFontDestroyWritingLine(&line);
    }

    WritingLine(const WritingLine&) = delete;
    WritingLine& operator=(const WritingLine&) = delete;

    FontMemory memory{};
    void* line = nullptr;
};

const FontWritingMetrics runA{10.0f, 0.0f, {-8.0f, 2.0f, 1.0f, 9.0f}};
const FontWritingMetrics runB{6.0f, 0.0f, {-12.0f, 3.0f, -1.0f, 5.0f}};
const FontWritingMetrics runC{4.0f, 0.0f, {-4.0f, 1.0f, 0.0f, 20.0f}};

const Case createInvalid{"CreateWritingLine_InvalidModeOrDetail_Throws", [] {
    FontMemory memory{};
    void* line = nullptr;
    RequireRejected([&] { sceFontCreateWritingLine(&memory, 0x10, nullptr, nullptr); }, "null output");
    RequireRejected([&] { sceFontCreateWritingLine(&memory, 0x11, nullptr, &line); }, "mode 0x11");
    Require(line == nullptr, "no line for mode 0x11");
    RequireRejected([&] { sceFontCreateWritingLine(&memory, 0, nullptr, &line); }, "mode 0");
    Require(line == nullptr, "no line for mode 0");
    WritingLineDetail detail{0x0FD4, {}, {}};
    RequireRejected([&] { sceFontCreateWritingLine(&memory, 0x10, &detail, &line); }, "detail id 0x0FD4");
    Require(line == nullptr, "no line for a wrong detail id");
}};

const Case createValid{"CreateWritingLine_DetailOrLeftToRightMode_CreatesDistinctLines", [] {
    const WritingLine detailed(0x10);
    const WritingLine ltr(0x12);
    Require(detailed.line != ltr.line, "distinct lines");
}};

const Case emptyLine{"WritingLine_Empty_HasNoSpaceStepsOrMetrics", [] {
    const WritingLine fixture;
    FontWritingMetrics metrics{};
    RequireNoSpace(fixture.line);
    Require(sceFontWritingLineRefersRenderStep(fixture.line) == nullptr, "no step");
    RequireRejected([&] { sceFontWritingLineGetRenderMetrics(fixture.line, &metrics); }, "metrics of an empty line");
}};

const Case accumulate{"WritesOrder_TwoRuns_AccumulatesMetrics", [] {
    const WritingLine fixture;
    int ordererA = 0;
    int ordererB = 0;
    FontWritingMetrics metrics{};
    RequireEqual(sceFontWritingLineWritesOrder(fixture.line, 0, &runA, &ordererA), SCE_FONT_OK, "write run A");
    RequireEqual(sceFontWritingLineGetRenderMetrics(fixture.line, &metrics), SCE_FONT_OK, "metrics after A");
    RequireSame(metrics, runA, "after A");
    RequireEqual(sceFontWritingLineWritesOrder(fixture.line, 0, &runB, &ordererB), SCE_FONT_OK, "write run B");
    RequireEqual(sceFontWritingLineGetRenderMetrics(fixture.line, &metrics), SCE_FONT_OK, "metrics after B");
    RequireSame(metrics, {16.0f, 0.0f, {-12.0f, 3.0f, 1.0f, 15.0f}}, "after B");
    RequireNoSpace(fixture.line);
}};

const Case orderingSpaceNull{"GetOrderingSpace_NullOutput_Throws", [] {
    const WritingLine fixture;
    float space = 0.0f;
    RequireRejected([&] { sceFontWritingLineGetOrderingSpace(fixture.line, nullptr, &space, &space, &space); }, "null head");
    RequireRejected([&] { sceFontWritingLineGetOrderingSpace(fixture.line, &space, &space, &space, nullptr); }, "null advance");
}};

const Case steps{"RefersRenderStep_WrittenRuns_YieldsStepsOnceThenRestartsAfterWrite", [] {
    const WritingLine fixture;
    int ordererA = 0;
    int ordererB = 0;
    RequireEqual(sceFontWritingLineWritesOrder(fixture.line, 0, &runA, &ordererA), SCE_FONT_OK, "write run A");
    RequireEqual(sceFontWritingLineWritesOrder(fixture.line, 0, &runB, &ordererB), SCE_FONT_OK, "write run B");
    RequireStep(sceFontWritingLineRefersRenderStep(fixture.line), 0.0f, runA, &ordererA, "first pass step A");
    RequireStep(sceFontWritingLineRefersRenderStep(fixture.line), 10.0f, runB, &ordererB, "first pass step B");
    Require(sceFontWritingLineRefersRenderStep(fixture.line) == nullptr, "first pass end");
    Require(sceFontWritingLineRefersRenderStep(fixture.line) == nullptr, "first pass stays at end");

    RequireEqual(sceFontWritingLineWritesOrder(fixture.line, 0, &runC, nullptr), SCE_FONT_OK, "write run C");
    FontWritingMetrics metrics{};
    RequireEqual(sceFontWritingLineGetRenderMetrics(fixture.line, &metrics), SCE_FONT_OK, "metrics after C");
    RequireSame(metrics, {20.0f, 0.0f, {-12.0f, 3.0f, 1.0f, 36.0f}}, "after C");
    RequireStep(sceFontWritingLineRefersRenderStep(fixture.line), 0.0f, runA, &ordererA, "second pass step A");
    RequireStep(sceFontWritingLineRefersRenderStep(fixture.line), 10.0f, runB, &ordererB, "second pass step B");
    RequireStep(sceFontWritingLineRefersRenderStep(fixture.line), 16.0f, runC, nullptr, "second pass step C");
    Require(sceFontWritingLineRefersRenderStep(fixture.line) == nullptr, "second pass end");
}};

const Case invalidWrites{"WritesOrder_InvalidArguments_ThrowWithoutChangingLine", [] {
    const WritingLine fixture;
    RequireEqual(sceFontWritingLineWritesOrder(fixture.line, 0, &runA, nullptr), SCE_FONT_OK, "write run A");
    RequireRejected([&] { sceFontWritingLineWritesOrder(fixture.line, 1, &runA, nullptr); }, "flags 1");
    RequireRejected([&] { sceFontWritingLineWritesOrder(fixture.line, 0, nullptr, nullptr); }, "null metrics");
    RequireRejected([&] { sceFontWritingLineGetRenderMetrics(fixture.line, nullptr); }, "null metrics output");
    FontWritingMetrics metrics{};
    RequireEqual(sceFontWritingLineGetRenderMetrics(fixture.line, &metrics), SCE_FONT_OK, "metrics");
    RequireEqual(metrics.advanceX, 10.0f, "advance unchanged");
}};

const Case clear{"Clear_WrittenLine_RemovesRunsAndAcceptsNewOnes", [] {
    const WritingLine fixture;
    int ordererB = 0;
    RequireEqual(sceFontWritingLineWritesOrder(fixture.line, 0, &runA, nullptr), SCE_FONT_OK, "write run A");
    RequireEqual(sceFontWritingLineClear(fixture.line), SCE_FONT_OK, "clear");
    Require(sceFontWritingLineRefersRenderStep(fixture.line) == nullptr, "no step after clear");
    FontWritingMetrics metrics{};
    RequireRejected([&] { sceFontWritingLineGetRenderMetrics(fixture.line, &metrics); }, "metrics after clear");
    RequireEqual(sceFontWritingLineWritesOrder(fixture.line, 0, &runB, &ordererB), SCE_FONT_OK, "write run B");
    RequireEqual(sceFontWritingLineGetRenderMetrics(fixture.line, &metrics), SCE_FONT_OK, "metrics after B");
    RequireSame(metrics, runB, "after B");
    RequireStep(sceFontWritingLineRefersRenderStep(fixture.line), 0.0f, runB, &ordererB, "step B");
}};

const Case leftToRight{"WritesOrder_LeftToRightLine_ReportsRunMetrics", [] {
    const WritingLine fixture(0x12);
    RequireEqual(sceFontWritingLineWritesOrder(fixture.line, 0, &runC, nullptr), SCE_FONT_OK, "write run C");
    FontWritingMetrics metrics{};
    RequireEqual(sceFontWritingLineGetRenderMetrics(fixture.line, &metrics), SCE_FONT_OK, "metrics");
    RequireSame(metrics, runC, "run C");
}};

const Case invalidHandles{"WritingLine_NullOrForeignHandle_IsRejected", [] {
    int notALine = 0;
    float space = 0.0f;
    Require(sceFontWritingLineRefersRenderStep(nullptr) == nullptr, "null line has no step");
    RequireRejected([&] { sceFontWritingLineClear(nullptr); }, "clear null");
    RequireRejected([&] { sceFontWritingLineClear(&notALine); }, "clear foreign");
    RequireRejected([&] { sceFontWritingLineRefersRenderStep(&notALine); }, "step of foreign");
    RequireRejected([&] { sceFontWritingLineGetOrderingSpace(&notALine, &space, &space, &space, &space); }, "space of foreign");
    void* bogus = &notALine;
    RequireRejected([&] { sceFontDestroyWritingLine(&bogus); }, "destroy foreign");
    Require(bogus == &notALine, "foreign handle untouched");
}};

const Case destroy{"DestroyWritingLine_Twice_SucceedsAndInvalidatesLine", [] {
    WritingLine fixture;
    void* destroyed = fixture.line;
    RequireEqual(sceFontDestroyWritingLine(&fixture.line), SCE_FONT_OK, "destroy");
    Require(fixture.line == nullptr, "handle cleared");
    RequireEqual(sceFontDestroyWritingLine(&fixture.line), SCE_FONT_OK, "destroy a null handle");
    Require(fixture.line == nullptr, "handle stays null");
    RequireEqual(sceFontDestroyWritingLine(nullptr), SCE_FONT_OK, "destroy with null pointer");
    RequireRejected([&] { sceFontWritingLineClear(destroyed); }, "clear destroyed");
    RequireRejected([&] { sceFontWritingLineWritesOrder(destroyed, 0, &runA, nullptr); }, "write destroyed");
}};

} // namespace
