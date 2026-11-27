#pragma once

#include <cstdint>
#include <vector>
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libSceLibreSsl3/include/PkeyState.hpp"

inline constexpr int LibreSsl3PemBufferSize = 1024;

enum class LibreSsl3PemDes { None, Single, TwoKey, ThreeKey };

struct LibreSsl3PemCipherInfo {
    const LibreSslEvpCipher* cipher = nullptr;
    LibreSsl3PemDes des = LibreSsl3PemDes::None;
    unsigned char iv[16] = {};
};

bool PemCipherInfoFromDekInfo_nid_no_patch(const char* dekInfo, LibreSsl3PemCipherInfo& info);
bool PemDoHeader_nid_no_patch(const LibreSsl3PemCipherInfo& info, std::vector<unsigned char>& data, int& length,
    LibreSslPemPasswordCallback passwordCallback, void* userData);
int PemReadPassword_nid_no_patch(char* buffer, int writeFlag, LibreSslPemPasswordCallback passwordCallback,
    void* userData);
LibreSsl3Pkcs8PrivKeyInfo* PemDecryptPkcs8_nid_no_patch(const unsigned char* der, int length,
    LibreSslPemPasswordCallback passwordCallback, void* userData, bool& asn1Failure);
bool PemEncryptPkcs8_nid_no_patch(const LibreSsl3EvpCipher* cipher, const unsigned char* plain, int plainLength,
    const char* password, int passwordLength, std::vector<unsigned char>& der);
