/* Derived from api/video_codecs/sdp_video_format.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from api/video_codecs/h264_profile_level_id.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from api/video_codecs/vp9_profile.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from api/video_codecs/av1_profile.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from api/video_codecs/video_codec.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from rtc_base/string_to_number.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/libc.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using Parameters = std::map<std::string, std::string>;

struct HostFormat {
    std::string name;
    Parameters parameters;
};

enum class CodecType {
    Vp8,
    Vp9,
    Av1,
    H264,
    H265,
    Generic,
};

enum class H264Profile {
    ConstrainedBaseline,
    Baseline,
    Main,
    ConstrainedHigh,
    High,
    PredictiveHigh444,
};

struct ErrnoGuard {
    int savedErrno = errno;
    ~ErrnoGuard() { errno = savedErrno; }
};

HostFormat ReadFormat(const WebRtcSdpVideoFormat* format, const char* message) {
    if (!format)
        throw std::invalid_argument(message);
    WebRtcSdpVideoFormat snapshot{};
    {
        ErrnoGuard guard;
        if (!GuestCopyIn_nid_no_patch(&snapshot, format, sizeof(snapshot)))
            throw std::invalid_argument(message);
    }
    HostFormat host;
    WebRtcReadSdpNameAndParameters_nid_no_patch(&format->name, &format->parameters, message, &host.name, &host.parameters);
    return host;
}

char AsciiToLower(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
}

bool EqualsIgnoreCase(std::string_view left, std::string_view right) {
    if (left.size() != right.size())
        return false;
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (AsciiToLower(left[index]) != AsciiToLower(right[index]))
            return false;
    }
    return true;
}

CodecType PayloadStringToCodecType(std::string_view name) {
    if (EqualsIgnoreCase(name, "VP8"))
        return CodecType::Vp8;
    if (EqualsIgnoreCase(name, "VP9"))
        return CodecType::Vp9;
    if (EqualsIgnoreCase(name, "AV1") || EqualsIgnoreCase(name, "AV1X"))
        return CodecType::Av1;
    if (EqualsIgnoreCase(name, "H264"))
        return CodecType::H264;
    if (EqualsIgnoreCase(name, "H265"))
        return CodecType::H265;
    return CodecType::Generic;
}

std::string GetFmtpParameterOrDefault(const Parameters& params, const std::string& name, const std::string& defaultValue) {
    const auto it = params.find(name);
    if (it != params.end())
        return it->second;
    return defaultValue;
}

std::optional<int> StringToInt(const std::string& text) {
    if (text.empty())
        return std::nullopt;
    const auto first = static_cast<unsigned char>(text[0]);
    if (!(first >= '0' && first <= '9') && text[0] != '-')
        return std::nullopt;
    ErrnoGuard guard;
    char* end = nullptr;
    errno = 0;
    const long long value = std::strtoll(text.c_str(), &end, 10);
    if (end != text.c_str() + text.size() || errno != 0)
        return std::nullopt;
    if (value < std::numeric_limits<int>::lowest() || value > std::numeric_limits<int>::max())
        return std::nullopt;
    return static_cast<int>(value);
}

std::optional<int> ParseProfileNumber(const Parameters& params, const std::string& key, int highestProfile) {
    const auto it = params.find(key);
    if (it == params.end())
        return 0;
    const std::optional<int> number = StringToInt(it->second);
    if (!number || *number < 0 || *number > highestProfile)
        return std::nullopt;
    return number;
}

bool IsSameProfileNumber(const Parameters& left, const Parameters& right, const std::string& key, int highestProfile) {
    const std::optional<int> profile = ParseProfileNumber(left, key, highestProfile);
    const std::optional<int> otherProfile = ParseProfileNumber(right, key, highestProfile);
    return profile && otherProfile && *profile == *otherProfile;
}

constexpr std::uint8_t ByteMaskString(char c, const char (&str)[9]) {
    return static_cast<std::uint8_t>((str[0] == c) << 7 | (str[1] == c) << 6 | (str[2] == c) << 5 | (str[3] == c) << 4 | (str[4] == c) << 3 | (str[5] == c) << 2 | (str[6] == c) << 1 | (str[7] == c) << 0);
}

struct BitPattern {
    std::uint8_t mask;
    std::uint8_t maskedValue;

    explicit constexpr BitPattern(const char (&str)[9])
        : mask(static_cast<std::uint8_t>(~ByteMaskString('x', str))), maskedValue(ByteMaskString('1', str)) {}

    constexpr bool IsMatch(std::uint8_t value) const { return maskedValue == (value & mask); }
};

struct ProfilePattern {
    std::uint8_t profileIdc;
    BitPattern profileIop;
    H264Profile profile;
};

constexpr ProfilePattern ProfilePatterns[] = {
    {0x42, BitPattern("x1xx0000"), H264Profile::ConstrainedBaseline},
    {0x4D, BitPattern("1xxx0000"), H264Profile::ConstrainedBaseline},
    {0x58, BitPattern("11xx0000"), H264Profile::ConstrainedBaseline},
    {0x42, BitPattern("x0xx0000"), H264Profile::Baseline},
    {0x58, BitPattern("10xx0000"), H264Profile::Baseline},
    {0x4D, BitPattern("0x0x0000"), H264Profile::Main},
    {0x64, BitPattern("00000000"), H264Profile::High},
    {0x64, BitPattern("00001100"), H264Profile::ConstrainedHigh},
    {0xF4, BitPattern("00000000"), H264Profile::PredictiveHigh444},
};

bool IsKnownH264LevelIdc(std::uint8_t levelIdc) {
    switch (levelIdc) {
    case 10:
    case 11:
    case 12:
    case 13:
    case 20:
    case 21:
    case 22:
    case 30:
    case 31:
    case 32:
    case 40:
    case 41:
    case 42:
    case 50:
    case 51:
    case 52:
        return true;
    default:
        return false;
    }
}

std::optional<H264Profile> ParseH264Profile(const char* str) {
    if (std::strlen(str) != 6u)
        return std::nullopt;
    ErrnoGuard guard;
    const auto numeric = static_cast<std::uint32_t>(std::strtoll(str, nullptr, 16));
    if (numeric == 0)
        return std::nullopt;
    const auto levelIdc = static_cast<std::uint8_t>(numeric & 0xFF);
    const auto profileIop = static_cast<std::uint8_t>((numeric >> 8) & 0xFF);
    const auto profileIdc = static_cast<std::uint8_t>((numeric >> 16) & 0xFF);
    if (!IsKnownH264LevelIdc(levelIdc))
        return std::nullopt;
    for (const ProfilePattern& pattern : ProfilePatterns) {
        if (profileIdc == pattern.profileIdc && pattern.profileIop.IsMatch(profileIop))
            return pattern.profile;
    }
    return std::nullopt;
}

std::optional<H264Profile> ParseSdpForH264Profile(const Parameters& params) {
    const auto it = params.find("profile-level-id");
    if (it == params.end())
        return H264Profile::ConstrainedBaseline;
    return ParseH264Profile(it->second.c_str());
}

bool H264IsSameProfile(const Parameters& left, const Parameters& right) {
    const std::optional<H264Profile> profile = ParseSdpForH264Profile(left);
    const std::optional<H264Profile> otherProfile = ParseSdpForH264Profile(right);
    return profile && otherProfile && *profile == *otherProfile;
}

bool IsSameParameterOrDefault(const Parameters& left, const Parameters& right, const std::string& key, const std::string& defaultValue) {
    return GetFmtpParameterOrDefault(left, key, defaultValue) == GetFmtpParameterOrDefault(right, key, defaultValue);
}

bool IsSameCodecSpecific(const HostFormat& format, const HostFormat& other) {
    switch (PayloadStringToCodecType(format.name)) {
    case CodecType::H264:
        return H264IsSameProfile(format.parameters, other.parameters) && IsSameParameterOrDefault(format.parameters, other.parameters, "packetization-mode", "0");
    case CodecType::Vp9:
        return IsSameProfileNumber(format.parameters, other.parameters, "profile-id", 3);
    case CodecType::Av1:
        return IsSameProfileNumber(format.parameters, other.parameters, "profile", 2) && IsSameParameterOrDefault(format.parameters, other.parameters, "tier", "0") && IsSameParameterOrDefault(format.parameters, other.parameters, "level-idx", "5");
    case CodecType::H265:
        NotImplemented_nid_no_patch("SdpVideoFormat::IsCodecInList: H265 matching depends on RTC_ENABLE_H265 and differs between M127 and M134");
    case CodecType::Vp8:
    case CodecType::Generic:
        return true;
    }
    return true;
}

bool IsSameCodec(const HostFormat& format, const HostFormat& other) {
    return EqualsIgnoreCase(format.name, other.name) && IsSameCodecSpecific(format, other);
}

}

extern "C" {

bool APS5_VABI _ZNK6webrtc14SdpVideoFormat13IsCodecInListEN3rtc9ArrayViewIKS0_Lln4711EEE_nid_postfix(const WebRtcSdpVideoFormat* format, const WebRtcSdpVideoFormat* formats, std::size_t count) {
    constexpr const char* message = "SdpVideoFormat::IsCodecInList requires valid formats";
    if (count == 0)
        return false;
    const HostFormat self = ReadFormat(format, message);
    if (!formats || count > std::numeric_limits<std::size_t>::max() / sizeof(WebRtcSdpVideoFormat))
        throw std::invalid_argument(message);
    for (std::size_t index = 0; index < count; ++index) {
        if (IsSameCodec(self, ReadFormat(formats + index, message)))
            return true;
    }
    return false;
}

}
