#include "prx/libSceJxr/include/Api.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libkernel/libkernel.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

extern "C" {
void* JxrRowsMake_nid_no_patch(std::uint32_t seed, std::size_t channels, std::int32_t format, std::int32_t secondary);
void* JxrRowsClone_nid_no_patch(const void* source);
int JxrRowsEqual_nid_no_patch(const void* left, const void* right);
void* JxrRowsCodec_nid_no_patch(void* value, std::size_t which);
void* JxrRowsContext_nid_no_patch(void* value);
std::size_t JxrRowsCodecBytes_nid_no_patch();
void JxrRowsSetImage_nid_no_patch(void* value, std::int32_t yuv, std::int32_t imageFormat, std::int32_t paramFormat,
                                  std::int32_t depth, std::size_t bitsPerUnit, std::size_t lines, std::size_t stride);
void JxrRowsSetTiles_nid_no_patch(void* value, std::uint32_t columns, std::uint32_t rows, std::size_t column,
                                  std::size_t row);
void JxrRowsSetChannels_nid_no_patch(void* value, std::size_t which, std::size_t channels);
void JxrRowsSetFormat_nid_no_patch(void* value, std::int32_t format);
void JxrRowsSetSecondary_nid_no_patch(void* value, std::int32_t secondary);
void JxrRowsLink_nid_no_patch(void* codec, void* next);
void JxrRowsSetRowBuffer_nid_no_patch(void* value, std::size_t which, std::size_t channel, std::int32_t current,
                                      void* buffer);
std::int32_t JxrRowsReferenceCheck_nid_no_patch(void* value, std::size_t width, std::size_t rows);
void JxrRowsReferenceTile_nid_no_patch(void* value, std::size_t column, std::size_t row);
void JxrRowsReferenceInit_nid_no_patch(void* value);
void JxrRowsReferenceAdvance_nid_no_patch(void* value);
void JxrRowsReferenceSwap_nid_no_patch(void* value);
void JxrRowsReferenceNextRow_nid_no_patch(void* value);
void JxrRowsReferenceReset_nid_no_patch(void* value);
}

namespace {

constexpr std::int32_t YOnly = 0;
constexpr std::int32_t Yuv420 = 1;
constexpr std::int32_t Yuv422 = 2;
constexpr std::int32_t Yuv444 = 3;
constexpr std::int32_t ColorFormats = 9;
constexpr std::size_t Width = 12;
constexpr std::size_t Height = 8;
constexpr std::size_t ContextBytes = 704;

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "JXR rows: %s\n", message);
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
    Pair(std::uint32_t seed, std::size_t channels, std::int32_t format, bool secondary) {
        guest = JxrRowsMake_nid_no_patch(seed, channels, format, secondary);
        Require(guest != nullptr, "allocation");
        native = JxrRowsClone_nid_no_patch(guest);
        Require(native != nullptr, "allocation");
    }
    ~Pair() {
        std::free(guest);
        std::free(native);
    }
    void* Codec() const { return JxrRowsCodec_nid_no_patch(guest, 0); }
    bool Same() const { return JxrRowsEqual_nid_no_patch(guest, native) != 0; }
};

void TestImageBuffer() {
    const std::int32_t formats[] = {YOnly, Yuv420, Yuv422, Yuv444, 4, 6, 8};
    const std::int32_t depths[] = {0, 1, 2, 3, 4, 7};
    const std::size_t units[] = {1, 8, 16, 24, 32, 48, 64, 128};
    const std::size_t widths[] = {0, 1, 7, 64, 200, 1000};
    const std::size_t rows[] = {0, 1, 15, 16, 17, 31, 33};
    std::uint32_t seed = 0x5eed;
    Pair pair(1, 3, Yuv444, false);
    for (std::int32_t yuv = 0; yuv < 2; ++yuv)
        for (std::int32_t format : formats)
            for (std::int32_t depth : depths)
                for (std::size_t unit : units) {
                    const std::size_t stride = (seed = seed * 1103515245u + 12345u) % 8192;
                    JxrRowsSetImage_nid_no_patch(pair.guest, yuv, format, formats[seed % 7], depth, unit, 16, stride);
                    JxrRowsSetImage_nid_no_patch(pair.native, yuv, format, formats[seed % 7], depth, unit, 16, stride);
                    for (std::size_t width : widths)
                        for (std::size_t row : rows)
                            Require(checkImageBuffer_nid_postfix(pair.Codec(), width, row) ==
                                        JxrRowsReferenceCheck_nid_no_patch(pair.native, width, row),
                                    "checkImageBuffer differs from jxrlib");
                }
    Require(pair.Same(), "checkImageBuffer changed the codec");
    RequireInvalidArgument([] { checkImageBuffer_nid_postfix(nullptr, 1, 1); }, "null codec");
}

void TestTiles() {
    for (std::uint32_t columns = 0; columns < 4; ++columns)
        for (std::uint32_t rows = 0; rows < 4; ++rows) {
            Pair pair(columns * 4 + rows, 3, Yuv444, false);
            JxrRowsSetTiles_nid_no_patch(pair.guest, columns, rows, 0, 0);
            JxrRowsSetTiles_nid_no_patch(pair.native, columns, rows, 0, 0);
            for (std::size_t y = 0; y < Height; ++y)
                for (std::size_t x = 0; x < Width; ++x) {
                    getTilePos_nid_postfix(pair.Codec(), x, y);
                    JxrRowsReferenceTile_nid_no_patch(pair.native, x, y);
                    Require(pair.Same(), "getTilePos differs from jxrlib");
                }
        }

    Pair pair(20, 3, Yuv444, false);
    RequireInvalidArgument([] { getTilePos_nid_postfix(nullptr, 0, 0); }, "null codec");
    JxrRowsSetTiles_nid_no_patch(pair.guest, 4096, 0, 0, 0);
    RequireInvalidArgument([&] { getTilePos_nid_postfix(pair.Codec(), 0, 0); }, "too many tile columns");
    JxrRowsSetTiles_nid_no_patch(pair.guest, 0, 4096, 0, 0);
    RequireInvalidArgument([&] { getTilePos_nid_postfix(pair.Codec(), 0, 0); }, "too many tile rows");
    JxrRowsSetTiles_nid_no_patch(pair.guest, 1, 1, 2, 0);
    RequireInvalidArgument([&] { getTilePos_nid_postfix(pair.Codec(), 1, 0); }, "current tile column past the last");
    getTilePos_nid_postfix(pair.Codec(), 0, 0);
    JxrRowsSetTiles_nid_no_patch(pair.guest, 1, 1, 0, 2);
    RequireInvalidArgument([&] { getTilePos_nid_postfix(pair.Codec(), 0, 1); }, "current tile row past the last");
}

void TestRowPointers() {
    std::uint32_t seed = 0x7000;
    for (std::int32_t format = 0; format < ColorFormats; ++format)
        for (std::size_t channels : {std::size_t{1}, std::size_t{3}, std::size_t{4}, std::size_t{16}})
            for (bool secondary : {false, true}) {
                Pair pair(++seed, channels, format, secondary);
                initMRPtr_nid_postfix(pair.Codec());
                JxrRowsReferenceInit_nid_no_patch(pair.native);
                Require(pair.Same(), "initMRPtr differs from jxrlib");
                for (int round = 0; round < 3; ++round) {
                    advanceMRPtr_nid_postfix(pair.Codec());
                    JxrRowsReferenceAdvance_nid_no_patch(pair.native);
                    Require(pair.Same(), "advanceMRPtr differs from jxrlib");
                }
                swapMRPtr_nid_postfix(pair.Codec());
                JxrRowsReferenceSwap_nid_no_patch(pair.native);
                Require(pair.Same(), "swapMRPtr differs from jxrlib");
                advanceOneMBRow_nid_postfix(pair.Codec());
                JxrRowsReferenceNextRow_nid_no_patch(pair.native);
                Require(pair.Same(), "advanceOneMBRow differs from jxrlib");
            }

    RequireInvalidArgument([] { initMRPtr_nid_postfix(nullptr); }, "initMRPtr null codec");
    RequireInvalidArgument([] { advanceMRPtr_nid_postfix(nullptr); }, "advanceMRPtr null codec");
    RequireInvalidArgument([] { swapMRPtr_nid_postfix(nullptr); }, "swapMRPtr null codec");
    RequireInvalidArgument([] { advanceOneMBRow_nid_postfix(nullptr); }, "advanceOneMBRow null codec");

    Pair pair(30, 3, Yuv444, true);
    JxrRowsSetSecondary_nid_no_patch(pair.guest, 1);
    RequireInvalidArgument([&] { advanceMRPtr_nid_postfix(pair.Codec()); }, "secondary codec");
    JxrRowsSetSecondary_nid_no_patch(pair.guest, 0);
    JxrRowsSetFormat_nid_no_patch(pair.guest, ColorFormats);
    RequireInvalidArgument([&] { advanceMRPtr_nid_postfix(pair.Codec()); }, "unknown color format");
    JxrRowsSetFormat_nid_no_patch(pair.guest, -1);
    RequireInvalidArgument([&] { advanceMRPtr_nid_postfix(pair.Codec()); }, "negative color format");
    JxrRowsSetFormat_nid_no_patch(pair.guest, Yuv444);
    JxrRowsSetChannels_nid_no_patch(pair.guest, 0, 17);
    RequireInvalidArgument([&] { advanceMRPtr_nid_postfix(pair.Codec()); }, "advanceMRPtr more than 16 channels");
    RequireInvalidArgument([&] { advanceOneMBRow_nid_postfix(pair.Codec()); }, "advanceOneMBRow more than 16 channels");
    JxrRowsSetChannels_nid_no_patch(pair.guest, 0, 3);
    JxrRowsSetChannels_nid_no_patch(pair.guest, 1, 17);
    RequireInvalidArgument([&] { advanceMRPtr_nid_postfix(pair.Codec()); }, "alpha codec with more than 16 channels");
    RequireInvalidArgument([&] { advanceOneMBRow_nid_postfix(pair.Codec()); }, "alpha codec row with more than 16 channels");
    JxrRowsSetChannels_nid_no_patch(pair.guest, 1, 1);
    JxrRowsSetRowBuffer_nid_no_patch(pair.guest, 0, 2, 0, nullptr);
    RequireInvalidArgument([&] { advanceMRPtr_nid_postfix(pair.Codec()); }, "null previous row buffer");
    JxrRowsSetRowBuffer_nid_no_patch(pair.guest, 0, 2, 0, JxrRowsCodec_nid_no_patch(pair.guest, 0));
    JxrRowsSetRowBuffer_nid_no_patch(pair.guest, 1, 0, 1, nullptr);
    RequireInvalidArgument([&] { advanceMRPtr_nid_postfix(pair.Codec()); }, "null alpha current row buffer");
}

void TestCodingContext() {
    for (std::uint32_t seed = 40; seed < 48; ++seed) {
        Pair pair(seed, 3, Yuv444, false);
        ResetCodingContext_nid_postfix(JxrRowsContext_nid_no_patch(pair.guest));
        JxrRowsReferenceReset_nid_no_patch(pair.native);
        Require(pair.Same(), "ResetCodingContext differs from jxrlib");
    }
    RequireInvalidArgument([] { ResetCodingContext_nid_postfix(nullptr); }, "null coding context");
}

#ifdef _WIN32
class GuestPages {
public:
    static constexpr std::size_t Page = 0x4000;
    static constexpr std::size_t Pages = 7;

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
    const std::size_t codecBytes = JxrRowsCodecBytes_nid_no_patch();
    Require(codecBytes <= GuestPages::Page * 3, "codec fits three pages");
    Pair pair(50, 3, Yuv444, true);
    GuestPages pages;
    unsigned char* codec = pages.At(0);
    unsigned char* secondary = pages.At(3);
    unsigned char* context = pages.At(6);
    std::memcpy(codec, JxrRowsCodec_nid_no_patch(pair.guest, 0), codecBytes);
    std::memcpy(secondary, JxrRowsCodec_nid_no_patch(pair.guest, 1), codecBytes);
    std::memcpy(context, JxrRowsContext_nid_no_patch(pair.guest), ContextBytes);
    JxrRowsLink_nid_no_patch(codec, secondary);
    pages.Protect(0, GuestPages::Pages, 1);
    Require(checkImageBuffer_nid_postfix(codec, 1, 1) == JxrRowsReferenceCheck_nid_no_patch(pair.native, 1, 1),
            "checkImageBuffer reads a read-only codec");
    pages.Reject([&] { getTilePos_nid_postfix(codec, 0, 0); }, "getTilePos on a read-only codec");
    pages.Reject([&] { initMRPtr_nid_postfix(codec); }, "initMRPtr on a read-only codec");
    pages.Reject([&] { advanceMRPtr_nid_postfix(codec); }, "advanceMRPtr on a read-only codec");
    pages.Reject([&] { swapMRPtr_nid_postfix(codec); }, "swapMRPtr on a read-only codec");
    pages.Reject([&] { advanceOneMBRow_nid_postfix(codec); }, "advanceOneMBRow on a read-only codec");
    pages.Reject([&] { ResetCodingContext_nid_postfix(context); }, "ResetCodingContext on a read-only context");
    pages.Protect(0, 3, 3);
    pages.Reject([&] { initMRPtr_nid_postfix(codec); }, "initMRPtr with a read-only alpha codec");
    pages.Reject([&] { advanceMRPtr_nid_postfix(codec); }, "advanceMRPtr with a read-only alpha codec");
    pages.Reject([&] { swapMRPtr_nid_postfix(codec); }, "swapMRPtr with a read-only alpha codec");
    pages.Reject([&] { advanceOneMBRow_nid_postfix(codec); }, "advanceOneMBRow with a read-only alpha codec");
    getTilePos_nid_postfix(codec, 0, 0);
    pages.Protect(3, 4, 3);
    initMRPtr_nid_postfix(codec);
    ResetCodingContext_nid_postfix(context);
    JxrRowsReferenceReset_nid_no_patch(pair.native);
    Require(std::memcmp(context, JxrRowsContext_nid_no_patch(pair.native), ContextBytes) == 0,
            "ResetCodingContext after write access is restored");
}
#endif

} // namespace

int main() {
    TestImageBuffer();
    TestTiles();
    TestRowPointers();
    TestCodingContext();
#ifdef _WIN32
    TestReadOnlyOutputs();
#endif
    return 0;
}
