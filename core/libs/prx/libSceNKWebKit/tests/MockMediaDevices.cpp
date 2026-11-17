#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"
#include "Testing.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

int main() {
    WKClearMockMediaDevices();
    WKResetMockMediaDevices();

    auto str = NKWebKit::String::FromUtf8("mock-device-1");
    const void* strRef = NKWebKit::ToRef(str);
    WKRemoveMockMediaDevice(strRef);
    WKRemoveMockMediaDevice(nullptr);

    WKRelease(strRef);
    return 0;
}
