/* Adapted for AnyPS5 from webrtc 8d78f5de6c27b2c793039989ea381f1428fb0100
 * modules/rtp_rtcp/source/rtcp_packet/transport_feedback_unittest.cc
 * (https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/modules/rtp_rtcp/source/rtcp_packet/transport_feedback_unittest.cc).
 */
/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#pragma once

#include <array>
#include <cstdint>

namespace WebRtcUpstreamRtcp {

struct FeedbackCase {
    const char* name;
    const char* payloadHex;
    int hasPadding;
    std::uint16_t baseSeq;
    std::uint16_t statusCount;
    std::int32_t baseTimeTicks;
    int includeTimestamps;
    int receivedCount;
    std::uint16_t firstRecvSeq;
    std::uint16_t lastRecvSeq;
    std::int16_t firstDeltaTicks;
    std::int16_t lastDeltaTicks;
};

inline constexpr std::array<FeedbackCase, 15> FeedbackCases {{
    {"OneBitVector", "12345678876543210001000d00000001b0f20404140404040c000003", 1, 1, 13, 0, 1, 7, 1, 13, 4, 12},
    {"OneBitVectorNoRecvDelta", "12345678876543210001000d00000001b0f20002", 1, 1, 13, 0, 0, 7, 1, 13, 0, 0},
    {"FullOneBitVector", "12345678876543210001000e00000001b0f30404140404040c040002", 1, 1, 14, 0, 1, 8, 1, 14, 4, 4},
    {"OneBitVectorWrapReceived", "1234567887654321fffd00060003ff01bf00f40404040404", 0, 65533, 6, 1023, 1, 6, 65533, 2, 244, 4},
    {"OneBitVectorWrapMissing", "1234567887654321fffd00060003ff01b300f4040c040002", 1, 65533, 6, 1023, 1, 4, 65533, 2, 244, 4},
    {"TwoBitVector", "12345678876543210001000700000101d80a00010004000100000003", 1, 1, 7, 1, 1, 4, 1, 7, 0, 256},
    {"TwoBitVectorFull", "12345678876543210001000800000101d80ae000000100040001000100000003", 1, 1, 8, 1, 1, 5, 1, 8, 0, 256},
    {"LargeAndNegativeDeltas", "12345678876543210001000800000001d806e00008fffc0cfffc0100", 0, 1, 8, 0, 1, 5, 1, 8, 8, 256},
    {"MaxRle", "12345678876543210000200e00000001a0001fffa0000404", 0, 0, 8206, 0, 1, 2, 0, 8205, 4, 4},
    {"MinRle", "12345678876543210000001e00000001a000000fa0000404", 0, 0, 30, 0, 1, 2, 0, 29, 4, 4},
    {"OneToTwoBitVector", "12345678876543210000000700000001d002000100000003", 1, 0, 7, 0, 1, 2, 0, 6, 0, 256},
    {"OneToTwoBitVectorSimpleSplit", "12345678876543210000000800000001d000e00000010001", 1, 0, 8, 0, 1, 2, 0, 7, 0, 256},
    {"OneToTwoBitVectorSplit", "12345678876543210000001600000001d555d955bfc00404040404040404010004040404040404040404040404000003", 1, 0, 22, 0, 1, 22, 0, 21, 4, 4},
    {"MaxNegativeDelta", "12345678876543210001000200000001d800008000000003", 1, 1, 2, 0, 1, 2, 1, 2, 0, -32768},
    {"LargePositiveAndMinNegativeWrap", "1234567887654321fffe000400000101da40007fff8001ff", 0, 65534, 4, 1, 1, 4, 65534, 1, 0, 255},
}};

}
