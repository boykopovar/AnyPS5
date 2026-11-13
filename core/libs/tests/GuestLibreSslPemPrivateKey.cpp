#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/FileStream.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <initializer_list>
#include <source_location>
#include <string>

namespace {

constexpr char RsaKey[] =
    "-----BEGIN RSA PRIVATE KEY-----\n"
    "MIIBPAIBAAJBANLAVsNYAwOloR0kh+ZkPXB3lVX6JLGayh8DxN0X5T14XJKqV0dy\n"
    "Rk11ypd1/tLNJ3P+pEf7JS656KNl9F1WE40CAwEAAQJBAJX8foL9cv+4c4YbBoDH\n"
    "3BXbdTNAF7cyUyBpfURgCiyI3hCnI+PSg67gM3Uj26kwlQWcxeyXfvyikVMK/fzP\n"
    "vj0CIQD3JJ03OexbpdmpVheo5bepVl2sQSb7J4F0n2A4pX1zNwIhANpN2m2kSniK\n"
    "tPQgBzg1uHOz2d4bXByGc6kOM8cw0FlbAiEAkuW2dN7++DKKP7amdYOlOQfWwc4w\n"
    "B1SiA9m/bawf9t0CIHwZlfqOUueDsqq1KmWB5IGh1FC3LP3qUQXh1P1/enHRAiEA\n"
    "m9zhglUBoQst4upwxOYfgar9ZmtI5HHu+pdzor6Lldg=\n"
    "-----END RSA PRIVATE KEY-----\n";

constexpr char Pkcs8Key[] =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIIBVgIBADANBgkqhkiG9w0BAQEFAASCAUAwggE8AgEAAkEA0sBWw1gDA6WhHSSH\n"
    "5mQ9cHeVVfoksZrKHwPE3RflPXhckqpXR3JGTXXKl3X+0s0nc/6kR/slLrnoo2X0\n"
    "XVYTjQIDAQABAkEAlfx+gv1y/7hzhhsGgMfcFdt1M0AXtzJTIGl9RGAKLIjeEKcj\n"
    "49KDruAzdSPbqTCVBZzF7Jd+/KKRUwr9/M++PQIhAPcknTc57Ful2alWF6jlt6lW\n"
    "XaxBJvsngXSfYDilfXM3AiEA2k3abaRKeIq09CAHODW4c7PZ3htcHIZzqQ4zxzDQ\n"
    "WVsCIQCS5bZ03v74Moo/tqZ1g6U5B9bBzjAHVKID2b9trB/23QIgfBmV+o5S54Oy\n"
    "qrUqZYHkgaHUULcs/epRBeHU/X96cdECIQCb3OGCVQGhCy3i6nDE5h+Bqv1ma0jk\n"
    "ce76l3OivouV2A==\n"
    "-----END PRIVATE KEY-----\n";

constexpr char UnknownPkcs8Key[] =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIIBVgIBADANBgkqhkiG9w0BAX8FAASCAUAwggE8AgEAAkEA0sBWw1gDA6WhHSSH\n"
    "5mQ9cHeVVfoksZrKHwPE3RflPXhckqpXR3JGTXXKl3X+0s0nc/6kR/slLrnoo2X0\n"
    "XVYTjQIDAQABAkEAlfx+gv1y/7hzhhsGgMfcFdt1M0AXtzJTIGl9RGAKLIjeEKcj\n"
    "49KDruAzdSPbqTCVBZzF7Jd+/KKRUwr9/M++PQIhAPcknTc57Ful2alWF6jlt6lW\n"
    "XaxBJvsngXSfYDilfXM3AiEA2k3abaRKeIq09CAHODW4c7PZ3htcHIZzqQ4zxzDQ\n"
    "WVsCIQCS5bZ03v74Moo/tqZ1g6U5B9bBzjAHVKID2b9trB/23QIgfBmV+o5S54Oy\n"
    "qrUqZYHkgaHUULcs/epRBeHU/X96cdECIQCb3OGCVQGhCy3i6nDE5h+Bqv1ma0jk\n"
    "ce76l3OivouV2A==\n"
    "-----END PRIVATE KEY-----\n";

constexpr char EncryptedRsaKey[] =
    "-----BEGIN RSA PRIVATE KEY-----\n"
    "Proc-Type: 4,ENCRYPTED\n"
    "DEK-Info: AES-128-CBC,5E85BDFE683369EB3F468C204E928729\n"
    "\n"
    "DNQVBD7Y15Ud/Rf00RN7FoWAeH1YxcBbwUo/OT7d/bIWsp4NckdRPxyT56g5Nq5v\n"
    "f9wRj/KalM9hOAryzdtwiP0pOQRreoyam+Fo6X0YCSCPlF2lYZ7prXYLGC5h4sIP\n"
    "0fszA45NfdZupWVnG9SMGXnxhuccY/LRHH9UP2KkQft1ToLQigyRp4BVgJK2+I30\n"
    "JhB6nqGuwaZSGGtyl/f5CPRenKaDeQF6IxVuZzifGBr2p/IJXI6opDIQqmx9HuDT\n"
    "DhA6GbZCSGDsPt7n0TdqKQavHVXqv+lf9omOGi2x/y9hWVpvNX0gc9CRN1V0Yf4N\n"
    "H23qSS27nms+hzECCKmO5rilHk20sVqtxqeLmZk3ojLufhIG8/+zUKrIf7a/MlmF\n"
    "RdzgyXBmvAHz4tteqV9cWH4Ymzz86CAaEqgKN3PJ9u1yO4e2K4AvloPR31CmvLHb\n"
    "-----END RSA PRIVATE KEY-----\n";

constexpr char EncryptedPkcs8Key[] =
    "-----BEGIN ENCRYPTED PRIVATE KEY-----\n"
    "MIIBxTBfBgkqhkiG9w0BBQ0wUjAxBgkqhkiG9w0BBQwwJAQQlJ/brjLdMh7Wnqr7\n"
    "fgcL8AICCAAwDAYIKoZIhvcNAgkFADAdBglghkgBZQMEAQIEEEE2FkR1Ee2mORRN\n"
    "VVhAX6sEggFg745xIBaAmv1m9Y2P2dhQsU1wvTwYxAbRQbgSKac/voNuzi7b0elY\n"
    "1h7RO6ibyNWQLqn0B8QmCt2rYczKqKSGGM1Pw+OZn5Hwgda6wL/Uuz4P3Buvlkbr\n"
    "eDoTbbURsok3MMCS5ZI0DBWu+XoJbPMFPVLTuV5ZxePgATeVjVdX2aW0fbqKSbp1\n"
    "mgKtBKGoo5+R/mnwEqMVkftAZ6m+8J8HSi/KLUERwPxTUw6zCPUc+S2raAAayK5f\n"
    "WYWEi3hVTWgD7OYTgkPp1rIfiOyRnKbAxismiKjHI/skOesIgGWWKnpRTbfw9t2+\n"
    "a4bKTlR88TdKYDNh0bGqp/ZhrumFdjkUoqPzP9GEgv34ksxALd2ffMnWAnn6avFE\n"
    "6VTAacCT0xGVvdjvU7p64pw75Kg1bGjpFNxsTu6j7ciWzJIXGbu55oAWoBHMcAIG\n"
    "TAxZByRP7+BtPAkMGrM5gR6WEOdabmVM3g==\n"
    "-----END ENCRYPTED PRIVATE KEY-----\n";

constexpr char EcKey[] =
    "-----BEGIN EC PRIVATE KEY-----\n"
    "MHcCAQEEIJ0uTzDKADEDYlQfHVb/RxDD4uPfMUZnVFdYXj9YzEwXoAoGCCqGSM49\n"
    "AwEHoUQDQgAErKFNU9WIlmT6ZSGVn+9BvcxRVqTOjzIMQFJTy19InKv+QMcg3Jwk\n"
    "zYirxNgg1RH/tyZaYYHioJhiZ2Wq8YAVKQ==\n"
    "-----END EC PRIVATE KEY-----\n";

constexpr char EcPkcs8Key[] =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgnS5PMMoAMQNiVB8d\n"
    "Vv9HEMPi498xRmdUV1heP1jMTBehRANCAASsoU1T1YiWZPplIZWf70G9zFFWpM6P\n"
    "MgxAUlPLX0icq/5AxyDcnCTNiKvE2CDVEf+3JlphgeKgmGJnZarxgBUp\n"
    "-----END PRIVATE KEY-----\n";

constexpr char Certificate[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBbzCCARmgAwIBAgIUCN9mscYw2Bnyw0a+JrAKTSCDHd0wDQYJKoZIhvcNAQEL\n"
    "BQAwDDEKMAgGA1UEAwwBdDAeFw0yNjExMTMxMjE1MTJaFw0yNjExMTQxMjE1MTJa\n"
    "MAwxCjAIBgNVBAMMAXQwXDANBgkqhkiG9w0BAQEFAANLADBIAkEA0sBWw1gDA6Wh\n"
    "HSSH5mQ9cHeVVfoksZrKHwPE3RflPXhckqpXR3JGTXXKl3X+0s0nc/6kR/slLrno\n"
    "o2X0XVYTjQIDAQABo1MwUTAdBgNVHQ4EFgQUow41oJsXWOPSZTKydDRUCB3KoXgw\n"
    "HwYDVR0jBBgwFoAUow41oJsXWOPSZTKydDRUCB3KoXgwDwYDVR0TAQH/BAUwAwEB\n"
    "/zANBgkqhkiG9w0BAQsFAANBAFIQmR3N3qe5yI16EzcHlE9GpiLkpHcIKqVkYXAN\n"
    "iWZjkQPvxYEluRZYTNj3YtAFoEkCOx0WACCzvNPG0YMQbbE=\n"
    "-----END CERTIFICATE-----\n";

constexpr const char* KeyFields[] {
    "D2C056C3580303A5A11D2487E6643D70779555FA24B19ACA1F03C4DD17E53D785C92AA574772464D75CA9775FED2CD2773FEA447FB252EB9E8A365F45D56138D",
    "010001",
    "95FC7E82FD72FFB873861B0680C7DC15DB75334017B7325320697D44600A2C88DE10A723E3D283AEE0337523DBA93095059CC5EC977EFCA291530AFDFCCFBE3D",
    "F7249D3739EC5BA5D9A95617A8E5B7A9565DAC4126FB2781749F6038A57D7337",
    "DA4DDA6DA44A788AB4F420073835B873B3D9DE1B5C1C8673A90E33C730D0595B",
    "92E5B674DEFEF8328A3FB6A67583A53907D6C1CE300754A203D9BF6DAC1FF6DD",
    "7C1995FA8E52E783B2AAB52A6581E481A1D450B72CFDEA5105E1D4FD7F7A71D1",
    "9BDCE1825501A10B2DE2EA70C4E61F81AAFD666B48E471EEFA9773A2BE8B95D8",
};
constexpr unsigned char Sha256Abc[] {
    0xBA, 0x78, 0x16, 0xBF, 0x8F, 0x01, 0xCF, 0xEA, 0x41, 0x41, 0x40, 0xDE, 0x5D, 0xAE, 0x22, 0x23,
    0xB0, 0x03, 0x61, 0xA3, 0x96, 0x17, 0x7A, 0x9C, 0xB4, 0x10, 0xFF, 0x61, 0xF2, 0x00, 0x15, 0xAD,
};
constexpr char Sha256AbcSignature[] =
    "C07F57EFC13A4CCBD6F9D7D2368573E621F731DAE336F51CC783B5A75EF7B7AFCEE214C194E40D0CF778A0D0D61C344D967DBF3BE1D1A059EC5A0EB6432D0EE9";
constexpr int DigestSha256 = 672;
constexpr int PkeyRsa = 6;

struct TestPkey {
    int type;
    int saveType;
    int references;
};

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL PEM private key check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::exception&) {
        return;
    }
    Require(false, location);
}

std::uint64_t Error(int library, int reason) {
    return static_cast<std::uint64_t>(library) << 24 | std::uint64_t{0xfff} << 12 | static_cast<std::uint64_t>(reason);
}

void RequireErrors(std::initializer_list<std::uint64_t> expected, std::source_location location = std::source_location::current()) {
    for (const auto code : expected) {
        const auto actual = ERR_get_error_nid_postfix();
        if (actual != code)
            std::fprintf(stderr, "error %llx, expected %llx\n", static_cast<unsigned long long>(actual), static_cast<unsigned long long>(code));
        Require(actual == code, location);
    }
    Require(ERR_get_error_nid_postfix() == 0, location);
}

std::string Replace(std::string text, const std::string& from, const std::string& to) {
    for (auto position = text.find(from); position != std::string::npos; position = text.find(from, position + to.size()))
        text.replace(position, from.size(), to);
    return text;
}

std::string Relabel(const std::string& text, const std::string& from, const std::string& to) {
    return Replace(Replace(text, "BEGIN " + from + "-", "BEGIN " + to + "-"), "END " + from + "-", "END " + to + "-");
}

std::string Hex(const unsigned char* bytes, int length) {
    constexpr char digits[] = "0123456789ABCDEF";
    std::string text;
    for (int index = 0; index < length; ++index) {
        text += digits[bytes[index] >> 4];
        text += digits[bytes[index] & 15];
    }
    return text;
}

std::string Hex(const void* number) {
    unsigned char bytes[128];
    return Hex(bytes, BN_bn2bin_nid_postfix(static_cast<const Bignum*>(number), bytes));
}

void RequireKey(LibreSslRsa* rsa, std::source_location location = std::source_location::current()) {
    Require(rsa != nullptr && rsa->version == 0, location);
    const void* fields[] {rsa->n, rsa->e, rsa->d, rsa->p, rsa->q, rsa->dmp1, rsa->dmq1, rsa->iqmp};
    for (int index = 0; index < 8; ++index) Require(Hex(fields[index]) == KeyFields[index], location);
    unsigned char signature[64];
    unsigned int length = 0;
    Require(RSA_sign_nid_postfix(DigestSha256, Sha256Abc, sizeof(Sha256Abc), signature, &length, rsa) == 1, location);
    Require(Hex(signature, static_cast<int>(length)) == Sha256AbcSignature, location);
    Require(RSA_verify_nid_postfix(DigestSha256, Sha256Abc, sizeof(Sha256Abc), signature, length, rsa) == 1, location);
}

void* ReadBio(const std::string& text, void** reuse = nullptr) {
    auto* bio = BIO_new_mem_buf_nid_postfix(text.data(), static_cast<int>(text.size()));
    Require(bio != nullptr);
    struct Guard {
        LibreSsl::Bio* bio;
        ~Guard() { BIO_free_nid_postfix(bio); }
    } guard{bio};
    return PEM_read_bio_PrivateKey_nid_postfix(bio, reuse, nullptr, nullptr);
}

LibreSslRsa* ReadFile(const std::string& text, LibreSslRsa** reuse = nullptr) {
    std::FILE* native = std::tmpfile();
    Require(native != nullptr);
    FileStream stream(native);
    struct Guard {
        FileStream* stream;
        ~Guard() { fclose_nid_postfix(stream); }
    } guard{&stream};
    Require(fwrite_nid_postfix(text.data(), 1, text.size(), &stream) == text.size());
    Require(fflush_nid_postfix(&stream) == 0 && fseek_nid_postfix(&stream, 0, SEEK_SET) == 0);
    return PEM_read_RSAPrivateKey_nid_postfix(&stream, reuse, nullptr, nullptr);
}

void RequireBioKey(const std::string& text, std::initializer_list<std::uint64_t> errors = {},
                   std::source_location location = std::source_location::current()) {
    ERR_clear_error_nid_postfix();
    void* key = ReadBio(text);
    Require(key != nullptr && EVP_PKEY_id_nid_postfix(key) == PkeyRsa, location);
    auto* rsa = EVP_PKEY_get1_RSA_nid_postfix(key);
    RequireKey(rsa, location);
    RequireErrors(errors, location);
    RSA_free_nid_postfix(rsa);
    EVP_PKEY_free_nid_postfix(key);
}

void RequireBioFailure(const std::string& text, std::initializer_list<std::uint64_t> errors,
                       std::source_location location = std::source_location::current()) {
    ERR_clear_error_nid_postfix();
    Require(ReadBio(text) == nullptr, location);
    RequireErrors(errors, location);
}

void TestKeys() {
    for (const std::string text : {RsaKey, Pkcs8Key}) {
        RequireBioKey(text);
        ERR_clear_error_nid_postfix();
        auto* rsa = ReadFile(text);
        RequireKey(rsa);
        RequireErrors({});
        RSA_free_nid_postfix(rsa);
    }
    RequireBioKey(std::string(Certificate) + RsaKey);
    RequireBioKey(Relabel(RsaKey, "RSA PRIVATE KEY", "DH PRIVATE KEY") + Pkcs8Key);
    RequireBioKey(Relabel(RsaKey, "RSA PRIVATE KEY", "rsa PRIVATE KEY"));
    RequireBioKey(Replace(RsaKey, "KEY-----\n", "KEY-----\n\n"));
    RequireBioKey(Relabel(Pkcs8Key, "PRIVATE KEY", "RSA PRIVATE KEY"), {Error(4, 4)});
}

void TestReuse() {
    auto* old = RSA_new_nid_postfix();
    void* pkey = EVP_PKEY_new_nid_postfix();
    Require(EVP_PKEY_set1_RSA_nid_postfix(pkey, old) == 1 && old->references == 2);
    void* out = pkey;
    ERR_clear_error_nid_postfix();
    Require(ReadBio(RsaKey, &out) == pkey && out == pkey && old->references == 1);
    auto* rsa = EVP_PKEY_get1_RSA_nid_postfix(pkey);
    RequireKey(rsa);
    RSA_free_nid_postfix(rsa);
    RequireErrors({});

    Require(EVP_PKEY_set1_RSA_nid_postfix(pkey, old) == 1 && old->references == 2);
    Require(ReadBio(Replace(RsaKey, "MIIBPAIBAAJBANLA", "AAAAAAAAAAAAAAAA"), &out) == nullptr && out == pkey && old->references == 1);
    RequireErrors({Error(4, 4), Error(9, 13)});

    static_cast<TestPkey*>(pkey)->references = 2;
    Require(ReadBio(Pkcs8Key, &out) == out && out != pkey && static_cast<TestPkey*>(pkey)->references == 1);
    rsa = EVP_PKEY_get1_RSA_nid_postfix(out);
    RequireKey(rsa);
    RSA_free_nid_postfix(rsa);
    EVP_PKEY_free_nid_postfix(out);

    static_cast<TestPkey*>(pkey)->references = 2;
    out = pkey;
    Require(ReadBio(UnknownPkcs8Key, &out) == nullptr && out == nullptr && static_cast<TestPkey*>(pkey)->references == 1);
    RequireErrors({Error(6, 156), Error(6, 118), Error(9, 13)});
    EVP_PKEY_free_nid_postfix(pkey);

    old->references = 2;
    LibreSslRsa* target = old;
    rsa = ReadFile(Pkcs8Key, &target);
    Require(rsa == target && rsa != old && old->references == 1);
    RequireKey(rsa);
    RSA_free_nid_postfix(rsa);
    RSA_free_nid_postfix(old);
    RequireErrors({});
}

void TestErrors() {
    RequireBioFailure("no PEM here\n", {Error(9, 108)});
    RequireBioFailure(Relabel(RsaKey, "RSA PRIVATE KEY", "DH PRIVATE KEY"), {Error(9, 108)});
    RequireBioFailure(Certificate, {Error(9, 108)});
    RequireBioFailure(Replace(RsaKey, "END RSA PRIVATE", "END PRIVATE"), {Error(9, 102)});
    RequireBioFailure(Replace(RsaKey, "END RSA PRIVATE", "END DSA PRIVATE"), {Error(9, 102)});
    RequireBioFailure(Replace(RsaKey, "Lldg=\n", "Lldg=AAAA\n"), {Error(9, 100)});
    RequireBioFailure(Replace(RsaKey, "m9zhglUBoQst4upwxOYfgar9ZmtI5HHu+pdzor6Lldg=\n", "AAA\n"), {Error(9, 100)});
    RequireBioFailure(Replace(RsaKey, "MIIBPAIB", "MIIB!AIB"), {});
    RequireBioFailure(Replace(RsaKey, "KEY-----\n", "KEY-----\nComment: x\n\n"), {Error(9, 107)});
    RequireBioFailure(Replace(RsaKey, "KEY-----\n", "KEY-----\nProc-Type: 4,MIC-ONLY\n\n"), {Error(9, 106)});
    RequireBioFailure(Replace(RsaKey, "KEY-----\n", "KEY-----\nProc-Type: 4,ENCRYPTED\nComment: x\n\n"), {Error(9, 105)});
    RequireBioFailure(Relabel(RsaKey, "RSA PRIVATE KEY", "ANY PRIVATE KEY"), {Error(9, 13)});
    RequireBioFailure(UnknownPkcs8Key, {Error(6, 156), Error(6, 118), Error(9, 13)});

    ERR_clear_error_nid_postfix();
    Require(ReadFile("no PEM here\n") == nullptr);
    RequireErrors({Error(9, 108)});
    Require(PEM_read_RSAPrivateKey_nid_postfix(nullptr, nullptr, nullptr, nullptr) == nullptr);
    RequireErrors({Error(9, 7)});

    RequireThrows([] { ReadBio(EncryptedRsaKey); });
    RequireThrows([] { ReadBio(EncryptedPkcs8Key); });
    RequireThrows([] { ReadBio(EcKey); });
    RequireThrows([] { ReadBio(EcPkcs8Key); });
    RequireThrows([] { ReadFile(EcKey); });
}

void TestCertificate() {
    const std::string text = std::string(RsaKey) + Certificate;
    LibreSslX509* certificate = nullptr;
    auto* bio = BIO_new_mem_buf_nid_postfix(text.data(), static_cast<int>(text.size()));
    RequireThrows([&] { PEM_read_bio_X509_nid_postfix(bio, &certificate, nullptr, nullptr); });
    BIO_free_nid_postfix(bio);
    bio = BIO_new_mem_buf_nid_postfix(RsaKey, static_cast<int>(sizeof(RsaKey) - 1));
    ERR_clear_error_nid_postfix();
    Require(PEM_read_bio_X509_nid_postfix(bio, &certificate, nullptr, nullptr) == nullptr && certificate == nullptr);
    RequireErrors({Error(9, 108)});
    BIO_free_nid_postfix(bio);
}

}

int main() {
    void* const api[10] {};
    ApplicationHeapRegister_nid_no_patch(api);
    TestKeys();
    TestReuse();
    TestErrors();
    TestCertificate();
    std::puts("libSceLibreSsl PEM private key tests passed");
}
