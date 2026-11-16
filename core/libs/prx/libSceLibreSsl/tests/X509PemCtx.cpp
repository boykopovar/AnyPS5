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
#include <barrier>
#include <mutex>
#include <thread>
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

struct CurveKey {
    const void* method;
    std::int32_t version;
    const void* group;
    const void* publicKey;
    const void* privateKey;
    std::uint32_t encodingFlags;
    std::int32_t conversionForm;
    std::int32_t references;
    std::int32_t flags;
    void* extraData;
};

static_assert(sizeof(CurveKey) == 64 && offsetof(CurveKey, references) == 48 && offsetof(CurveKey, conversionForm) == 44 &&
    offsetof(CurveKey, group) == 16 && offsetof(CurveKey, publicKey) == 24);

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
constexpr int EcLibrary = 16;
constexpr int X509Library = 11;
constexpr int EvpLibrary = 6;
constexpr int PemLibrary = 9;
constexpr int SystemLibrary = 2;
constexpr int BioLibrary = 32;
constexpr int RsaKeyType = 6;
constexpr int EcKeyType = 408;
constexpr int PointConversionUncompressed = 4;
constexpr int PointConversionCompressed = 2;
constexpr int FieldMissing = 121;
constexpr int RsaLibraryError = 4;
constexpr int EcLibraryError = 16;
constexpr int EcDecodeError = 142;
constexpr int ExpectingAnRsaKey = 127;
constexpr int ExpectingAnEcKey = 142;
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
constexpr int SequenceNotConstructed = 149;
constexpr int PemLibraryFailure = 9;

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

int OpenDescriptors() {
    std::error_code error;
    const auto begin = std::filesystem::directory_iterator("/proc/self/fd",
        std::filesystem::directory_options::skip_permission_denied, error);
    Require(!error, "the descriptors of this process can be listed");
    return static_cast<int>(std::distance(begin, std::filesystem::directory_iterator{}));
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

struct StoreFixture {
    explicit StoreFixture(const char* message) : store(X509_STORE_new_nid_postfix()) {
        Require(store, message);
        context.internal = &internal;
        context.certStore = store;
    }
    StoreFixture(const StoreFixture&) = delete;
    StoreFixture& operator=(const StoreFixture&) = delete;
    ~StoreFixture() {
        if (store) X509_STORE_free_nid_postfix(store);
    }
    int Load(const std::string& file) { return SSL_CTX_load_verify_locations_nid_postfix(&context, file.c_str(), nullptr); }
    void Close() {
        X509_STORE_free_nid_postfix(store);
        store = nullptr;
    }
    const void* Objects() const { return static_cast<const void*>(store->objects); }
    const void* Lookups() const { return static_cast<const void*>(store->lookupMethods); }
    LibreSslX509Store* store;
    LibreSslCtxInternal internal;
    LibreSslCtx context;
};

void PublicKeys() {
    auto* certificate = ReadCertificate(RsaCertificatePem);
    Require(certificate && certificate->certificateInfo && certificate->certificateInfo->key, "the RSA-2048 certificate decodes");
    RequireNoCodes("reading an RSA certificate queues nothing");
    auto* const slot = certificate->certificateInfo->key;
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
    Require(slot->key == first, "the decoded key is the one held in the cache");
    EVP_PKEY_free_nid_postfix(first);
    EVP_PKEY_free_nid_postfix(second);
    Require(pkey->references == 1, "the certificate keeps the cached public key");
    X509_free_nid_postfix(certificate);
    RequireNoCodes("freeing the references first and then the certificate succeeds");

    auto* other = ReadCertificate(RsaCertificatePem);
    Require(other && other->certificateInfo && other->certificateInfo->key, "a second RSA-2048 certificate decodes");
    void* const kept = X509_get_pubkey_nid_postfix(other);
    Require(kept, "the second certificate also returns a public key");
    auto* const cached = static_cast<Pkey*>(kept);
    Require(cached->references == 2, "the cache and the caller each hold the second public key");
    X509_free_nid_postfix(other);
    RequireNoCodes("freeing the certificate before the returned reference succeeds");
    Require(EVP_PKEY_id_nid_postfix(kept) == RsaKeyType, "the key stays a usable RSA key after the certificate is freed");
    auto* const keptRsa = EVP_PKEY_get1_RSA_nid_postfix(kept);
    Require(keptRsa && keptRsa->n && keptRsa->e, "the RSA object of the kept key is still readable");
    Require(RSA_size_nid_postfix(keptRsa) == 256, "the kept RSA key is still 2048 bits long");
    RSA_free_nid_postfix(keptRsa);
    EVP_PKEY_free_nid_postfix(kept);
    RequireNoCodes("the reference released last closes the key without errors");
}

void EcCertificate() {
    auto* certificate = ReadCertificate(EcCertificatePem);
    Require(certificate && certificate->certificateInfo && certificate->certificateInfo->key, "the EC P-256 certificate decodes");
    auto* const key = certificate->certificateInfo->key;
    Require(Holds(key->algorithm->algorithm, EcPublicKey), "the public key names id-ecPublicKey");
    Require(key->algorithm->parameter && key->algorithm->parameter->type == 6, "the curve is an object");
    Require(Holds(static_cast<const LibreSslAsn1Object*>(key->algorithm->parameter->value), CurvePrime256), "the curve is prime256v1");
    Require(Holds(key->publicKey, 3, EcPoint), "the public key holds a 65 byte point");
    Require(key->publicKey->length == 65 && key->publicKey->data[0] == 0x04, "the point is uncompressed");
    void* const publicKey = X509_get_pubkey_nid_postfix(certificate);
    Require(publicKey, "the elliptic curve public key is decoded");
    RequireNoCodes("an EC P-256 public key is decoded without errors");
    Require(key->key == publicKey, "the elliptic curve key is cached in the public key slot");
    Require(EVP_PKEY_id_nid_postfix(publicKey) == EcKeyType, "the public key holds an elliptic curve key");
    auto* const curveKey = EVP_PKEY_get1_EC_KEY_nid_postfix(publicKey);
    Require(curveKey, "EVP_PKEY_get1_EC_KEY returns the curve key");
    RequireNoCodes("the curve key is handed out without errors");
    auto* const mirror = reinterpret_cast<CurveKey*>(curveKey);
    Require(mirror->references == 2, "the key and the caller each hold the curve key");
    Require(mirror->group, "the curve key holds its group");
    Require(mirror->publicKey, "the curve key holds its public point");
    Require(mirror->conversionForm == PointConversionUncompressed, "the point conversion form comes from the encoded point");
    auto* const group = static_cast<EcGroup*>(const_cast<void*>(mirror->group));
    Require(EC_GROUP_get_degree_nid_postfix(group) == 256, "the curve is 256 bits wide");
    Require(EC_KEY_check_key_nid_postfix(curveKey) == 1, "the public point lies on the curve");
    std::array<unsigned char, 65> encoded{};
    Require(EC_POINT_point2oct_nid_postfix(group, static_cast<::EcPoint*>(const_cast<void*>(mirror->publicKey)), PointConversionUncompressed,
                   encoded.data(), encoded.size(), nullptr) == EcPoint.size(),
        "the public point encodes back into the certificate bytes");
    Require(std::memcmp(encoded.data(), EcPoint.data(), EcPoint.size()) == 0, "the public point matches the certificate point");
    auto* const refusedRsa = EVP_PKEY_get1_RSA_nid_postfix(publicKey);
    Require(!refusedRsa, "an elliptic curve key is not an RSA key");
    RequireCodes({Code(EvpLibrary, ExpectingAnRsaKey)}, "asking an elliptic curve key for RSA queues the EVP reason");
    EC_KEY_free_nid_postfix(curveKey);
    Require(mirror->references == 1, "the key alone still holds the curve key");
    X509_free_nid_postfix(certificate);
    RequireNoCodes("the certificate is freed while the caller still holds the public key");
    Require(EVP_PKEY_id_nid_postfix(publicKey) == EcKeyType, "the public key survives the certificate that cached it");
    auto* const againKey = EVP_PKEY_get1_EC_KEY_nid_postfix(publicKey);
    Require(againKey && againKey == curveKey, "the same curve key is still reachable");
    Require(EC_KEY_check_key_nid_postfix(againKey) == 1, "the curve key is still coherent after the certificate is gone");
    EC_KEY_free_nid_postfix(againKey);
    EVP_PKEY_free_nid_postfix(publicKey);
    Require(mirror->references == 0, "the curve key is released with its last reference");
    RequireNoCodes("the elliptic curve sequence queues nothing");
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
        X509_free_nid_postfix(tolerant);
        RequireNoCodes("the certificate whose public key is cached is freed after the returned reference");
    }

    {
        const Bytes bits = Tlv(0x30, Cat({Tlv(0x02, {0x00, 0x01}), Tlv(0x02, {0x01, 0x03})}));
        auto* certificate = ReadCertificate(Record("CERTIFICATE", SignedCertificate(KeyInformation(DsaKey, Tlv(0x05, {}), bits), "Leaf")));
        Require(certificate, "a certificate with a DSA algorithm decodes");
        RequireThrows([&] { X509_get_pubkey_nid_postfix(certificate); }, "a DSA public key needs a key this library cannot hold");
        Require(!certificate->certificateInfo->key->key, "a refused DSA key is not cached");
        ERR_clear_error_nid_postfix();
        X509_free_nid_postfix(certificate);
        RequireNoCodes("the certificate whose DSA key was refused is freed");
    }

    struct EcFailure {
        Bytes parameter;
        Bytes bits;
        std::vector<std::uint64_t> codes;
    };
    const Bytes curve = Tlv(0x06, CurvePrime256);
    const Bytes shortPoint = Bytes(EcPoint.begin(), EcPoint.begin() + 64);
    Bytes offCurve = EcPoint;
    offCurve[1] = static_cast<unsigned char>(offCurve[1] ^ 0x01);
    const Bytes explicitCurves = Tlv(0x30, Cat({Tlv(0x06, CurvePrime256), Tlv(0x02, {0x01, 0x01})}));
    const Bytes compressedPoint = Cat({Bytes{0x03}, Bytes(EcPoint.begin() + 1, EcPoint.begin() + 33)});
    const std::array<EcFailure, 5> ecFailures{{
        {curve, shortPoint, {Code(EcLibrary, EcLibraryError), Code(EcLibrary, EcDecodeError), Code(X509Library, PublicKeyDecodeError)}},
        {curve, offCurve, {Code(EcLibrary, EcLibraryError), Code(EcLibrary, EcDecodeError), Code(X509Library, PublicKeyDecodeError)}},
        {Tlv(0x05, {}), EcPoint, {Code(EcLibrary, EcDecodeError), Code(EcLibrary, EcLibraryError), Code(X509Library, PublicKeyDecodeError)}},
        {Tlv(0x06, Bytes{0x2a}), EcPoint, {Code(EcLibrary, EcLibraryError), Code(X509Library, PublicKeyDecodeError)}},
        {explicitCurves, EcPoint, {}},
    }};
    for (const EcFailure& failure : ecFailures) {
        auto* certificate =
            ReadCertificate(Record("CERTIFICATE", SignedCertificate(KeyInformation(EcPublicKey, failure.parameter, failure.bits), "Leaf")));
        Require(certificate, "a certificate with these elliptic curve fields decodes");
        auto* const slot = certificate->certificateInfo->key;
        if (failure.codes.empty()) {
            RequireThrows([&] { X509_get_pubkey_nid_postfix(certificate); }, "elliptic curve parameters given as a sequence are not implemented");
        } else {
            Require(!X509_get_pubkey_nid_postfix(certificate), "these elliptic curve fields yield no public key");
            for (const std::uint64_t code : failure.codes)
                Require(ERR_get_error_nid_postfix() == code, "the elliptic curve reasons match the reference order");
            Require(ERR_get_error_nid_postfix() == 0, "a refused elliptic curve key queues nothing else");
        }
        Require(!slot->key, "a refused elliptic curve key is not cached");
        ERR_clear_error_nid_postfix();
        X509_free_nid_postfix(certificate);
        RequireNoCodes("the certificate with the refused elliptic curve key is freed");
    }

    {
        auto* certificate = ReadCertificate(Record("CERTIFICATE",
            SignedCertificate(KeyInformation(EcPublicKey, Tlv(0x06, Bytes{0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x02}), EcPoint), "Leaf")));
        Require(certificate, "a certificate on an elliptic curve mbedtls does not provide decodes");
        RequireThrows([&] { X509_get_pubkey_nid_postfix(certificate); }, "an elliptic curve mbedtls does not provide is not implemented");
        Require(!certificate->certificateInfo->key->key, "an unbuildable elliptic curve key is not cached");
        ERR_clear_error_nid_postfix();
        X509_free_nid_postfix(certificate);
        RequireNoCodes("the certificate with the unbuildable elliptic curve key is freed");
    }

    {
        auto* certificate =
            ReadCertificate(Record("CERTIFICATE", SignedCertificate(KeyInformation(EcPublicKey, Tlv(0x06, CurvePrime256), compressedPoint), "Leaf")));
        Require(certificate, "a certificate with a compressed elliptic curve point decodes");
        void* const publicKey = X509_get_pubkey_nid_postfix(certificate);
        Require(publicKey, "a compressed elliptic curve point decodes into a curve key");
        RequireNoCodes("the compressed elliptic curve point is decoded without errors");
        auto* const curveKey = EVP_PKEY_get1_EC_KEY_nid_postfix(publicKey);
        Require(curveKey, "the compressed curve key is handed out");
        Require(reinterpret_cast<CurveKey*>(curveKey)->conversionForm == PointConversionCompressed,
            "the conversion form comes from the compressed point");
        Require(EC_KEY_check_key_nid_postfix(curveKey) == 1, "the point recovered from the compressed form lies on the curve");
        EC_KEY_free_nid_postfix(curveKey);
        EVP_PKEY_free_nid_postfix(publicKey);
        X509_free_nid_postfix(certificate);
        RequireNoCodes("the certificate with the compressed elliptic curve point is freed");
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

    const std::string skippedThenLeaf = Record("RSA PRIVATE KEY", SignedCertificate(EcKey(), "Skipped")) +
        Record("CERTIFICATE", SignedCertificate(RsaKey(64, 0x28), "Decoded"));
    auto* afterSkipped = ReadCertificate(skippedThenLeaf);
    Require(afterSkipped, "a record under another label is skipped and the next record is decoded");
    RequireNoCodes("skipping a record under another label queues nothing");
    Require(afterSkipped && Holds(afterSkipped->certificateInfo->key->algorithm->algorithm, RsaEncryption),
        "the record that is decoded is the one under the CERTIFICATE label");
    X509_free_nid_postfix(afterSkipped);

    const std::string skippedThenTrusted = Record("RSA PRIVATE KEY", SignedCertificate(EcKey(), "Skipped")) +
        Trusted(SignedCertificate(RsaKey(64, 0x29), "Trusted"), AuxiliaryAllFields());
    auto* trustedAfterSkipped = ReadTrusted(skippedThenTrusted);
    Require(trustedAfterSkipped, "a trusted read skips a record under another label");
    RequireNoCodes("the trusted read that skipped a record queues nothing");
    Require(trustedAfterSkipped && trustedAfterSkipped->auxiliary, "the trusted certificate keeps its auxiliary data");
    X509_free_nid_postfix(trustedAfterSkipped);

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
    X509_free_nid_postfix(replaced);
    RequireNoCodes("the trusted certificate that replaced the caller's certificate is freed");

    auto* kept = ReadCertificate(EcCertificatePem);
    auto* const keptBefore = kept;
    RequireThrows([&] { ReadTrustedWith(&kept, Trusted(leaf, AuxiliaryOther())); },
        "a trusted read that reaches the unimplemented field throws");
    Require(kept == keptBefore, "the thrown trusted read leaves the caller's certificate alone");
    ERR_clear_error_nid_postfix();
    Require(kept->certificateInfo && kept->auxiliary == nullptr, "the surviving certificate keeps its own fields");
    X509_free_nid_postfix(kept);
    RequireNoCodes("the certificate that survived the thrown trusted read is freed");
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
    X509_free_nid_postfix(simple);
    RequireNoCodes("the auxiliary trust stack and the certificate are freed together");

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
    X509_free_nid_postfix(full);
    RequireNoCodes("the trust and reject stacks, the alias and the certificate are freed together");

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

    const std::string leafDer = Record("CERTIFICATE", SignedCertificate(RsaKey(64, 0x21), "Leaf"));
    const std::string leafDerAgain = Record("CERTIFICATE", SignedCertificate(RsaKey(64, 0x22), "Leaf"));
    const std::string otherDer = Record("CERTIFICATE", SignedCertificate(RsaKey(64, 0x23), "Other"));
    WriteFile(files.Host("undecodable-after-good.pem"), std::string(RsaCertificatePem) + Record("CERTIFICATE", Bytes{0x10, 0x00}));
    WriteFile(files.Host("same-subject-twice.pem"), leafDer + leafDerAgain);
    WriteFile(files.Host("other.pem"), otherDer);
    WriteFile(files.Host("other-then-single.pem"), otherDer + std::string(RsaCertificatePem));
    WriteFile(files.Host("crl-then-certificate.pem"), std::string(CertificateListPem) + RsaCertificatePem);
    WriteFile(files.Host("key-then-certificate.pem"), Record("RSA PRIVATE KEY", SignedCertificate(RsaKey(64, 0x24), "Leaf")) + RsaCertificatePem);
    WriteFile(files.Host("certificate-then-key.pem"), std::string(RsaCertificatePem) +
        Record("EC PRIVATE KEY", SignedCertificate(RsaKey(64, 0x25), "Leaf")));
    WriteFile(files.Host("trusted-bundle.pem"),
        Record("TRUSTED CERTIFICATE", Cat({SignedCertificate(RsaKey(64, 0x26), "Trust"), AuxiliaryOneTrust()})) + leafDer);

    {
        StoreFixture store("a store for the undecodable record");
        Require(store.Load(files.Path("undecodable-after-good.pem")) == 0, "a file whose second record cannot be decoded is refused");
        RequireCodes({Code(Asn1Library, SequenceNotConstructed), Code(PemLibrary, Asn1LibraryError), Code(X509Library, PemLibraryFailure)},
            "the undecodable record queues the sequence, PEM and X509 reasons in the reference order");
        Require(StackCount(store.Objects()) == 0, "the decodable record of a refused file is not stored");
        Require(StackCount(store.Lookups()) == 1, "the refused file still adds one lookup");
        Require(store.Load(files.Path("other.pem")) == 1, "the store is still usable after a refused file");
        RequireNoCodes("the load after the refused file queues nothing");
        Require(StackCount(store.Objects()) == 1, "the store reached by the second load holds one certificate");
    }

    {
        StoreFixture store("a store for the duplicate subject in one file");
        RequireThrows([&] { store.Load(files.Path("same-subject-twice.pem")); },
            "two certificates of the same subject in one file reach the store guard");
        ERR_clear_error_nid_postfix();
        Require(StackCount(store.Objects()) == 1, "the certificate stored before the guard stays in the store");
        Require(store.Load(files.Path("single.pem")) == 1, "loading works again after the guard threw");
        RequireNoCodes("the load that follows the guard queues nothing");
        Require(StackCount(store.Objects()) == 2, "both certificates stay in the store");
    }

    {
        StoreFixture store("a store for the duplicate after an insertion");
        Require(store.Load(files.Path("single.pem")) == 1, "the store already holds one subject");
        RequireThrows([&] { store.Load(files.Path("other-then-single.pem")); },
            "the second record of a file duplicates a subject the store already holds");
        ERR_clear_error_nid_postfix();
        Require(StackCount(store.Objects()) == 2, "the record read before the guard stays in the store");
        auto* added = StoredCertificate(store.Objects(), 1);
        Require(added && Holds(added->certificateInfo->key->algorithm->algorithm, RsaEncryption), "the stored record is the RSA one");
        Require(added->references == 1, "only the store holds the certificate once the read list is released");
    }

    {
        StoreFixture store("a store for the revocation list first");
        RequireThrows([&] { store.Load(files.Path("crl-then-certificate.pem")); }, "a revocation list before a certificate is not read yet");
        ERR_clear_error_nid_postfix();
        Require(StackCount(store.Objects()) == 0, "the certificate after the revocation list is not stored");
    }

    {
        StoreFixture store("a store for the private key first");
        RequireThrows([&] { store.Load(files.Path("key-then-certificate.pem")); }, "a private key before a certificate is not read yet");
        ERR_clear_error_nid_postfix();
        Require(StackCount(store.Objects()) == 0, "the certificate after the private key is not stored");
    }

    {
        StoreFixture store("a store for the private key after a certificate");
        RequireThrows([&] { store.Load(files.Path("certificate-then-key.pem")); }, "a private key after a certificate is not read yet");
        ERR_clear_error_nid_postfix();
        Require(StackCount(store.Objects()) == 0, "a file with a private key stores nothing, as the reference builds the whole list first");
    }

    {
        StoreFixture store("a store for the trusted bundle");
        Require(store.Load(files.Path("trusted-bundle.pem")) == 1, "a trusted certificate and a plain one are both loaded");
        RequireNoCodes("the trusted bundle queues nothing");
        Require(StackCount(store.Objects()) == 2, "both bundle records reach the store");
        auto* const first = StoredCertificate(store.Objects(), 0);
        auto* const second = StoredCertificate(store.Objects(), 1);
        Require(first && second, "both stored objects are certificates");
        const bool firstTrusted = first->auxiliary != nullptr;
        Require(firstTrusted != (second->auxiliary != nullptr), "exactly one stored certificate keeps its auxiliary data");
        const auto* trusted = firstTrusted ? first : second;
        Require(StackCount(Auxiliary(trusted)->trust) == 1, "the stored trust object is kept");
        store.Close();
        RequireNoCodes("the store with the trusted certificate is freed without leftovers");
    }

    {
        const int descriptors = OpenDescriptors();
        for (int attempt = 0; attempt < 8; ++attempt) {
            StoreFixture store("a store for one more read of the same file");
            Require(store.Load(files.Path("single.pem")) == 1, "a file that loaded once loads again");
        }
        Require(OpenDescriptors() == descriptors, "a loaded certificate file leaves no descriptor behind");
    }

    {
        const int descriptors = OpenDescriptors();
        for (int attempt = 0; attempt < 8; ++attempt) {
            StoreFixture store("a store for one more thrown read");
            RequireThrows([&] { store.Load(files.Path("crl-then-certificate.pem")); }, "the revocation list throws on every read");
            ERR_clear_error_nid_postfix();
        }
        Require(OpenDescriptors() == descriptors, "a file read that threw leaves no descriptor behind");
    }

    {
        const int descriptors = OpenDescriptors();
        for (int attempt = 0; attempt < 8; ++attempt) {
            StoreFixture store("a store for one more refused file");
            Require(store.Load(files.Path("undecodable-after-good.pem")) == 0, "a refused file is refused again");
            ERR_clear_error_nid_postfix();
        }
        Require(OpenDescriptors() == descriptors, "a refused file leaves no descriptor behind");
    }
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

std::size_t OutstandingSince(std::uint64_t baseline);
extern std::uint64_t generation;

void ClientCa() {
    const std::uint64_t baseline = generation;
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
    Require(OutstandingSince(baseline) > 0,
        "the authority stack and its name copies stay allocated until the context release of the client CA list exists");
}

constexpr std::size_t TrackerSlots = 65536;

void* trackedPointer[TrackerSlots];
std::size_t trackedSize[TrackerSlots];
std::uint64_t trackedGeneration[TrackerSlots];
const char* trackedOrigin[TrackerSlots];
std::size_t trackedCount = 0;
std::uint64_t generation = 0;
std::size_t keyShapeAllocations = 0;
std::mutex trackerMutex;
int failAt = 0;
int calls = 0;

void* Track(void* pointer, std::size_t size, const char* origin) {
    if (!pointer) return nullptr;
    const std::lock_guard guard(trackerMutex);
    Require(trackedCount < TrackerSlots, "the tracker has room for every live allocation");
    ++generation;
    trackedPointer[trackedCount] = pointer;
    trackedSize[trackedCount] = size;
    trackedGeneration[trackedCount] = generation;
    trackedOrigin[trackedCount] = origin;
    ++trackedCount;
    if (size == sizeof(Pkey)) ++keyShapeAllocations;
    return pointer;
}

void Untrack(void* pointer) {
    if (!pointer) return;
    const std::lock_guard guard(trackerMutex);
    for (std::size_t index = 0; index < trackedCount; ++index) {
        if (trackedPointer[index] != pointer) continue;
        --trackedCount;
        trackedPointer[index] = trackedPointer[trackedCount];
        trackedSize[index] = trackedSize[trackedCount];
        trackedGeneration[index] = trackedGeneration[trackedCount];
        trackedOrigin[index] = trackedOrigin[trackedCount];
        return;
    }
    Require(false, "only live allocations are freed");
}

std::size_t OutstandingSince(std::uint64_t baseline) {
    const std::lock_guard guard(trackerMutex);
    std::size_t outstanding = 0;
    for (std::size_t index = 0; index < trackedCount; ++index)
        if (trackedGeneration[index] > baseline) ++outstanding;
    return outstanding;
}

std::size_t OutstandingSince(std::uint64_t baseline, std::size_t size) {
    const std::lock_guard guard(trackerMutex);
    std::size_t outstanding = 0;
    for (std::size_t index = 0; index < trackedCount; ++index)
        if (trackedGeneration[index] > baseline && trackedSize[index] == size) ++outstanding;
    return outstanding;
}

void ReportOutstanding(std::uint64_t baseline, int injection, const char* message) {
    {
        const std::lock_guard guard(trackerMutex);
        for (std::size_t index = 0; index < trackedCount; ++index)
            if (trackedGeneration[index] > baseline)
                std::fprintf(stderr, "%s: allocation %d is still live origin=%s size=%zu\n", message, injection,
                    trackedOrigin[index], trackedSize[index]);
    }
    std::fflush(stderr);
    Require(false, message);
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    if (++calls == failAt) return nullptr;
    return Track(std::malloc(size), size, "malloc");
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) {
    if (++calls == failAt) return nullptr;
    return Track(std::calloc(count, size), count * size, "calloc");
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    Untrack(pointer);
    std::free(pointer);
}

void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) {
    if (++calls == failAt) return nullptr;
    Untrack(pointer);
    return Track(std::realloc(pointer, size), size, "realloc");
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

struct SweepOutcome {
    bool thrown;
    bool returned;
};

struct InjectionResult {
    SweepOutcome outcome;
    std::uint64_t baseline;
    bool exhausted;
};

std::size_t KeyShapeAllocations() {
    const std::lock_guard guard(trackerMutex);
    return keyShapeAllocations;
}

template<typename TInject, typename TFinal>
void SweepAllocations(TInject inject, TFinal finalExpectation, int minimumChecked, const char* message) {
    for (int failing = 1;; ++failing)
        if (inject(failing).exhausted) break;
    failAt = 0;
    int failures = 0;
    for (int failing = 1;; ++failing) {
        const InjectionResult result = inject(failing);
        if (OutstandingSince(result.baseline) != 0) ReportOutstanding(result.baseline, failing, message);
        if (result.exhausted) {
            finalExpectation(result.outcome);
            break;
        }
        ++failures;
    }
    failAt = 0;
    Require(failures > minimumChecked, message);
}

void Sweep(const std::string& record, bool trusted, bool succeeds) {
    const auto inject = [&](int failing) -> InjectionResult {
        ERR_clear_error_nid_postfix();
        calls = 0;
        failAt = failing;
        const std::uint64_t baseline = generation;
        LibreSslX509* certificate = nullptr;
        bool thrown = false;
        auto* bio = BIO_new_mem_buf_nid_postfix(record.data(), static_cast<int>(record.size()));
        try {
            if (bio)
                certificate = trusted ? PEM_read_bio_X509_AUX_nid_postfix(bio, nullptr, nullptr, nullptr)
                                      : PEM_read_bio_X509_nid_postfix(bio, nullptr, nullptr, nullptr);
        } catch (...) {
            thrown = true;
        }
        if (bio) BIO_free_nid_postfix(bio);
        const bool returned = certificate != nullptr;
        if (certificate) X509_free_nid_postfix(certificate);
        Require(succeeds || !returned, "a record that cannot be kept is never returned");
        Require(thrown || returned || ERR_peek_error_nid_postfix() != 0, "a read that returns nothing queues an error");
        return InjectionResult{SweepOutcome{thrown, returned}, baseline, calls < failing};
    };
    SweepAllocations(inject, [&](const SweepOutcome& outcome) {
        Require(outcome.returned == succeeds, "the run without a failing allocation behaves as the fixed run");
    }, 10, "the reading of one PEM record releases every allocation");
}

void SweepKey(const std::string& record, bool expectsKey, bool expectsThrow, const char* message) {
    const auto inject = [&](int failing) -> InjectionResult {
        ERR_clear_error_nid_postfix();
        calls = 0;
        failAt = failing;
        const std::uint64_t baseline = generation;
        LibreSslX509* certificate = nullptr;
        void* publicKey = nullptr;
        bool thrown = false;
        auto* bio = BIO_new_mem_buf_nid_postfix(record.data(), static_cast<int>(record.size()));
        try {
            if (bio) certificate = PEM_read_bio_X509_nid_postfix(bio, nullptr, nullptr, nullptr);
            if (certificate) publicKey = X509_get_pubkey_nid_postfix(certificate);
        } catch (...) {
            thrown = true;
        }
        if (bio) BIO_free_nid_postfix(bio);
        const bool returned = publicKey != nullptr;
        if (publicKey) EVP_PKEY_free_nid_postfix(publicKey);
        if (certificate) X509_free_nid_postfix(certificate);
        Require(expectsThrow || !thrown, message);
        Require(thrown || returned || ERR_peek_error_nid_postfix() != 0, message);
        return InjectionResult{SweepOutcome{thrown, returned}, baseline, calls < failing};
    };
    SweepAllocations(inject, [&](const SweepOutcome& outcome) {
        Require(outcome.thrown == expectsThrow, message);
        Require(outcome.returned == expectsKey, message);
    }, 5, message);
}

void SweepTrusted(const std::string& record, bool expectsThrow, const char* message) {
    const auto inject = [&](int failing) -> InjectionResult {
        ERR_clear_error_nid_postfix();
        calls = 0;
        failAt = failing;
        const std::uint64_t baseline = generation;
        LibreSslX509* certificate = nullptr;
        bool thrown = false;
        auto* bio = BIO_new_mem_buf_nid_postfix(record.data(), static_cast<int>(record.size()));
        try {
            if (bio) certificate = PEM_read_bio_X509_AUX_nid_postfix(bio, nullptr, nullptr, nullptr);
        } catch (...) {
            thrown = true;
        }
        if (bio) BIO_free_nid_postfix(bio);
        const bool returned = certificate != nullptr;
        if (certificate) X509_free_nid_postfix(certificate);
        Require(expectsThrow || !thrown, message);
        Require(thrown || returned || ERR_peek_error_nid_postfix() != 0, message);
        return InjectionResult{SweepOutcome{thrown, returned}, baseline, calls < failing};
    };
    SweepAllocations(inject, [&](const SweepOutcome& outcome) {
        Require(outcome.thrown == expectsThrow, message);
        Require(outcome.returned == !expectsThrow, message);
    }, 5, message);
}

void SweepStoreLoad(const std::string& path, bool expectsThrow, const char* message) {
    const auto inject = [&](int failing) -> InjectionResult {
        ERR_clear_error_nid_postfix();
        StoreFixture store(message);
        calls = 0;
        failAt = failing;
        const std::uint64_t baseline = generation;
        bool thrown = false;
        int loaded = 0;
        try {
            loaded = store.Load(path);
        } catch (...) {
            thrown = true;
        }
        Require(loaded >= 0, message);
        Require(expectsThrow || !thrown, message);
        return InjectionResult{SweepOutcome{thrown, loaded > 0}, baseline, calls < failing};
    };
    SweepAllocations(inject, [&](const SweepOutcome& outcome) {
        Require(outcome.thrown == expectsThrow, message);
    }, 5, message);
}

void UseTrackingHeap() {
    static std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch), reinterpret_cast<void*>(Calloc_nid_no_patch),
        reinterpret_cast<void*>(Reallocate_nid_no_patch), reinterpret_cast<void*>(Align_nid_no_patch),
        reinterpret_cast<void*>(Realign_nid_no_patch), reinterpret_cast<void*>(PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
}

void Allocator() {
    UseTrackingHeap();
    const Bytes leaf = SignedCertificate(RsaKey(64, 0x08), "Leaf");
    Sweep(Record("CERTIFICATE", leaf), false, true);
    Sweep(Trusted(leaf, AuxiliaryInvalidObject()), true, false);
    Sweep(Record("RSA PRIVATE KEY", SignedCertificate(EcKey(), "Skipped")) + Record("CERTIFICATE", leaf), false, true);
    Sweep(Record("RSA PRIVATE KEY", SignedCertificate(EcKey(), "Skipped")) + Trusted(leaf, AuxiliaryAllFields()), true, true);
    SweepKey(Record("CERTIFICATE", SignedCertificate(RsaKey(64, 0x09), "Leaf")), true, false,
        "a decoded RSA public key and its certificate leave nothing behind");
    SweepKey(Record("CERTIFICATE", SignedCertificate(EcKey(), "Leaf")), true, false,
        "a decoded elliptic curve public key and its certificate leave nothing behind");
    SweepKey(Record("CERTIFICATE", SignedCertificate(KeyInformation(EcPublicKey, Tlv(0x05, {}), EcPoint), "Leaf")), false, false,
        "a refused elliptic curve key leaves nothing behind");
    SweepKey(Record("CERTIFICATE", SignedCertificate(KeyInformation(EcPublicKey, Tlv(0x30, Cat({Tlv(0x06, CurvePrime256), Tlv(0x02, {0x01, 0x01})})), EcPoint), "Leaf")),
        false, true, "elliptic curve parameters given as a sequence leave nothing behind");
    SweepKey(Record("CERTIFICATE", SignedCertificate(KeyInformation(DsaKey, Tlv(0x05, {}),
        Tlv(0x30, Cat({Tlv(0x02, {0x00, 0x01}), Tlv(0x02, {0x01, 0x03})}))), "Leaf")), false, true, "a refused DSA key leaves nothing behind");
    SweepKey(Record("CERTIFICATE", SignedCertificate(KeyInformation(EcPublicKey,
        Tlv(0x06, Bytes{0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x02}), EcPoint), "Leaf")), false, true,
        "an elliptic curve the library cannot build releases the curve key it created");
    SweepKey(Record("CERTIFICATE", SignedCertificate(
        KeyInformation(EcPublicKey, Tlv(0x06, CurvePrime256), Cat({Bytes{0x03}, Bytes(EcPoint.begin() + 1, EcPoint.begin() + 33)})), "Leaf")),
        true, false, "a compressed elliptic curve point releases every allocation it made");
    SweepTrusted(Trusted(leaf, AuxiliaryAllFields()), false, "the auxiliary fields and the trusted certificate are freed together");
    SweepTrusted(Trusted(leaf, AuxiliaryOther()), true, "auxiliary information that is not implemented leaves nothing behind");

    const GuestFiles files;
    WriteFile(files.Host("sweep-single.pem"), RsaCertificatePem);
    WriteFile(files.Host("sweep-same-subject.pem"),
        Record("CERTIFICATE", SignedCertificate(RsaKey(64, 0x21), "Leaf")) + Record("CERTIFICATE", SignedCertificate(RsaKey(64, 0x22), "Leaf")));
    WriteFile(files.Host("sweep-trusted.pem"),
        Record("TRUSTED CERTIFICATE", Cat({SignedCertificate(RsaKey(64, 0x26), "Trust"), AuxiliaryOneTrust()})) +
            Record("CERTIFICATE", SignedCertificate(RsaKey(64, 0x27), "Leaf")));
    WriteFile(files.Host("sweep-crl.pem"), std::string(RsaCertificatePem) + CertificateListPem);
    WriteFile(files.Host("sweep-key-after.pem"),
        std::string(RsaCertificatePem) + Record("RSA PRIVATE KEY", SignedCertificate(RsaKey(64, 0x24), "Leaf")));
    WriteFile(files.Host("sweep-broken-after-good.pem"), std::string(RsaCertificatePem) + Record("CERTIFICATE", Bytes{0x10, 0x00}));
    const std::string sweepOne = Record("CERTIFICATE", SignedCertificate(RsaKey(64, 0x28), "One"));
    const std::string sweepTwo = Record("CERTIFICATE", SignedCertificate(RsaKey(64, 0x29), "Two"));
    const std::string sweepThree = Record("CERTIFICATE", SignedCertificate(RsaKey(64, 0x2a), "Three"));
    const std::string sweepFour = Record("CERTIFICATE", SignedCertificate(RsaKey(64, 0x2b), "Four"));
    WriteFile(files.Host("sweep-bundle4.pem"), sweepOne + sweepTwo + sweepThree + sweepFour);
    WriteFile(files.Host("sweep-three-with-duplicate.pem"),
        sweepOne + sweepTwo + Record("CERTIFICATE", SignedCertificate(RsaKey(64, 0x2c), "One")));
    WriteFile(files.Host("sweep-three-then-encrypted.pem"), sweepOne + sweepTwo + sweepThree +
        RecordWithHeader("CERTIFICATE", "Proc-Type: 4,ENCRYPTED\nDEK-Info: AES-128-CBC,00112233445566778899AABBCCDDEEFF", Bytes{0x30, 0x00}));
    SweepStoreLoad(files.Path("sweep-single.pem"), false, "the store built from one file releases the certificate and the file");
    SweepStoreLoad(files.Path("sweep-same-subject.pem"), true, "the store guard of a duplicate subject releases the list and the read certificate");
    SweepStoreLoad(files.Path("sweep-trusted.pem"), false, "the store guard frees the auxiliary data of the trusted certificate");
    SweepStoreLoad(files.Path("sweep-crl.pem"), true, "a revocation list in the file releases the list built before it");
    SweepStoreLoad(files.Path("sweep-key-after.pem"), true, "a private key after a certificate releases the certificate read before it");
    SweepStoreLoad(files.Path("sweep-broken-after-good.pem"), false, "a record that cannot be decoded releases the record read before it");
    SweepStoreLoad(files.Path("sweep-bundle4.pem"), false, "a four-certificate bundle grows and releases its info stack cleanly");
    SweepStoreLoad(files.Path("sweep-three-with-duplicate.pem"), true, "a duplicate third record frees every item in the info stack");
    SweepStoreLoad(files.Path("sweep-three-then-encrypted.pem"), true, "an encrypted fourth record frees the preceding info stack cleanly");
}

void ConcurrentCache() {
    constexpr int Workers = 4;
    auto run = [&](const char* pem, int rounds, bool requireRaceAndPkeyShape) {
        int roundsWithSeveralCandidates = 0;
        for (int round = 0; round < rounds; ++round) {
            auto* certificate = ReadCertificate(pem);
            Require(certificate && certificate->certificateInfo && certificate->certificateInfo->key, "the concurrent certificate decodes");
            std::array<void*, Workers> returned{};
            std::array<std::thread, Workers> workers{};
            std::barrier start(Workers);
            const std::uint64_t baseline = generation;
            const std::size_t candidatesBefore = KeyShapeAllocations();
            for (int index = 0; index < Workers; ++index)
                workers[index] = std::thread([&, index] {
                    start.arrive_and_wait();
                    returned[index] = X509_get_pubkey_nid_postfix(certificate);
                });
            for (auto& worker : workers)
                worker.join();
            if (KeyShapeAllocations() - candidatesBefore > 1) ++roundsWithSeveralCandidates;
            void* shared = nullptr;
            for (void* pointer : returned) {
                Require(pointer, "every worker returns a public key");
                if (!shared) shared = pointer;
                Require(pointer == shared, "every worker reaches the same cached public key");
            }
            RequireNoCodes("decoding the public key concurrently queues nothing");
            Require(static_cast<Pkey*>(shared)->references == Workers + 1, "the cache and every worker hold the public key");
            if (requireRaceAndPkeyShape) Require(OutstandingSince(baseline, sizeof(Pkey)) == 1, "the losing candidate of the race is released");
            for (void* pointer : returned)
                EVP_PKEY_free_nid_postfix(pointer);
            X509_free_nid_postfix(certificate);
            Require(OutstandingSince(baseline) == 0, "the concurrent cache and the certificate release every allocation");
            RequireNoCodes("the concurrent sequence queues nothing");
        }
        if (requireRaceAndPkeyShape) Require(roundsWithSeveralCandidates > 0, "the concurrent readers really create more than one candidate");
    };
    run(RsaCertificatePem, 40, true);
    run(EcCertificatePem, 16, false);
}

void StoreInfoReconciliation() {
    {
        const std::uint64_t baseline = generation;
        auto* certificate = ReadTrusted(TrustedCertificatePem);
        Require(certificate && certificate->auxiliary, "a trusted certificate is created for X509_INFO_free");
        auto* info = static_cast<LibreSslX509Info*>(calloc_nid_postfix(1, sizeof(LibreSslX509Info)));
        Require(info, "a LibreSslX509Info structure is allocated");
        info->certificate = certificate;
        info->references = 1;
        info->encryptedLength = 4;
        info->encryptedData = static_cast<char*>(malloc_nid_postfix(4));
        Require(info->encryptedData, "encrypted data buffer is allocated");
        int marker = 0;
        info->crl = &marker;
        RequireThrows([&] { X509_INFO_free_nid_postfix(info); }, "X509_INFO_free refuses a held CRL on the last reference");
        Require(info->references == 1 && certificate->references == 1 && OutstandingSince(baseline) > 0,
            "a thrown CRL check leaves references and allocations untouched");
        info->crl = nullptr;
        info->privateKey = &marker;
        RequireThrows([&] { X509_INFO_free_nid_postfix(info); }, "X509_INFO_free refuses a held X509_PKEY on the last reference");
        Require(info->references == 1 && certificate->references == 1 && OutstandingSince(baseline) > 0,
            "a thrown X509_PKEY check leaves references and allocations untouched");
        info->privateKey = nullptr;
        X509_INFO_free_nid_postfix(info);
        Require(OutstandingSince(baseline) == 0, "X509_INFO_free releases the trusted certificate, encrypted data and info");
        RequireNoCodes("X509_INFO_free cleanup queues nothing");
    }

    const GuestFiles files;
    const std::string certOne = Record("CERTIFICATE", SignedCertificate(RsaKey(64, 0x31), "One"));
    const std::string certTwo = Record("CERTIFICATE", SignedCertificate(RsaKey(64, 0x32), "Two"));
    const std::string certThree = Record("CERTIFICATE", SignedCertificate(RsaKey(64, 0x33), "Three"));
    const std::string certFour = Record("CERTIFICATE", SignedCertificate(RsaKey(64, 0x34), "Four"));
    const std::string certOneDup = Record("CERTIFICATE", SignedCertificate(RsaKey(64, 0x35), "One"));
    WriteFile(files.Host("bundle4.pem"), certOne + certTwo + certThree + certFour);
    WriteFile(files.Host("three-then-crl.pem"), certOne + certTwo + certThree + CertificateListPem);
    WriteFile(files.Host("three-then-encrypted.pem"), certOne + certTwo + certThree +
        RecordWithHeader("CERTIFICATE", "Proc-Type: 4,ENCRYPTED\nDEK-Info: AES-128-CBC,00112233445566778899AABBCCDDEEFF", Bytes{0x30, 0x00}));
    WriteFile(files.Host("three-then-broken.pem"), certOne + certTwo + certThree + Record("CERTIFICATE", Bytes{0x10, 0x00}));
    WriteFile(files.Host("three-with-dup.pem"), certOne + certTwo + certOneDup);

    {
        const std::uint64_t baseline = generation;
        StoreFixture store("a store for a four-certificate bundle");
        Require(store.Load(files.Path("bundle4.pem")) == 1, "a four-certificate bundle loads past initial stack capacity");
        RequireNoCodes("loading a four-certificate bundle queues nothing");
        Require(StackCount(store.Objects()) == 4, "all four certificates reach the store");
        for (int index = 0; index < 4; ++index) {
            auto* stored = StoredCertificate(store.Objects(), index);
            Require(stored && stored->references == 1, "the store owns each certificate from the grown stack");
        }
        store.Close();
        Require(OutstandingSince(baseline) == 0, "the four-certificate bundle releases every info, stack and store allocation");
    }

    {
        const std::uint64_t baseline = generation;
        StoreFixture store("a store for three certs before a CRL");
        RequireThrows([&] { store.Load(files.Path("three-then-crl.pem")); }, "a CRL after three certificates throws");
        ERR_clear_error_nid_postfix();
        Require(StackCount(store.Objects()) == 0, "a CRL failure leaves the store empty per by_file.c");
        store.Close();
        Require(OutstandingSince(baseline) == 0, "a CRL after three certificates releases every allocation");
    }

    {
        const std::uint64_t baseline = generation;
        StoreFixture store("a store for three certs before an encrypted record");
        RequireThrows([&] { store.Load(files.Path("three-then-encrypted.pem")); }, "an encrypted record after three certificates throws");
        ERR_clear_error_nid_postfix();
        Require(StackCount(store.Objects()) == 0, "an encrypted record leaves the store empty per by_file.c");
        store.Close();
        Require(OutstandingSince(baseline) == 0, "an encrypted record after three certificates releases every allocation");
    }

    {
        const std::uint64_t baseline = generation;
        StoreFixture store("a store for three certs before a malformed record");
        Require(store.Load(files.Path("three-then-broken.pem")) == 0, "a malformed fourth record refuses the file");
        RequireCodes({Code(Asn1Library, SequenceNotConstructed), Code(PemLibrary, Asn1LibraryError), Code(X509Library, PemLibraryFailure)},
            "a malformed fourth record queues the reference reasons");
        Require(StackCount(store.Objects()) == 0, "a malformed fourth record leaves the store empty per by_file.c");
        store.Close();
        Require(OutstandingSince(baseline) == 0, "a malformed fourth record releases every allocation");
    }

    {
        const std::uint64_t baseline = generation;
        StoreFixture store("a store for a duplicate third certificate");
        RequireThrows([&] { store.Load(files.Path("three-with-dup.pem")); }, "a duplicate third subject reaches the store guard");
        ERR_clear_error_nid_postfix();
        Require(StackCount(store.Objects()) == 2, "the two distinct certificates inserted before the duplicate stay in the store");
        for (int index = 0; index < 2; ++index) {
            auto* stored = StoredCertificate(store.Objects(), index);
            Require(stored && stored->references == 1, "each kept certificate keeps a single store reference");
        }
        store.Close();
        Require(OutstandingSince(baseline) == 0, "the duplicate-subject unwind releases the uninserted cert, info items and stack");
    }
}

}  // namespace

void* operator new(std::size_t size) { return Track(std::malloc(size), size, "new"); }
void* operator new[](std::size_t size) { return Track(std::malloc(size), size, "new"); }
void operator delete(void* pointer) noexcept { Untrack(pointer); std::free(pointer); }
void operator delete[](void* pointer) noexcept { Untrack(pointer); std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { Untrack(pointer); std::free(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { Untrack(pointer); std::free(pointer); }

int main(int argc, char**) {
    if (argc > 1) {
        Allocator();
        return 0;
    }
    UseTrackingHeap();
    ERR_clear_error_nid_postfix();
    PublicKeys();
    EcCertificate();
    PubkeyErrors();
    ConcurrentCache();
    PemRecords();
    PemHeaders();
    PemFile();
    PemReuse();
    TrustedCertificates();
    TrustedFields();
    TrustedErrors();
    StoreLocations();
    StoreInfoReconciliation();
    ClientCa();
    return 0;
}
