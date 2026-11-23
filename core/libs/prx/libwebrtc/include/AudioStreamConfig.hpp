#pragma once

#include "prx/libwebrtc/include/CryptoOptions.hpp"
#include "prx/libwebrtc/include/GuestString.hpp"
#include "prx/libwebrtc/include/RtpExtension.hpp"
#include "prx/libwebrtc/include/SdpAudioFormat.hpp"

#include <cstddef>
#include <cstdint>

struct WebRtcAudioStreamOptionalInt {
    std::int32_t value;
    bool engaged;
};

struct WebRtcAudioStreamOptionalUint64 {
    std::uint64_t value;
    bool engaged;
};

struct WebRtcAudioStreamOptionalString {
    WebRtcGuestString value;
    bool engaged;
};

struct WebRtcAudioStreamRawVector {
    unsigned char* begin;
    unsigned char* end;
    unsigned char* capacityEnd;
};

struct WebRtcRtpExtensionVector {
    WebRtcRtpExtension* begin;
    WebRtcRtpExtension* end;
    WebRtcRtpExtension* capacityEnd;
};

struct WebRtcAudioSendStreamStats {
    alignas(8) unsigned char counters[64];
    WebRtcGuestString codecName;
    alignas(8) unsigned char metrics[224];
    WebRtcAudioStreamRawVector reportBlockDatas;
    std::uint32_t nacksReceived;
};

struct WebRtcAudioSendCodecSpec {
    std::int32_t payloadType;
    WebRtcSdpAudioFormat format;
    bool flags[4];
    WebRtcAudioStreamOptionalInt cngPayloadType;
    WebRtcAudioStreamOptionalInt redPayloadType;
    WebRtcAudioStreamOptionalInt targetBitrateBps;
};

struct WebRtcAudioStreamOptionalSendCodecSpec {
    WebRtcAudioSendCodecSpec value;
    bool engaged;
};

struct WebRtcAudioSendStreamRtp {
    std::uint32_t ssrc;
    WebRtcGuestString rid;
    WebRtcGuestString mid;
    bool extmapAllowMixed;
    WebRtcRtpExtensionVector extensions;
    WebRtcGuestString cName;
    std::int32_t rtcpMode;
};

struct WebRtcAudioSendStreamConfig {
    WebRtcAudioSendStreamRtp rtp;
    std::int32_t rtcpReportIntervalMs;
    void* sendTransport;
    std::int32_t minBitrateBps;
    std::int32_t maxBitrateBps;
    double bitratePriority;
    bool hasDscp;
    WebRtcAudioStreamOptionalString audioNetworkAdaptorConfig;
    WebRtcAudioStreamOptionalSendCodecSpec sendCodecSpec;
    void* encoderFactory;
    WebRtcAudioStreamOptionalUint64 codecPairId;
    WebRtcGuestString trackId;
    WebRtcCryptoOptions cryptoOptions;
    void* frameEncryptor;
    void* frameTransformer;
};

struct WebRtcAudioReceiveStreamStatsPrefix {
    alignas(8) unsigned char counters[72];
    WebRtcGuestString codecName;
};

struct WebRtcAudioDecoderMapNode;

struct WebRtcAudioDecoderMapLinks {
    WebRtcAudioDecoderMapNode* left;
    WebRtcAudioDecoderMapNode* right;
    WebRtcAudioDecoderMapNode* parent;
    bool isBlack;
};

struct WebRtcAudioDecoderMapNode {
    WebRtcAudioDecoderMapLinks links;
    std::int32_t payloadType;
    WebRtcSdpAudioFormat format;
};

struct WebRtcAudioDecoderMap {
    WebRtcAudioDecoderMapNode* beginNode;
    WebRtcAudioDecoderMapNode* root;
    std::uint64_t size;
};

struct WebRtcAudioReceiveStreamConfig {
    std::uint32_t remoteSsrc;
    std::uint32_t localSsrc;
    std::int32_t nackRtpHistoryMs;
    std::int32_t rtcpMode;
    bool enableNonSenderRtt;
    void* rtcpSendTransport;
    std::uint64_t jitterBufferMaxPackets;
    bool jitterBufferFastAccelerate;
    std::int32_t jitterBufferMinDelayMs;
    WebRtcGuestString syncGroup;
    WebRtcAudioDecoderMap decoderMap;
    void* decoderFactory;
    WebRtcAudioStreamOptionalUint64 codecPairId;
    WebRtcCryptoOptions cryptoOptions;
    void* frameDecryptor;
    void* frameTransformer;
};

static_assert(sizeof(WebRtcAudioStreamOptionalInt) == 8);
static_assert(sizeof(WebRtcAudioStreamOptionalUint64) == 16);
static_assert(sizeof(WebRtcAudioStreamOptionalString) == 48);
static_assert(offsetof(WebRtcAudioStreamOptionalString, engaged) == 40);
static_assert(sizeof(WebRtcAudioStreamRawVector) == 24);
static_assert(sizeof(WebRtcRtpExtensionVector) == 24);

static_assert(sizeof(WebRtcAudioSendStreamStats) == 360);
static_assert(offsetof(WebRtcAudioSendStreamStats, codecName) == 64);
static_assert(offsetof(WebRtcAudioSendStreamStats, metrics) == 104);
static_assert(offsetof(WebRtcAudioSendStreamStats, reportBlockDatas) == 328);
static_assert(offsetof(WebRtcAudioSendStreamStats, nacksReceived) == 352);

static_assert(sizeof(WebRtcAudioSendCodecSpec) == 120);
static_assert(offsetof(WebRtcAudioSendCodecSpec, format) == 8);
static_assert(offsetof(WebRtcAudioSendCodecSpec, flags) == 88);
static_assert(offsetof(WebRtcAudioSendCodecSpec, cngPayloadType) == 92);
static_assert(offsetof(WebRtcAudioSendCodecSpec, redPayloadType) == 100);
static_assert(offsetof(WebRtcAudioSendCodecSpec, targetBitrateBps) == 108);
static_assert(sizeof(WebRtcAudioStreamOptionalSendCodecSpec) == 128);
static_assert(offsetof(WebRtcAudioStreamOptionalSendCodecSpec, engaged) == 120);

static_assert(sizeof(WebRtcAudioSendStreamRtp) == 168);
static_assert(offsetof(WebRtcAudioSendStreamRtp, rid) == 8);
static_assert(offsetof(WebRtcAudioSendStreamRtp, mid) == 48);
static_assert(offsetof(WebRtcAudioSendStreamRtp, extmapAllowMixed) == 88);
static_assert(offsetof(WebRtcAudioSendStreamRtp, extensions) == 96);
static_assert(offsetof(WebRtcAudioSendStreamRtp, cName) == 120);
static_assert(offsetof(WebRtcAudioSendStreamRtp, rtcpMode) == 160);

static_assert(sizeof(WebRtcAudioSendStreamConfig) == 472);
static_assert(offsetof(WebRtcAudioSendStreamConfig, rtcpReportIntervalMs) == 168);
static_assert(offsetof(WebRtcAudioSendStreamConfig, sendTransport) == 176);
static_assert(offsetof(WebRtcAudioSendStreamConfig, bitratePriority) == 192);
static_assert(offsetof(WebRtcAudioSendStreamConfig, hasDscp) == 200);
static_assert(offsetof(WebRtcAudioSendStreamConfig, audioNetworkAdaptorConfig) == 208);
static_assert(offsetof(WebRtcAudioSendStreamConfig, sendCodecSpec) == 256);
static_assert(offsetof(WebRtcAudioSendStreamConfig, encoderFactory) == 384);
static_assert(offsetof(WebRtcAudioSendStreamConfig, codecPairId) == 392);
static_assert(offsetof(WebRtcAudioSendStreamConfig, trackId) == 408);
static_assert(offsetof(WebRtcAudioSendStreamConfig, cryptoOptions) == 448);
static_assert(offsetof(WebRtcAudioSendStreamConfig, frameEncryptor) == 456);
static_assert(offsetof(WebRtcAudioSendStreamConfig, frameTransformer) == 464);

static_assert(sizeof(WebRtcAudioReceiveStreamStatsPrefix) == 112);
static_assert(offsetof(WebRtcAudioReceiveStreamStatsPrefix, codecName) == 72);

static_assert(sizeof(WebRtcAudioDecoderMapLinks) == 32);
static_assert(sizeof(WebRtcAudioDecoderMapNode) == 120);
static_assert(offsetof(WebRtcAudioDecoderMapNode, payloadType) == 32);
static_assert(offsetof(WebRtcAudioDecoderMapNode, format) == 40);
static_assert(sizeof(WebRtcAudioDecoderMap) == 24);

static_assert(sizeof(WebRtcAudioReceiveStreamConfig) == 160);
static_assert(offsetof(WebRtcAudioReceiveStreamConfig, enableNonSenderRtt) == 16);
static_assert(offsetof(WebRtcAudioReceiveStreamConfig, rtcpSendTransport) == 24);
static_assert(offsetof(WebRtcAudioReceiveStreamConfig, jitterBufferMaxPackets) == 32);
static_assert(offsetof(WebRtcAudioReceiveStreamConfig, jitterBufferMinDelayMs) == 44);
static_assert(offsetof(WebRtcAudioReceiveStreamConfig, syncGroup) == 48);
static_assert(offsetof(WebRtcAudioReceiveStreamConfig, decoderMap) == 88);
static_assert(offsetof(WebRtcAudioReceiveStreamConfig, decoderFactory) == 112);
static_assert(offsetof(WebRtcAudioReceiveStreamConfig, codecPairId) == 120);
static_assert(offsetof(WebRtcAudioReceiveStreamConfig, cryptoOptions) == 136);
static_assert(offsetof(WebRtcAudioReceiveStreamConfig, frameDecryptor) == 144);
static_assert(offsetof(WebRtcAudioReceiveStreamConfig, frameTransformer) == 152);
