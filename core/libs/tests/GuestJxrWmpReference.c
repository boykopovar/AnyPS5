#include "JXRGlue.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

size_t JxrWmpReferenceParameterSize(void) {
    return sizeof(CWMIStrCodecParam);
}

void JxrWmpReferenceParameters(void* value, int32_t spatial, uint8_t alphaMode) {
    CWMIStrCodecParam* parameters = (CWMIStrCodecParam*)value;
    memset(parameters, 0, sizeof(*parameters));
    parameters->cfColorFormat = YUV_444;
    parameters->bdBitDepth = BD_LONG;
    parameters->bfBitstreamFormat = spatial ? SPATIAL : FREQUENCY;
    parameters->bProgressiveMode = TRUE;
    parameters->olOverlap = OL_ONE;
    parameters->sbSubband = SB_ALL;
    parameters->uAlphaMode = alphaMode;
    parameters->uiDefaultQPIndex = 1;
    parameters->uiDefaultQPIndexAlpha = 1;
}

size_t JxrWmpReferenceTranscodeSize(void) {
    return sizeof(CWMTranscodingParam);
}

void JxrWmpReferenceTranscodeParameters(void* value, uint32_t width, uint32_t height, uint8_t alphaMode) {
    CWMTranscodingParam* parameters = (CWMTranscodingParam*)value;
    memset(parameters, 0, sizeof(*parameters));
    parameters->cWidth = width;
    parameters->cHeight = height;
    parameters->bfBitstreamFormat = SPATIAL;
    parameters->sbSubband = SB_ALL;
    parameters->oOrientation = O_FLIPH;
    parameters->uAlphaMode = alphaMode;
}

size_t JxrWmpReferenceMetadataSize(void) {
    return sizeof(DESCRIPTIVEMETADATA);
}

void JxrWmpReferenceMetadata(void* value, const char* software, uint16_t rating, int32_t wide) {
    DESCRIPTIVEMETADATA* metadata = (DESCRIPTIVEMETADATA*)value;
    static U16 wideText[] = {'A', 'P', 'S', 0};
    memset(metadata, 0, sizeof(*metadata));
    metadata->pvarSoftware.vt = DPKVT_LPSTR;
    metadata->pvarSoftware.VT.pszVal = (char*)software;
    metadata->pvarRatingStars.vt = DPKVT_UI2;
    metadata->pvarRatingStars.VT.uiVal = rating;
    if (wide) {
        metadata->pvarArtist.vt = DPKVT_LPWSTR;
        metadata->pvarArtist.VT.pwszVal = wideText;
    }
}

void JxrWmpReferenceSetCaption(void* value, uint16_t* caption) {
    DESCRIPTIVEMETADATA* metadata = (DESCRIPTIVEMETADATA*)value;
    metadata->pvarCaption.vt = DPKVT_LPWSTR;
    metadata->pvarCaption.VT.pwszVal = caption;
}

const uint16_t* JxrWmpReferenceCaption(const void* value) {
    const DESCRIPTIVEMETADATA* metadata = (const DESCRIPTIVEMETADATA*)value;
    return metadata->pvarCaption.vt == DPKVT_LPWSTR ? metadata->pvarCaption.VT.pwszVal : NULL;
}

const char* JxrWmpReferenceSoftware(const void* value) {
    const DESCRIPTIVEMETADATA* metadata = (const DESCRIPTIVEMETADATA*)value;
    return metadata->pvarSoftware.vt == DPKVT_LPSTR ? metadata->pvarSoftware.VT.pszVal : NULL;
}

uint32_t JxrWmpReferenceRating(const void* value) {
    const DESCRIPTIVEMETADATA* metadata = (const DESCRIPTIVEMETADATA*)value;
    return metadata->pvarRatingStars.vt == DPKVT_UI2 ? metadata->pvarRatingStars.VT.uiVal : 0xffffffffu;
}

static const PKPixelFormatGUID* Format(int32_t alpha) {
    return alpha ? &GUID_PKPixelFormat32bppBGRA : &GUID_PKPixelFormat24bppBGR;
}

int64_t JxrWmpReferenceEncode(const void* parameters, int32_t alpha, uint32_t width, uint32_t height,
                              const uint8_t* pixels, uint32_t stride, int32_t banded, const uint8_t* colorContext,
                              uint32_t colorContextSize, const void* metadata, uint8_t* temporary, uint8_t* output,
                              size_t capacity, size_t* written) {
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
    Call(encoder->SetPixelFormat(encoder, *Format(alpha)));
    Call(encoder->SetSize(encoder, (I32)width, (I32)height));
    if (colorContext != NULL) {
        Call(encoder->SetColorContext(encoder, colorContext, colorContextSize));
    }
    if (metadata != NULL) {
        Call(encoder->SetDescriptiveMetadata(encoder, (const DESCRIPTIVEMETADATA*)metadata));
    }
    if (banded) {
        Call(encoder->WritePixelsBandedBegin(encoder, alphaStream));
        Call(encoder->WritePixelsBanded(encoder, 16, (U8*)pixels, stride, FALSE));
        Call(encoder->WritePixelsBanded(encoder, height - 16, (U8*)pixels + 16 * stride, stride, TRUE));
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

void JxrWmpReferenceConfigureDecoder(void* value) {
    PKImageDecode* decoder = (PKImageDecode*)value;
    PKPixelInfo info;
    memset(&info, 0, sizeof(info));
    info.pGUIDPixFmt = &decoder->guidPixFormat;
    PixelFormatLookup(&info, LOOKUP_FORWARD);
    decoder->WMP.wmiSCP.bfBitstreamFormat = SPATIAL;
    decoder->WMP.wmiSCP.uAlphaMode = (info.grBit & PK_pixfmtHasAlpha) ? 2 : 0;
    decoder->WMP.wmiSCP.sbSubband = SB_ALL;
    decoder->WMP.wmiI.cfColorFormat = info.cfColorFormat;
    decoder->WMP.wmiI.bdBitDepth = info.bdBitDepth;
    decoder->WMP.wmiI.cBitsPerUnit = info.cbitUnit;
    decoder->WMP.wmiI.cThumbnailWidth = decoder->WMP.wmiI.cWidth;
    decoder->WMP.wmiI.cThumbnailHeight = decoder->WMP.wmiI.cHeight;
    decoder->WMP.wmiI.bSkipFlexbits = FALSE;
    decoder->WMP.wmiI.cROILeftX = 0;
    decoder->WMP.wmiI.cROITopY = 0;
    decoder->WMP.wmiI.cROIWidth = decoder->WMP.wmiI.cThumbnailWidth;
    decoder->WMP.wmiI.cROIHeight = decoder->WMP.wmiI.cThumbnailHeight;
    decoder->WMP.wmiI.oOrientation = O_NONE;
    decoder->WMP.wmiI.cPostProcStrength = 0;
}

int64_t JxrWmpReferenceDecode(const uint8_t* data, size_t size, uint8_t* pixels, uint32_t stride, int32_t* width,
                              int32_t* height) {
    ERR err = WMP_errSuccess;
    struct WMPStream* stream = NULL;
    PKImageDecode* decoder = NULL;
    PKRect rect = {0, 0, 0, 0};
    Call(CreateWS_Memory(&stream, (void*)data, size));
    Call(PKImageDecode_Create_WMP(&decoder));
    Call(decoder->Initialize(decoder, stream));
    JxrWmpReferenceConfigureDecoder(decoder);
    Call(decoder->GetSize(decoder, width, height));
    rect.Width = *width;
    rect.Height = *height;
    Call(decoder->Copy(decoder, &rect, pixels, stride));
Cleanup:
    if (decoder != NULL) decoder->Release(&decoder);
    if (stream != NULL) stream->Close(&stream);
    return err;
}

int64_t JxrWmpReferenceTranscode(const uint8_t* data, size_t size, const void* encodeParameters,
                                 const void* transcodeParameters, uint8_t* output, size_t capacity,
                                 size_t* written) {
    ERR err = WMP_errSuccess;
    struct WMPStream* input = NULL;
    struct WMPStream* stream = NULL;
    PKImageDecode* decoder = NULL;
    PKImageEncode* encoder = NULL;
    CWMTranscodingParam parameters;
    *written = 0;
    memcpy(&parameters, transcodeParameters, sizeof(parameters));
    Call(CreateWS_Memory(&input, (void*)data, size));
    Call(PKImageDecode_Create_WMP(&decoder));
    Call(decoder->Initialize(decoder, input));
    Call(CreateWS_Memory(&stream, output, capacity));
    Call(PKImageEncode_Create_WMP(&encoder));
    Call(encoder->Initialize(encoder, stream, (void*)encodeParameters, sizeof(CWMIStrCodecParam)));
    Call(encoder->Transcode(encoder, decoder, &parameters));
    Call(stream->GetPos(stream, written));
Cleanup:
    if (encoder != NULL) encoder->Release(&encoder);
    else if (stream != NULL) stream->Close(&stream);
    if (decoder != NULL) decoder->Release(&decoder);
    if (input != NULL) input->Close(&input);
    return err;
}
