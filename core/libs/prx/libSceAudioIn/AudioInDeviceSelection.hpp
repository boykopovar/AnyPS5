#pragma once

#include <string_view>

namespace audioin::detail {

enum class CaptureDeviceMatchKind {
    Default,
    Found,
    Missing,
    Ambiguous,
};

struct CaptureDeviceMatch {
    CaptureDeviceMatchKind kind;
    int index;
};

template <typename TGetName>
CaptureDeviceMatch selectCaptureDevice(std::string_view requestedName, int deviceCount, TGetName&& getName) {
    if (requestedName.empty() || requestedName == "default") return {CaptureDeviceMatchKind::Default, -1};
    if (deviceCount < 0) return {CaptureDeviceMatchKind::Missing, -1};
    int match = -1;
    for (int index = 0; index < deviceCount; ++index) {
        const char* name = getName(index);
        if (!name || requestedName != name) continue;
        if (match >= 0) return {CaptureDeviceMatchKind::Ambiguous, -1};
        match = index;
    }
    if (match < 0) return {CaptureDeviceMatchKind::Missing, -1};
    return {CaptureDeviceMatchKind::Found, match};
}

}
