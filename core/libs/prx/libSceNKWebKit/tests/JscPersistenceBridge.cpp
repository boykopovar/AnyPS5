#include "prx/libSceJsc/libSceJsc.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "JscPersistenceBridge check failed at line %d\n", line);
        std::abort();
    }
}

}

int main() {
    NKWebKit::WtfAbi::Encoder encoder;
    _ZN3WTF11Persistence7EncoderC1Ev(&encoder);
    auto* jscEncoder = reinterpret_cast<Jsc::WtfAbi::PersistenceEncoder*>(&encoder);

    for (unsigned i = 0; i < 4085; ++i) {
        _ZN3WTF11Persistence7EncoderlsEh(&encoder, static_cast<std::uint8_t>(i & 0xFF));
    }
    Check(encoder.buffer == encoder.inlineBuffer, __LINE__);
    Check(encoder.capacity == 4096 && encoder.size == 4085, __LINE__);

    Jsc::WtfAbi::SHA1Digest firstDigest{};
    for (std::size_t i = 0; i < firstDigest.size(); ++i) firstDigest[i] = static_cast<std::uint8_t>(0xA0 + i);
    _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6encodeERNS0_7EncoderERKS3_(jscEncoder, &firstDigest);
    Check(encoder.buffer != encoder.inlineBuffer, __LINE__);
    Check(encoder.capacity == 8192 && encoder.size == 4105, __LINE__);
    Check(std::memcmp(encoder.buffer + 4085, firstDigest.data(), firstDigest.size()) == 0, __LINE__);

    for (unsigned i = 4105; i < 8185; ++i) {
        _ZN3WTF11Persistence7EncoderlsEh(&encoder, static_cast<std::uint8_t>(i & 0xFF));
    }
    Check(encoder.capacity == 8192 && encoder.size == 8185, __LINE__);

    Jsc::WtfAbi::SHA1Digest secondDigest{};
    for (std::size_t i = 0; i < secondDigest.size(); ++i) secondDigest[i] = static_cast<std::uint8_t>(0x50 + i);
    _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6encodeERNS0_7EncoderERKS3_(jscEncoder, &secondDigest);
    Check(encoder.capacity == 16384 && encoder.size == 8205, __LINE__);
    Check(std::memcmp(encoder.buffer + 8185, secondDigest.data(), secondDigest.size()) == 0, __LINE__);

    const NKWebKit::WtfAbi::SHA1 payloadSha1 = encoder.sha1;
    const unsigned payloadSize = encoder.size;
    _ZN3WTF11Persistence7Encoder14encodeChecksumEv(&encoder);
    Check(encoder.size == payloadSize + 20, __LINE__);

    NKWebKit::WtfAbi::Decoder checksumDecoder{encoder.buffer, encoder.size, encoder.buffer + payloadSize, payloadSha1};
    Check(_ZN3WTF11Persistence7Decoder14verifyChecksumEv(&checksumDecoder), __LINE__);

    Jsc::WtfAbi::SHA1 emptySha1{};
    emptySha1.hash[0] = 0x67452301;
    emptySha1.hash[1] = 0xefcdab89;
    emptySha1.hash[2] = 0x98badcfe;
    emptySha1.hash[3] = 0x10325476;
    emptySha1.hash[4] = 0xc3d2e1f0;
    Jsc::WtfAbi::PersistenceDecoder firstDecoder{encoder.buffer, encoder.buffer + 4085, encoder.buffer + encoder.size, emptySha1};
    const auto decodedFirst = _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6decodeERNS0_7DecoderE(&firstDecoder);
    Check(decodedFirst.hasValue && decodedFirst.value == firstDigest, __LINE__);

    Jsc::WtfAbi::PersistenceDecoder secondDecoder{encoder.buffer, encoder.buffer + 8185, encoder.buffer + encoder.size, emptySha1};
    const auto decodedSecond = _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6decodeERNS0_7DecoderE(&secondDecoder);
    Check(decodedSecond.hasValue && decodedSecond.value == secondDigest, __LINE__);

    _ZN3WTF11Persistence7EncoderD1Ev(&encoder);

    Jsc::WtfAbi::StringImpl* uuid = nullptr;
    Check(_ZN3WTF25createCanonicalUUIDStringEv(&uuid) == &uuid && uuid != nullptr, __LINE__);
    Check(uuid->refCount == 2 && uuid->length == 36, __LINE__);
    Check(_ZN3WTF14fastMallocSizeEPKv(uuid) == 56, __LINE__);
    _ZN3WTF8fastFreeEPv(uuid);
    return 0;
}
