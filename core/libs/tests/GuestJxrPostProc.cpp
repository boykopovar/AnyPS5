#include "prx/libSceJxr/include/Api.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libkernel/libkernel.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

extern "C" {
void* JxrPostMake_nid_no_patch(std::uint32_t seed, std::int32_t zeros, std::int32_t extreme, std::int32_t flat);
void* JxrPostClone_nid_no_patch(const void* source);
std::size_t JxrPostCaseBytes_nid_no_patch();
void JxrPostCopyTo_nid_no_patch(void* target, const void* source);
int JxrPostEqual_nid_no_patch(const void* left, const void* right);
void* JxrPostTable_nid_no_patch(void* value);
std::int32_t* JxrPostPixels_nid_no_patch(void* value, std::size_t which);
std::size_t JxrPostPixelBytes_nid_no_patch();
void JxrPostSetRow_nid_no_patch(void* value, std::size_t channel, std::size_t row, void* pointer);
void JxrPostReferenceSlide_nid_no_patch(void* value, std::size_t channels, std::size_t width, std::int32_t top,
                                        std::int32_t bottom);
void JxrPostReferenceUpdate_nid_no_patch(void* value, std::size_t column, std::size_t channel);
void JxrPostReferenceMB_nid_no_patch(void* value, std::size_t column, std::size_t channel, std::int32_t threshold);
void JxrPostReferenceBlock_nid_no_patch(void* value, std::size_t column, std::size_t channel, std::int32_t threshold);
}

namespace {

constexpr std::size_t Width = 12;
constexpr std::size_t Channels = 16;
constexpr std::int32_t Thresholds[] = {0, 1, 4, 100};
constexpr std::int32_t Zeros[] = {0, 90, 100};
constexpr std::size_t PixelOrigin = 256;

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "JXR post-processing: %s\n", message);
    std::abort();
}

template<typename TAction>
void RequireInvalidArgument(TAction action, const char* message) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, message);
}

struct Pair {
    void* guest;
    void* native;
    Pair(std::uint32_t seed, std::int32_t zeros, bool extreme, bool flat) {
        guest = JxrPostMake_nid_no_patch(seed, zeros, extreme, flat);
        Require(guest != nullptr, "allocation");
        native = JxrPostClone_nid_no_patch(guest);
        Require(native != nullptr, "allocation");
    }
    ~Pair() {
        std::free(guest);
        std::free(native);
    }
    void* Table() const { return JxrPostTable_nid_no_patch(guest); }
    std::int32_t* Previous() const { return JxrPostPixels_nid_no_patch(guest, 0); }
    std::int32_t* Current() const { return JxrPostPixels_nid_no_patch(guest, 1); }
    bool Same() const { return JxrPostEqual_nid_no_patch(guest, native) != 0; }
    void SetRow(std::size_t channel, std::size_t row, void* pointer) const {
        JxrPostSetRow_nid_no_patch(guest, channel, row, pointer);
        JxrPostSetRow_nid_no_patch(native, channel, row, pointer);
    }
};

void TestSlide() {
    const std::size_t channelCounts[] = {0, 1, 3, Channels};
    const std::size_t widths[] = {0, 1, 5, Width};
    std::uint32_t seed = 1;
    for (std::size_t channels : channelCounts)
        for (std::size_t width : widths)
            for (std::int32_t top = 0; top < 2; ++top)
                for (std::int32_t bottom = 0; bottom < 2; ++bottom) {
                    Pair pair(seed++, 0, false, false);
                    for (int step = 0; step < 3; ++step) {
                        slideOneMBRow_nid_postfix(pair.Table(), channels, width, top, bottom);
                        JxrPostReferenceSlide_nid_no_patch(pair.native, channels, width, top, bottom);
                        Require(pair.Same(), "slideOneMBRow differs from jxrlib");
                    }
                }
    Pair pair(seed, 0, false, false);
    RequireInvalidArgument([&] { slideOneMBRow_nid_postfix(nullptr, 1, 1, 1, 1); }, "null table");
    RequireInvalidArgument([&] { slideOneMBRow_nid_postfix(pair.Table(), Channels + 1, 1, 0, 0); }, "17 channels");
    RequireInvalidArgument([&] { slideOneMBRow_nid_postfix(pair.Table(), 1, std::numeric_limits<std::size_t>::max(), 1, 0); },
                           "row width overflow");
    pair.SetRow(2, 1, nullptr);
    RequireInvalidArgument([&] { slideOneMBRow_nid_postfix(pair.Table(), 3, 1, 1, 0); }, "null row under a top slide");
    slideOneMBRow_nid_postfix(pair.Table(), 3, 1, 0, 1);
    JxrPostReferenceSlide_nid_no_patch(pair.native, 3, 1, 0, 1);
    Require(pair.Same(), "a bottom slide leaves the old current row unread");
    RequireInvalidArgument([&] { slideOneMBRow_nid_postfix(pair.Table(), 3, 1, 0, 1); }, "null row under a bottom slide");
    slideOneMBRow_nid_postfix(pair.Table(), 3, 0, 1, 1);
    JxrPostReferenceSlide_nid_no_patch(pair.native, 3, 0, 1, 1);
    Require(pair.Same(), "a zero-width slide only swaps rows");
}

void TestUpdate() {
    std::uint32_t seed = 100;
    for (std::int32_t zeros : Zeros)
        for (std::size_t channel = 0; channel < Channels; channel += 5)
            for (std::size_t column = 0; column <= Width; ++column) {
                Pair pair(seed++, zeros, false, false);
                updatePostProcInfo_nid_postfix(pair.Table(), pair.Current(), column, channel);
                JxrPostReferenceUpdate_nid_no_patch(pair.native, column, channel);
                Require(pair.Same(), "updatePostProcInfo differs from jxrlib");
            }
    Pair pair(seed, 0, false, false);
    RequireInvalidArgument([&] { updatePostProcInfo_nid_postfix(nullptr, pair.Current(), 0, 0); }, "null table");
    RequireInvalidArgument([&] { updatePostProcInfo_nid_postfix(pair.Table(), pair.Current(), 0, Channels); },
                           "channel 16");
    RequireInvalidArgument([&] { updatePostProcInfo_nid_postfix(pair.Table(), nullptr, 0, 0); }, "null macroblock");
    RequireInvalidArgument(
        [&] { updatePostProcInfo_nid_postfix(pair.Table(), pair.Current(), std::numeric_limits<std::size_t>::max(), 0); },
        "column overflow");
    pair.SetRow(4, 1, nullptr);
    RequireInvalidArgument([&] { updatePostProcInfo_nid_postfix(pair.Table(), pair.Current(), 0, 4); }, "null row");
    Require(pair.Same(), "rejected updates leave the state unchanged");
}

template<typename TGuest, typename TReference>
void TestNeighbours(TGuest guest, TReference reference, std::uint32_t seed, const char* message) {
    for (std::int32_t zeros : Zeros)
        for (std::int32_t threshold : Thresholds)
            for (std::size_t channel = 0; channel < Channels; channel += 7)
                for (std::size_t column = 0; column <= Width; ++column) {
                    Pair pair(seed++, zeros, false, zeros == 100 || (column & 1) != 0);
                    guest(pair.Table(), pair.Previous(), pair.Current(), column, channel, threshold);
                    reference(pair.native, column, channel, threshold);
                    Require(pair.Same(), message);
                }
    const std::int32_t extremes[] = {0, 1, std::numeric_limits<std::int32_t>::max()};
    for (std::int32_t threshold : extremes)
        for (std::size_t column = 0; column <= Width; ++column) {
            Pair pair(seed++, 0, true, true);
            guest(pair.Table(), pair.Previous(), pair.Current(), column, 3, threshold);
            reference(pair.native, column, 3, threshold);
            Require(pair.Same(), message);
        }
    Pair pair(seed, 0, false, true);
    RequireInvalidArgument([&] { guest(nullptr, pair.Previous(), pair.Current(), 1, 0, 4); }, "null table");
    RequireInvalidArgument([&] { guest(pair.Table(), pair.Previous(), pair.Current(), 1, Channels, 4); }, "channel 16");
    RequireInvalidArgument([&] { guest(pair.Table(), nullptr, pair.Current(), 1, 0, 4); }, "null previous row");
    RequireInvalidArgument([&] { guest(pair.Table(), pair.Previous(), nullptr, 1, 0, 4); }, "null current row");
    RequireInvalidArgument(
        [&] { guest(pair.Table(), pair.Previous(), pair.Current(), std::numeric_limits<std::size_t>::max(), 0, 4); },
        "column overflow");
    pair.SetRow(1, 0, nullptr);
    pair.SetRow(2, 1, nullptr);
    RequireInvalidArgument([&] { guest(pair.Table(), pair.Previous(), pair.Current(), 1, 1, 4); }, "null previous info");
    RequireInvalidArgument([&] { guest(pair.Table(), pair.Previous(), pair.Current(), 1, 2, 4); }, "null current info");
    Require(pair.Same(), "rejected calls leave the state unchanged");
}

void TestMacroblocks() {
    TestNeighbours(postProcMB_nid_postfix, JxrPostReferenceMB_nid_no_patch, 1000, "postProcMB differs from jxrlib");
}

void TestBlocks() {
    TestNeighbours(postProcBlock_nid_postfix, JxrPostReferenceBlock_nid_no_patch, 5000,
                   "postProcBlock differs from jxrlib");
}

#ifdef _WIN32
class GuestPages {
public:
    static constexpr std::size_t Page = 0x4000;
    static constexpr std::size_t Pages = 4;

    GuestPages() {
        Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, Page * Pages, 0, 0, &physical) == 0, "allocate");
        Require(sceKernelMapDirectMemory(&mapping, Page * Pages, 3, 0, physical, 0) == 0, "map");
        Require(GuestArena::GuestArenaContains_nid_postfix(mapping, Page * Pages), "guest mapping");
    }

    ~GuestPages() {
        Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(mapping), Page * Pages) == 0, "unmap");
        Require(sceKernelReleaseDirectMemory(physical, Page * Pages) == 0, "release");
    }

    unsigned char* At(std::size_t page) const { return static_cast<unsigned char*>(mapping) + Page * page; }

    void Protect(std::size_t first, std::size_t count, int protection) const {
        Require(sceKernelMprotect(At(first), Page * count, protection) == 0, "protect");
        if (protection != 1) return;
        MEMORY_BASIC_INFORMATION information{};
        Require(VirtualQuery(At(first), &information, sizeof(information)) == sizeof(information), "query");
        Require(information.Protect == PAGE_READONLY, "read-only page");
    }

    template<typename TAction>
    void Reject(TAction action, const char* message) const {
        const std::vector<unsigned char> before(At(0), At(Pages));
        RequireInvalidArgument(action, message);
        Require(std::memcmp(At(0), before.data(), before.size()) == 0, message);
    }

private:
    std::int64_t physical = 0;
    void* mapping = nullptr;
};

void TestReadOnlyOutputs() {
    const std::size_t caseBytes = JxrPostCaseBytes_nid_no_patch();
    const std::size_t pixelBytes = JxrPostPixelBytes_nid_no_patch();
    Require(caseBytes <= GuestPages::Page * 3 && pixelBytes <= GuestPages::Page, "case fits four pages");
    Pair pair(9000, 0, false, true);
    GuestPages pages;
    void* post = pages.At(0);
    JxrPostCopyTo_nid_no_patch(post, pair.guest);
    auto* pixels = reinterpret_cast<std::int32_t*>(pages.At(3));
    std::memcpy(pixels, JxrPostPixels_nid_no_patch(pair.guest, 0) - PixelOrigin, pixelBytes);
    std::int32_t* previous = pixels + PixelOrigin;
    std::int32_t* current = pixels + 3 * PixelOrigin + PixelOrigin;
    void* table = JxrPostTable_nid_no_patch(post);
    pages.Protect(0, GuestPages::Pages, 1);
    pages.Reject([&] { slideOneMBRow_nid_postfix(table, 3, Width, 1, 1); }, "slideOneMBRow on a read-only table");
    pages.Reject([&] { updatePostProcInfo_nid_postfix(table, current, 2, 1); }, "updatePostProcInfo on a read-only row");
    pages.Reject([&] { postProcMB_nid_postfix(table, previous, current, 2, 1, 100); }, "postProcMB on read-only data");
    pages.Reject([&] { postProcBlock_nid_postfix(table, previous, current, 2, 1, 100); },
                 "postProcBlock on read-only pixels");
    pages.Protect(0, 3, 3);
    pages.Reject([&] { postProcMB_nid_postfix(table, previous, current, 2, 1, 100); }, "postProcMB on read-only pixels");
    pages.Reject([&] { postProcBlock_nid_postfix(table, previous, current, 2, 1, 100); },
                 "postProcBlock on read-only pixels");
    updatePostProcInfo_nid_postfix(table, current, 2, 1);
    JxrPostReferenceUpdate_nid_no_patch(pair.native, 2, 1);
    pages.Protect(0, 3, 1);
    pages.Protect(3, 1, 3);
    pages.Reject([&] { postProcMB_nid_postfix(table, previous, current, 2, 1, 100); }, "postProcMB on read-only rows");
    pages.Reject([&] { updatePostProcInfo_nid_postfix(table, current, 3, 1); }, "updatePostProcInfo on read-only rows");
    pages.Reject([&] { slideOneMBRow_nid_postfix(table, 3, Width, 1, 0); }, "slideOneMBRow on read-only rows");
    postProcBlock_nid_postfix(table, previous, current, 2, 1, 100);
    JxrPostReferenceBlock_nid_no_patch(pair.native, 2, 1, 100);
    pages.Protect(0, 3, 3);
    postProcMB_nid_postfix(table, previous, current, 3, 1, 100);
    JxrPostReferenceMB_nid_no_patch(pair.native, 3, 1, 100);
    Require(std::memcmp(pixels, JxrPostPixels_nid_no_patch(pair.native, 0) - PixelOrigin, pixelBytes) == 0,
            "pixels after write access is restored");
    JxrPostCopyTo_nid_no_patch(pair.guest, pair.native);
    std::memcpy(JxrPostPixels_nid_no_patch(pair.guest, 0) - PixelOrigin, pixels, pixelBytes);
    void* rows = JxrPostClone_nid_no_patch(post);
    Require(rows != nullptr, "allocation");
    std::memcpy(JxrPostPixels_nid_no_patch(rows, 0) - PixelOrigin, pixels, pixelBytes);
    Require(JxrPostEqual_nid_no_patch(rows, pair.native) != 0, "rows after write access is restored");
    std::free(rows);
}
#endif

} // namespace

int main() {
    TestSlide();
    TestUpdate();
    TestMacroblocks();
    TestBlocks();
#ifdef _WIN32
    TestReadOnlyOutputs();
#endif
    return 0;
}
