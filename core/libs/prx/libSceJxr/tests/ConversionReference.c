#include "strcodec.h"
#include "JXRGlue.h"
#include <stdint.h>
#include <string.h>

int JxrConversionSource_nid_no_patch(uint8_t index, void* bytes) {
    static const PKPixelFormatGUID* const sources[] = {
        &GUID_PKPixelFormatDontCare, &GUID_PKPixelFormatBlackWhite, &GUID_PKPixelFormat8bppGray,
        &GUID_PKPixelFormat16bppRGB555, &GUID_PKPixelFormat16bppRGB565, &GUID_PKPixelFormat16bppGray,
        &GUID_PKPixelFormat24bppBGR, &GUID_PKPixelFormat24bppRGB, &GUID_PKPixelFormat32bppBGR,
        &GUID_PKPixelFormat32bppBGRA, &GUID_PKPixelFormat32bppPBGRA, &GUID_PKPixelFormat32bppGrayFloat,
        &GUID_PKPixelFormat32bppRGB, &GUID_PKPixelFormat32bppRGBA, &GUID_PKPixelFormat32bppPRGBA,
        &GUID_PKPixelFormat48bppRGBFixedPoint, &GUID_PKPixelFormat16bppGrayFixedPoint, &GUID_PKPixelFormat32bppRGB101010,
        &GUID_PKPixelFormat48bppRGB, &GUID_PKPixelFormat64bppRGBA, &GUID_PKPixelFormat64bppPRGBA,
        &GUID_PKPixelFormat96bppRGBFixedPoint, &GUID_PKPixelFormat96bppRGBFloat, &GUID_PKPixelFormat128bppRGBAFloat,
        &GUID_PKPixelFormat128bppPRGBAFloat, &GUID_PKPixelFormat128bppRGBFloat, &GUID_PKPixelFormat32bppCMYK,
        &GUID_PKPixelFormat64bppRGBAFixedPoint, &GUID_PKPixelFormat64bppRGBFixedPoint, &GUID_PKPixelFormat128bppRGBAFixedPoint,
        &GUID_PKPixelFormat128bppRGBFixedPoint, &GUID_PKPixelFormat64bppRGBAHalf, &GUID_PKPixelFormat64bppRGBHalf,
        &GUID_PKPixelFormat48bppRGBHalf, &GUID_PKPixelFormat32bppRGBE, &GUID_PKPixelFormat16bppGrayHalf,
        &GUID_PKPixelFormat32bppGrayFixedPoint, &GUID_PKPixelFormat64bppCMYK, &GUID_PKPixelFormat24bpp3Channels,
        &GUID_PKPixelFormat32bpp4Channels, &GUID_PKPixelFormat40bpp5Channels, &GUID_PKPixelFormat48bpp6Channels,
        &GUID_PKPixelFormat56bpp7Channels, &GUID_PKPixelFormat64bpp8Channels, &GUID_PKPixelFormat48bpp3Channels,
        &GUID_PKPixelFormat64bpp4Channels, &GUID_PKPixelFormat80bpp5Channels, &GUID_PKPixelFormat96bpp6Channels,
        &GUID_PKPixelFormat112bpp7Channels, &GUID_PKPixelFormat128bpp8Channels, &GUID_PKPixelFormat40bppCMYKAlpha,
        &GUID_PKPixelFormat80bppCMYKAlpha, &GUID_PKPixelFormat32bpp3ChannelsAlpha, &GUID_PKPixelFormat40bpp4ChannelsAlpha,
        &GUID_PKPixelFormat48bpp5ChannelsAlpha, &GUID_PKPixelFormat56bpp6ChannelsAlpha, &GUID_PKPixelFormat64bpp7ChannelsAlpha,
        &GUID_PKPixelFormat72bpp8ChannelsAlpha, &GUID_PKPixelFormat64bpp3ChannelsAlpha, &GUID_PKPixelFormat80bpp4ChannelsAlpha,
        &GUID_PKPixelFormat96bpp5ChannelsAlpha, &GUID_PKPixelFormat112bpp6ChannelsAlpha, &GUID_PKPixelFormat128bpp7ChannelsAlpha,
        &GUID_PKPixelFormat144bpp8ChannelsAlpha, &GUID_PKPixelFormat12bppYCC420, &GUID_PKPixelFormat16bppYCC422,
        &GUID_PKPixelFormat20bppYCC422, &GUID_PKPixelFormat32bppYCC422, &GUID_PKPixelFormat24bppYCC444,
        &GUID_PKPixelFormat30bppYCC444, &GUID_PKPixelFormat48bppYCC444, &GUID_PKPixelFormat16bpp48bppYCC444FixedPoint,
        &GUID_PKPixelFormat20bppYCC420Alpha, &GUID_PKPixelFormat24bppYCC422Alpha, &GUID_PKPixelFormat30bppYCC422Alpha,
        &GUID_PKPixelFormat48bppYCC422Alpha, &GUID_PKPixelFormat32bppYCC444Alpha, &GUID_PKPixelFormat40bppYCC444Alpha,
        &GUID_PKPixelFormat64bppYCC444Alpha, &GUID_PKPixelFormat64bppYCC444AlphaFixedPoint, &GUID_PKPixelFormat32bppCMYKDIRECT,
        &GUID_PKPixelFormat64bppCMYKDIRECT, &GUID_PKPixelFormat40bppCMYKDIRECTAlpha, &GUID_PKPixelFormat80bppCMYKDIRECTAlpha,
    };
    if (index >= sizeof(sources) / sizeof(sources[0])) return 0;
    memcpy(bytes, sources[index], sizeof(*sources[index]));
    return 1;
}

int64_t JxrConversionReference_nid_no_patch(const void* source, uint32_t index, void* bytes) {
    GUID input;
    const PKPixelFormatGUID* output = NULL;
    ERR result;
    memcpy(&input, source, sizeof(input));
    result = PKFormatConverter_EnumConversions(&input, index, &output);
    memcpy(bytes, output, sizeof(*output));
    return result;
}
