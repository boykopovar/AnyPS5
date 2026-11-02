#include "prx/libSceFreeTypeOt/include/GlyphLoader.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

using FreeType::GuestGlyphLoaderRec;
using FreeType::GuestMemoryRec;
using FreeType::GuestSubGlyphRec;
using FreeType::GuestVector;

struct Recorder {
    GuestMemoryRec memory{};
    int allocs = 0;
    int frees = 0;
    int reallocs = 0;
    std::int64_t lastSize = 0;
    std::int64_t lastCurrent = 0;
    std::int64_t lastNew = 0;
    bool fail = false;
};

Recorder* Owner(GuestMemoryRec* memory) {
    return static_cast<Recorder*>(memory->user);
}

void* APS5_VABI RecordAlloc(GuestMemoryRec* memory, std::int64_t size) {
    auto* recorder = Owner(memory);
    ++recorder->allocs;
    recorder->lastSize = size;
    if (recorder->fail) return nullptr;
    void* block = std::malloc(static_cast<std::size_t>(size));
    std::memset(block, 0xAA, static_cast<std::size_t>(size));
    return block;
}

void APS5_VABI RecordFree(GuestMemoryRec* memory, void* block) {
    ++Owner(memory)->frees;
    std::free(block);
}

void* APS5_VABI RecordRealloc(GuestMemoryRec* memory, std::int64_t current, std::int64_t size, void* block) {
    auto* recorder = Owner(memory);
    ++recorder->reallocs;
    recorder->lastCurrent = current;
    recorder->lastNew = size;
    if (recorder->fail) return nullptr;
    auto* resized = static_cast<unsigned char*>(std::realloc(block, static_cast<std::size_t>(size)));
    if (size > current) std::memset(resized + current, 0xAA, static_cast<std::size_t>(size - current));
    return resized;
}

void Reset(Recorder& recorder) {
    recorder = Recorder{};
    recorder.memory = {&recorder, RecordAlloc, RecordFree, RecordRealloc};
}

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "FreeType Ot glyph loader: %s\n", message);
    std::exit(1);
}

bool Zero(const void* block, std::size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(block);
    for (std::size_t i = 0; i < size; ++i) {
        if (bytes[i] != 0) return false;
    }
    return true;
}

GuestGlyphLoaderRec* Create(Recorder& recorder) {
    GuestGlyphLoaderRec* loader = nullptr;
    Require(FT_GlyphLoader_New_nid_postfix(&recorder.memory, &loader) == 0 && loader, "New succeeds");
    return loader;
}

void Creation() {
    Recorder recorder;
    Reset(recorder);
    GuestGlyphLoaderRec* loader = Create(recorder);
    Require(recorder.allocs == 1 && recorder.lastSize == 176, "New allocates a 176-byte FT_GlyphLoaderRec");
    Require(loader->memory == &recorder.memory, "New stores the memory object");
    Require(Zero(reinterpret_cast<unsigned char*>(loader) + 8, sizeof(GuestGlyphLoaderRec) - 8), "New zeroes the rest of the record");
    FT_GlyphLoader_Done_nid_postfix(loader);
    Require(recorder.frees == 1, "Done of an empty loader frees only the record");

    recorder.fail = true;
    auto* sentinel = reinterpret_cast<GuestGlyphLoaderRec*>(&recorder);
    GuestGlyphLoaderRec* untouched = sentinel;
    Require(FT_GlyphLoader_New_nid_postfix(&recorder.memory, &untouched) == 0x40 && untouched == sentinel, "New reports Out_Of_Memory and leaves the output alone");

    FT_GlyphLoader_Done_nid_postfix(nullptr);
    FT_GlyphLoader_Add_nid_postfix(nullptr);
}

void PointsAndContours() {
    Recorder recorder;
    Reset(recorder);
    GuestGlyphLoaderRec* loader = Create(recorder);

    Require(FT_GlyphLoader_CheckPoints_nid_postfix(loader, 5, 2) == 0, "CheckPoints grows empty tables");
    Require(loader->max_points == 8 && loader->max_contours == 4, "CheckPoints pads points to 8 and contours to 4");
    Require(loader->base.outline.points && loader->base.outline.tags && loader->base.outline.contours, "CheckPoints allocates points, tags and contours");
    Require(Zero(loader->base.outline.points, 8 * sizeof(GuestVector)) && Zero(loader->base.outline.tags, 8) && Zero(loader->base.outline.contours, 8), "CheckPoints zeroes new entries");
    Require(loader->current.outline.points == loader->base.outline.points && loader->current.outline.contours == loader->base.outline.contours, "current tables start at the base end");
    Require(loader->base.outline.n_points == 0 && loader->current.outline.n_points == 0, "CheckPoints does not change counts");

    const int reallocs = recorder.reallocs;
    const int allocs = recorder.allocs;
    Require(FT_GlyphLoader_CheckPoints_nid_postfix(loader, 8, 4) == 0 && recorder.reallocs == reallocs && recorder.allocs == allocs, "CheckPoints within capacity does not allocate");

    loader->current.outline.n_points = 5;
    loader->current.outline.n_contours = 2;
    loader->current.outline.points[4] = {40, 41};
    loader->current.outline.tags[4] = 1;
    loader->current.outline.contours[0] = 1;
    loader->current.outline.contours[1] = 4;
    FT_GlyphLoader_Add_nid_postfix(loader);
    Require(loader->base.outline.n_points == 5 && loader->base.outline.n_contours == 2, "Add moves current counts into the base");
    Require(loader->base.outline.contours[0] == 1 && loader->base.outline.contours[1] == 4, "Add keeps the first glyph's contour ends");
    Require(loader->current.outline.n_points == 0 && loader->current.outline.n_contours == 0, "Add prepares an empty current glyph");
    Require(loader->current.outline.points == loader->base.outline.points + 5 && loader->current.outline.tags == loader->base.outline.tags + 5, "Add moves the current points past the base");
    Require(loader->current.outline.contours == loader->base.outline.contours + 2, "Add moves the current contours past the base");

    Require(FT_GlyphLoader_CheckPoints_nid_postfix(loader, 3, 1) == 0 && loader->max_points == 8 && loader->max_contours == 4, "CheckPoints counts the base glyph");
    loader->current.outline.n_points = 3;
    loader->current.outline.n_contours = 1;
    loader->current.outline.contours[0] = 2;
    FT_GlyphLoader_Add_nid_postfix(loader);
    Require(loader->base.outline.n_points == 8 && loader->base.outline.n_contours == 3 && loader->base.outline.contours[2] == 7, "Add offsets new contour ends by the base point count");

    Require(FT_GlyphLoader_CheckPoints_nid_postfix(loader, 1, 0) == 0 && loader->max_points == 16 && loader->max_contours == 4, "CheckPoints grows points by padding the total to 8");
    Require(recorder.lastCurrent == 8 && recorder.lastNew == 16, "CheckPoints resizes tags from the old to the new maximum");
    Require(loader->base.outline.points[4].x == 40 && loader->base.outline.points[4].y == 41 && loader->base.outline.tags[4] == 1, "CheckPoints keeps existing points and tags");
    Require(loader->current.outline.points == loader->base.outline.points + 8, "CheckPoints readjusts the current points after growing");

    const std::uint32_t maxPoints = loader->max_points;
    Require(FT_GlyphLoader_CheckPoints_nid_postfix(loader, 0x7FF8, 0) == 0x0A && loader->max_points == maxPoints, "CheckPoints over 0x7FFF points returns Array_Too_Large");
    Require(FT_GlyphLoader_CheckPoints_nid_postfix(loader, 0, 0x7FFD) == 0x0A && loader->max_contours == 4, "CheckPoints over 0x7FFF contours returns Array_Too_Large");

    recorder.fail = true;
    Require(FT_GlyphLoader_CheckPoints_nid_postfix(loader, 9, 0) == 0x40 && loader->max_points == maxPoints && loader->base.outline.points, "a failed resize reports Out_Of_Memory and keeps the tables");
    recorder.fail = false;

    FT_GlyphLoader_Rewind_nid_postfix(loader);
    Require(loader->base.outline.n_points == 0 && loader->base.outline.n_contours == 0 && loader->base.num_subglyphs == 0, "Rewind clears the base counts");
    Require(std::memcmp(&loader->current, &loader->base, sizeof(loader->base)) == 0, "Rewind copies the base into current");
    Require(loader->max_points == maxPoints && loader->base.outline.points, "Rewind keeps the tables");

    FT_GlyphLoader_Done_nid_postfix(loader);
    Require(recorder.frees == 4, "Done frees points, tags, contours and the record");
}

void ExtraPoints() {
    Recorder recorder;
    Reset(recorder);
    GuestGlyphLoaderRec* loader = Create(recorder);
    Require(FT_GlyphLoader_CheckPoints_nid_postfix(loader, 4, 1) == 0 && loader->max_points == 8, "CheckPoints sets 8 points");
    const int allocs = recorder.allocs;
    Require(FT_GlyphLoader_CreateExtra_nid_postfix(loader) == 0 && recorder.allocs == allocs + 1 && recorder.lastSize == 16 * sizeof(GuestVector), "CreateExtra allocates two tables of max_points vectors");
    Require(loader->use_extra == 1 && loader->base.extra_points2 == loader->base.extra_points + 8, "CreateExtra sets use_extra and the second table");
    Require(Zero(loader->base.extra_points, 16 * sizeof(GuestVector)), "CreateExtra zeroes both tables");
    Require(loader->current.extra_points == loader->base.extra_points && loader->current.extra_points2 == loader->base.extra_points2, "CreateExtra adjusts the current tables");

    loader->base.extra_points[3] = {3, 4};
    loader->base.extra_points2[3] = {5, 6};
    loader->base.outline.n_points = 4;
    Require(FT_GlyphLoader_CheckPoints_nid_postfix(loader, 5, 0) == 0 && loader->max_points == 16, "CheckPoints grows with extra tables");
    Require(loader->base.extra_points2 == loader->base.extra_points + 16, "CheckPoints moves the second extra table");
    Require(loader->base.extra_points[3].x == 3 && loader->base.extra_points2[3].x == 5 && loader->base.extra_points2[3].y == 6, "CheckPoints keeps both extra tables");
    Require(loader->current.extra_points == loader->base.extra_points + 4 && loader->current.extra_points2 == loader->base.extra_points2 + 4, "CheckPoints adjusts the current extra tables");

    GuestGlyphLoaderRec* target = Create(recorder);
    Require(FT_GlyphLoader_CreateExtra_nid_postfix(target) == 0 && target->use_extra == 1 && !target->base.extra_points, "CreateExtra with no points allocates nothing");
    loader->base.outline.n_contours = 1;
    loader->base.outline.points[3] = {7, 8};
    loader->base.outline.tags[3] = 1;
    loader->base.outline.contours[0] = 3;
    Require(FT_GlyphLoader_CopyPoints_nid_postfix(target, loader) == 0, "CopyPoints succeeds");
    Require(target->base.outline.n_points == 4 && target->base.outline.n_contours == 1 && target->max_points == 8, "CopyPoints sets the target counts");
    Require(target->base.outline.points[3].x == 7 && target->base.outline.tags[3] == 1 && target->base.outline.contours[0] == 3, "CopyPoints copies points, tags and contours");
    Require(target->base.extra_points2 == target->base.extra_points + 8 && target->base.extra_points[3].x == 3 && target->base.extra_points2[3].y == 6, "CopyPoints copies both extra tables when both loaders use them");
    Require(target->current.outline.points == target->base.outline.points + 4, "CopyPoints adjusts the target current points");

    FT_GlyphLoader_Reset_nid_postfix(loader);
    Require(!loader->base.outline.points && !loader->base.outline.tags && !loader->base.outline.contours && !loader->base.extra_points && !loader->base.extra_points2, "Reset frees and clears every table");
    Require(loader->max_points == 0 && loader->max_contours == 0 && loader->max_subglyphs == 0 && loader->base.outline.n_points == 0, "Reset clears maxima and counts");
    Require(loader->use_extra == 1, "Reset keeps use_extra");
    FT_GlyphLoader_Done_nid_postfix(loader);
    FT_GlyphLoader_Done_nid_postfix(target);
    Require(recorder.frees == recorder.allocs, "every allocation is freed");
}

void Subglyphs() {
    Recorder recorder;
    Reset(recorder);
    GuestGlyphLoaderRec* loader = Create(recorder);
    Require(FT_GlyphLoader_CheckSubGlyphs_nid_postfix(loader, 3) == 0 && loader->max_subglyphs == 4 && recorder.lastSize == 4 * sizeof(GuestSubGlyphRec), "CheckSubGlyphs pads to 2 and allocates subglyph records");
    Require(Zero(loader->base.subglyphs, 4 * sizeof(GuestSubGlyphRec)), "CheckSubGlyphs zeroes new records");
    Require(loader->current.subglyphs == loader->base.subglyphs, "CheckSubGlyphs adjusts the current records");
    loader->current.num_subglyphs = 3;
    loader->current.subglyphs[2].index = 9;
    FT_GlyphLoader_Add_nid_postfix(loader);
    Require(loader->base.num_subglyphs == 3 && loader->current.num_subglyphs == 0 && loader->current.subglyphs == loader->base.subglyphs + 3, "Add moves subglyphs into the base");
    Require(FT_GlyphLoader_CheckSubGlyphs_nid_postfix(loader, 1) == 0 && loader->max_subglyphs == 4, "CheckSubGlyphs within capacity keeps the table");
    Require(FT_GlyphLoader_CheckSubGlyphs_nid_postfix(loader, 2) == 0 && loader->max_subglyphs == 6 && recorder.lastCurrent == 4 * sizeof(GuestSubGlyphRec), "CheckSubGlyphs grows from the old maximum");
    Require(loader->base.subglyphs[2].index == 9 && loader->current.subglyphs == loader->base.subglyphs + 3, "CheckSubGlyphs keeps records and readjusts");
    recorder.fail = true;
    Require(FT_GlyphLoader_CheckSubGlyphs_nid_postfix(loader, 4) == 0x40 && loader->max_subglyphs == 6, "a failed subglyph resize reports Out_Of_Memory");
    recorder.fail = false;
    loader->current.num_subglyphs = 2;
    FT_GlyphLoader_Prepare_nid_postfix(loader);
    Require(loader->current.num_subglyphs == 0 && loader->current.subglyphs == loader->base.subglyphs + 3, "Prepare empties the current glyph");
    FT_GlyphLoader_Done_nid_postfix(loader);
    Require(recorder.frees == 2, "Done frees the subglyphs and the record");
}

}

int main() {
    Creation();
    PointsAndContours();
    ExtraPoints();
    Subglyphs();
    std::puts("Guest FreeType Ot glyph loader tests passed");
    return 0;
}
