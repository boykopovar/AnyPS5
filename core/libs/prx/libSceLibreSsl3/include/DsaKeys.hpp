#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libSceLibreSsl3/include/Key.hpp"

inline constexpr std::int32_t LibreSsl3PkeyDsa = 116;

LibreSsl3EvpPkey* DsaDecodeTraditionalKey_nid_no_patch(const unsigned char* der, std::size_t length);
LibreSsl3EvpPkey* DsaDecodePkcs8Key_nid_no_patch(const LibreSsl3X509Algor* algorithm, const unsigned char* key,
    std::size_t keyLength);
void DsaEncodePkcs8Parts_nid_no_patch(const LibreSsl3Dsa* dsa, std::vector<unsigned char>& parameters,
    std::vector<unsigned char>& privateKey);
void DsaFree_nid_no_patch(LibreSsl3Dsa* dsa);
