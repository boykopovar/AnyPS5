#pragma once

#include <cstddef>

struct WebRtcCryptoOptionsSrtp {
    bool enableGcmCryptoSuites;
    bool enableAes128Sha1_32CryptoCipher;
    bool enableAes128Sha1_80CryptoCipher;
    bool enableEncryptedRtpHeaderExtensions;
};

struct WebRtcCryptoOptionsSFrame {
    bool requireFrameEncryption;
};

struct WebRtcCryptoOptions {
    WebRtcCryptoOptionsSrtp srtp;
    WebRtcCryptoOptionsSFrame sframe;
};

static_assert(sizeof(WebRtcCryptoOptions) == 5);
static_assert(offsetof(WebRtcCryptoOptions, sframe) == 4);
