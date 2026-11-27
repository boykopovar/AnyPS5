#include "JXRGlue.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

void JxrWmpRoundtripReferenceSetDecoderAlphaMode(void* value, uint8_t alphaMode) {
    PKImageDecode* decoder = (PKImageDecode*)value;
    decoder->WMP.wmiSCP.uAlphaMode = alphaMode;
}

size_t JxrWmpRoundtripReferenceParameterSize(void) {
    return sizeof(CWMIStrCodecParam);
}

void JxrWmpRoundtripReferenceParameters(void* value, int32_t spatial, uint8_t alphaMode, uint8_t quality) {
    CWMIStrCodecParam* parameters = (CWMIStrCodecParam*)value;
    memset(parameters, 0, sizeof(*parameters));
    parameters->cfColorFormat = YUV_444;
    parameters->bdBitDepth = BD_LONG;
    parameters->bfBitstreamFormat = spatial ? SPATIAL : FREQUENCY;
    parameters->bProgressiveMode = TRUE;
    parameters->olOverlap = OL_ONE;
    parameters->sbSubband = SB_ALL;
    parameters->uAlphaMode = alphaMode;
    parameters->uiDefaultQPIndex = quality;
    parameters->uiDefaultQPIndexAlpha = quality;
}

const void* JxrWmpRoundtripReferenceFormat(int32_t format) {
    switch (format) {
        case 0: return &GUID_PKPixelFormat24bppRGB;
        case 1: return &GUID_PKPixelFormat32bppBGRA;
        case 2: return &GUID_PKPixelFormat8bppGray;
        default: return &GUID_PKPixelFormat24bppRGB;
    }
}

int64_t JxrWmpRoundtripReferenceEncode(const void* parameters, int32_t format, uint32_t width, uint32_t height,
                                       const uint8_t* pixels, uint32_t stride, int32_t banded,
                                       uint8_t* temporary, uint8_t* output, size_t capacity, size_t* written) {
    ERR err = WMP_errSuccess;
    struct WMPStream* stream = NULL;
    struct WMPStream* alphaStream = NULL;
    PKImageEncode* encoder = NULL;
    *written = 0;
    if (temporary != NULL) {
        Call(CreateWS_Memory(&alphaStream, temporary, capacity));
    }
    Call(CreateWS_Memory(&stream, output, capacity));
    Call(PKImageEncode_Create_WMP(&encoder));
    Call(encoder->Initialize(encoder, stream, (void*)parameters, sizeof(CWMIStrCodecParam)));
    Call(encoder->SetPixelFormat(encoder, *(const PKPixelFormatGUID*)JxrWmpRoundtripReferenceFormat(format)));
    Call(encoder->SetSize(encoder, (I32)width, (I32)height));
    if (banded) {
        Call(encoder->WritePixelsBandedBegin(encoder, alphaStream));
        uint32_t linesWritten = 0;
        while (linesWritten + 16 <= height) {
            Call(encoder->WritePixelsBanded(encoder, 16, (U8*)pixels + linesWritten * stride, stride, FALSE));
            linesWritten += 16;
        }
        if (linesWritten < height) {
            Call(encoder->WritePixelsBanded(encoder, height - linesWritten, (U8*)pixels + linesWritten * stride, stride, TRUE));
        }
        Call(encoder->WritePixelsBandedEnd(encoder));
    } else {
        Call(encoder->WritePixels(encoder, height, (U8*)pixels, stride));
    }
    Call(stream->GetPos(stream, written));
Cleanup:
    if (encoder != NULL) encoder->Release(&encoder);
    else if (stream != NULL) stream->Close(&stream);
    if (alphaStream != NULL) alphaStream->Close(&alphaStream);
    return err;
}

int64_t JxrWmpRoundtripReferenceEncodeBandedError(const void* parameters, int32_t format, uint32_t width, uint32_t height,
                                                   const uint8_t* pixels, uint32_t stride, int32_t linesPerCall, int32_t lastCall,
                                                   uint8_t* temporary, uint8_t* output, size_t capacity, size_t* written) {
    ERR err = WMP_errSuccess;
    struct WMPStream* stream = NULL;
    struct WMPStream* alphaStream = NULL;
    PKImageEncode* encoder = NULL;
    *written = 0;
    if (temporary != NULL) {
        Call(CreateWS_Memory(&alphaStream, temporary, capacity));
    }
    Call(CreateWS_Memory(&stream, output, capacity));
    Call(PKImageEncode_Create_WMP(&encoder));
    Call(encoder->Initialize(encoder, stream, (void*)parameters, sizeof(CWMIStrCodecParam)));
    Call(encoder->SetPixelFormat(encoder, *(const PKPixelFormatGUID*)JxrWmpRoundtripReferenceFormat(format)));
    Call(encoder->SetSize(encoder, (I32)width, (I32)height));
    Call(encoder->WritePixelsBandedBegin(encoder, alphaStream));
    Call(encoder->WritePixelsBanded(encoder, linesPerCall, (U8*)pixels, stride, lastCall));
    if (lastCall) {
        Call(encoder->WritePixelsBandedEnd(encoder));
    }
    Call(stream->GetPos(stream, written));
Cleanup:
    if (encoder != NULL) encoder->Release(&encoder);
    else if (stream != NULL) stream->Close(&stream);
    if (alphaStream != NULL) alphaStream->Close(&alphaStream);
    return err;
}

int64_t JxrWmpRoundtripReferenceDecode(const uint8_t* data, size_t size, uint8_t* pixels, uint32_t stride,
                                       int32_t* width, int32_t* height) {
    ERR err = WMP_errSuccess;
    struct WMPStream* stream = NULL;
    PKImageDecode* decoder = NULL;
    PKRect rect = {0, 0, 0, 0};
    Call(CreateWS_Memory(&stream, (void*)data, size));
    Call(PKImageDecode_Create_WMP(&decoder));
    Call(decoder->Initialize(decoder, stream));
    Call(decoder->GetSize(decoder, width, height));
    rect.Width = *width;
    rect.Height = *height;
    Call(decoder->Copy(decoder, &rect, pixels, stride));
Cleanup:
    if (decoder != NULL) decoder->Release(&decoder);
    if (stream != NULL) stream->Close(&stream);
    return err;
}
