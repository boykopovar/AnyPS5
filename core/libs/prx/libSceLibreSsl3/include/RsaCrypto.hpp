#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include "prx/libSceLibreSsl3/include/EvpDigest.hpp"
#include "prx/libSceLibreSsl3/include/Key.hpp"

namespace LibreSsl3 {

int RsaByteCount_nid_no_patch(const LibreSsl3Rsa* rsa);
int RsaBitCount_nid_no_patch(const LibreSsl3Rsa* rsa);
int RsaComponentBits_nid_no_patch(const void* component);
LibreSsl3Bignum* NewRsaBignum_nid_no_patch(const unsigned char* bytes, std::size_t length);
LibreSsl3Bignum* DuplicateRsaBignum_nid_no_patch(const LibreSsl3Bignum* number);
void FreeRsa_nid_no_patch(LibreSsl3Rsa* rsa);

int RsaPublicEncrypt_nid_no_patch(int length, const unsigned char* from, unsigned char* to, const LibreSsl3Rsa* rsa,
    int padding);
int RsaPrivateDecrypt_nid_no_patch(int length, const unsigned char* from, unsigned char* to, const LibreSsl3Rsa* rsa,
    int padding);
int RsaPrivateEncrypt_nid_no_patch(int length, const unsigned char* from, unsigned char* to, const LibreSsl3Rsa* rsa,
    int padding);
int RsaPublicDecrypt_nid_no_patch(int length, const unsigned char* from, unsigned char* to, const LibreSsl3Rsa* rsa,
    int padding);

int RsaPaddingAddNone_nid_no_patch(unsigned char* to, int size, const unsigned char* from, int length);
int RsaPaddingCheckNone_nid_no_patch(unsigned char* to, int size, const unsigned char* from, int length, int modulusLength);
int RsaPaddingAddPkcs1Type1_nid_no_patch(unsigned char* to, int size, const unsigned char* from, int length);
int RsaPaddingCheckPkcs1Type1_nid_no_patch(unsigned char* to, int size, const unsigned char* from, int length,
    int modulusLength);
int RsaPaddingAddPkcs1Type2_nid_no_patch(unsigned char* to, int size, const unsigned char* from, int length);
int RsaPaddingCheckPkcs1Type2_nid_no_patch(unsigned char* to, int size, const unsigned char* from, int length,
    int modulusLength);
int RsaPaddingAddPkcs1OaepMgf1_nid_no_patch(unsigned char* to, int size, const unsigned char* from, int length,
    const unsigned char* label, int labelLength, const DigestMethod* digest, const DigestMethod* maskDigest);
int RsaPaddingCheckPkcs1OaepMgf1_nid_no_patch(unsigned char* to, int size, const unsigned char* from, int length,
    int modulusLength, const unsigned char* label, int labelLength, const DigestMethod* digest,
    const DigestMethod* maskDigest);

int RsaSign_nid_no_patch(int type, const unsigned char* message, unsigned int messageLength, unsigned char* signature,
    unsigned int* signatureLength, const LibreSsl3Rsa* rsa);
int RsaVerify_nid_no_patch(int type, const unsigned char* message, unsigned int messageLength,
    const unsigned char* signature, unsigned int signatureLength, const LibreSsl3Rsa* rsa);
int RsaPaddingAddPkcs1PssMgf1_nid_no_patch(const LibreSsl3Rsa* rsa, unsigned char* encoded, const unsigned char* messageHash,
    const DigestMethod* digest, const DigestMethod* maskDigest, int saltLength);
int RsaVerifyPkcs1PssMgf1_nid_no_patch(const LibreSsl3Rsa* rsa, const unsigned char* messageHash, const DigestMethod* digest,
    const DigestMethod* maskDigest, const unsigned char* encoded, int saltLength);

bool RsaMaskGenerationFunction_nid_no_patch(unsigned char* mask, int length, const unsigned char* seed, int seedLength,
    const DigestMethod* digest);
bool RsaDigestSpans_nid_no_patch(const DigestMethod* digest, std::span<const unsigned char> first,
    std::span<const unsigned char> second, unsigned char* output);

} // namespace LibreSsl3
