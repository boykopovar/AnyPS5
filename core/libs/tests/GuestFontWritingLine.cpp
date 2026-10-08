#include "prx/libSceFont/include/FontTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>

extern "C" {
int APS5_VABI sceFontCreateWritingLine(const FontMemory*, std::int32_t, const void*, void**);
int APS5_VABI sceFontDestroyWritingLine(void**);
int APS5_VABI sceFontWritingLineClear(void*);
int APS5_VABI sceFontWritingLineWritesOrder(void*, std::uint64_t, const FontWritingMetrics*, void*);
const FontWritingLineStep* APS5_VABI sceFontWritingLineRefersRenderStep(void*);
int APS5_VABI sceFontWritingLineGetRenderMetrics(void*, FontWritingMetrics*);
int APS5_VABI sceFontWritingLineGetOrderingSpace(void*, float*, float*, float*, float*);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Font writing line check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename F>
static bool Throws(F f) {
    try {
        f();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

struct WritingLineDetail {
    std::uint16_t detailId;
    std::uint16_t reserved[3];
    void* reservedPointers[3];
};

static bool Same(const FontWritingMetrics& a, const FontWritingMetrics& b) {
    return a.advanceX == b.advanceX && a.advanceY == b.advanceY && a.Extent.top == b.Extent.top &&
           a.Extent.bottom == b.Extent.bottom && a.Extent.left == b.Extent.left && a.Extent.right == b.Extent.right;
}

static bool NoSpace(void* line) {
    float head = 1.0f;
    float inner = 1.0f;
    float tail = 1.0f;
    float advance = 1.0f;
    return sceFontWritingLineGetOrderingSpace(line, &head, &inner, &tail, &advance) == SCE_FONT_OK && head == 0.0f && inner == 0.0f &&
           tail == 0.0f && advance == 0.0f;
}

static bool StepIs(const FontWritingLineStep* step, float x, const FontWritingMetrics& run, void* orderer) {
    return step && step->x == x && step->y == 0.0f && step->advanceX == run.advanceX && step->advanceY == run.advanceY &&
           step->spacingProgress == 0.0f && step->writingOrderer == orderer && step->Adjusting.x == 0.0f &&
           step->Adjusting.y == 0.0f && Same(step->Metrics, run);
}

int main() {
    FontMemory memory{};
    void* line = nullptr;
    Require(Throws([&] { sceFontCreateWritingLine(&memory, 0x10, nullptr, nullptr); }));
    Require(Throws([&] { sceFontCreateWritingLine(&memory, 0x11, nullptr, &line); }) && line == nullptr);
    Require(Throws([&] { sceFontCreateWritingLine(&memory, 0, nullptr, &line); }) && line == nullptr);
    WritingLineDetail detail{0x0FD4, {}, {}};
    Require(Throws([&] { sceFontCreateWritingLine(&memory, 0x10, &detail, &line); }) && line == nullptr);
    detail.detailId = 0x0FD5;
    Require(sceFontCreateWritingLine(&memory, 0x10, &detail, &line) == SCE_FONT_OK && line != nullptr);
    void* ltr = nullptr;
    Require(sceFontCreateWritingLine(&memory, 0x12, nullptr, &ltr) == SCE_FONT_OK && ltr != nullptr && ltr != line);

    FontWritingMetrics metrics{};
    Require(NoSpace(line));
    Require(sceFontWritingLineRefersRenderStep(line) == nullptr);
    Require(Throws([&] { sceFontWritingLineGetRenderMetrics(line, &metrics); }));

    int ordererA = 0;
    int ordererB = 0;
    const FontWritingMetrics runA{10.0f, 0.0f, {-8.0f, 2.0f, 1.0f, 9.0f}};
    const FontWritingMetrics runB{6.0f, 0.0f, {-12.0f, 3.0f, -1.0f, 5.0f}};
    Require(sceFontWritingLineWritesOrder(line, 0, &runA, &ordererA) == SCE_FONT_OK);
    Require(sceFontWritingLineGetRenderMetrics(line, &metrics) == SCE_FONT_OK && Same(metrics, runA));
    Require(sceFontWritingLineWritesOrder(line, 0, &runB, &ordererB) == SCE_FONT_OK);
    Require(sceFontWritingLineGetRenderMetrics(line, &metrics) == SCE_FONT_OK);
    Require(Same(metrics, {16.0f, 0.0f, {-12.0f, 3.0f, 1.0f, 15.0f}}));
    Require(NoSpace(line));
    float space = 0.0f;
    Require(Throws([&] { sceFontWritingLineGetOrderingSpace(line, nullptr, &space, &space, &space); }));
    Require(Throws([&] { sceFontWritingLineGetOrderingSpace(line, &space, &space, &space, nullptr); }));

    Require(StepIs(sceFontWritingLineRefersRenderStep(line), 0.0f, runA, &ordererA));
    Require(StepIs(sceFontWritingLineRefersRenderStep(line), 10.0f, runB, &ordererB));
    Require(sceFontWritingLineRefersRenderStep(line) == nullptr);
    Require(sceFontWritingLineRefersRenderStep(line) == nullptr);

    const FontWritingMetrics runC{4.0f, 0.0f, {-4.0f, 1.0f, 0.0f, 20.0f}};
    Require(sceFontWritingLineWritesOrder(line, 0, &runC, nullptr) == SCE_FONT_OK);
    Require(sceFontWritingLineGetRenderMetrics(line, &metrics) == SCE_FONT_OK);
    Require(Same(metrics, {20.0f, 0.0f, {-12.0f, 3.0f, 1.0f, 36.0f}}));
    Require(StepIs(sceFontWritingLineRefersRenderStep(line), 0.0f, runA, &ordererA));
    Require(StepIs(sceFontWritingLineRefersRenderStep(line), 10.0f, runB, &ordererB));
    Require(StepIs(sceFontWritingLineRefersRenderStep(line), 16.0f, runC, nullptr));
    Require(sceFontWritingLineRefersRenderStep(line) == nullptr);

    Require(Throws([&] { sceFontWritingLineWritesOrder(line, 1, &runA, nullptr); }));
    Require(Throws([&] { sceFontWritingLineWritesOrder(line, 0, nullptr, nullptr); }));
    Require(Throws([&] { sceFontWritingLineGetRenderMetrics(line, nullptr); }));
    Require(sceFontWritingLineGetRenderMetrics(line, &metrics) == SCE_FONT_OK && metrics.advanceX == 20.0f);

    Require(sceFontWritingLineClear(line) == SCE_FONT_OK);
    Require(sceFontWritingLineRefersRenderStep(line) == nullptr);
    Require(Throws([&] { sceFontWritingLineGetRenderMetrics(line, &metrics); }));
    Require(sceFontWritingLineWritesOrder(line, 0, &runB, &ordererB) == SCE_FONT_OK);
    Require(sceFontWritingLineGetRenderMetrics(line, &metrics) == SCE_FONT_OK && Same(metrics, runB));
    Require(StepIs(sceFontWritingLineRefersRenderStep(line), 0.0f, runB, &ordererB));

    Require(sceFontWritingLineWritesOrder(ltr, 0, &runC, nullptr) == SCE_FONT_OK);
    Require(sceFontWritingLineGetRenderMetrics(ltr, &metrics) == SCE_FONT_OK && Same(metrics, runC));

    int notALine = 0;
    Require(sceFontWritingLineRefersRenderStep(nullptr) == nullptr);
    Require(Throws([&] { sceFontWritingLineClear(nullptr); }));
    Require(Throws([&] { sceFontWritingLineClear(&notALine); }));
    Require(Throws([&] { sceFontWritingLineRefersRenderStep(&notALine); }));
    Require(Throws([&] { sceFontWritingLineGetOrderingSpace(&notALine, &space, &space, &space, &space); }));
    void* bogus = &notALine;
    Require(Throws([&] { sceFontDestroyWritingLine(&bogus); }) && bogus == &notALine);

    void* destroyed = line;
    Require(sceFontDestroyWritingLine(&line) == SCE_FONT_OK && line == nullptr);
    Require(sceFontDestroyWritingLine(&line) == SCE_FONT_OK && line == nullptr);
    Require(sceFontDestroyWritingLine(nullptr) == SCE_FONT_OK);
    Require(Throws([&] { sceFontWritingLineClear(destroyed); }));
    Require(Throws([&] { sceFontWritingLineWritesOrder(destroyed, 0, &runA, nullptr); }));
    Require(sceFontDestroyWritingLine(&ltr) == SCE_FONT_OK && ltr == nullptr);
    std::puts("font writing line: ok");
}
