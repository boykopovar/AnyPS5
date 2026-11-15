#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <set>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace {

using Bytes = std::vector<unsigned char>;

constexpr std::uint64_t Code(int library, int reason) {
    return (static_cast<std::uint64_t>(library) << 24) | (0xfffull << 12) | static_cast<std::uint64_t>(reason);
}

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
}

template<typename TAction>
void RequireThrows(TAction action, const char* message) {
    try {
        action();
    } catch (const std::exception&) {
        return;
    }
    Require(false, message);
}

void RequireCodes(std::initializer_list<std::uint64_t> codes, const char* message) {
    for (const std::uint64_t code : codes) Require(ERR_get_error_nid_postfix() == code, message);
    Require(ERR_get_error_nid_postfix() == 0, message);
}

void RequireNoCodes(const char* message) {
    Require(ERR_get_error_nid_postfix() == 0, message);
}

Bytes Cat(std::initializer_list<Bytes> parts) {
    Bytes joined;
    for (const auto& part : parts)
        joined.insert(joined.end(), part.begin(), part.end());
    return joined;
}

Bytes Tlv(unsigned char tag, const Bytes& content) {
    Bytes encoded{tag};
    if (content.size() < 128) {
        encoded.push_back(static_cast<unsigned char>(content.size()));
    } else if (content.size() < 256) {
        encoded.push_back(0x81);
        encoded.push_back(static_cast<unsigned char>(content.size()));
    } else {
        encoded.push_back(0x82);
        encoded.push_back(static_cast<unsigned char>(content.size() >> 8));
        encoded.push_back(static_cast<unsigned char>(content.size()));
    }
    encoded.insert(encoded.end(), content.begin(), content.end());
    return encoded;
}

Bytes Lengthed(unsigned char tag, const Bytes& content, std::size_t declared) {
    Bytes encoded{tag};
    if (declared < 128) {
        encoded.push_back(static_cast<unsigned char>(declared));
    } else if (declared < 256) {
        encoded.push_back(0x81);
        encoded.push_back(static_cast<unsigned char>(declared));
    } else {
        encoded.push_back(0x82);
        encoded.push_back(static_cast<unsigned char>(declared >> 8));
        encoded.push_back(static_cast<unsigned char>(declared));
    }
    encoded.insert(encoded.end(), content.begin(), content.end());
    return encoded;
}

Bytes Text(const std::string& text) {
    return Bytes(text.begin(), text.end());
}

Bytes FromHex(const char* hex) {
    Bytes bytes;
    while (hex[0] && hex[1]) {
        bytes.push_back(static_cast<unsigned char>(std::strtoul(std::string(hex, 2).c_str(), nullptr, 16)));
        hex += 2;
    }
    return bytes;
}

const Bytes CommonName{0x55, 0x04, 0x03};
const Bytes Organization{0x55, 0x04, 0x0a};
const Bytes Sha256WithRsa{0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b};
const Bytes RsaEncryption{0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01};
const Bytes DsaKey{0x2a, 0x86, 0x48, 0xce, 0x38, 0x04, 0x01};
const Bytes EcPublicKey{0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01};
const Bytes DhKeyAgreement{0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x03, 0x01};
const Bytes UnknownAlgorithm{0x2b, 0x81, 0xd9, 0x01};
const Bytes ServerAuthTrust{0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01};
const Bytes SmallTrust{0x2a};
const Bytes OtherTrust{0x2a, 0x03};
const Bytes InvalidObject{0x2b, 0x06, 0x01};
const Bytes CurvePrime256{0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07};
const Bytes EcPoint = FromHex("04aaef40aa75bab39abc01b30f7205fbd592fafc7fd7bcada439712dd1327f90bb8b626d8f043ceaa61b4d2c5b9704386f9"
    "f029db0a0d8c64dd00343a261fe2bb2");

Bytes Entry(const Bytes& object, unsigned char type, const Bytes& value) {
    return Tlv(0x30, Cat({Tlv(0x06, object), Tlv(type, value)}));
}

Bytes Algorithm() {
    return Tlv(0x30, Cat({Tlv(0x06, Sha256WithRsa), Tlv(0x05, {})}));
}

Bytes Name(const std::string& text) {
    return Tlv(0x30, Tlv(0x31, Entry(CommonName, 0x0c, Text(text))));
}

Bytes Validity() {
    return Tlv(0x30, Cat({Tlv(0x17, Text("200101000000Z")), Tlv(0x18, Text("20500101000000Z"))}));
}

Bytes KeyInformation(const Bytes& algorithmObject, const Bytes& parameter, const Bytes& bits) {
    return Tlv(0x30, Cat({Tlv(0x30, Cat({Tlv(0x06, algorithmObject), parameter})), Tlv(0x03, Cat({{0x00}, bits}))}));
}

Bytes RsaKey(std::size_t modulusBytes, unsigned char modulusFill) {
    const Bytes modulus = Cat({{0x00}, Bytes(modulusBytes, modulusFill)});
    const Bytes exponent{0x01, 0x00, 0x01};
    return KeyInformation(RsaEncryption, Tlv(0x05, {}), Tlv(0x30, Cat({Tlv(0x02, modulus), Tlv(0x02, exponent)})));
}

Bytes RsaKeyWithBrokenBits(const Bytes& bits) {
    return KeyInformation(RsaEncryption, Tlv(0x05, {}), bits);
}

Bytes EcKey() {
    return KeyInformation(EcPublicKey, Tlv(0x06, CurvePrime256), EcPoint);
}

Bytes SignedCertificate(const Bytes& key, const std::string& subject) {
    const Bytes information = Tlv(0x30, Cat({Tlv(0x02, {0x01}), Algorithm(), Name("Test CA"), Validity(), Name(subject), key}));
    return Tlv(0x30, Cat({information, Algorithm(), Tlv(0x03, {0x03, 0xde, 0xad, 0xbf})}));
}

std::string Base64(const Bytes& data) {
    static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string encoded;
    for (std::size_t index = 0; index < data.size(); index += 3) {
        const bool second = index + 1 < data.size();
        const bool third = index + 2 < data.size();
        const unsigned long group = (static_cast<unsigned long>(data[index]) << 16) |
            (second ? static_cast<unsigned long>(data[index + 1]) << 8UL : 0UL) | (third ? static_cast<unsigned long>(data[index + 2]) : 0UL);
        encoded += alphabet[(group >> 18) & 0x3f];
        encoded += alphabet[(group >> 12) & 0x3f];
        encoded += second ? alphabet[(group >> 6) & 0x3f] : '=';
        encoded += third ? alphabet[group & 0x3f] : '=';
    }
    return encoded;
}

std::string Record(const char* label, const Bytes& body) {
    const std::string encoded = Base64(body);
    std::string text = std::string("-----BEGIN ") + label + "-----\n";
    for (std::size_t index = 0; index < encoded.size(); index += 64) {
        text += encoded.substr(index, 64);
        text += '\n';
    }
    text += std::string("-----END ") + label + "-----\n";
    return text;
}

std::string Trusted(const Bytes& certificate, const Bytes& auxiliary) {
    return Record("TRUSTED CERTIFICATE", Cat({certificate, auxiliary}));
}

struct CertAux {
    LibreSslStack* trust;
    LibreSslStack* reject;
    Asn1String* alias;
    Asn1String* keyid;
    LibreSslStack* other;
};

static_assert(sizeof(CertAux) == 40 && offsetof(CertAux, reject) == 8 && offsetof(CertAux, alias) == 16 && offsetof(CertAux, keyid) == 24 &&
    offsetof(CertAux, other) == 32);

struct Pkey {
    int type;
    int saveType;
    int references;
    const void* algorithmMethod;
    void* engine;
    void* key;
    int saveParameters;
    void* attributes;
};

static_assert(sizeof(Pkey) == 56 && offsetof(Pkey, references) == 8 && offsetof(Pkey, key) == 32);

bool Holds(const LibreSslAsn1Object* object, const Bytes& data) {
    return object && object->flags == 9 && object->nid == 0 && !object->shortName && !object->longName &&
        object->length == static_cast<int>(data.size()) && (data.empty() || std::memcmp(object->data, data.data(), data.size()) == 0);
}

bool Holds(const Asn1String* string, int type, const Bytes& data) {
    return string && string->type == type && string->length == static_cast<int>(data.size()) &&
        (data.empty() || std::memcmp(string->data, data.data(), data.size()) == 0);
}

int StackCount(const void* pointer) {
    const auto* stack = static_cast<const LibreSslStack*>(pointer);
    return stack ? sk_num_nid_postfix(stack) : -1;
}

const LibreSslAsn1Object* StackObject(const void* pointer, int index) {
    const auto* stack = static_cast<const LibreSslStack*>(pointer);
    if (!stack) return nullptr;
    return static_cast<const LibreSslAsn1Object*>(sk_value_nid_postfix(stack, index));
}

const CertAux* Auxiliary(const LibreSslX509* certificate) {
    return static_cast<const CertAux*>(certificate->auxiliary);
}

void RequireAux(const LibreSslX509* certificate, const char* message) {
    Require(certificate && certificate->auxiliary, message);
}

LibreSslX509* ReadCertificate(const std::string& pem, LibreSslX509** stored = nullptr) {
    auto* bio = BIO_new_mem_buf_nid_postfix(pem.data(), static_cast<int>(pem.size()));
    Require(bio, "a memory BIO holds the record");
    LibreSslX509* certificate = PEM_read_bio_X509_nid_postfix(bio, stored, nullptr, nullptr);
    BIO_free_nid_postfix(bio);
    return certificate;
}

LibreSslX509* ReadTrusted(const std::string& pem, LibreSslX509** stored = nullptr) {
    auto* bio = BIO_new_mem_buf_nid_postfix(pem.data(), static_cast<int>(pem.size()));
    Require(bio, "a memory BIO holds the record");
    LibreSslX509* certificate = PEM_read_bio_X509_AUX_nid_postfix(bio, stored, nullptr, nullptr);
    BIO_free_nid_postfix(bio);
    return certificate;
}

const char* RsaCertificatePem = R"PEM(-----BEGIN CERTIFICATE-----
MIIDbTCCAlWgAwIBAgIUOZ194/1Ge4t3golB3c8jUKtya5AwDQYJKoZIhvcNAQEL
BQAwRjELMAkGA1UEBhMCSVQxHzAdBgNVBAoMFlVuaXZlcnNpdGEgZGkgQ2FnbGlh
cmkxFjAUBgNVBAMMDUFueVBTNSBSU0EgQ0EwHhcNMjYxMTE0MjE1NDA4WhcNMzYx
MTExMjE1NDA4WjBGMQswCQYDVQQGEwJJVDEfMB0GA1UECgwWVW5pdmVyc2l0YSBk
aSBDYWdsaWFyaTEWMBQGA1UEAwwNQW55UFM1IFJTQSBDQTCCASIwDQYJKoZIhvcN
AQEBBQADggEPADCCAQoCggEBAJtyrUw3MP/QAOLEHjlByisPNl1QARhuU7c+JtUY
xRYXJ/oxqTDd0JicWrYKA/4MP5BvhF2F3NoE6E0O07Q+NqUrwXJmst7JwcXZ6IUE
5zJ1I94H779CdBPGTdSe3rUdHZKoaKaWYV3jPoahjpYdEP7dxbFnTUrYhn689dfV
ZOX+BoTL3BNJFK7V3Yj2mV1KDoXYX3TX2I8mZ0gmwm1RDh0J2p1/B+rrYtNw+RCR
N4Q94eAz1e6giI5vODGc4vq8yGXweOvFZOkJxUCwlVZ93IHYgBoDObNlfIbHz5Mv
d7vTzgSJVG7C0hbuBhbc8dv77vJJxNqjZK/UW+mFFaFYokECAwEAAaNTMFEwHQYD
VR0OBBYEFENgDvxd9lg88+ln2b91ql0hWEudMB8GA1UdIwQYMBaAFENgDvxd9lg8
8+ln2b91ql0hWEudMA8GA1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQELBQADggEB
AAC1+zlwWY/Zrg8/tzdf1zSij+9WFJsL+qvsekPaaRHbXcHywxXvCP9rjYt+QZpc
WqEsP1ybXoCK9EHvhFA7NDfOOaXpZvmUSkoH3PfzYRfLfORtzIFru7VLAjnsLSN3
662o7aUdPL9RfYBki5sV1ZFIeqQjdo3HId9WtKyD3yp1LJeoI8woNq+XmR7CLSSJ
WjSZa2YwCVHYWL13xU8c6gqPvSdLWLOvxJyM+86GFDUDOTJMfyzHHqNPtcB/Ywl5
D5oOhVHkMEYnzWUNPZa+05Y2qkC4GXuramlvb21/F8lc+Ai57Ke0I9EgZ7q+TX/6
+zCK/Gxbn9AZGKzNGiqXKCI=
-----END CERTIFICATE-----
)PEM";
const char* EcCertificatePem = R"PEM(-----BEGIN CERTIFICATE-----
MIIB3zCCAYWgAwIBAgIUHSmFCSJdqtYgfmqNhxFF0v5/NBswCgYIKoZIzj0EAwIw
RTELMAkGA1UEBhMCSVQxHzAdBgNVBAoMFlVuaXZlcnNpdGEgZGkgQ2FnbGlhcmkx
FTATBgNVBAMMDEFueVBTNSBFQyBDQTAeFw0yNjExMTQyMTU0MDhaFw0zNjExMTEy
MTU0MDhaMEUxCzAJBgNVBAYTAklUMR8wHQYDVQQKDBZVbml2ZXJzaXRhIGRpIENh
Z2xpYXJpMRUwEwYDVQQDDAxBbnlQUzUgRUMgQ0EwWTATBgcqhkjOPQIBBggqhkjO
PQMBBwNCAASq70CqdbqzmrwBsw9yBfvVkvr8f9e8raQ5cS3RMn+Qu4tibY8EPOqm
G00sW5cEOG+fAp2woNjGTdADQ6Jh/iuyo1MwUTAdBgNVHQ4EFgQUv847MGNjv6eo
OCUVc0uoBYMKVQowHwYDVR0jBBgwFoAUv847MGNjv6eoOCUVc0uoBYMKVQowDwYD
VR0TAQH/BAUwAwEB/zAKBggqhkjOPQQDAgNIADBFAiBYkLT9QRlX9tGnG9oPWRCH
yjp0ODA7SWL/xlM2QeNRKQIhAPpYeVKBSOjOD3cJPjZjEeFgdI/2TT4FimcOz1u3
YD/Z
-----END CERTIFICATE-----
)PEM";
const char* TrustedCertificatePem = R"PEM(-----BEGIN TRUSTED CERTIFICATE-----
MIIDbTCCAlWgAwIBAgIUOZ194/1Ge4t3golB3c8jUKtya5AwDQYJKoZIhvcNAQEL
BQAwRjELMAkGA1UEBhMCSVQxHzAdBgNVBAoMFlVuaXZlcnNpdGEgZGkgQ2FnbGlh
cmkxFjAUBgNVBAMMDUFueVBTNSBSU0EgQ0EwHhcNMjYxMTE0MjE1NDA4WhcNMzYx
MTExMjE1NDA4WjBGMQswCQYDVQQGEwJJVDEfMB0GA1UECgwWVW5pdmVyc2l0YSBk
aSBDYWdsaWFyaTEWMBQGA1UEAwwNQW55UFM1IFJTQSBDQTCCASIwDQYJKoZIhvcN
AQEBBQADggEPADCCAQoCggEBAJtyrUw3MP/QAOLEHjlByisPNl1QARhuU7c+JtUY
xRYXJ/oxqTDd0JicWrYKA/4MP5BvhF2F3NoE6E0O07Q+NqUrwXJmst7JwcXZ6IUE
5zJ1I94H779CdBPGTdSe3rUdHZKoaKaWYV3jPoahjpYdEP7dxbFnTUrYhn689dfV
ZOX+BoTL3BNJFK7V3Yj2mV1KDoXYX3TX2I8mZ0gmwm1RDh0J2p1/B+rrYtNw+RCR
N4Q94eAz1e6giI5vODGc4vq8yGXweOvFZOkJxUCwlVZ93IHYgBoDObNlfIbHz5Mv
d7vTzgSJVG7C0hbuBhbc8dv77vJJxNqjZK/UW+mFFaFYokECAwEAAaNTMFEwHQYD
VR0OBBYEFENgDvxd9lg88+ln2b91ql0hWEudMB8GA1UdIwQYMBaAFENgDvxd9lg8
8+ln2b91ql0hWEudMA8GA1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQELBQADggEB
AAC1+zlwWY/Zrg8/tzdf1zSij+9WFJsL+qvsekPaaRHbXcHywxXvCP9rjYt+QZpc
WqEsP1ybXoCK9EHvhFA7NDfOOaXpZvmUSkoH3PfzYRfLfORtzIFru7VLAjnsLSN3
662o7aUdPL9RfYBki5sV1ZFIeqQjdo3HId9WtKyD3yp1LJeoI8woNq+XmR7CLSSJ
WjSZa2YwCVHYWL13xU8c6gqPvSdLWLOvxJyM+86GFDUDOTJMfyzHHqNPtcB/Ywl5
D5oOhVHkMEYnzWUNPZa+05Y2qkC4GXuramlvb21/F8lc+Ai57Ke0I9EgZ7q+TX/6
+zCK/Gxbn9AZGKzNGiqXKCIwDDAKBggrBgEFBQcDAQ==
-----END TRUSTED CERTIFICATE-----
)PEM";
const char* TrustedFullCertificatePem = R"PEM(-----BEGIN TRUSTED CERTIFICATE-----
MIIDbTCCAlWgAwIBAgIUOZ194/1Ge4t3golB3c8jUKtya5AwDQYJKoZIhvcNAQEL
BQAwRjELMAkGA1UEBhMCSVQxHzAdBgNVBAoMFlVuaXZlcnNpdGEgZGkgQ2FnbGlh
cmkxFjAUBgNVBAMMDUFueVBTNSBSU0EgQ0EwHhcNMjYxMTE0MjE1NDA4WhcNMzYx
MTExMjE1NDA4WjBGMQswCQYDVQQGEwJJVDEfMB0GA1UECgwWVW5pdmVyc2l0YSBk
aSBDYWdsaWFyaTEWMBQGA1UEAwwNQW55UFM1IFJTQSBDQTCCASIwDQYJKoZIhvcN
AQEBBQADggEPADCCAQoCggEBAJtyrUw3MP/QAOLEHjlByisPNl1QARhuU7c+JtUY
xRYXJ/oxqTDd0JicWrYKA/4MP5BvhF2F3NoE6E0O07Q+NqUrwXJmst7JwcXZ6IUE
5zJ1I94H779CdBPGTdSe3rUdHZKoaKaWYV3jPoahjpYdEP7dxbFnTUrYhn689dfV
ZOX+BoTL3BNJFK7V3Yj2mV1KDoXYX3TX2I8mZ0gmwm1RDh0J2p1/B+rrYtNw+RCR
N4Q94eAz1e6giI5vODGc4vq8yGXweOvFZOkJxUCwlVZ93IHYgBoDObNlfIbHz5Mv
d7vTzgSJVG7C0hbuBhbc8dv77vJJxNqjZK/UW+mFFaFYokECAwEAAaNTMFEwHQYD
VR0OBBYEFENgDvxd9lg88+ln2b91ql0hWEudMB8GA1UdIwQYMBaAFENgDvxd9lg8
8+ln2b91ql0hWEudMA8GA1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQELBQADggEB
AAC1+zlwWY/Zrg8/tzdf1zSij+9WFJsL+qvsekPaaRHbXcHywxXvCP9rjYt+QZpc
WqEsP1ybXoCK9EHvhFA7NDfOOaXpZvmUSkoH3PfzYRfLfORtzIFru7VLAjnsLSN3
662o7aUdPL9RfYBki5sV1ZFIeqQjdo3HId9WtKyD3yp1LJeoI8woNq+XmR7CLSSJ
WjSZa2YwCVHYWL13xU8c6gqPvSdLWLOvxJyM+86GFDUDOTJMfyzHHqNPtcB/Ywl5
D5oOhVHkMEYnzWUNPZa+05Y2qkC4GXuramlvb21/F8lc+Ai57Ke0I9EgZ7q+TX/6
+zCK/Gxbn9AZGKzNGiqXKCIwKjAVBggrBgEFBQcDAQYJKoZIhvcNAQkBoAoGCCsG
AQUFBwMCDAVNeSBDQQ==
-----END TRUSTED CERTIFICATE-----
)PEM";

const Bytes RsaModulus = FromHex("9b72ad4c3730ffd000e2c41e3941ca2b0f365d5001186e53b73e26d518c5161727fa31a930ddd0989c5ab60a03fe0c3f906f845d85dcda04e84d0ed3b43e"
    "36a52bc17266b2dec9c1c5d9e88504e7327523de07efbf427413c64dd49edeb51d1d92a868a696615de33e86a18e961d10feddc5b1674d4ad8867ebcf5d7d564e5fe0684cbdc134914a"
    "ed5dd88f6995d4a0e85d85f74d7d88f26674826c26d510e1d09da9d7f07eaeb62d370f9109137843de1e033d5eea0888e6f38319ce2fabcc865f078ebc564e909c540b095567ddc81d88"
    "01a0339b3657c86c7cf932f77bbd3ce0489546ec2d216ee0616dcf1dbfbeef249c4daa364afd45be98515a158a241");

constexpr int Asn1Library = 13;
constexpr int RsaLibrary = 4;
constexpr int X509Library = 11;
constexpr int EvpLibrary = 6;
constexpr int PemLibrary = 9;
constexpr int SystemLibrary = 2;
constexpr int BioLibrary = 32;
constexpr int RsaKeyType = 6;
constexpr int FieldMissing = 121;
constexpr int RsaLibraryError = 4;
constexpr int PublicKeyDecodeError = 125;
constexpr int EvpUnsupportedAlgorithm = 156;
constexpr int UnsupportedAlgorithm = 111;
constexpr int BadEndLine = 102;
constexpr int BadBase64Decode = 100;
constexpr int NotProcType = 107;
constexpr int NotEncrypted = 106;
constexpr int NotDekInfo = 105;
constexpr int NoStartLine = 108;
constexpr int Asn1LibraryError = 13;
constexpr int NoSuchFile = 128;
constexpr int MissingFile = 2;
constexpr int NestedAsn1Error = 58;
constexpr int BadObjectHeader = 102;
constexpr int UnexpectedEoc = 159;
constexpr int TooLong = 155;
constexpr int WrongTag = 168;
constexpr int InvalidObjectEncoding = 216;
constexpr int HeaderTooLong = 123;
constexpr int SequenceLengthMismatch = 148;

std::string RecordWithHeader(const char* label, const std::string& header, const Bytes& body) {
    const std::string encoded = Base64(body);
    std::string text = std::string("-----BEGIN ") + label + "-----\n";
    text += header;
    if (!header.empty()) text += '\n';
    text += '\n';
    for (std::size_t index = 0; index < encoded.size(); index += 64) {
        text += encoded.substr(index, 64);
        text += '\n';
    }
    text += std::string("-----END ") + label + "-----\n";
    return text;
}

LibreSslX509* ReadWith(LibreSslX509** stored, const std::string& pem) {
    auto* bio = BIO_new_mem_buf_nid_postfix(pem.data(), static_cast<int>(pem.size()));
    Require(bio, "a memory BIO holds the record");
    LibreSslX509* certificate = PEM_read_bio_X509_nid_postfix(bio, stored, nullptr, nullptr);
    BIO_free_nid_postfix(bio);
    return certificate;
}

LibreSslX509* ReadTrustedWith(LibreSslX509** stored, const std::string& pem) {
    auto* bio = BIO_new_mem_buf_nid_postfix(pem.data(), static_cast<int>(pem.size()));
    Require(bio, "a memory BIO holds the record");
    LibreSslX509* certificate = PEM_read_bio_X509_AUX_nid_postfix(bio, stored, nullptr, nullptr);
    BIO_free_nid_postfix(bio);
    return certificate;
}

constexpr const char* GuestPrefix = "/anyps5-x509-pem-ctx";

struct GuestFiles {
    std::filesystem::path directory;

    GuestFiles() {
        directory = std::filesystem::temp_directory_path() /
            ("anyps5-x509-pem-ctx-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Require(std::filesystem::create_directory(directory), "the host directory behind the guest prefix is created");
        AddPathAlias_nid_no_patch(GuestPrefix, directory.string().c_str());
    }

    ~GuestFiles() {
        RemovePathAlias_nid_no_patch(GuestPrefix);
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
    }

    std::string Path(const char* name) const {
        return std::string(GuestPrefix) + "/" + name;
    }

    std::filesystem::path Host(const char* name) const {
        return directory / name;
    }
};

void WriteFile(const std::filesystem::path& path, const std::string& text) {
    std::ofstream stream(path, std::ios::binary);
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    stream.close();
    Require(!stream.fail(), "the host writes the file the guest reads");
}

struct StoreObject {
    std::int32_t type;
    LibreSslX509* certificate;
};

static_assert(sizeof(StoreObject) == 16 && offsetof(StoreObject, certificate) == 8);

struct NameCache {
    std::uint64_t length;
    char* data;
    std::uint64_t max;
};

static_assert(sizeof(NameCache) == 24 && offsetof(NameCache, data) == 8 && offsetof(NameCache, max) == 16);

void* StackElement(const void* pointer, int index) {
    const auto* stack = static_cast<const LibreSslStack*>(pointer);
    if (!stack || index < 0 || StackCount(pointer) <= index) return nullptr;
    return sk_value_nid_postfix(stack, index);
}

LibreSslX509* StoredCertificate(const void* pointer, int index) {
    const auto* object = static_cast<const StoreObject*>(StackElement(pointer, index));
    return object ? object->certificate : nullptr;
}

void PublicKeys() {
    auto* certificate = ReadCertificate(RsaCertificatePem);
    Require(certificate && certificate->certificateInfo && certificate->certificateInfo->key, "the RSA-2048 certificate decodes");
    RequireNoCodes("reading an RSA certificate queues nothing");
    void* const first = X509_get_pubkey_nid_postfix(certificate);
    Require(first, "X509_get_pubkey returns a public key");
    RequireNoCodes("an RSA public key is decoded without errors");
    Require(EVP_PKEY_id_nid_postfix(first) == RsaKeyType, "the public key holds an RSA key");
    auto* pkey = static_cast<Pkey*>(first);
    Require(pkey->references == 2, "the cache and the caller each hold the public key");
    auto* rsa = EVP_PKEY_get1_RSA_nid_postfix(first);
    Require(rsa && rsa->n && rsa->e, "the RSA key holds its modulus and exponent");
    Require(rsa->references == 2, "EVP_PKEY_get1_RSA keeps an extra reference");
    Require(RSA_size_nid_postfix(rsa) == 256, "the RSA key is 2048 bits long");
    std::array<unsigned char, 256> modulus{};
    Require(BN_num_bits_nid_postfix(static_cast<Bignum*>(rsa->n)) == 2048, "the modulus is 2048 bits");
    Require(BN_bn2bin_nid_postfix(static_cast<Bignum*>(rsa->n), modulus.data()) == static_cast<int>(RsaModulus.size()),
        "the modulus has as many bytes as the certificate");
    Require(std::memcmp(modulus.data(), RsaModulus.data(), RsaModulus.size()) == 0, "the modulus matches the certificate");
    std::array<unsigned char, 3> exponent{};
    Require(BN_bn2bin_nid_postfix(static_cast<Bignum*>(rsa->e), exponent.data()) == 3, "the exponent is three bytes");
    Require(exponent[0] == 0x01 && exponent[1] == 0x00 && exponent[2] == 0x01, "the exponent is 65537");
    RSA_free_nid_postfix(rsa);
    void* const second = X509_get_pubkey_nid_postfix(certificate);
    Require(second == first, "the second call returns the cached public key");
    Require(pkey->references == 3, "every call adds a reference to the public key");
    EVP_PKEY_free_nid_postfix(first);
    EVP_PKEY_free_nid_postfix(second);
    Require(pkey->references == 1, "the certificate keeps the cached public key");
    RequireThrows([&] { X509_free_nid_postfix(certificate); }, "a certificate with a cached public key is not freed");
}

void EcCertificate() {
    auto* certificate = ReadCertificate(EcCertificatePem);
    Require(certificate && certificate->certificateInfo && certificate->certificateInfo->key, "the EC P-256 certificate decodes");
    LibreSslX509PublicKey* const key = certificate->certificateInfo->key;
    Require(Holds(key->algorithm->algorithm, EcPublicKey), "the public key names id-ecPublicKey");
    Require(key->algorithm->parameter && key->algorithm->parameter->type == 6, "the curve is an object");
    Require(Holds(static_cast<const LibreSslAsn1Object*>(key->algorithm->parameter->value), CurvePrime256), "the curve is prime256v1");
    Require(Holds(key->publicKey, 3, EcPoint), "the public key holds a 65 byte point");
    Require(key->publicKey->length == 65 && key->publicKey->data[0] == 0x04, "the point is uncompressed");
    RequireThrows([&] { X509_get_pubkey_nid_postfix(certificate); }, "an EC public key needs a key this library cannot hold");
    Require(!key->key, "a refused public key is not cached");
    X509_free_nid_postfix(certificate);
    RequireNoCodes("freeing an EC certificate queues nothing");
}

void PubkeyErrors() {
    Require(!X509_get_pubkey_nid_postfix(nullptr), "a null certificate has no public key");
    RequireNoCodes("a null certificate queues nothing");

    auto* withKey = ReadCertificate(RsaCertificatePem);
    Require(withKey && withKey->certificateInfo && withKey->certificateInfo->key, "the certificate holds a public key");
    auto* const key = withKey->certificateInfo->key;
    withKey->certificateInfo->key = nullptr;
    Require(!X509_get_pubkey_nid_postfix(withKey), "certificate information without a public key yields nothing");
    RequireNoCodes("a missing public key queues no error");
    withKey->certificateInfo->key = key;
    auto* const bitString = key->publicKey;
    key->publicKey = nullptr;
    Require(!X509_get_pubkey_nid_postfix(withKey), "a public key without bits yields nothing");
    RequireNoCodes("a public key without bits queues no error");
    key->publicKey = bitString;
    X509_free_nid_postfix(withKey);
    RequireNoCodes("the certificate with its fields put back is freed");

    const Bytes broken = SignedCertificate(RsaKeyWithBrokenBits(Tlv(0x30, Tlv(0x02, {0x05}))), "Leaf");
    auto* undecodable = ReadCertificate(Record("CERTIFICATE", broken));
    Require(undecodable, "a public key with broken bits decodes");
    Require(!X509_get_pubkey_nid_postfix(undecodable), "broken public key bits are refused");
    RequireCodes({Code(Asn1Library, FieldMissing), Code(RsaLibrary, RsaLibraryError), Code(X509Library, PublicKeyDecodeError)},
        "an undecodable RSA key queues the ASN1, RSA and X509 reasons");
    X509_free_nid_postfix(undecodable);

    struct Decode {
        Bytes bits;
        std::vector<std::uint64_t> codes;
    };
    const std::array<Decode, 7> decodes{{
        {Bytes{0x30, 0x00}, {Code(Asn1Library, FieldMissing)}},
        {Bytes{0x02, 0x01, 0x05}, {Code(Asn1Library, WrongTag), Code(Asn1Library, NestedAsn1Error)}},
        {Bytes{0x30, 0x06, 0x02, 0x01, 0x05, 0x03, 0x00},
            {Code(Asn1Library, TooLong), Code(Asn1Library, BadObjectHeader), Code(Asn1Library, NestedAsn1Error)}},
        {Bytes{}, {Code(Asn1Library, HeaderTooLong), Code(Asn1Library, BadObjectHeader), Code(Asn1Library, NestedAsn1Error)}},
        {Bytes{0x30, 0x09, 0x02, 0x01, 0x05},
            {Code(Asn1Library, TooLong), Code(Asn1Library, BadObjectHeader), Code(Asn1Library, NestedAsn1Error)}},
        {Bytes{0x30, 0x81, 0x02, 0x02, 0x01, 0x05},
            {Code(Asn1Library, TooLong), Code(Asn1Library, BadObjectHeader), Code(Asn1Library, NestedAsn1Error),
                Code(Asn1Library, NestedAsn1Error)}},
        {Bytes{0x30, 0x09, 0x02, 0x01, 0x05, 0x02, 0x01, 0x03, 0x02, 0x01, 0x02}, {Code(Asn1Library, SequenceLengthMismatch)}},
    }};
    for (const Decode& decode : decodes) {
        auto* refused = ReadCertificate(Record("CERTIFICATE", SignedCertificate(RsaKeyWithBrokenBits(decode.bits), "Leaf")));
        Require(refused, "a certificate with these public key bits still decodes");
        Require(!X509_get_pubkey_nid_postfix(refused), "these public key bits are refused");
        for (const std::uint64_t code : decode.codes)
            Require(ERR_get_error_nid_postfix() == code, "the decode reasons match the reference order");
        Require(ERR_get_error_nid_postfix() == Code(RsaLibrary, RsaLibraryError), "the RSA library reason follows the decode reasons");
        Require(ERR_get_error_nid_postfix() == Code(X509Library, PublicKeyDecodeError), "the X509 reason closes the queue");
        Require(ERR_get_error_nid_postfix() == 0, "a refused public key queues nothing else");
        X509_free_nid_postfix(refused);
    }

    const std::array<Bytes, 4> accepted{{Bytes{0x30, 0x06, 0x02, 0x01, 0x85, 0x02, 0x01, 0x03}, Bytes{0x30, 0x04, 0x02, 0x00, 0x02, 0x00},
        Bytes{0x30, 0x06, 0x02, 0x01, 0x05, 0x02, 0x01, 0x03}, Bytes{0x30, 0x08, 0x02, 0x02, 0x00, 0x05, 0x02, 0x02, 0x01, 0x03}}};
    for (const Bytes& bits : accepted) {
        auto* tolerant = ReadCertificate(Record("CERTIFICATE", SignedCertificate(RsaKeyWithBrokenBits(bits), "Leaf")));
        Require(tolerant, "a certificate with loosely encoded public key bits decodes");
        void* const publicKey = X509_get_pubkey_nid_postfix(tolerant);
        Require(publicKey, "the loosely encoded public key is accepted");
        RequireNoCodes("loose public key encodings queue nothing");
        EVP_PKEY_free_nid_postfix(publicKey);
        RequireThrows([&] { X509_free_nid_postfix(tolerant); }, "the cached public key keeps the certificate");
    }

    for (const Bytes& algorithm : {Bytes(DsaKey), Bytes(EcPublicKey)}) {
        const Bytes bits = Tlv(0x30, Cat({Tlv(0x02, {0x00, 0x01}), Tlv(0x02, {0x01, 0x03})}));
        const Bytes certificateDer = SignedCertificate(KeyInformation(algorithm, Tlv(0x05, {}), bits), "Leaf");
        auto* certificate = ReadCertificate(Record("CERTIFICATE", certificateDer));
        Require(certificate, "a certificate with another algorithm decodes");
        RequireThrows([&] { X509_get_pubkey_nid_postfix(certificate); }, "DSA and EC keys need a key this library cannot hold");
        X509_free_nid_postfix(certificate);
    }

    for (const Bytes& algorithm : {Bytes(DhKeyAgreement), Bytes(UnknownAlgorithm)}) {
        const Bytes bits = Tlv(0x30, Cat({Tlv(0x02, {0x00, 0x01}), Tlv(0x02, {0x01, 0x03})}));
        const Bytes certificateDer = SignedCertificate(KeyInformation(algorithm, Tlv(0x05, {}), bits), "Leaf");
        auto* certificate = ReadCertificate(Record("CERTIFICATE", certificateDer));
        Require(certificate, "a certificate with an unsupported algorithm decodes");
        Require(!X509_get_pubkey_nid_postfix(certificate), "an unsupported algorithm yields no public key");
        RequireCodes({Code(EvpLibrary, EvpUnsupportedAlgorithm), Code(X509Library, UnsupportedAlgorithm)},
            "an unsupported algorithm queues the algorithm and X509 reasons");
        X509_free_nid_postfix(certificate);
    }
}

const char* CertificateListPem = R"PEM(-----BEGIN X509 CRL-----
MIIBWjBEMA0GCSqGSIb3DQEBCwUAMBUxEzARBgNVBAMMCkNSTCBpc3N1ZXIXDTI2
MTExNDIxNTQyNFoXDTI2MTIxNDIxNTQyNFowDQYJKoZIhvcNAQELBQADggEBAKsM
ZdacK4Se44TS/U71I/NVC6JxwIu+lIZM/PTsq0jyGwTfxlsmPUcwUkfHSz++9VPO
brcqw7LgEj/rBfywtkOxjMKxMsjWndHZ8GgTpb7PHomNFAGOY7RUwMm1q+MtpBip
SF6esLqFrye4qgKQHJ58F+g5eDDYEtZ3OI3FpNCjmdcesz8Zi1wyzdY7TPU0Wxs+
wfR871PgN/QyAslwkaE9FdEQnZCW+Yk8/ryUEJjGayoknFHsD+HJ763QHx91/x2X
j5kQ9qKtXaBOpcsKsMd8Z0AF6MOZrOnPtj0J91CLoCOnd6Gb9eu6JXXipzeqs6+a
dChe9QFN3M1tGymVP3w=
-----END X509 CRL-----
)PEM";

const Bytes EmailAddress{0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x01};
const Bytes ClientAuthTrust{0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x02};

Bytes AuxiliaryTwoTrust() {
    return Tlv(0x30, Tlv(0x30, Cat({Tlv(0x06, SmallTrust), Tlv(0x06, OtherTrust)})));
}

Bytes AuxiliaryOneTrust() {
    return Tlv(0x30, Tlv(0x30, Tlv(0x06, SmallTrust)));
}

Bytes AuxiliaryReject() {
    return Tlv(0x30, Lengthed(0xa0, Tlv(0x06, SmallTrust), 3));
}

Bytes AuxiliaryAlias() {
    return Tlv(0x30, Tlv(0x0c, Text("ABC")));
}

Bytes AuxiliaryKeyId() {
    return Tlv(0x30, Tlv(0x04, {0x01, 0x02}));
}

Bytes AuxiliaryAllFields() {
    return Tlv(0x30, Cat({Tlv(0x30, Tlv(0x06, SmallTrust)), Lengthed(0xa0, Tlv(0x06, {0x2b}), 3), Tlv(0x0c, Text("ABC")),
        Tlv(0x04, {0x01, 0x02})}));
}

Bytes AuxiliaryConstructedKeyId() {
    return Tlv(0x30, Lengthed(0x24, {0x04, 0x01, 0x42}, 3));
}

Bytes AuxiliaryEmptyTrustSequence() {
    return Tlv(0x30, Tlv(0x30, {}));
}

Bytes AuxiliaryEmpty() {
    return Tlv(0x30, {});
}

Bytes AuxiliaryOther() {
    return Tlv(0x30, Lengthed(0xa1, Tlv(0x30, Tlv(0x06, SmallTrust)), 5));
}

Bytes AuxiliaryIndefiniteOuter() {
    return Cat({Bytes{0x30, 0x80}, Tlv(0x30, Tlv(0x06, SmallTrust)), Bytes{0x00, 0x00}});
}

Bytes AuxiliaryIndefiniteTrust() {
    return Lengthed(0x30, Cat({Bytes{0x30, 0x80}, Tlv(0x06, SmallTrust), Bytes{0x00, 0x00}}), 9);
}

Bytes AuxiliaryWrongFieldTag() {
    return Tlv(0x30, Tlv(0x06, SmallTrust));
}

Bytes AuxiliaryNullElements() {
    return Tlv(0x30, Tlv(0x30, Cat({Tlv(0x05, {}), Tlv(0x05, {})})));
}

Bytes AuxiliaryEarlyEndOfContent() {
    return Tlv(0x30, Lengthed(0x30, {0x00, 0x00}, 2));
}

Bytes AuxiliaryInvalidObject() {
    return Tlv(0x30, Tlv(0x30, Tlv(0x06, {0x80})));
}

void PemRecords() {
    ERR_clear_error_nid_postfix();
    auto* certificate = ReadCertificate(RsaCertificatePem);
    Require(certificate && certificate->certificateInfo, "a CERTIFICATE record decodes");
    RequireNoCodes("a first record queues nothing");
    Require(certificate->references == 1, "the caller owns the decoded certificate");
    X509_free_nid_postfix(certificate);

    {
        auto* bio = BIO_new_mem_buf_nid_postfix(RsaCertificatePem, static_cast<int>(std::strlen(RsaCertificatePem)));
        Require(bio, "a memory BIO holds one record");
        auto* first = PEM_read_bio_X509_nid_postfix(bio, nullptr, nullptr, nullptr);
        Require(first, "the first read of the BIO succeeds");
        auto* second = PEM_read_bio_X509_nid_postfix(bio, nullptr, nullptr, nullptr);
        Require(!second, "the same BIO holds no further record");
        RequireCodes({Code(PemLibrary, NoStartLine)}, "a finished BIO queues the missing start line");
        X509_free_nid_postfix(first);
        BIO_free_nid_postfix(bio);
    }

    {
        const std::string bundle = std::string(RsaCertificatePem) + EcCertificatePem;
        auto* bio = BIO_new_mem_buf_nid_postfix(bundle.data(), static_cast<int>(bundle.size()));
        auto* first = PEM_read_bio_X509_nid_postfix(bio, nullptr, nullptr, nullptr);
        Require(first && first->certificateInfo->key && Holds(first->certificateInfo->key->algorithm->algorithm, RsaEncryption),
            "the first certificate of the bundle is the RSA one");
        auto* second = PEM_read_bio_X509_nid_postfix(bio, nullptr, nullptr, nullptr);
        Require(second && second->certificateInfo->key && Holds(second->certificateInfo->key->algorithm->algorithm, EcPublicKey),
            "the second certificate of the bundle is the EC one");
        RequireNoCodes("the second record of the bundle queues nothing");
        auto* third = PEM_read_bio_X509_nid_postfix(bio, nullptr, nullptr, nullptr);
        Require(!third, "the bundle ends after two records");
        RequireCodes({Code(PemLibrary, NoStartLine)}, "the end of the bundle queues the missing start line");
        X509_free_nid_postfix(first);
        X509_free_nid_postfix(second);
        BIO_free_nid_postfix(bio);
    }

    const std::string leafRecord = Record("CERTIFICATE", SignedCertificate(RsaKey(64, 0x02), "Leaf"));
    auto prefixed = ReadCertificate("garbage line\nanother line\n" + leafRecord);
    Require(prefixed, "text before the start line is skipped");
    RequireNoCodes("skipped text queues nothing");
    X509_free_nid_postfix(prefixed);

    auto* oldLabel = ReadCertificate(Record("X509 CERTIFICATE", SignedCertificate(RsaKey(64, 0x03), "Leaf")));
    Require(oldLabel, "an X509 CERTIFICATE record is accepted");
    RequireNoCodes("the old label queues nothing");
    X509_free_nid_postfix(oldLabel);

    Require(!ReadCertificate(Record("PRIVATE KEY", SignedCertificate(RsaKey(64, 0x02), "Leaf"))), "a private key record is refused");
    RequireCodes({Code(PemLibrary, NoStartLine)}, "a wrong label queues the missing start line");

    Require(!ReadCertificate(Record("XY", SignedCertificate(RsaKey(64, 0x04), "Leaf"))), "a short label is refused");
    RequireCodes({Code(PemLibrary, NoStartLine)}, "a short label queues the missing start line");

    const std::string truncated = std::string(RsaCertificatePem).substr(0, std::string(RsaCertificatePem).find("-----END"));
    Require(!ReadCertificate(truncated), "a record without an end line is refused");
    RequireCodes({Code(PemLibrary, BadEndLine)}, "a missing end line queues the bad end line");

    std::string broken = RsaCertificatePem;
    broken.replace(broken.find("MIID"), 4, "!!!!");
    Require(!ReadCertificate(broken), "a record with broken base64 yields nothing");
    RequireNoCodes("broken base64 queues nothing");

    Require(!ReadCertificate("-----BEGIN CERTIFICATE-----\nAAAAA\n-----END CERTIFICATE-----\n"),
        "a body that is not a multiple of four is refused");
    RequireCodes({Code(PemLibrary, BadBase64Decode)}, "the bad base64 body queues the decode reason");

    Require(!ReadCertificate("-----BEGIN CERTIFICATE-----\n====\n-----END CERTIFICATE-----\n"),
        "a body of padding only is refused");
    RequireCodes({Code(PemLibrary, BadBase64Decode)}, "the padding body queues the decode reason");

    Require(!ReadCertificate("-----BEGIN CERTIFICATE-----\n-----END CERTIFICATE-----\n"),
        "a record without a body is refused");
    RequireNoCodes("an empty body queues nothing");

    auto* empty = BIO_new_mem_buf_nid_postfix("", 0);
    Require(empty, "an empty memory BIO is created");
    Require(!PEM_read_bio_X509_nid_postfix(empty, nullptr, nullptr, nullptr), "an empty BIO yields nothing");
    RequireCodes({Code(PemLibrary, NoStartLine)}, "an empty BIO queues the missing start line");
    BIO_free_nid_postfix(empty);
}

void PemHeaders() {
    const Bytes leaf = SignedCertificate(RsaKey(64, 0x05), "Leaf");
    Require(!ReadCertificate(RecordWithHeader("CERTIFICATE", "junk", leaf)), "a header without Proc-Type is refused");
    RequireCodes({Code(PemLibrary, NotProcType)}, "a wrong Proc-Type is queued");

    Require(!ReadCertificate(RecordWithHeader("CERTIFICATE", "Proc-Type: 7,ENCRYPTED", leaf)), "a header that is not version 4 is refused");
    RequireNoCodes("a wrong version queues nothing");

    Require(!ReadCertificate(RecordWithHeader("CERTIFICATE", "Proc-Type: 4;ENCRYPTED", leaf)), "a header without a comma is refused");
    RequireNoCodes("a wrong separator queues nothing");

    Require(!ReadCertificate(RecordWithHeader("CERTIFICATE", "Proc-Type: 4,PLAIN", leaf)), "a header that is not encrypted is refused");
    RequireCodes({Code(PemLibrary, NotEncrypted)}, "a wrong Proc-Type value is queued");

    Require(!ReadCertificate(RecordWithHeader("CERTIFICATE", "Proc-Type: 4,ENCRYPTED\njunk", leaf)), "a header without DEK-Info is refused");
    RequireCodes({Code(PemLibrary, NotDekInfo)}, "a missing DEK-Info is queued");

    RequireThrows([&] {
        ReadCertificate(RecordWithHeader("CERTIFICATE", "Proc-Type: 4,ENCRYPTED\nDEK-Info: AES-128-CBC,00112233445566778899AABBCCDDEEFF", leaf));
    }, "an encrypted record needs a cipher this library does not read");
    ERR_clear_error_nid_postfix();

    auto* withoutHeader = ReadCertificate(RecordWithHeader("CERTIFICATE", "", leaf));
    Require(withoutHeader, "an empty header is accepted");
    RequireNoCodes("an empty header queues nothing");
    X509_free_nid_postfix(withoutHeader);
}

void PemFile() {
    const GuestFiles files;
    const std::filesystem::path host = files.Host("anyps5-x509-pem-read.pem");
    WriteFile(host, RsaCertificatePem);
    const std::string guest = files.Path("anyps5-x509-pem-read.pem");
    auto* stream = fopen_nid_postfix(guest.c_str(), "rb");
    Require(stream, "the guest opens the record file");
    auto* certificate = PEM_read_X509_nid_postfix(stream, nullptr, nullptr, nullptr);
    Require(certificate, "PEM_read_X509 reads a certificate from a file");
    RequireNoCodes("reading from a file queues nothing");
    Require(!PEM_read_X509_nid_postfix(stream, nullptr, nullptr, nullptr), "the file holds one record");
    RequireCodes({Code(PemLibrary, NoStartLine)}, "the end of the file queues the missing start line");
    X509_free_nid_postfix(certificate);
    Require(fclose_nid_postfix(stream) == 0, "the record file is closed");
    std::error_code ignored;
    Require(std::filesystem::remove(host), "the record file is removed");
}

void PemReuse() {
    auto* first = ReadCertificate(EcCertificatePem);
    Require(first, "a certificate is read before reuse");
    auto* same = ReadWith(&first, RsaCertificatePem);
    Require(same == first, "a plain read decodes into the certificate that was passed");
    Require(Holds(first->certificateInfo->key->algorithm->algorithm, RsaEncryption), "the reused certificate is the new one");
    RequireNoCodes("reusing a certificate queues nothing");
    X509_free_nid_postfix(first);

    auto* plain = ReadCertificate(EcCertificatePem);
    auto* const previous = plain;
    const Bytes leaf = SignedCertificate(RsaKey(64, 0x06), "Leaf");
    auto* replaced = ReadTrustedWith(&plain, Trusted(leaf, AuxiliaryEmpty()));
    Require(replaced && replaced != previous, "a trusted read decodes a new certificate");
    Require(plain == replaced, "the caller's pointer now holds the new certificate");
    RequireNoCodes("replacing a certificate queues nothing");
    RequireThrows([&] { X509_free_nid_postfix(replaced); }, "a trusted certificate cannot be freed");
}

void TrustedCertificates() {
    auto* simple = ReadTrusted(TrustedCertificatePem);
    Require(simple, "a trusted certificate with one trust object decodes");
    RequireNoCodes("reading a trusted certificate queues nothing");
    RequireAux(simple, "the auxiliary data is attached");
    const CertAux* simpleAux = Auxiliary(simple);
    Require(StackCount(simpleAux->trust) == 1, "one trust object is kept");
    Require(Holds(StackObject(simpleAux->trust, 0), ServerAuthTrust), "the trust object is id-kp-serverAuth");
    Require(!simpleAux->reject && !simpleAux->alias && !simpleAux->keyid && !simpleAux->other, "the other auxiliary fields stay empty");
    RequireThrows([&] { X509_free_nid_postfix(simple); }, "a certificate with auxiliary data cannot be freed");

    auto* full = ReadTrusted(TrustedFullCertificatePem);
    Require(full, "a trusted certificate with every auxiliary field decodes");
    RequireNoCodes("the full auxiliary data queues nothing");
    const CertAux* fullAux = Auxiliary(full);
    Require(StackCount(fullAux->trust) == 2, "two trust objects are kept");
    Require(Holds(StackObject(fullAux->trust, 0), ServerAuthTrust) && Holds(StackObject(fullAux->trust, 1), EmailAddress),
        "both trust objects are kept in order");
    Require(StackCount(fullAux->reject) == 1, "one reject object is kept");
    Require(Holds(StackObject(fullAux->reject, 0), ClientAuthTrust), "the reject object is id-kp-clientAuth");
    Require(Holds(fullAux->alias, 12, Text("My CA")), "the alias is a UTF8String");
    Require(!fullAux->keyid && !fullAux->other, "the key id and other fields stay empty");
    RequireThrows([&] { X509_free_nid_postfix(full); }, "the full trusted certificate cannot be freed");

    Require(!ReadCertificate(TrustedCertificatePem), "a trusted record is refused by the plain reader");
    RequireCodes({Code(PemLibrary, NoStartLine)}, "a label that only the auxiliary reader accepts is queued as missing");

    auto* plainAsTrusted = ReadTrusted(RsaCertificatePem);
    Require(plainAsTrusted && plainAsTrusted->certificateInfo, "a plain certificate is accepted as trusted");
    Require(plainAsTrusted->auxiliary == nullptr, "a plain certificate has no auxiliary data");
    RequireNoCodes("reading a plain certificate as trusted queues nothing");
    X509_free_nid_postfix(plainAsTrusted);
}

void TrustedFields() {
    struct Case {
        Bytes auxiliary;
        int trust;
        int reject;
        bool alias;
        bool keyid;
    };
    const Bytes leaf = SignedCertificate(RsaKey(64, 0x06), "Leaf");
    const std::array<Case, 4> cases{{
        {AuxiliaryEmpty(), -1, -1, false, false},
        {AuxiliaryTwoTrust(), 2, -1, false, false},
        {AuxiliaryEmptyTrustSequence(), 0, -1, false, false},
        {AuxiliaryIndefiniteOuter(), 1, -1, false, false},
    }};
    for (const Case& testCase : cases) {
        auto* certificate = ReadTrusted(Trusted(leaf, testCase.auxiliary));
        Require(certificate, "an auxiliary sequence decodes");
        RequireNoCodes("a valid auxiliary sequence queues nothing");
        RequireAux(certificate, "the auxiliary data is attached");
        const CertAux* aux = Auxiliary(certificate);
        Require(StackCount(aux->trust) == testCase.trust, "the trust stack matches");
        Require(StackCount(aux->reject) == testCase.reject, "the reject stack matches");
        Require((aux->alias != nullptr) == testCase.alias, "the alias matches");
        Require((aux->keyid != nullptr) == testCase.keyid, "the key id matches");
    }

    auto* trust = ReadTrusted(Trusted(leaf, Tlv(0x30, Tlv(0x30, Tlv(0x06, SmallTrust)))));
    Require(StackCount(Auxiliary(trust)->trust) == 1 && Holds(StackObject(Auxiliary(trust)->trust, 0), SmallTrust),
        "one trust object is kept as it is encoded");
    auto* second = ReadTrusted(Trusted(leaf, AuxiliaryTwoTrust()));
    Require(StackCount(Auxiliary(second)->trust) == 2 && Holds(StackObject(Auxiliary(second)->trust, 1), OtherTrust),
        "the second trust object is kept");
    auto* reject = ReadTrusted(Trusted(leaf, AuxiliaryReject()));
    Require(StackCount(Auxiliary(reject)->reject) == 1 && Holds(StackObject(Auxiliary(reject)->reject, 0), SmallTrust),
        "an implicit reject sequence is read");
    auto* alias = ReadTrusted(Trusted(leaf, AuxiliaryAlias()));
    Require(Holds(Auxiliary(alias)->alias, 12, Text("ABC")), "an alias is read as a UTF8String");
    auto* keyId = ReadTrusted(Trusted(leaf, AuxiliaryKeyId()));
    Require(Holds(Auxiliary(keyId)->keyid, 4, Bytes{0x01, 0x02}), "a key id is read as an octet string");
    auto* constructed = ReadTrusted(Trusted(leaf, AuxiliaryConstructedKeyId()));
    Require(Holds(Auxiliary(constructed)->keyid, 4, Bytes{0x42}), "a constructed key id keeps its content");
    auto* all = ReadTrusted(Trusted(leaf, AuxiliaryAllFields()));
    Require(StackCount(Auxiliary(all)->trust) == 1 && StackCount(Auxiliary(all)->reject) == 1 && Auxiliary(all)->alias &&
        Auxiliary(all)->keyid, "every optional field is read in order");
    auto* extra = ReadTrusted(Trusted(leaf, Cat({AuxiliaryOneTrust(), Bytes{0x00}})));
    Require(extra && StackCount(Auxiliary(extra)->trust) == 1, "a byte after the auxiliary sequence is left alone");
    RequireNoCodes("a trailing byte queues nothing");
    RequireThrows([&] { ReadTrusted(Trusted(leaf, AuxiliaryOther())); }, "other auxiliary information is not implemented");
    ERR_clear_error_nid_postfix();
}

void TrustedErrors() {
    const Bytes leaf = SignedCertificate(RsaKey(64, 0x07), "Leaf");
    struct Failure {
        Bytes auxiliary;
        std::vector<std::uint64_t> codes;
    };
    const std::array<Failure, 5> failures{{
        {AuxiliaryInvalidObject(), {Code(Asn1Library, InvalidObjectEncoding), Code(Asn1Library, NestedAsn1Error), Code(PemLibrary, Asn1LibraryError)}},
        {AuxiliaryWrongFieldTag(), {Code(Asn1Library, WrongTag), Code(Asn1Library, NestedAsn1Error), Code(PemLibrary, Asn1LibraryError)}},
        {AuxiliaryNullElements(),
            {Code(Asn1Library, WrongTag), Code(Asn1Library, NestedAsn1Error), Code(Asn1Library, NestedAsn1Error), Code(PemLibrary, Asn1LibraryError)}},
        {AuxiliaryEarlyEndOfContent(), {Code(Asn1Library, UnexpectedEoc), Code(PemLibrary, Asn1LibraryError)}},
        {AuxiliaryIndefiniteTrust(),
            {Code(Asn1Library, TooLong), Code(Asn1Library, BadObjectHeader), Code(Asn1Library, NestedAsn1Error), Code(PemLibrary, Asn1LibraryError)}},
    }};
    for (const Failure& failure : failures) {
        const std::string record = Trusted(leaf, failure.auxiliary);
        auto* input = BIO_new_mem_buf_nid_postfix(record.data(), static_cast<int>(record.size()));
        auto* certificate = PEM_read_bio_X509_AUX_nid_postfix(input, nullptr, nullptr, nullptr);
        Require(!certificate, "a broken auxiliary sequence yields no certificate");
        for (const std::uint64_t code : failure.codes) Require(ERR_get_error_nid_postfix() == code, "the queued auxiliary reasons match");
        Require(ERR_get_error_nid_postfix() == 0, "only the listed reasons are queued");
        BIO_free_nid_postfix(input);
    }

    auto* supplied = ReadCertificate(EcCertificatePem);
    auto* const before = supplied;
    const std::string failing = Trusted(leaf, AuxiliaryInvalidObject());
    auto* bio = BIO_new_mem_buf_nid_postfix(failing.data(), static_cast<int>(failing.size()));
    auto* failed = PEM_read_bio_X509_AUX_nid_postfix(bio, &supplied, nullptr, nullptr);
    Require(!failed, "a failed auxiliary decode yields nothing");
    Require(supplied == before, "a failed auxiliary decode leaves the supplied certificate alone");
    RequireCodes({Code(Asn1Library, InvalidObjectEncoding), Code(Asn1Library, NestedAsn1Error), Code(PemLibrary, Asn1LibraryError)},
        "a failed auxiliary decode on a supplied certificate queues the same reasons");
    BIO_free_nid_postfix(bio);
    X509_free_nid_postfix(supplied);
}

void StoreLocations() {
    const GuestFiles files;
    WriteFile(files.Host("single.pem"), RsaCertificatePem);
    WriteFile(files.Host("bundle.pem"), std::string(RsaCertificatePem) + EcCertificatePem);
    WriteFile(files.Host("empty.pem"), std::string());
    WriteFile(files.Host("cert-and-crl.pem"), std::string(RsaCertificatePem) + CertificateListPem);
    const std::string single = files.Path("single.pem");
    const std::string bundle = files.Path("bundle.pem");
    const std::string empty = files.Path("empty.pem");
    const std::string missing = files.Path("no-such-file.pem");
    const std::string withCrl = files.Path("cert-and-crl.pem");

    {
        auto* store = X509_STORE_new_nid_postfix();
        Require(store, "a verify store is created");
        LibreSslCtxInternal internal{};
        LibreSslCtx context{};
        context.internal = &internal;
        context.certStore = store;

        Require(SSL_CTX_load_verify_locations_nid_postfix(&context, single.c_str(), nullptr) == 1, "a file with one certificate is loaded");
        RequireNoCodes("loading one certificate queues nothing");
        Require(StackCount(store->objects) == 1, "the store holds the certificate of the file");
        Require(StackCount(store->lookupMethods) == 1, "loading a file adds one lookup");
        const auto* lookup = static_cast<const LibreSslX509Lookup*>(StackElement(store->lookupMethods, 0));
        Require(lookup && lookup->method == X509_LOOKUP_file_nid_postfix() && lookup->storeContext == store,
            "the lookup is the file method of this store");
        auto* loaded = StoredCertificate(store->objects, 0);
        Require(loaded && loaded->certificateInfo && loaded->references == 1, "the store owns the decoded certificate");
        Require(Holds(loaded->certificateInfo->key->algorithm->algorithm, RsaEncryption), "the stored certificate is the RSA one");

        Require(SSL_CTX_load_verify_locations_nid_postfix(&context, missing.c_str(), nullptr) == 0, "a missing file is refused");
        RequireCodes({Code(SystemLibrary, MissingFile), Code(BioLibrary, NoSuchFile), Code(X509Library, MissingFile)},
            "a missing file queues the open error, the BIO reason and the X509 reason");
        Require(StackCount(store->objects) == 1, "a missing file leaves the stored certificates alone");
        Require(StackCount(store->lookupMethods) == 1, "a missing file reuses the file lookup");

        Require(SSL_CTX_load_verify_locations_nid_postfix(&context, empty.c_str(), nullptr) == 0, "a file without certificates is refused");
        RequireNoCodes("a file without certificates queues nothing");
        Require(StackCount(store->objects) == 1, "a file without certificates changes nothing");

        X509_STORE_free_nid_postfix(store);
    }

    {
        auto* store = X509_STORE_new_nid_postfix();
        LibreSslCtxInternal internal{};
        LibreSslCtx context{};
        context.internal = &internal;
        context.certStore = store;

        Require(SSL_CTX_load_verify_locations_nid_postfix(&context, bundle.c_str(), nullptr) == 1, "a bundle of certificates is loaded");
        RequireNoCodes("a loaded bundle queues nothing");
        Require(StackCount(store->objects) == 2, "both certificates of the bundle reach the store");
        Require(StackCount(store->lookupMethods) == 1, "the whole bundle uses one lookup");
        auto* first = StoredCertificate(store->objects, 0);
        auto* second = StoredCertificate(store->objects, 1);
        Require(first && second && first != second, "the two certificates are separate objects");
        Require(first->references == 1 && second->references == 1, "the store owns both certificates");
        Require(Holds(first->certificateInfo->key->algorithm->algorithm, RsaEncryption) &&
            Holds(second->certificateInfo->key->algorithm->algorithm, EcPublicKey), "the bundle keeps its order");
        X509_STORE_free_nid_postfix(store);
    }

    {
        auto* store = X509_STORE_new_nid_postfix();
        LibreSslCtxInternal internal{};
        LibreSslCtx context{};
        context.internal = &internal;
        context.certStore = store;
        Require(SSL_CTX_load_verify_locations_nid_postfix(&context, single.c_str(), nullptr) == 1, "the file is loaded once");
        RequireThrows([&] { SSL_CTX_load_verify_locations_nid_postfix(&context, single.c_str(), nullptr); },
            "a second load of the same subject reaches the store guard");
        ERR_clear_error_nid_postfix();
        Require(StackCount(store->objects) == 1, "the refused second load keeps one certificate in the store");
        Require(StackCount(store->lookupMethods) == 1, "the refused second load keeps one lookup");
        X509_STORE_free_nid_postfix(store);
    }

    {
        auto* store = X509_STORE_new_nid_postfix();
        LibreSslCtxInternal internal{};
        LibreSslCtx context{};
        context.internal = &internal;
        context.certStore = store;
        Require(SSL_CTX_load_verify_locations_nid_postfix(&context, nullptr, nullptr) == 0, "without file and path nothing is loaded");
        RequireNoCodes("neither file nor path queues nothing");
        Require(StackCount(store->objects) == 0 && StackCount(store->lookupMethods) == 0, "no lookup is added without a file");
        X509_STORE_free_nid_postfix(store);
    }

    {
        auto* store = X509_STORE_new_nid_postfix();
        LibreSslCtxInternal internal{};
        LibreSslCtx context{};
        context.internal = &internal;
        context.certStore = store;
        RequireThrows([&] { SSL_CTX_load_verify_locations_nid_postfix(&context, withCrl.c_str(), nullptr); },
            "a revocation list in the file is not read yet");
        ERR_clear_error_nid_postfix();
        X509_STORE_free_nid_postfix(store);
    }

    {
        auto* store = X509_STORE_new_nid_postfix();
        LibreSslCtxInternal internal{};
        LibreSslCtx context{};
        context.internal = &internal;
        context.certStore = store;
        RequireThrows([&] { SSL_CTX_load_verify_locations_nid_postfix(&context, nullptr, files.Path("ca").c_str()); },
            "a directory of certificates is not read yet");
        ERR_clear_error_nid_postfix();
        Require(StackCount(store->lookupMethods) == 0, "a refused directory adds no lookup");
        X509_STORE_free_nid_postfix(store);
    }

    RequireThrows([&] { SSL_CTX_load_verify_locations_nid_postfix(nullptr, single.c_str(), nullptr); }, "a null context is refused");
}

const LibreSslX509NameEntry* NameEntryAt(const LibreSslX509Name* name, int index) {
    if (!name || !name->entries || index < 0 || index >= name->entries->num) return nullptr;
    return reinterpret_cast<const LibreSslX509NameEntry*>(name->entries->data[index]);
}

void RequireSameName(const LibreSslX509Name* copy, const LibreSslX509Name* original, const char* message) {
    Require(copy && original && copy != original, message);
    Require(copy->modified == 0, message);
    Require(copy->entries != original->entries && copy->entries->num == original->entries->num, message);
    Require(copy->entries->num > 0, message);
    for (int index = 0; index < original->entries->num; ++index) {
        const auto* left = NameEntryAt(original, index);
        const auto* right = NameEntryAt(copy, index);
        Require(left && right && left != right && right->set == left->set && right->size == left->size, message);
        const auto* leftObject = static_cast<const LibreSslAsn1Object*>(left->object);
        const auto* rightObject = static_cast<const LibreSslAsn1Object*>(right->object);
        Require(rightObject && rightObject != leftObject && rightObject->nid == leftObject->nid, message);
        Require(rightObject->length == leftObject->length && rightObject->data != leftObject->data, message);
        Require(rightObject->data && std::memcmp(rightObject->data, leftObject->data, static_cast<std::size_t>(leftObject->length)) == 0, message);
        const auto* leftValue = static_cast<const Asn1String*>(left->value);
        const auto* rightValue = static_cast<const Asn1String*>(right->value);
        Require(rightValue && rightValue != leftValue && rightValue->type == leftValue->type, message);
        Require(rightValue->length == leftValue->length && rightValue->data != leftValue->data, message);
        Require(rightValue->data && std::memcmp(rightValue->data, leftValue->data, static_cast<std::size_t>(leftValue->length)) == 0, message);
    }
    const auto* leftBytes = static_cast<const NameCache*>(original->bytes);
    const auto* rightBytes = static_cast<const NameCache*>(copy->bytes);
    Require(rightBytes && rightBytes != leftBytes && rightBytes->length == leftBytes->length, message);
    Require(rightBytes->data != leftBytes->data &&
        std::memcmp(rightBytes->data, leftBytes->data, static_cast<std::size_t>(leftBytes->length)) == 0, message);
    Require(copy->canonEnclen == original->canonEnclen && copy->canonEnc != original->canonEnc, message);
    Require(std::memcmp(copy->canonEnc, original->canonEnc, static_cast<std::size_t>(original->canonEnclen)) == 0, message);
}

void ClientCa() {
    auto* certificate = ReadCertificate(RsaCertificatePem);
    Require(certificate, "the client certificate authority is decoded");
    const auto* subject = static_cast<const LibreSslX509Name*>(X509_get_subject_name_nid_postfix(certificate));
    Require(subject, "the authority has a subject");

    LibreSslCtxInternal internal{};
    LibreSslCtx context{};
    context.internal = &internal;

    Require(SSL_CTX_add_client_CA_nid_postfix(&context, nullptr) == 0, "a null certificate adds nothing");
    RequireNoCodes("a null certificate queues nothing");
    Require(internal.clientCa == nullptr, "a null certificate leaves the authority stack uncreated");

    Require(SSL_CTX_add_client_CA_nid_postfix(&context, certificate) == 1, "a certificate authority is added");
    RequireNoCodes("adding a certificate authority queues nothing");
    const auto* stack = static_cast<const LibreSslStack*>(internal.clientCa);
    Require(stack && StackCount(stack) == 1, "the stack holds one name");
    const auto* copy = reinterpret_cast<const LibreSslX509Name*>(stack->data[0]);
    RequireSameName(copy, subject, "the stack holds a deep copy of the subject");
    char* copyLine = X509_NAME_oneline_nid_postfix(copy, nullptr, 0);
    char* subjectLine = X509_NAME_oneline_nid_postfix(subject, nullptr, 0);
    Require(copyLine && subjectLine && std::strcmp(copyLine, subjectLine) == 0, "the copy reads as the subject");
    free_nid_postfix(copyLine);

    Require(SSL_CTX_add_client_CA_nid_postfix(&context, certificate) == 1, "the same authority is added again");
    RequireNoCodes("the second authority queues nothing");
    Require(StackCount(stack) == 2, "LibreSSL keeps one entry per call");
    Require(stack->data[1] != stack->data[0] && stack->data[1] != reinterpret_cast<char*>(const_cast<LibreSslX509Name*>(subject)),
        "the second entry is another copy");
    RequireSameName(reinterpret_cast<const LibreSslX509Name*>(stack->data[1]), subject, "the second copy holds the subject too");

    auto* other = ReadCertificate(EcCertificatePem);
    Require(SSL_CTX_add_client_CA_nid_postfix(&context, other) == 1, "a second authority is added");
    Require(StackCount(stack) == 3, "both authorities are kept");
    const auto* ecCopy = reinterpret_cast<const LibreSslX509Name*>(stack->data[2]);
    Require(ecCopy != copy && static_cast<const void*>(ecCopy) != static_cast<const void*>(stack->data[1]), "each authority brings its own copy");
    char* ecLine = X509_NAME_oneline_nid_postfix(ecCopy, nullptr, 0);
    Require(ecLine && std::strcmp(ecLine, subjectLine) != 0, "the second authority keeps its own subject");
    free_nid_postfix(ecLine);
    free_nid_postfix(subjectLine);

    X509_free_nid_postfix(certificate);
    X509_free_nid_postfix(other);
    RequireThrows([] { SSL_CTX_add_client_CA_nid_postfix(nullptr, nullptr); }, "a null context is refused");
}

std::set<void*> live;
int failAt = 0;
int calls = 0;

void* Track(void* pointer) {
    if (pointer) live.insert(pointer);
    return pointer;
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    if (++calls == failAt) return nullptr;
    return Track(std::malloc(size));
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) {
    if (++calls == failAt) return nullptr;
    return Track(std::calloc(count, size));
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (pointer) Require(live.erase(pointer) == 1, "only live allocations are freed");
    std::free(pointer);
}

void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) {
    if (++calls == failAt) return nullptr;
    if (pointer) Require(live.erase(pointer) == 1, "only live allocations are reallocated");
    return Track(std::realloc(pointer, size));
}

void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t) {
    std::abort();
}

void* APS5_VABI Realign_nid_no_patch(void*, std::size_t, std::size_t) {
    std::abort();
}

int APS5_VABI PosixAlign_nid_no_patch(void**, std::size_t, std::size_t) {
    std::abort();
}

void Sweep(const std::string& record, bool trusted, bool succeeds) {
    int failures = 0;
    for (int failing = 1;; ++failing) {
        ERR_clear_error_nid_postfix();
        calls = 0;
        failAt = failing;
        auto* bio = BIO_new_mem_buf_nid_postfix(record.data(), static_cast<int>(record.size()));
        LibreSslX509* certificate = nullptr;
        if (bio)
            certificate = trusted ? PEM_read_bio_X509_AUX_nid_postfix(bio, nullptr, nullptr, nullptr)
                                  : PEM_read_bio_X509_nid_postfix(bio, nullptr, nullptr, nullptr);
        if (bio) BIO_free_nid_postfix(bio);
        const bool exhausted = calls < failing;
        if (certificate)
            X509_free_nid_postfix(certificate);
        else
            Require(ERR_peek_error_nid_postfix() != 0, "a refused read leaves an error queued");
        Require(live.empty(), "nothing leaks");
        Require(succeeds || certificate == nullptr, "a record that cannot be kept is never returned");
        if (exhausted) {
            Require(certificate != nullptr || !succeeds, "the run without a failing allocation behaves as the fixed run");
            break;
        }
        ++failures;
    }
    failAt = 0;
    Require(failures > 10, "most allocations are checked");
}

void Allocator() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch), reinterpret_cast<void*>(Calloc_nid_no_patch),
        reinterpret_cast<void*>(Reallocate_nid_no_patch), reinterpret_cast<void*>(Align_nid_no_patch),
        reinterpret_cast<void*>(Realign_nid_no_patch), reinterpret_cast<void*>(PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    const Bytes leaf = SignedCertificate(RsaKey(64, 0x08), "Leaf");
    Sweep(Record("CERTIFICATE", leaf), false, true);
    Sweep(Trusted(leaf, AuxiliaryInvalidObject()), true, false);
}

}  // namespace

int main(int argc, char**) {
    if (argc > 1) {
        Allocator();
        return 0;
    }
    std::array<void*, 10> api{};
    ApplicationHeapRegister_nid_no_patch(api.data());
    ERR_clear_error_nid_postfix();
    PublicKeys();
    EcCertificate();
    PubkeyErrors();
    PemRecords();
    PemHeaders();
    PemFile();
    PemReuse();
    TrustedCertificates();
    TrustedFields();
    TrustedErrors();
    StoreLocations();
    ClientCa();
    return 0;
}
