//*@@@+++@@@@******************************************************************
//
// Copyright © Microsoft Corp.
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
// • Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// • Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//*@@@---@@@@******************************************************************
#include "strcodec.h"
#include "JXRGlue.h"
#include "WmpBridge.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

ERR PKImageEncode_Initialize_WMP(PKImageEncode* pIE, struct WMPStream* pStream, void* pvParam, size_t cbParam);
ERR PKImageEncode_Terminate_WMP(PKImageEncode* pIE);
ERR PKImageEncode_SetDescriptiveMetadata_WMP(PKImageEncode* pIE, const DESCRIPTIVEMETADATA* pSrcMeta);
ERR PKImageEncode_WritePixels_WMP(PKImageEncode* pIE, U32 cLine, U8* pbPixels, U32 cbStride);
ERR PKImageEncode_WritePixelsBandedBegin_WMP(PKImageEncode* pIE, struct WMPStream* pPATempFile);
ERR PKImageEncode_WritePixelsBanded_WMP(PKImageEncode* pIE, U32 cLine, U8* pbPixels, U32 cbStride, Bool fLastCall);
ERR PKImageEncode_WritePixelsBandedEnd_WMP(PKImageEncode* pIE);
ERR PKImageEncode_CreateNewFrame_WMP(PKImageEncode* pIE, void* pvParam, size_t cbParam);
ERR PKImageEncode_Release_WMP(PKImageEncode** ppIE);
ERR WriteContainerPre(PKImageEncode* pIE);
ERR WriteContainerPost(PKImageEncode* pIE);
ERR PKImageDecode_Initialize_WMP(PKImageDecode* pID, struct WMPStream* pWS);
ERR PKImageDecode_GetSize_WMP(PKImageDecode* pID, I32* piWidth, I32* piHeight);
ERR PKImageDecode_GetRawStream_WMP(PKImageDecode* pID, struct WMPStream** ppWS);
ERR PKImageDecode_Copy_WMP(PKImageDecode* pID, const PKRect* pRect, U8* pb, U32 cbStride);
ERR PKImageDecode_GetColorContext_WMP(PKImageDecode* pID, U8* pbColorContext, U32* pcbColorContext);
ERR PKImageDecode_GetDescriptiveMetadata_WMP(PKImageDecode* pID, DESCRIPTIVEMETADATA* pDescMetadata);
ERR PKImageDecode_Release_WMP(PKImageDecode** ppID);

_Static_assert(sizeof(void*) == 8, "JXR WMP guest pointer width");
_Static_assert(sizeof(PKImageEncode) <= 66688, "JXR WMP encoder fits the guest encoder record");
_Static_assert(offsetof(PKImageEncode, Initialize) == 0 && offsetof(PKImageEncode, Release) == 112,
               "JXR WMP encoder slots");
_Static_assert(offsetof(PKImageEncode, pStream) == 120 && offsetof(PKImageEncode, fHeaderDone) == 176,
               "JXR WMP encoder stream and header flag");
_Static_assert(offsetof(PKImageEncode, bWMP) == 520, "JXR WMP encoder flag");
_Static_assert(offsetof(PKImageDecode, GetRawStream) == 48 && offsetof(PKImageDecode, Release) == 80,
               "JXR WMP decoder slots");
_Static_assert(offsetof(PKImageDecode, pStream) == 88 && offsetof(PKImageDecode, fStreamOwner) == 96,
               "JXR WMP decoder stream fields");
_Static_assert(sizeof(DESCRIPTIVEMETADATA) == 14 * sizeof(DPKPROPVARIANT), "JXR WMP descriptive metadata entries");

typedef int64_t (APS5_VABI *JxrGuestStreamClose)(struct WMPStream**);
typedef int32_t (APS5_VABI *JxrGuestStreamEos)(struct WMPStream*);
typedef int64_t (APS5_VABI *JxrGuestStreamRead)(struct WMPStream*, void*, size_t);
typedef int64_t (APS5_VABI *JxrGuestStreamWrite)(struct WMPStream*, const void*, size_t);
typedef int64_t (APS5_VABI *JxrGuestStreamSetPos)(struct WMPStream*, size_t);
typedef int64_t (APS5_VABI *JxrGuestStreamGetPos)(struct WMPStream*, size_t*);
typedef int64_t (APS5_VABI *JxrGuestDecodeGetPixelFormat)(PKImageDecode*, PKPixelFormatGUID*);
typedef int64_t (APS5_VABI *JxrGuestDecodeGetResolution)(PKImageDecode*, Float*, Float*);
typedef int64_t (APS5_VABI *JxrGuestDecodeGetRawStream)(PKImageDecode*, struct WMPStream**);
typedef int64_t (APS5_VABI *JxrGuestEncodeSetPixelFormat)(PKImageEncode*, PKPixelFormatGUID);
typedef int64_t (APS5_VABI *JxrGuestEncodeSetSize)(PKImageEncode*, I32, I32);
typedef int64_t (APS5_VABI *JxrGuestEncodeSetResolution)(PKImageEncode*, Float, Float);

typedef struct JxrWmpProxy {
    struct WMPStream stream;
    struct WMPStream* guest;
} JxrWmpProxy;

typedef struct JxrWmpScope {
    JxrWmpProxy proxy;
    struct WMPStream** field;
    struct WMPStream* guest;
} JxrWmpScope;

static ERR ProxyClose(struct WMPStream** me);
static Bool ProxyEos(struct WMPStream* me);
static ERR ProxyRead(struct WMPStream* me, void* pv, size_t cb);
static ERR ProxyWrite(struct WMPStream* me, const void* pv, size_t cb);
static ERR ProxySetPos(struct WMPStream* me, size_t offPos);
static ERR ProxyGetPos(struct WMPStream* me, size_t* poffPos);

static int IsProxy(const struct WMPStream* stream) {
    return stream != NULL && stream->Read == ProxyRead;
}

static struct WMPStream* Target(struct WMPStream* stream) {
    return IsProxy(stream) ? ((JxrWmpProxy*)stream)->guest : stream;
}

static struct WMPStream* Callback(struct WMPStream* me, size_t offset, const char* name) {
    struct WMPStream* guest = Target(me);
    void* callback = NULL;
    memcpy(&callback, (const uint8_t*)guest + offset, sizeof(callback));
    if (callback == NULL) JxrWmpMissingCallback_nid_no_patch(name);
    return guest;
}

static ERR ProxyClose(struct WMPStream** me) {
    struct WMPStream* guest = Callback(*me, offsetof(struct WMPStream, Close), "Close");
    *me = guest;
    return (ERR)((JxrGuestStreamClose)guest->Close)(me);
}

static Bool ProxyEos(struct WMPStream* me) {
    struct WMPStream* guest = Callback(me, offsetof(struct WMPStream, EOS), "EOS");
    return (Bool)((JxrGuestStreamEos)guest->EOS)(guest);
}

static ERR ProxyRead(struct WMPStream* me, void* pv, size_t cb) {
    struct WMPStream* guest = Callback(me, offsetof(struct WMPStream, Read), "Read");
    return (ERR)((JxrGuestStreamRead)guest->Read)(guest, pv, cb);
}

static ERR ProxyWrite(struct WMPStream* me, const void* pv, size_t cb) {
    struct WMPStream* guest = Callback(me, offsetof(struct WMPStream, Write), "Write");
    return (ERR)((JxrGuestStreamWrite)guest->Write)(guest, pv, cb);
}

static ERR ProxySetPos(struct WMPStream* me, size_t offPos) {
    struct WMPStream* guest = Callback(me, offsetof(struct WMPStream, SetPos), "SetPos");
    return (ERR)((JxrGuestStreamSetPos)guest->SetPos)(guest, offPos);
}

static ERR ProxyGetPos(struct WMPStream* me, size_t* poffPos) {
    struct WMPStream* guest = Callback(me, offsetof(struct WMPStream, GetPos), "GetPos");
    return (ERR)((JxrGuestStreamGetPos)guest->GetPos)(guest, poffPos);
}

static void Bind(JxrWmpProxy* proxy, struct WMPStream* guest) {
    memset(proxy, 0, sizeof(*proxy));
    proxy->stream.Close = ProxyClose;
    proxy->stream.EOS = ProxyEos;
    proxy->stream.Read = ProxyRead;
    proxy->stream.Write = ProxyWrite;
    proxy->stream.SetPos = ProxySetPos;
    proxy->stream.GetPos = ProxyGetPos;
    proxy->guest = Target(guest);
}

static struct WMPStream* Retain(void* record, struct WMPStream* guest) {
    JxrWmpProxy* proxy;
    if (guest == NULL) return NULL;
    proxy = (JxrWmpProxy*)JxrWmpRetainProxy_nid_no_patch(record, sizeof(JxrWmpProxy));
    Bind(proxy, guest);
    return &proxy->stream;
}

static void Restore(struct WMPStream** field, struct WMPStream* proxy) {
    if (proxy != NULL && *field == proxy) *field = Target(proxy);
}

static void Enter(JxrWmpScope* scope, struct WMPStream** field) {
    scope->field = field;
    scope->guest = *field;
    if (scope->guest == NULL) return;
    Bind(&scope->proxy, scope->guest);
    *field = &scope->proxy.stream;
}

static void Leave(JxrWmpScope* scope) {
    if (scope->guest != NULL && *scope->field == &scope->proxy.stream) *scope->field = scope->guest;
}

static int KnownFormat(const PKPixelFormatGUID* format) {
    PKPixelInfo info;
    memset(&info, 0, sizeof(info));
    info.pGUIDPixFmt = format;
    return !Failed(PixelFormatLookup(&info, LOOKUP_FORWARD));
}

static int HasWideText(const DESCRIPTIVEMETADATA* metadata) {
    const DPKPROPVARIANT* entries = (const DPKPROPVARIANT*)metadata;
    size_t i;
    for (i = 0; i < sizeof(*metadata) / sizeof(*entries); ++i)
        if (entries[i].vt == DPKVT_LPWSTR) return 1;
    return 0;
}

static int32_t EncoderRefusal(const PKImageEncode* pIE, int lookup, int header) {
    if (pIE->pStream == NULL || pIE->WMP.wmiSCP.pWStream == NULL) return JXR_WMP_REFUSE_STREAM;
    if (lookup && !KnownFormat(&pIE->guidPixFormat)) return JXR_WMP_REFUSE_FORMAT;
    if (header && HasWideText(&pIE->sDescMetadata)) return JXR_WMP_REFUSE_WIDE_TEXT;
    return JXR_WMP_ACCEPTED;
}

size_t JxrWmpEncodeParameterSize_nid_no_patch(void) {
    return sizeof(CWMIStrCodecParam);
}

int64_t JxrWmpEncodeInitialize_nid_no_patch(void* encoder, void* stream, void* parameters, size_t size) {
    PKImageEncode* pIE = (PKImageEncode*)encoder;
    struct WMPStream* proxy = Retain(pIE, (struct WMPStream*)stream);
    const ERR err = PKImageEncode_Initialize_WMP(pIE, proxy, parameters, size);
    Restore(&pIE->pStream, proxy);
    return err;
}

int64_t JxrWmpEncodeTerminate_nid_no_patch(void* encoder) {
    return PKImageEncode_Terminate_WMP((PKImageEncode*)encoder);
}

int64_t JxrWmpEncodeSetDescriptiveMetadata_nid_no_patch(void* encoder, const void* metadata, int32_t* refusal) {
    PKImageEncode* pIE = (PKImageEncode*)encoder;
    if (pIE->fHeaderDone) return WMP_errOutOfSequence;
    if (HasWideText((const DESCRIPTIVEMETADATA*)metadata)) {
        *refusal = JXR_WMP_REFUSE_WIDE_TEXT;
        return WMP_errSuccess;
    }
    return PKImageEncode_SetDescriptiveMetadata_WMP(pIE, (const DESCRIPTIVEMETADATA*)metadata);
}

int64_t JxrWmpEncodeWritePixels_nid_no_patch(void* encoder, uint32_t lines, uint8_t* pixels, uint32_t stride,
                                             int32_t* refusal) {
    PKImageEncode* pIE = (PKImageEncode*)encoder;
    JxrWmpScope scope;
    ERR err;
    if (pIE->WMP.eBandedEncState != BANDEDENCSTATE_UNINITIALIZED) {
        *refusal = JXR_WMP_REFUSE_STATE;
        return WMP_errSuccess;
    }
    *refusal = EncoderRefusal(pIE, 1, !pIE->fHeaderDone);
    if (*refusal != JXR_WMP_ACCEPTED) return WMP_errSuccess;
    Enter(&scope, &pIE->pStream);
    err = PKImageEncode_WritePixels_WMP(pIE, lines, pixels, stride);
    Leave(&scope);
    return err;
}

int64_t JxrWmpEncodeBandedBegin_nid_no_patch(void* encoder, void* temporary, int32_t* refusal) {
    PKImageEncode* pIE = (PKImageEncode*)encoder;
    if (pIE->WMP.eBandedEncState != BANDEDENCSTATE_UNINITIALIZED) {
        *refusal = JXR_WMP_REFUSE_STATE;
        return WMP_errSuccess;
    }
    return PKImageEncode_WritePixelsBandedBegin_WMP(pIE, Retain(pIE, (struct WMPStream*)temporary));
}

int64_t JxrWmpEncodeBanded_nid_no_patch(void* encoder, uint32_t lines, uint8_t* pixels, uint32_t stride,
                                        int32_t last, int32_t* refusal) {
    PKImageEncode* pIE = (PKImageEncode*)encoder;
    JxrWmpScope scope;
    ERR err;
    if (last || lines % 16 == 0) {
        const int begin = pIE->WMP.eBandedEncState == BANDEDENCSTATE_INIT;
        if (!begin && pIE->WMP.eBandedEncState != BANDEDENCSTATE_ENCODING) {
            *refusal = JXR_WMP_REFUSE_STATE;
            return WMP_errSuccess;
        }
        *refusal = EncoderRefusal(pIE, !pIE->fHeaderDone || begin, !pIE->fHeaderDone);
        if (*refusal != JXR_WMP_ACCEPTED) return WMP_errSuccess;
    }
    Enter(&scope, &pIE->pStream);
    err = PKImageEncode_WritePixelsBanded_WMP(pIE, lines, pixels, stride, last);
    Leave(&scope);
    return err;
}

int64_t JxrWmpEncodeBandedEnd_nid_no_patch(void* encoder, int32_t* refusal) {
    PKImageEncode* pIE = (PKImageEncode*)encoder;
    JxrWmpScope scope;
    ERR err;
    if (pIE->WMP.eBandedEncState != BANDEDENCSTATE_ENCODING) {
        *refusal = JXR_WMP_REFUSE_STATE;
        return WMP_errSuccess;
    }
    *refusal = EncoderRefusal(pIE, 0, 0);
    if (*refusal != JXR_WMP_ACCEPTED) return WMP_errSuccess;
    Enter(&scope, &pIE->pStream);
    err = PKImageEncode_WritePixelsBandedEnd_WMP(pIE);
    Leave(&scope);
    return err;
}

int64_t JxrWmpEncodeTranscode_nid_no_patch(void* encoder, void* decoder, void* parameters, int32_t* refusal) {
    PKImageEncode* pIE = (PKImageEncode*)encoder;
    PKImageDecode* pID = (PKImageDecode*)decoder;
    CWMTranscodingParam* pParam = (CWMTranscodingParam*)parameters;
    ERR err = WMP_errSuccess;
    Float fResX = 0, fResY = 0;
    PKPixelFormatGUID pixGUID = {0};
    CWMTranscodingParam tcParamAlpha;
    size_t offPos = 0;
    Bool fPlanarAlpha;
    PKPixelInfo PI;
    struct WMPStream* pWSDec = NULL;
    JxrWmpProxy decoderStream;
    JxrWmpProxy encoderStream;
    JxrWmpScope scope;
    struct WMPStream* pWSEnc = pIE->pStream;

    scope.guest = NULL;
    memset(&PI, 0, sizeof(PI));

    if (pID->GetPixelFormat == NULL || pIE->SetPixelFormat == NULL) goto Refuse;
    Call(((JxrGuestDecodeGetPixelFormat)pID->GetPixelFormat)(pID, &pixGUID));
    Call(((JxrGuestEncodeSetPixelFormat)pIE->SetPixelFormat)(pIE, pixGUID));

    if (pIE->SetSize == NULL) goto Refuse;
    Call(((JxrGuestEncodeSetSize)pIE->SetSize)(pIE, (I32)pParam->cWidth, (I32)pParam->cHeight));

    if (pID->GetResolution == NULL || pIE->SetResolution == NULL) goto Refuse;
    Call(((JxrGuestDecodeGetResolution)pID->GetResolution)(pID, &fResX, &fResY));
    Call(((JxrGuestEncodeSetResolution)pIE->SetResolution)(pIE, fResX, fResY));

    PI.pGUIDPixFmt = &pIE->guidPixFormat;
    if (Failed(PixelFormatLookup(&PI, LOOKUP_FORWARD))) {
        *refusal = JXR_WMP_REFUSE_FORMAT;
        goto Cleanup;
    }
    pIE->WMP.bHasAlpha = !!(PI.grBit & PK_pixfmtHasAlpha) && (2 == pParam->uAlphaMode);

    PI.pGUIDPixFmt = &pixGUID;
    if (Failed(PixelFormatLookup(&PI, LOOKUP_FORWARD))) {
        *refusal = JXR_WMP_REFUSE_FORMAT;
        goto Cleanup;
    }
    FailIf(0 == (PI.grBit & PK_pixfmtHasAlpha) && pParam->uAlphaMode != 0,
        WMP_errAlphaModeCannotBeTranscoded);
    FailIf(!!(PI.grBit & PK_pixfmtHasAlpha) && 2 == pParam->uAlphaMode &&
        FALSE == pID->WMP.bHasAlpha, WMP_errAlphaModeCannotBeTranscoded);
    FailIf(!!(PI.grBit & PK_pixfmtHasAlpha) && 3 == pParam->uAlphaMode &&
        pID->WMP.bHasAlpha, WMP_errAlphaModeCannotBeTranscoded);

    fPlanarAlpha = pIE->WMP.bHasAlpha && (2 == pParam->uAlphaMode);

    if (pIE->pStream == NULL || pWSEnc == NULL) {
        *refusal = JXR_WMP_REFUSE_STREAM;
        goto Cleanup;
    }
    if (HasWideText(&pIE->sDescMetadata)) {
        *refusal = JXR_WMP_REFUSE_WIDE_TEXT;
        goto Cleanup;
    }
    Enter(&scope, &pIE->pStream);
    Call(WriteContainerPre(pIE));

    if (fPlanarAlpha)
        tcParamAlpha = *pParam;

    if (pID->GetRawStream == NULL) goto Refuse;
    Call(((JxrGuestDecodeGetRawStream)pID->GetRawStream)(pID, &pWSDec));
    if (pWSDec == NULL) {
        *refusal = JXR_WMP_REFUSE_STREAM;
        goto Cleanup;
    }
    Bind(&decoderStream, pWSDec);
    Bind(&encoderStream, pWSEnc);

    FailIf(ICERR_OK != WMPhotoTranscode(&decoderStream.stream, &encoderStream.stream, pParam), WMP_errFail);
    Call(pIE->pStream->GetPos(pIE->pStream, &offPos));
    pIE->WMP.nCbImage = (Long)offPos - pIE->WMP.nOffImage;

    if (fPlanarAlpha)
    {
        pIE->WMP.nOffAlpha = (Long)offPos;

        Call(decoderStream.stream.SetPos(&decoderStream.stream, pID->WMP.wmiDEMisc.uAlphaOffset));

        FailIf(ICERR_OK != WMPhotoTranscode(&decoderStream.stream, &encoderStream.stream, &tcParamAlpha), WMP_errFail);
        Call(pIE->pStream->GetPos(pIE->pStream, &offPos));
        pIE->WMP.nCbAlpha = (Long)offPos - pIE->WMP.nOffAlpha;
    }

    Call(WriteContainerPost(pIE));
    goto Cleanup;

Refuse:
    *refusal = JXR_WMP_REFUSE_CALLBACK;
Cleanup:
    if (scope.guest != NULL) Leave(&scope);
    return err;
}

int64_t JxrWmpEncodeCreateNewFrame_nid_no_patch(void* encoder, void* parameters, size_t size) {
    return PKImageEncode_CreateNewFrame_WMP((PKImageEncode*)encoder, parameters, size);
}

int64_t JxrWmpEncodeRelease_nid_no_patch(void** encoder, int32_t* refusal) {
    PKImageEncode* pIE = (PKImageEncode*)*encoder;
    JxrWmpScope scope;
    if (pIE->pStream == NULL) {
        *refusal = JXR_WMP_REFUSE_STREAM;
        return WMP_errSuccess;
    }
    Enter(&scope, &pIE->pStream);
    return PKImageEncode_Release_WMP((PKImageEncode**)encoder);
}

void JxrWmpDecodeInstall_nid_no_patch(void* decoder, const JxrWmpDecodeSlots* slots) {
    PKImageDecode* pID = (PKImageDecode*)decoder;
    pID->Initialize = (ERR (*)(PKImageDecode*, struct WMPStream*))slots->initialize;
    pID->GetSize = (ERR (*)(PKImageDecode*, I32*, I32*))slots->getSize;
    pID->GetRawStream = (ERR (*)(PKImageDecode*, struct WMPStream**))slots->getRawStream;
    pID->Copy = (ERR (*)(PKImageDecode*, const PKRect*, U8*, U32))slots->copy;
    pID->GetColorContext = (ERR (*)(PKImageDecode*, U8*, U32*))slots->getColorContext;
    pID->GetDescriptiveMetadata = (ERR (*)(PKImageDecode*, DESCRIPTIVEMETADATA*))slots->getDescriptiveMetadata;
    pID->Release = (ERR (*)(PKImageDecode**))slots->release;
}

int64_t JxrWmpDecodeInitialize_nid_no_patch(void* decoder, void* stream) {
    PKImageDecode* pID = (PKImageDecode*)decoder;
    struct WMPStream* proxy = Retain(pID, (struct WMPStream*)stream);
    const ERR err = PKImageDecode_Initialize_WMP(pID, proxy);
    Restore(&pID->pStream, proxy);
    return err;
}

int64_t JxrWmpDecodeGetSize_nid_no_patch(void* decoder, int32_t* width, int32_t* height) {
    return PKImageDecode_GetSize_WMP((PKImageDecode*)decoder, (I32*)width, (I32*)height);
}

int64_t JxrWmpDecodeGetRawStream_nid_no_patch(void* decoder, void** stream, int32_t* refusal) {
    PKImageDecode* pID = (PKImageDecode*)decoder;
    struct WMPStream* raw = NULL;
    JxrWmpScope scope;
    ERR err;
    if (pID->pStream == NULL) {
        *refusal = JXR_WMP_REFUSE_STREAM;
        return WMP_errSuccess;
    }
    Enter(&scope, &pID->pStream);
    err = PKImageDecode_GetRawStream_WMP(pID, &raw);
    Leave(&scope);
    *stream = raw == &scope.proxy.stream ? scope.guest : Target(raw);
    return err;
}

int64_t JxrWmpDecodeCopy_nid_no_patch(void* decoder, const void* rectangle, uint8_t* pixels, uint32_t stride,
                                      int32_t* refusal) {
    PKImageDecode* pID = (PKImageDecode*)decoder;
    JxrWmpScope scope;
    ERR err;
    if (pID->WMP.wmiSCP.pWStream == NULL ||
        (pID->WMP.bHasAlpha && pID->WMP.wmiSCP.uAlphaMode != 0 && pID->pStream == NULL)) {
        *refusal = JXR_WMP_REFUSE_STREAM;
        return WMP_errSuccess;
    }
    Enter(&scope, &pID->pStream);
    err = PKImageDecode_Copy_WMP(pID, (const PKRect*)rectangle, pixels, stride);
    Leave(&scope);
    return err;
}

int64_t JxrWmpDecodeGetColorContext_nid_no_patch(void* decoder, uint8_t* buffer, uint32_t* size, int32_t* refusal) {
    PKImageDecode* pID = (PKImageDecode*)decoder;
    JxrWmpScope scope;
    ERR err;
    if (buffer != NULL && pID->WMP.wmiDEMisc.uColorProfileOffset != 0 && pID->pStream == NULL) {
        *refusal = JXR_WMP_REFUSE_STREAM;
        return WMP_errSuccess;
    }
    Enter(&scope, &pID->pStream);
    err = PKImageDecode_GetColorContext_WMP(pID, buffer, size);
    Leave(&scope);
    return err;
}

int64_t JxrWmpDecodeGetDescriptiveMetadata_nid_no_patch(void* decoder, void* metadata) {
    return PKImageDecode_GetDescriptiveMetadata_WMP((PKImageDecode*)decoder, (DESCRIPTIVEMETADATA*)metadata);
}

int64_t JxrWmpDecodeRelease_nid_no_patch(void** decoder, int32_t* refusal) {
    PKImageDecode* pID = (PKImageDecode*)*decoder;
    JxrWmpScope scope;
    if (pID->fStreamOwner && pID->pStream == NULL) {
        *refusal = JXR_WMP_REFUSE_STREAM;
        return WMP_errSuccess;
    }
    Enter(&scope, &pID->pStream);
    return PKImageDecode_Release_WMP((PKImageDecode**)decoder);
}
