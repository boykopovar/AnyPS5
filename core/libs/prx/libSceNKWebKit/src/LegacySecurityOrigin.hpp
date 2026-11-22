#pragma once

#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace NKWebKit::LegacyOrigin {

struct OriginData {
    bool unique = true;
    std::u16string protocol;
    std::u16string host;
    std::optional<std::uint16_t> port;
};

OriginData OriginOf(const WtfAbi::URL& url);
OriginData OriginFromString(const std::u16string& originString);
std::u16string ToString(const OriginData& origin);
bool IsSameSchemeHostPort(const OriginData& origin, const OriginData& other);

}  // namespace NKWebKit::LegacyOrigin
