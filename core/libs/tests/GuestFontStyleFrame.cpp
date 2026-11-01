#include "prx/libSceFont/include/Api.hpp"
#include "prx/libSceFont/include/FontTypes.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Font style frame check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static bool Near(float value, float expected) {
    return std::fabs(value - expected) < 1e-5f;
}

int main() {
    Require(sceFontStyleFrameInit(nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);
    FontStyleFrame frame{};
    float w = -1.0f;
    float h = -1.0f;
    Require(sceFontStyleFrameSetScalePixel(&frame, 32.0f, 16.0f) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontStyleFrameGetScalePixel(&frame, &w, &h) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontStyleFrameUnsetScale(&frame) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(w == -1.0f && h == -1.0f);

    Require(sceFontStyleFrameInit(&frame) == SCE_FONT_OK);
    std::uint32_t hDpi = 0;
    std::uint32_t vDpi = 0;
    Require(sceFontStyleFrameGetResolutionDpi(&frame, &hDpi, &vDpi) == SCE_FONT_OK && hDpi == 72 && vDpi == 72);
    Require(sceFontStyleFrameGetResolutionDpi(&frame, nullptr, nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontStyleFrameGetScalePixel(&frame, &w, &h) == SCE_FONT_ERROR_UNSET_PARAMETER);
    Require(sceFontStyleFrameGetScalePoint(&frame, &w, &h) == SCE_FONT_ERROR_UNSET_PARAMETER);

    Require(sceFontStyleFrameSetScalePixel(&frame, 32.0f, 16.0f) == SCE_FONT_OK);
    Require(sceFontStyleFrameGetScalePixel(&frame, nullptr, nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontStyleFrameGetScalePixel(&frame, &w, &h) == SCE_FONT_OK && Near(w, 32.0f) && Near(h, 16.0f));
    Require(sceFontStyleFrameGetScalePoint(&frame, &w, &h) == SCE_FONT_OK && Near(w, 32.0f) && Near(h, 16.0f));
    Require(sceFontStyleFrameSetResolutionDpi(&frame, 144, 0) == SCE_FONT_OK);
    Require(sceFontStyleFrameGetResolutionDpi(&frame, &hDpi, &vDpi) == SCE_FONT_OK && hDpi == 144 && vDpi == 72);
    Require(sceFontStyleFrameGetScalePoint(&frame, &w, &h) == SCE_FONT_OK && Near(w, 16.0f) && Near(h, 16.0f));
    Require(sceFontStyleFrameSetScalePoint(&frame, 12.0f, 10.0f) == SCE_FONT_OK);
    Require(sceFontStyleFrameGetScalePixel(&frame, &w, nullptr) == SCE_FONT_OK && Near(w, 24.0f));
    Require(sceFontStyleFrameGetScalePoint(&frame, nullptr, &h) == SCE_FONT_OK && Near(h, 10.0f));
    Require(sceFontStyleFrameUnsetScale(&frame) == SCE_FONT_OK);
    Require(sceFontStyleFrameGetScalePixel(&frame, &w, &h) == SCE_FONT_ERROR_UNSET_PARAMETER);

    float slant = -9.0f;
    Require(sceFontStyleFrameGetEffectSlant(&frame, &slant) == SCE_FONT_ERROR_UNSET_PARAMETER);
    Require(sceFontStyleFrameSetEffectSlant(&frame, 2.0f) == SCE_FONT_OK);
    Require(sceFontStyleFrameGetEffectSlant(&frame, nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontStyleFrameGetEffectSlant(&frame, &slant) == SCE_FONT_OK && Near(slant, 1.0f));
    Require(sceFontStyleFrameSetEffectSlant(&frame, -0.25f) == SCE_FONT_OK);
    Require(sceFontStyleFrameGetEffectSlant(&frame, &slant) == SCE_FONT_OK && Near(slant, -0.25f));
    Require(sceFontStyleFrameUnsetEffectSlant(&frame) == SCE_FONT_OK);
    Require(sceFontStyleFrameGetEffectSlant(&frame, &slant) == SCE_FONT_ERROR_UNSET_PARAMETER);

    float weightX = -9.0f;
    float weightY = -9.0f;
    std::uint32_t mode = 9;
    Require(sceFontStyleFrameGetEffectWeight(&frame, &weightX, &weightY, &mode) == SCE_FONT_ERROR_UNSET_PARAMETER);
    Require(sceFontStyleFrameSetEffectWeight(&frame, 1.02f, 1.02f, 1) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontStyleFrameGetEffectWeight(&frame, &weightX, &weightY, &mode) == SCE_FONT_ERROR_UNSET_PARAMETER);
    Require(sceFontStyleFrameSetEffectWeight(&frame, 1.5f, 0.5f, 0) == SCE_FONT_OK);
    Require(sceFontStyleFrameGetEffectWeight(&frame, nullptr, nullptr, nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontStyleFrameGetEffectWeight(&frame, &weightX, &weightY, &mode) == SCE_FONT_OK && Near(weightX, 1.04f) && Near(weightY, 0.96f) && mode == 0);
    Require(sceFontStyleFrameSetEffectWeight(&frame, 1.02f, 0.99f, 0) == SCE_FONT_OK);
    Require(sceFontStyleFrameGetEffectWeight(&frame, &weightX, nullptr, nullptr) == SCE_FONT_OK && Near(weightX, 1.02f));
    Require(sceFontStyleFrameUnsetEffectWeight(&frame) == SCE_FONT_OK);
    Require(sceFontStyleFrameGetEffectWeight(&frame, &weightX, &weightY, &mode) == SCE_FONT_ERROR_UNSET_PARAMETER);
    return 0;
}
