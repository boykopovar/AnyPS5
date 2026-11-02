#ifndef CORE_LIBS_PRX_LIBSCELIBRESSL_INCLUDE_SSLINTERNAL_HPP
#define CORE_LIBS_PRX_LIBSCELIBRESSL_INCLUDE_SSLINTERNAL_HPP

#include "prx/libSceLibreSsl/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace SslLayout {

inline constexpr int ErrorLibrarySsl = 20;
inline constexpr int ErrorFunctionAny = 0xfff;
inline constexpr int ErrorMallocFailure = 65;
inline constexpr int ErrorShouldNotHaveBeenCalled = 66;
inline constexpr int ErrorInternalError = 68;
inline constexpr int ErrorBufferLibrary = 7;

inline constexpr int ReasonBadWriteRetry = 127;
inline constexpr int ReasonBioNotSet = 128;
inline constexpr int ReasonCertificateVerifyFailed = 134;
inline constexpr int ReasonNoMethodSpecified = 188;
inline constexpr int ReasonNullSslCtx = 195;
inline constexpr int ReasonNullSslMethodPassed = 196;
inline constexpr int ReasonSslCtxHasNoDefaultSslVersion = 228;
inline constexpr int ReasonProtocolIsShutdown = 207;
inline constexpr int ReasonUninitialized = 276;
inline constexpr int ReasonServerNameInvalid = 319;
inline constexpr int ReasonServerNameTypeInvalid = 320;
inline constexpr int ReasonVerificationSetupProblems = 269;

inline constexpr int StateOk = 0x03;
inline constexpr int StateConnect = 0x1000;
inline constexpr int StateAccept = 0x2000;
inline constexpr int StateBefore = 0x4000;
inline constexpr int StateReadHeader = 0xF0;

inline constexpr int RwNothing = 1;
inline constexpr int RwWriting = 2;
inline constexpr int RwReading = 3;

inline constexpr int ShutdownSent = 1;
inline constexpr int ShutdownReceived = 2;

inline constexpr std::uint64_t OptionLegacyServerConnect = 0x4;
inline constexpr int SessionCacheServer = 0x2;
inline constexpr std::uint64_t SessionCacheSizeDefault = 1024 * 20;
inline constexpr std::int64_t MaxCertListDefault = 1024 * 100;
inline constexpr unsigned int MaxPlainLength = 16384;
inline constexpr std::int64_t DefaultTimeout = 60 * 60 * 2;

inline constexpr int Tls1Version = 0x0301;
inline constexpr int Tls1_1Version = 0x0302;
inline constexpr int Tls1_2Version = 0x0303;
inline constexpr int Dtls1Version = 0xFEFF;

inline constexpr int ExClassSsl = 1;
inline constexpr int ExClassSslCtx = 2;

inline constexpr int BioTypeSocket = 5 | 0x0400 | 0x0100;
inline constexpr int BioTypeDescriptor = 0x0100;
inline constexpr int BioFlagRead = 0x01;
inline constexpr int BioFlagWrite = 0x02;
inline constexpr int BioFlagIoSpecial = 0x04;
inline constexpr int BioFlagShouldRetry = 0x08;
inline constexpr int BioRetryConnect = 0x02;
inline constexpr int BioRetryAccept = 0x03;

inline constexpr std::size_t InternalMinVersion = 0;
inline constexpr std::size_t InternalMaxVersion = 2;
inline constexpr std::size_t InternalInHandshake = 52;
inline constexpr std::size_t InternalAlpnList = 40;
inline constexpr std::size_t InternalAlpnListLength = 48;
inline constexpr std::size_t InternalGenerateSessionId = 80;
inline constexpr std::size_t InternalType = 152;
inline constexpr std::size_t InternalNewSession = 160;
inline constexpr std::size_t InternalQuietShutdown = 164;
inline constexpr std::size_t InternalPacketLength = 208;
inline constexpr std::size_t InternalHit = 216;
inline constexpr std::size_t InternalOcspResponseLength = 344;
inline constexpr std::size_t InternalEcPointFormatLength = 352;
inline constexpr std::size_t InternalEcPointFormats = 360;
inline constexpr std::size_t InternalSupportedGroupsLength = 368;
inline constexpr std::size_t InternalSupportedGroups = 376;
inline constexpr std::size_t InternalExData = 272;
inline constexpr std::size_t InternalClientCa = 280;
inline constexpr std::size_t InternalRenegotiate = 408;
inline constexpr std::size_t InternalReadState = 416;
inline constexpr std::size_t InternalSize = 432;

inline constexpr std::size_t Ssl3Size = 144;
inline constexpr std::size_t Ssl3InternalPointer = 136;
inline constexpr std::size_t Ssl3AlertDispatch = 120;
inline constexpr std::size_t Ssl3InternalSize = 1040;
inline constexpr std::size_t Ssl3InternalWarnAlert = 324;

template <typename TValue>
TValue Read(const void* base, std::size_t offset) {
    TValue value;
    std::memcpy(&value, static_cast<const unsigned char*>(base) + offset, sizeof(value));
    return value;
}

template <typename TValue>
void Write(void* base, std::size_t offset, TValue value) {
    std::memcpy(static_cast<unsigned char*>(base) + offset, &value, sizeof(value));
}

}

extern "C" {

bool LibreSslIsUndefinedFunction_nid_no_patch(SslStateFunction function);
void LibreSslPutSslError_nid_no_patch(const LibreSsl* ssl, int reason);
void LibreSslPutSslErrorX_nid_no_patch(int reason);
int LibreSslStateFunction_nid_no_patch(int state);

bool LibreSslNewExData_nid_no_patch(int classIndex, void* object, LibreSslExData* data);
void LibreSslFreeExData_nid_no_patch(int classIndex, void* object, LibreSslExData* data);

LibreSslX509VerifyParam* LibreSslNewVerifyParam_nid_no_patch();
void LibreSslFreeVerifyParam_nid_no_patch(LibreSslX509VerifyParam* param, const char* function);

LibreSslBio* LibreSslNewSocketBio_nid_no_patch(int descriptor);
void LibreSslFreeBioChain_nid_no_patch(LibreSslBio* bio);
int LibreSslBioDescriptor_nid_no_patch(const LibreSslBio* bio);
bool LibreSslIsSocketBio_nid_no_patch(const LibreSslBio* bio);

}

#endif
