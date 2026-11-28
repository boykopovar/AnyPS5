#include "prx/libSceJxr/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

extern "C" {
void* JxrPredictMake_nid_no_patch(std::uint32_t seed, std::size_t channels, std::int32_t format, std::uint32_t tiles,
                                  std::size_t width);
void* JxrPredictClone_nid_no_patch(const void* source);
int JxrPredictEqual_nid_no_patch(const void* left, const void* right);
void JxrPredictContext_nid_no_patch(void* value, std::int32_t left, std::int32_t top);
void JxrPredictFillMacroblock_nid_no_patch(void* macroblock, std::uint32_t seed, std::uint8_t index);
void* JxrPredictMacroblockOf_nid_no_patch(void* value);
void JxrPredictSetPredInfo_nid_no_patch(void* value, std::size_t channel, int previous, void* row);
void JxrPredictSetTileQuantizer_nid_no_patch(void* value, std::size_t tile, std::size_t band, std::size_t channel,
                                             void* quantizer);
void JxrPredictReferenceUniform_nid_no_patch(void* value, std::size_t band);
void JxrPredictReferenceUseDc_nid_no_patch(void* value, std::size_t tile);
void JxrPredictReferenceUseLp_nid_no_patch(void* value, std::size_t count, std::size_t tile);
std::int32_t JxrPredictReferenceMode_nid_no_patch(void* value, std::size_t column);
void JxrPredictReferenceUpdate_nid_no_patch(void* value, void* macroblock, std::size_t column, std::int32_t format);
std::int32_t JxrPredictReferenceAllocate_nid_no_patch(void* value);
void JxrPredictReferenceFree_nid_no_patch(void* value);
void* JxrPredictPredInfoMemory_nid_no_patch(void* value);
int JxrPredictPredInfoLaidOut_nid_no_patch(void* value);
void* JxrPredictPredInfoBlock_nid_no_patch(void* value);
void JxrPredictSetPredInfoBlock_nid_no_patch(void* value, void* memory);
}

namespace {

constexpr std::int32_t YOnly = 0;
constexpr std::int32_t Yuv420 = 1;
constexpr std::int32_t Yuv422 = 2;
constexpr std::int32_t Yuv444 = 3;
constexpr std::int32_t Cmyk = 4;
constexpr std::int32_t NComponent = 6;
constexpr std::int32_t Rgbe = 8;
constexpr std::size_t Width = 8;
constexpr std::size_t MacroblockSize = 1160;

bool failAllocation = false;
int liveBlocks = 0;

void* APS5_VABI Allocate(std::size_t size) {
    if (failAllocation) return nullptr;
    ++liveBlocks;
    return std::malloc(size);
}
void* releasingCodec = nullptr;
void* observedPredInfoBlock = nullptr;

void APS5_VABI Free(void* pointer) {
    if (pointer && releasingCodec) observedPredInfoBlock = JxrPredictPredInfoBlock_nid_no_patch(releasingCodec);
    if (pointer) --liveBlocks;
    std::free(pointer);
}
void* APS5_VABI Calloc(std::size_t count, std::size_t size) {
    if (failAllocation) return nullptr;
    ++liveBlocks;
    return std::calloc(count, size);
}
void* APS5_VABI Reallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Align(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI Realign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI PosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

void InitializeHeap() {
    alignas(16) static std::array<std::byte, 0x40> process{};
    alignas(16) static std::array<std::byte, 0x38> libc{};
    alignas(16) static std::array<std::byte, 0x78> replacement{};
    const auto write = [](auto& data, std::size_t offset, auto value) {
        std::memcpy(data.data() + offset, &value, sizeof(value));
    };
    write(process, 0, std::uint64_t{0x40});
    write(process, 8, std::uint32_t{0x4942524f});
    write(process, 0x38, libc.data());
    write(libc, 0, std::uint64_t{0x38});
    write(libc, 0x30, replacement.data());
    write(replacement, 0, std::uint64_t{0x78});
    write(replacement, 8, std::uint64_t{2});
    write(replacement, 0x20, reinterpret_cast<void*>(&Allocate));
    write(replacement, 0x28, reinterpret_cast<void*>(&Free));
    write(replacement, 0x30, reinterpret_cast<void*>(&Calloc));
    write(replacement, 0x38, reinterpret_cast<void*>(&Reallocate));
    write(replacement, 0x40, reinterpret_cast<void*>(&Align));
    write(replacement, 0x48, reinterpret_cast<void*>(&Realign));
    write(replacement, 0x50, reinterpret_cast<void*>(&PosixAlign));
    ApplicationHeapInitialize_nid_no_patch(process.data());
}




void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "JXR prediction: %s\n", message);
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
    Pair(std::uint32_t seed, std::size_t channels, std::int32_t format, std::uint32_t tiles) {
        guest = JxrPredictMake_nid_no_patch(seed, channels, format, tiles, Width);
        Require(guest != nullptr, "allocation");
        native = JxrPredictClone_nid_no_patch(guest);
        Require(native != nullptr, "allocation");
    }
    ~Pair() {
        std::free(guest);
        std::free(native);
    }
    bool Same() const { return JxrPredictEqual_nid_no_patch(guest, native) != 0; }
};

void TestPredInfoRelease() {
    RequireInvalidArgument([] { freePredInfo_nid_postfix(nullptr); }, "freePredInfo accepted a null codec");
    Pair pair(0x2b3c4d5e, 3, Yuv444, 1);
    void* const block = malloc_nid_postfix(2 * Width * 3 * 48);
    Require(block != nullptr, "allocation");
    JxrPredictSetPredInfoBlock_nid_no_patch(pair.guest, block);
    JxrPredictSetPredInfoBlock_nid_no_patch(pair.native, block);
    const int live = liveBlocks;
    std::array<std::byte, 35944> expectedCodec{};
    std::memcpy(expectedCodec.data(), pair.guest, expectedCodec.size());
    std::memset(expectedCodec.data() + 35648, 0, sizeof(void*));
    releasingCodec = pair.guest;
    freePredInfo_nid_postfix(pair.guest);
    releasingCodec = nullptr;
    Require(observedPredInfoBlock == block, "freePredInfo cleared the field before the guest free");
    Require(std::memcmp(pair.guest, expectedCodec.data(), expectedCodec.size()) == 0, "freePredInfo changed other codec bytes");
    Require(liveBlocks == live - 1, "freePredInfo did not release the guest block");
    Require(JxrPredictPredInfoBlock_nid_no_patch(pair.guest) == nullptr, "freePredInfo kept the prediction block");
    JxrPredictSetPredInfoBlock_nid_no_patch(pair.native, nullptr);
    Require(pair.Same(), "freePredInfo changed more than the prediction block");
    freePredInfo_nid_postfix(pair.guest);
    Require(liveBlocks == live - 1, "freePredInfo of an empty codec released a block");
    Require(JxrPredictPredInfoBlock_nid_no_patch(pair.guest) == nullptr && pair.Same(), "freePredInfo of an empty codec changed it");
}

void TestQuantizers() {
    std::uint32_t seed = 0x2468ace1;
    for (std::size_t channels = 1; channels <= 16; channels += 5) {
        for (std::uint32_t tiles = 1; tiles <= 4; ++tiles) {
            for (std::size_t band = 0; band < 4; ++band) {
                Pair pair(++seed, channels, Yuv444, tiles);
                setUniformQuantizer_nid_postfix(pair.guest, band);
                JxrPredictReferenceUniform_nid_no_patch(pair.native, band);
                Require(pair.Same(), "setUniformQuantizer differs from jxrlib");
            }
            for (std::size_t tile = 0; tile < tiles; ++tile) {
                Pair pair(++seed, channels, Yuv444, tiles);
                useDCQuantizer_nid_postfix(pair.guest, tile);
                JxrPredictReferenceUseDc_nid_no_patch(pair.native, tile);
                Require(pair.Same(), "useDCQuantizer differs from jxrlib");
                for (std::size_t count = 0; count <= 16; count += 4) {
                    Pair lp(++seed, channels, Yuv444, tiles);
                    useLPQuantizer_nid_postfix(lp.guest, count, tile);
                    JxrPredictReferenceUseLp_nid_no_patch(lp.native, count, tile);
                    Require(lp.Same(), "useLPQuantizer differs from jxrlib");
                }
            }
        }
    }

    Pair pair(7, 3, Yuv444, 2);
    RequireInvalidArgument([] { setUniformQuantizer_nid_postfix(nullptr, 0); }, "null codec");
    RequireInvalidArgument([&] { useDCQuantizer_nid_postfix(pair.guest, 2); }, "tile past the last column");
    RequireInvalidArgument([&] { useLPQuantizer_nid_postfix(pair.guest, 17, 0); }, "more than 16 quantizers");
    JxrPredictSetTileQuantizer_nid_no_patch(pair.guest, 1, 0, 2, nullptr);
    RequireInvalidArgument([&] { useDCQuantizer_nid_postfix(pair.guest, 1); }, "null DC quantizer");
    JxrPredictSetTileQuantizer_nid_no_patch(pair.guest, 0, 2, 1, nullptr);
    RequireInvalidArgument([&] { useLPQuantizer_nid_postfix(pair.guest, 1, 0); }, "null HP quantizer");
    useLPQuantizer_nid_postfix(pair.guest, 0, 0);
    Pair many(8, 17, Yuv444, 1);
    RequireInvalidArgument([&] { setUniformQuantizer_nid_postfix(many.guest, 0); }, "more than 16 channels");
}

void TestModes() {
    std::uint32_t seed = 0x1357;
    const std::int32_t formats[] = {YOnly, Yuv420, Yuv422, Yuv444, Cmyk, NComponent, Rgbe};
    for (std::int32_t format : formats) {
        for (std::int32_t left = 0; left < 2; ++left) {
            for (std::int32_t top = 0; top < 2; ++top) {
                for (std::size_t column = 0; column < Width; ++column) {
                    if (column == 0 && !left) continue;
                    for (int round = 0; round < 8; ++round) {
                        Pair pair(++seed, format == YOnly || format == NComponent ? 1 : 3, format, 1);
                        JxrPredictContext_nid_no_patch(pair.guest, left, top);
                        JxrPredictContext_nid_no_patch(pair.native, left, top);
                        Require(getDCACPredMode_nid_postfix(pair.guest, column) ==
                                    JxrPredictReferenceMode_nid_no_patch(pair.native, column),
                                "getDCACPredMode differs from jxrlib");
                    }
                }
            }
        }
    }

    Pair pair(9, 3, Yuv444, 1);
    RequireInvalidArgument([] { getDCACPredMode_nid_postfix(nullptr, 1); }, "null codec");
    RequireInvalidArgument([&] { getDCACPredMode_nid_postfix(pair.guest, 0); }, "no left macroblock");
    RequireInvalidArgument([&] { getDCACPredMode_nid_postfix(pair.guest, Width); }, "column past the row");
    JxrPredictContext_nid_no_patch(pair.guest, 1, 1);
    Require(getDCACPredMode_nid_postfix(pair.guest, 0) == 3 + (2 << 2), "corner predicts nothing");
    JxrPredictContext_nid_no_patch(pair.guest, 0, 0);
    JxrPredictSetPredInfo_nid_no_patch(pair.guest, 2, 1, nullptr);
    RequireInvalidArgument([&] { getDCACPredMode_nid_postfix(pair.guest, 1); }, "null chroma row");
    JxrPredictContext_nid_no_patch(pair.guest, 0, 1);
    JxrPredictContext_nid_no_patch(pair.native, 0, 1);
    Require(getDCACPredMode_nid_postfix(pair.guest, 1) == JxrPredictReferenceMode_nid_no_patch(pair.native, 1),
            "top row reads only the current luma row");
}

void TestUpdates() {
    std::uint32_t seed = 0xbeef;
    const std::int32_t formats[] = {YOnly, Yuv420, Yuv422, Yuv444, Cmyk, NComponent, Rgbe};
    alignas(8) unsigned char macroblock[MacroblockSize];
    for (std::int32_t format : formats) {
        for (std::size_t channels = 1; channels <= 16; channels += 3) {
            const std::size_t used = format == Yuv420 || format == Yuv422 ? 3 : channels;
            if (used > 16) continue;
            for (std::size_t column = 0; column < Width; column += 3) {
                Pair pair(++seed, format == Yuv420 || format == Yuv422 ? 3 : channels, format, 1);
                JxrPredictFillMacroblock_nid_no_patch(macroblock, ++seed, static_cast<std::uint8_t>(seed & 3));
                updatePredInfo_nid_postfix(pair.guest, macroblock, column, format);
                JxrPredictReferenceUpdate_nid_no_patch(pair.native, macroblock, column, format);
                Require(pair.Same(), "updatePredInfo differs from jxrlib");
            }
        }
    }

    Pair pair(10, 3, Yuv444, 1);
    void* own = JxrPredictMacroblockOf_nid_no_patch(pair.guest);
    JxrPredictFillMacroblock_nid_no_patch(own, 11, 2);
    JxrPredictFillMacroblock_nid_no_patch(JxrPredictMacroblockOf_nid_no_patch(pair.native), 11, 2);
    updatePredInfo_nid_postfix(pair.guest, own, 4, Yuv444);
    JxrPredictReferenceUpdate_nid_no_patch(pair.native, JxrPredictMacroblockOf_nid_no_patch(pair.native), 4, Yuv444);
    Require(pair.Same(), "updatePredInfo from the codec's own macroblock");
    RequireInvalidArgument([&] { updatePredInfo_nid_postfix(nullptr, macroblock, 0, Yuv444); }, "null codec");
    RequireInvalidArgument([&] { updatePredInfo_nid_postfix(pair.guest, nullptr, 0, Yuv444); }, "null macroblock");
    RequireInvalidArgument([&] { updatePredInfo_nid_postfix(pair.guest, macroblock, Width, Yuv444); },
                           "column past the row");
    RequireInvalidArgument([&] { updatePredInfo_nid_postfix(pair.guest, macroblock, 0, 9); }, "unknown format");
    RequireInvalidArgument([&] { updatePredInfo_nid_postfix(pair.guest, macroblock, 0, -1); }, "negative format");
    JxrPredictSetPredInfo_nid_no_patch(pair.guest, 1, 0, nullptr);
    RequireInvalidArgument([&] { updatePredInfo_nid_postfix(pair.guest, macroblock, 0, Yuv420); }, "null UV row");
}

} // namespace

void TestPredInfoAllocation() {
    RequireInvalidArgument([] { allocatePredInfo_nid_postfix(nullptr); }, "allocatePredInfo accepted a null codec");
    std::uint32_t seed = 0x13579bdf;
    for (std::size_t channels = 1; channels <= 16; channels += 5) {
        for (std::size_t width = 1; width <= 9; width += 4) {
            void* const guest = JxrPredictMake_nid_no_patch(++seed, channels, Yuv444, 1, width);
            Require(guest != nullptr, "allocation");
            void* const native = JxrPredictClone_nid_no_patch(guest);
            Require(native != nullptr, "allocation");
            const int live = liveBlocks;
            Require(allocatePredInfo_nid_postfix(guest) == 0, "allocatePredInfo did not return ICERR_OK");
            Require(liveBlocks == live + 1, "allocatePredInfo did not take one guest block");
            Require(JxrPredictReferenceAllocate_nid_no_patch(native) == 0, "jxrlib allocatePredInfo did not return ICERR_OK");
            Require(JxrPredictPredInfoLaidOut_nid_no_patch(native) != 0, "jxrlib prediction rows are not laid out as expected");
            Require(JxrPredictPredInfoLaidOut_nid_no_patch(guest) != 0, "allocatePredInfo rows differ from jxrlib");
            free_nid_postfix(JxrPredictPredInfoMemory_nid_no_patch(guest));
            Require(liveBlocks == live, "the prediction block was not a guest heap block");
            JxrPredictReferenceFree_nid_no_patch(native);
            std::free(guest);
            std::free(native);
        }
    }
    {
        Pair pair(++seed, 3, Yuv444, 1);
        failAllocation = true;
        const std::int32_t result = allocatePredInfo_nid_postfix(pair.guest);
        failAllocation = false;
        Require(result == -1, "allocatePredInfo did not return ICERR_ERROR when malloc failed");
        Require(JxrPredictPredInfoMemory_nid_no_patch(pair.guest) == nullptr && pair.Same(), "allocatePredInfo changed the codec when malloc failed");
    }
    void* const wide = JxrPredictMake_nid_no_patch(seed, 17, Yuv444, 1, Width);
    Require(wide != nullptr, "allocation");
    RequireInvalidArgument([wide] { allocatePredInfo_nid_postfix(wide); }, "allocatePredInfo accepted 17 channels");
    Require(JxrPredictPredInfoMemory_nid_no_patch(wide) == nullptr, "allocatePredInfo kept rows for 17 channels");
    std::free(wide);
}

int main() {
    InitializeHeap();
    InitializeHeap();
    TestPredInfoRelease();
    TestQuantizers();
    TestModes();
    TestUpdates();
    TestPredInfoAllocation();
    return 0;
}
