#include "prx/libSceFontFt/include/Api.hpp"
#include "prx/libSceFont/include/Api.hpp"
#include "prx/libSceFont/include/FontDriver.hpp"
#include "prx/libSceFont/include/FontTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Font renderer lifecycle check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static int allocations = 0;
static void* APS5_VABI Allocate(void*, std::uint32_t size) {
    ++allocations;
    return std::malloc(size);
}
static void APS5_VABI Release(void*, void* pointer) {
    if (pointer) --allocations;
    std::free(pointer);
}

static int APS5_VABI CreateRenderer(Font::RendererNative*) {
    return SCE_FONT_OK;
}

static int APS5_VABI DestroyRenderer(Font::RendererNative*) {
    return SCE_FONT_OK;
}

static std::uint64_t APS5_VABI QueryRenderer(Font::RendererNative*, std::uint8_t*, std::int64_t*, std::uint8_t*) {
    return 0;
}

int main() {
    const FontMemoryInterface iface{Allocate, Release, nullptr, nullptr, nullptr, nullptr};
    FontMemory memory{};
    Require(sceFontMemoryInit(&memory, nullptr, 0, &iface, &memory, nullptr, nullptr) == SCE_FONT_OK);

    const Font::RendererSelection selection{0, sizeof(Font::RendererFt), CreateRenderer, DestroyRenderer, QueryRenderer};
    FontRenderer renderer = reinterpret_cast<FontRenderer>(&memory);
    Require(sceFontCreateRendererWithEdition(nullptr, &selection, 0, &renderer) == SCE_FONT_ERROR_INVALID_PARAMETER && renderer == nullptr);
    renderer = reinterpret_cast<FontRenderer>(&memory);
    Require(sceFontCreateRendererWithEdition(&memory, nullptr, 0, &renderer) == SCE_FONT_ERROR_INVALID_PARAMETER && renderer == nullptr);
    Require(sceFontCreateRendererWithEdition(&memory, &selection, 0, nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);
    FontMemory uninitialized{};
    Require(sceFontCreateRendererWithEdition(&uninitialized, &selection, 0, &renderer) == SCE_FONT_ERROR_INVALID_MEMORY && renderer == nullptr);

    Require(sceFontCreateRendererWithEdition(&memory, &selection, 0x0093000000000000ull, &renderer) == SCE_FONT_OK && renderer != nullptr);
    auto* native = static_cast<Font::RendererNative*>(renderer);
    Require(native->magic == Font::RENDERER_MAGIC && native->workspace != nullptr && native->workspace_size == 0x4000);
    Require(native->feature_byte_0x0c == 1 && native->outline_policy_flag == 1);
    Require(sceFontRendererGetOutlineBufferSize(renderer, nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);
    std::uint32_t size = 0;
    Require(sceFontRendererGetOutlineBufferSize(renderer, &size) == SCE_FONT_OK && size == 0x4000);
    std::memset(native->workspace, 0xA5, 0x4000);
    Require(sceFontRendererResetOutlineBuffer(renderer) == SCE_FONT_OK);
    for (std::uint32_t i = 0; i < 0x4000; ++i) Require(static_cast<std::uint8_t*>(native->workspace)[i] == 0);
    Require(sceFontRendererSetOutlineBufferPolicy(renderer, 0, 0x8000, 0) == SCE_FONT_OK);
    Require(sceFontRendererGetOutlineBufferSize(renderer, &size) == SCE_FONT_OK && size == 0x8000);
    Require(sceFontRendererSetOutlineBufferPolicy(renderer, 0, 0x10000, 0x8000) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontRendererSetOutlineBufferPolicy(renderer, 0, 0x2000, 0x10000) == SCE_FONT_OK);
    Require(sceFontRendererGetOutlineBufferSize(renderer, &size) == SCE_FONT_OK && size == 0x8000);
    Require(sceFontRendererSetOutlineBufferPolicy(renderer, 0, 0x10000, 0x10000) == SCE_FONT_OK);
    Require(sceFontRendererGetOutlineBufferSize(renderer, &size) == SCE_FONT_OK && size == 0x10000);
    Require(sceFontRendererSetOutlineBufferPolicy(renderer, 0, 0, 0) == SCE_FONT_OK);
    Require(sceFontRendererGetOutlineBufferSize(renderer, &size) == SCE_FONT_OK && size == 0x10000);
    Require(sceFontDestroyRenderer(&renderer) == SCE_FONT_OK && renderer == nullptr);

    Require(sceFontCreateRendererWithEdition(&memory, &selection, 0x0093000000000000ull, &renderer) == SCE_FONT_OK);
    native = static_cast<Font::RendererNative*>(renderer);
    Require(native->feature_byte_0x0c == 1 && native->outline_policy_flag == 1);
    Require(sceFontDestroyRenderer(&renderer) == SCE_FONT_OK);
    Require(sceFontCreateRendererWithEdition(&memory, &selection, 0x008FFFFF00000000ull, &renderer) == SCE_FONT_OK);
    native = static_cast<Font::RendererNative*>(renderer);
    Require(native->feature_byte_0x0c == 0 && native->outline_policy_flag == 0);
    Require(sceFontDestroyRenderer(&renderer) == SCE_FONT_OK);
    Require(sceFontCreateRendererWithEdition(&memory, &selection, 0x0250000000000000ull, &renderer) == SCE_FONT_OK);
    native = static_cast<Font::RendererNative*>(renderer);
    Require(native->feature_byte_0x0c == 0 && native->outline_policy_flag == 0);
    Require(sceFontDestroyRenderer(&renderer) == SCE_FONT_OK);

    FontMemory nullContextMemory{};
    Require(sceFontMemoryInit(&nullContextMemory, nullptr, 0, &iface, nullptr, nullptr, nullptr) == SCE_FONT_OK);
    Require(sceFontCreateRendererWithEdition(&nullContextMemory, &selection, 0, &renderer) == SCE_FONT_OK);
    Require(sceFontRendererSetOutlineBufferPolicy(renderer, 0, 0x8000, 0) == SCE_FONT_ERROR_INVALID_MEMORY);
    Require(sceFontDestroyRenderer(&renderer) == SCE_FONT_OK);
    Require(sceFontMemoryTerm(&nullContextMemory) == SCE_FONT_OK);

    Require(sceFontRendererGetOutlineBufferSize(nullptr, &size) == SCE_FONT_ERROR_INVALID_RENDERER);
    Require(sceFontRendererResetOutlineBuffer(nullptr) == SCE_FONT_ERROR_INVALID_RENDERER);
    Require(sceFontRendererSetOutlineBufferPolicy(nullptr, 0, 0, 0) == SCE_FONT_ERROR_INVALID_RENDERER);
    Font::RendererNative notARenderer{};
    Require(sceFontRendererGetOutlineBufferSize(static_cast<FontRenderer>(&notARenderer), &size) == SCE_FONT_ERROR_INVALID_RENDERER);
    Require(sceFontRendererResetOutlineBuffer(static_cast<FontRenderer>(&notARenderer)) == SCE_FONT_ERROR_INVALID_RENDERER);
    Require(sceFontRendererSetOutlineBufferPolicy(static_cast<FontRenderer>(&notARenderer), 0, 0, 0) == SCE_FONT_ERROR_INVALID_RENDERER);

    Require(sceFontCreateRenderer(&memory, sceFontSelectRendererFt(0), &renderer) == SCE_FONT_OK && renderer != nullptr);
    Require(sceFontDestroyRenderer(&renderer) == SCE_FONT_OK && renderer == nullptr);
    Require(sceFontDestroyRenderer(&renderer) == SCE_FONT_ERROR_INVALID_RENDERER);
    Require(sceFontDestroyRenderer(nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);

    Require(allocations == 0);
    Require(sceFontMemoryTerm(&memory) == SCE_FONT_OK);
}
