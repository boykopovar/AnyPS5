#ifndef CORE_LIBS_PRX_LIBSCEJXR_SRC_CONVERTERCALLBACKS_H
#define CORE_LIBS_PRX_LIBSCEJXR_SRC_CONVERTERCALLBACKS_H

#define JXR_CONVERTER_CALLBACKS(X) \
    X(RGB24_BGR24) \
    X(BGR24_RGB24) \
    X(RGB24_BGR32) \
    X(BGR32_RGB24) \
    X(RGB24_Gray8) \
    X(Gray8_RGB24) \
    X(BGR24_Gray8) \
    X(Gray8_BGR24) \
    X(RGBA128Fixed_RGBA128Float) \
    X(RGBA128Float_RGBA128Fixed) \
    X(RGB96Fixed_RGB96Float) \
    X(RGB96Float_RGB96Fixed) \
    X(RGB96Float_RGB128Float) \
    X(RGB128Float_RGB96Float) \
    X(RGB64Half_RGB48Half) \
    X(RGB48Half_RGB64Half) \
    X(BGR32_BGR24) \
    X(BGR24_BGR32) \
    X(RGB96Float_RGB128Fixed) \
    X(RGB128Fixed_RGB96Float) \
    X(Gray32Fixed_Gray32Float) \
    X(Gray32Float_Gray32Fixed) \
    X(Gray16Fixed_Gray32Float) \
    X(Gray32Float_Gray16Fixed) \
    X(RGB48Fixed_RGB96Float) \
    X(RGB96Float_RGB48Fixed) \
    X(RGB64Fixed_RGB96Float) \
    X(RGB96Float_RGB64Fixed) \
    X(RGBA64Fixed_RGBA128Float) \
    X(RGBA128Float_RGBA64Fixed) \
    X(RGBE_RGB96Float) \
    X(RGB96Float_RGBE) \
    X(RGBA64Half_RGBA128Float) \
    X(RGBA128Float_RGBA64Half) \
    X(RGB64Half_RGB96Float) \
    X(RGB96Float_RGB64Half) \
    X(RGB48Half_RGB96Float) \
    X(RGB96Float_RGB48Half) \
    X(Gray16Half_Gray32Float) \
    X(Gray32Float_Gray16Half) \
    X(RGB555_RGB24) \
    X(RGB24_RGB555) \
    X(RGB565_RGB24) \
    X(RGB24_RGB565) \
    X(RGB101010_RGB48) \
    X(RGB48_RGB101010) \
    X(RGBA32_BGRA32) \
    X(BGRA32_RGBA32) \
    X(BlackWhite_Gray8) \
    X(Gray16_Gray8) \
    X(RGB48_RGB24) \
    X(RGBA64_RGBA32) \
    X(Gray32Float_Gray8) \
    X(RGB96Float_RGB24) \
    X(RGB128Float_RGB24) \
    X(RGBA128Float_RGBA32) \
    X(Gray16Fixed_Gray8) \
    X(Gray32Fixed_Gray8) \
    X(RGB48Fixed_RGB24) \
    X(RGB64Fixed_RGB24) \
    X(RGB96Fixed_RGB24) \
    X(RGB128Fixed_RGB24) \
    X(RGBA64Fixed_RGBA32) \
    X(RGBA128Fixed_RGBA32) \
    X(Gray16Half_Gray8) \
    X(RGB48Half_RGB24) \
    X(RGB64Half_RGB24) \
    X(RGBA64Half_RGBA32) \
    X(RGB101010_RGB24) \
    X(RGBE_RGB24)

enum JxrConverterCallbackId {
#define JXR_CONVERTER_CALLBACK_ID(name) JXR_CONVERT_##name,
    JXR_CONVERTER_CALLBACKS(JXR_CONVERTER_CALLBACK_ID)
#undef JXR_CONVERTER_CALLBACK_ID
    JXR_CONVERT_CALLBACK_COUNT
};

#endif
