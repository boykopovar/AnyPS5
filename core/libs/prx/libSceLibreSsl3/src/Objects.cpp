#include "prx/libSceLibreSsl3/include/Objects.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libSceLibreSsl/include/ObjectTable.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

constexpr int ObjectLibrary = 8;
constexpr int AnyFunction = 0xfff;
constexpr int UnknownNid = 101;

constexpr unsigned char LaterData[751] = {
    0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x3c, 0x02, 0x01, 0x01, 0x2b, 0x06, 0x01, 0x04, 0x01,
    0x82, 0x37, 0x3c, 0x02, 0x01, 0x02, 0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x3c, 0x02, 0x01,
    0x03, 0x2a, 0x81, 0x1c, 0xcf, 0x55, 0x01, 0x83, 0x11, 0x2a, 0x81, 0x1c, 0xcf, 0x55, 0x01, 0x83,
    0x78, 0x2a, 0x81, 0x1c, 0x2a, 0x81, 0x1c, 0xcf, 0x55, 0x2a, 0x81, 0x1c, 0xcf, 0x55, 0x01, 0x2a,
    0x81, 0x1c, 0xcf, 0x55, 0x01, 0x68, 0x01, 0x2a, 0x81, 0x1c, 0xcf, 0x55, 0x01, 0x68, 0x02, 0x2a,
    0x81, 0x1c, 0xcf, 0x55, 0x01, 0x68, 0x03, 0x2a, 0x81, 0x1c, 0xcf, 0x55, 0x01, 0x68, 0x04, 0x2a,
    0x81, 0x1c, 0xcf, 0x55, 0x01, 0x68, 0x05, 0x2a, 0x81, 0x1c, 0xcf, 0x55, 0x01, 0x68, 0x06, 0x2a,
    0x81, 0x1c, 0xcf, 0x55, 0x01, 0x68, 0x07, 0x2b, 0x81, 0x05, 0x10, 0x86, 0x48, 0x3f, 0x00, 0x02,
    0x2b, 0x81, 0x04, 0x01, 0x0b, 0x00, 0x2b, 0x81, 0x04, 0x01, 0x0b, 0x01, 0x2b, 0x81, 0x04, 0x01,
    0x0b, 0x02, 0x2b, 0x81, 0x04, 0x01, 0x0b, 0x03, 0x2b, 0x81, 0x05, 0x10, 0x86, 0x48, 0x3f, 0x00,
    0x03, 0x2b, 0x81, 0x04, 0x01, 0x0e, 0x00, 0x2b, 0x81, 0x04, 0x01, 0x0e, 0x01, 0x2b, 0x81, 0x04,
    0x01, 0x0e, 0x02, 0x2b, 0x81, 0x04, 0x01, 0x0e, 0x03, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01,
    0x01, 0x09, 0x2a, 0x85, 0x03, 0x07, 0x01, 0x02, 0x01, 0x01, 0x01, 0x2a, 0x85, 0x03, 0x07, 0x01,
    0x02, 0x01, 0x01, 0x02, 0x2a, 0x85, 0x03, 0x07, 0x01, 0x02, 0x01, 0x01, 0x03, 0x2a, 0x85, 0x03,
    0x07, 0x01, 0x02, 0x01, 0x01, 0x04, 0x2a, 0x85, 0x03, 0x07, 0x01, 0x02, 0x01, 0x02, 0x00, 0x2a,
    0x85, 0x03, 0x07, 0x01, 0x02, 0x01, 0x02, 0x03, 0x2a, 0x85, 0x03, 0x07, 0x01, 0x01, 0x04, 0x01,
    0x2a, 0x85, 0x03, 0x07, 0x01, 0x01, 0x04, 0x02, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09,
    0x10, 0x01, 0x18, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x10, 0x01, 0x1a, 0x2a, 0x86,
    0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x10, 0x01, 0x23, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01,
    0x09, 0x10, 0x01, 0x24, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x0e, 0x2b, 0x06, 0x01, 0x05, 0x05,
    0x07, 0x01, 0x1c, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x01, 0x1d, 0x2b, 0x06, 0x01, 0x05, 0x05,
    0x07, 0x0e, 0x02, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x0e, 0x03, 0x2b, 0x06, 0x01, 0x05, 0x05,
    0x07, 0x30, 0x0a, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x0b, 0x2b, 0x06, 0x01, 0x05, 0x05,
    0x07, 0x30, 0x0d, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x10, 0x01, 0x2f, 0x2a, 0x86,
    0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x10, 0x01, 0x30, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03,
    0x1e, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x01, 0x18, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01,
    0x09, 0x10, 0x01, 0x31, 0x2b, 0x06, 0x01, 0x04, 0x01, 0xd6, 0x79, 0x02, 0x04, 0x02, 0x2b, 0x06,
    0x01, 0x04, 0x01, 0xd6, 0x79, 0x02, 0x04, 0x03, 0x2b, 0x06, 0x01, 0x04, 0x01, 0xd6, 0x79, 0x02,
    0x04, 0x04, 0x2b, 0x06, 0x01, 0x04, 0x01, 0xd6, 0x79, 0x02, 0x04, 0x05, 0x2a, 0x86, 0x48, 0x86,
    0xf7, 0x0d, 0x01, 0x09, 0x10, 0x02, 0x2f, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x10,
    0x01, 0x32, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0f, 0x2a, 0x86, 0x48, 0x86, 0xf7,
    0x0d, 0x01, 0x01, 0x10, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x02, 0x0c, 0x2a, 0x86, 0x48, 0x86,
    0xf7, 0x0d, 0x02, 0x0d, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x05, 0x60, 0x86, 0x48,
    0x01, 0x65, 0x03, 0x04, 0x02, 0x06, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x07, 0x60,
    0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x08, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02,
    0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x0a, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03,
    0x04, 0x02, 0x0d, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x0e, 0x60, 0x86, 0x48, 0x01,
    0x65, 0x03, 0x04, 0x02, 0x0f, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x10, 0x60, 0x86,
    0x48, 0x01, 0x65, 0x03, 0x04, 0x03, 0x03, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x03, 0x04,
    0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x03, 0x05, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04,
    0x03, 0x06, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x03, 0x07, 0x60, 0x86, 0x48, 0x01, 0x65,
    0x03, 0x04, 0x03, 0x08, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x03, 0x09, 0x60, 0x86, 0x48,
    0x01, 0x65, 0x03, 0x04, 0x03, 0x0a, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x03, 0x0b, 0x60,
    0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x03, 0x0c, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x03,
    0x0d, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x03, 0x0e, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03,
    0x04, 0x03, 0x0f, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x03, 0x10, 0x2b, 0x06, 0x01, 0x05,
    0x05, 0x07, 0x01, 0x1f, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x10, 0x01, 0x33,
};

constexpr LibreSslAsn1Object LaterObjects[100] = {
    {"jurisdictionLocalityName", "jurisdictionLocalityName", 956, 11, LaterData + 0, 0},
    {"jurisdictionStateOrProvinceName", "jurisdictionStateOrProvinceName", 957, 11, LaterData + 11, 0},
    {"jurisdictionCountryName", "jurisdictionCountryName", 958, 11, LaterData + 22, 0},
    {"KxRSA", "kx-rsa", 959, 0, nullptr, 0},
    {"KxECDHE", "kx-ecdhe", 960, 0, nullptr, 0},
    {"KxDHE", "kx-dhe", 961, 0, nullptr, 0},
    {"KxGOST", "kx-gost", 962, 0, nullptr, 0},
    {"AuthRSA", "auth-rsa", 963, 0, nullptr, 0},
    {"AuthECDSA", "auth-ecdsa", 964, 0, nullptr, 0},
    {"AuthGOST01", "auth-gost01", 965, 0, nullptr, 0},
    {"AuthNULL", "auth-null", 966, 0, nullptr, 0},
    {"ChaCha20-Poly1305", "chacha20-poly1305", 967, 0, nullptr, 0},
    {"SM3", "sm3", 968, 8, LaterData + 33, 0},
    {"RSA-SM3", "sm3WithRSAEncryption", 969, 8, LaterData + 41, 0},
    {"ISO-CN", "ISO CN Member Body", 970, 3, LaterData + 49, 0},
    {"oscca", "oscca", 971, 5, LaterData + 52, 0},
    {"sm-scheme", "sm-scheme", 972, 6, LaterData + 57, 0},
    {"SM4-ECB", "sm4-ecb", 973, 8, LaterData + 63, 0},
    {"SM4-CBC", "sm4-cbc", 974, 8, LaterData + 71, 0},
    {"SM4-OFB", "sm4-ofb", 975, 8, LaterData + 79, 0},
    {"SM4-CFB", "sm4-cfb", 976, 8, LaterData + 87, 0},
    {"SM4-CFB1", "sm4-cfb1", 977, 8, LaterData + 95, 0},
    {"SM4-CFB8", "sm4-cfb8", 978, 8, LaterData + 103, 0},
    {"SM4-CTR", "sm4-ctr", 979, 8, LaterData + 111, 0},
    {"dhSinglePass-stdDH-sha1kdf-scheme", "dhSinglePass-stdDH-sha1kdf-scheme", 980, 9, LaterData + 119, 0},
    {"dhSinglePass-stdDH-sha224kdf-scheme", "dhSinglePass-stdDH-sha224kdf-scheme", 981, 6, LaterData + 128, 0},
    {"dhSinglePass-stdDH-sha256kdf-scheme", "dhSinglePass-stdDH-sha256kdf-scheme", 982, 6, LaterData + 134, 0},
    {"dhSinglePass-stdDH-sha384kdf-scheme", "dhSinglePass-stdDH-sha384kdf-scheme", 983, 6, LaterData + 140, 0},
    {"dhSinglePass-stdDH-sha512kdf-scheme", "dhSinglePass-stdDH-sha512kdf-scheme", 984, 6, LaterData + 146, 0},
    {"dhSinglePass-cofactorDH-sha1kdf-scheme", "dhSinglePass-cofactorDH-sha1kdf-scheme", 985, 9, LaterData + 152, 0},
    {"dhSinglePass-cofactorDH-sha224kdf-scheme", "dhSinglePass-cofactorDH-sha224kdf-scheme", 986, 6, LaterData + 161, 0},
    {"dhSinglePass-cofactorDH-sha256kdf-scheme", "dhSinglePass-cofactorDH-sha256kdf-scheme", 987, 6, LaterData + 167, 0},
    {"dhSinglePass-cofactorDH-sha384kdf-scheme", "dhSinglePass-cofactorDH-sha384kdf-scheme", 988, 6, LaterData + 173, 0},
    {"dhSinglePass-cofactorDH-sha512kdf-scheme", "dhSinglePass-cofactorDH-sha512kdf-scheme", 989, 6, LaterData + 179, 0},
    {"dh-std-kdf", "dh-std-kdf", 990, 0, nullptr, 0},
    {"dh-cofactor-kdf", "dh-cofactor-kdf", 991, 0, nullptr, 0},
    {"PSPECIFIED", "pSpecified", 992, 9, LaterData + 185, 0},
    {"id-tc26-gost-3410-12-256-paramSetA", "GOST R 34.10-2012 (256 bit) ParamSet A", 993, 9, LaterData + 194, 0},
    {"id-tc26-gost-3410-12-256-paramSetB", "GOST R 34.10-2012 (256 bit) ParamSet B", 994, 9, LaterData + 203, 0},
    {"id-tc26-gost-3410-12-256-paramSetC", "GOST R 34.10-2012 (256 bit) ParamSet C", 995, 9, LaterData + 212, 0},
    {"id-tc26-gost-3410-12-256-paramSetD", "GOST R 34.10-2012 (256 bit) ParamSet D", 996, 9, LaterData + 221, 0},
    {"id-tc26-gost-3410-12-512-paramSetTest", "GOST R 34.10-2012 (512 bit) testing parameter set", 997, 9, LaterData + 230, 0},
    {"id-tc26-gost-3410-12-512-paramSetC", "GOST R 34.10-2012 (512 bit) ParamSet C", 998, 9, LaterData + 239, 0},
    {"id-tc26-hmac-gost-3411-12-256", "HMAC STREEBOG 256", 999, 8, LaterData + 248, 0},
    {"id-tc26-hmac-gost-3411-12-512", "HMAC STREEBOG 512", 1000, 8, LaterData + 256, 0},
    {"id-ct-routeOriginAuthz", "id-ct-routeOriginAuthz", 1001, 11, LaterData + 264, 0},
    {"id-ct-rpkiManifest", "id-ct-rpkiManifest", 1002, 11, LaterData + 275, 0},
    {"id-ct-rpkiGhostbusters", "id-ct-rpkiGhostbusters", 1003, 11, LaterData + 286, 0},
    {"id-ct-resourceTaggedAttest", "id-ct-resourceTaggedAttest", 1004, 11, LaterData + 297, 0},
    {"id-cp", "id-cp", 1005, 7, LaterData + 308, 0},
    {"sbgp-ipAddrBlockv2", "sbgp-ipAddrBlockv2", 1006, 8, LaterData + 315, 0},
    {"sbgp-autonomousSysNumv2", "sbgp-autonomousSysNumv2", 1007, 8, LaterData + 323, 0},
    {"ipAddr-asNumber", "ipAddr-asNumber", 1008, 8, LaterData + 331, 0},
    {"ipAddr-asNumberv2", "ipAddr-asNumberv2", 1009, 8, LaterData + 339, 0},
    {"rpkiManifest", "RPKI Manifest", 1010, 8, LaterData + 347, 0},
    {"signedObject", "Signed Object", 1011, 8, LaterData + 355, 0},
    {"rpkiNotify", "RPKI Notify", 1012, 8, LaterData + 363, 0},
    {"id-ct-geofeedCSVwithCRLF", "id-ct-geofeedCSVwithCRLF", 1013, 11, LaterData + 371, 0},
    {"id-ct-signedChecklist", "id-ct-signedChecklist", 1014, 11, LaterData + 382, 0},
    {"id-kp-bgpsec-router", "BGPsec Router", 1015, 8, LaterData + 393, 0},
    {"tlsfeature", "TLS Feature", 1016, 8, LaterData + 401, 0},
    {"id-ct-ASPA", "id-ct-ASPA", 1017, 11, LaterData + 409, 0},
    {"ct_precert_scts", "CT Precertificate SCTs", 1018, 10, LaterData + 420, 0},
    {"ct_precert_poison", "CT Precertificate Poison", 1019, 10, LaterData + 430, 0},
    {"ct_precert_signer", "CT Precertificate Signer", 1020, 10, LaterData + 440, 0},
    {"ct_cert_scts", "CT Certificate SCTs", 1021, 10, LaterData + 450, 0},
    {"HKDF", "hkdf", 1022, 0, nullptr, 0},
    {"id-smime-aa-signingCertificateV2", "id-smime-aa-signingCertificateV2", 1023, 11, LaterData + 460, 0},
    {"id-ct-signedTAL", "id-ct-signedTAL", 1024, 11, LaterData + 471, 0},
    {"RSA-SHA512/224", "sha512-224WithRSAEncryption", 1025, 9, LaterData + 482, 0},
    {"RSA-SHA512/256", "sha512-256WithRSAEncryption", 1026, 9, LaterData + 491, 0},
    {"hmacWithSHA512-224", "hmacWithSHA512-224", 1027, 8, LaterData + 500, 0},
    {"hmacWithSHA512-256", "hmacWithSHA512-256", 1028, 8, LaterData + 508, 0},
    {"SHA512-224", "sha512-224", 1029, 9, LaterData + 516, 0},
    {"SHA512-256", "sha512-256", 1030, 9, LaterData + 525, 0},
    {"SHA3-224", "sha3-224", 1031, 9, LaterData + 534, 0},
    {"SHA3-256", "sha3-256", 1032, 9, LaterData + 543, 0},
    {"SHA3-384", "sha3-384", 1033, 9, LaterData + 552, 0},
    {"SHA3-512", "sha3-512", 1034, 9, LaterData + 561, 0},
    {"id-hmacWithSHA3-224", "hmac-sha3-224", 1035, 9, LaterData + 570, 0},
    {"id-hmacWithSHA3-256", "hmac-sha3-256", 1036, 9, LaterData + 579, 0},
    {"id-hmacWithSHA3-384", "hmac-sha3-384", 1037, 9, LaterData + 588, 0},
    {"id-hmacWithSHA3-512", "hmac-sha3-512", 1038, 9, LaterData + 597, 0},
    {"id-dsa-with-sha384", "dsa_with_SHA384", 1039, 9, LaterData + 606, 0},
    {"id-dsa-with-sha512", "dsa_with_SHA512", 1040, 9, LaterData + 615, 0},
    {"id-dsa-with-sha3-224", "dsa_with_SHA3-224", 1041, 9, LaterData + 624, 0},
    {"id-dsa-with-sha3-256", "dsa_with_SHA3-256", 1042, 9, LaterData + 633, 0},
    {"id-dsa-with-sha3-384", "dsa_with_SHA3-384", 1043, 9, LaterData + 642, 0},
    {"id-dsa-with-sha3-512", "dsa_with_SHA3-512", 1044, 9, LaterData + 651, 0},
    {"id-ecdsa-with-sha3-224", "ecdsa_with_SHA3-224", 1045, 9, LaterData + 660, 0},
    {"id-ecdsa-with-sha3-256", "ecdsa_with_SHA3-256", 1046, 9, LaterData + 669, 0},
    {"id-ecdsa-with-sha3-384", "ecdsa_with_SHA3-384", 1047, 9, LaterData + 678, 0},
    {"id-ecdsa-with-sha3-512", "ecdsa_with_SHA3-512", 1048, 9, LaterData + 687, 0},
    {"id-rsassa-pkcs1-v1_5-with-sha3-224", "RSA-SHA3-224", 1049, 9, LaterData + 696, 0},
    {"id-rsassa-pkcs1-v1_5-with-sha3-256", "RSA-SHA3-256", 1050, 9, LaterData + 705, 0},
    {"id-rsassa-pkcs1-v1_5-with-sha3-384", "RSA-SHA3-384", 1051, 9, LaterData + 714, 0},
    {"id-rsassa-pkcs1-v1_5-with-sha3-512", "RSA-SHA3-512", 1052, 9, LaterData + 723, 0},
    {"acmeIdentifier", "ACME Identifier", 1053, 8, LaterData + 732, 0},
    {"id-ct-rpkiSignedPrefixList", "id-ct-rpkiSignedPrefixList", 1054, 11, LaterData + 740, 0},
    {"TLS1-PRF", "tls1-prf", 1055, 0, nullptr, 0},
};

static_assert(LibreSslObjectCount + static_cast<int>(std::size(LaterObjects)) == LibreSsl3::ObjectCount);

const LibreSslAsn1Object& Entry(int nid) {
    return nid < LibreSslObjectCount ? LibreSslObjects_nid_no_patch[nid] : LaterObjects[nid - LibreSslObjectCount];
}

const std::vector<std::uint16_t>& Order() {
    static const std::vector<std::uint16_t> order = [] {
        std::vector<std::uint16_t> nids(LibreSslObjectOrder_nid_no_patch, LibreSslObjectOrder_nid_no_patch + LibreSslObjectOrderCount);
        for (const auto& object : LaterObjects) {
            if (object.length > 0) nids.push_back(static_cast<std::uint16_t>(object.nid));
        }
        std::stable_sort(nids.begin(), nids.end(), [](std::uint16_t first, std::uint16_t second) {
            return LibreSsl3::CompareObjects(&Entry(first), &Entry(second)) < 0;
        });
        return nids;
    }();
    return order;
}

} // namespace

namespace LibreSsl3 {

int CompareObjects(const LibreSslAsn1Object* first, const LibreSslAsn1Object* second) {
    if (!first || !second) throw std::invalid_argument("OBJ_cmp: null object");
    const std::int64_t difference = static_cast<std::int64_t>(first->length) - second->length;
    if (difference < std::numeric_limits<int>::min() || difference > std::numeric_limits<int>::max())
        throw std::overflow_error("OBJ_cmp: length difference overflows int");
    if (difference != 0) return static_cast<int>(difference);
    if (first->length == 0) return 0;
    if (first->length < 0) throw std::invalid_argument("OBJ_cmp: negative encoded length");
    if (!first->data || !second->data) throw std::invalid_argument("OBJ_cmp: null encoded data");
    return std::memcmp(first->data, second->data, static_cast<std::size_t>(first->length));
}

const LibreSslAsn1Object* ObjectByNid(int nid) {
    if (nid >= 0 && nid < ObjectCount) {
        const LibreSslAsn1Object& object = Entry(nid);
        if (nid == 0 || object.nid != 0) return &object;
    } else if (const LibreSslAsn1Object* added = LibreSslAddedObjectByNid_nid_no_patch(nid)) {
        return added;
    }
    LibreSslPutError_nid_no_patch(ObjectLibrary, AnyFunction, UnknownNid);
    return nullptr;
}

int ObjectNid(const LibreSslAsn1Object* object) {
    if (!object || object->length == 0) return 0;
    if (object->nid != 0) return object->nid;
    if (LibreSslAddedObjectsExist_nid_no_patch()) {
        if (object->length < 0 || object->length > std::numeric_limits<int>::max() >> 20)
            throw std::overflow_error("OBJ_obj2nid: encoded length overflows the added-object hash");
        if (!object->data) throw std::invalid_argument("OBJ_obj2nid: null encoded data");
        if (const int nid = LibreSslAddedObjectNid_nid_no_patch(object)) return nid;
    }
    const std::vector<std::uint16_t>& order = Order();
    const std::uint16_t* base = order.data();
    for (std::size_t limit = order.size(); limit != 0; limit >>= 1) {
        const std::uint16_t* probe = base + (limit >> 1);
        const int comparison = CompareObjects(object, &Entry(*probe));
        if (comparison == 0) return *probe;
        if (comparison > 0) {
            base = probe + 1;
            --limit;
        }
    }
    return 0;
}

} // namespace LibreSsl3
