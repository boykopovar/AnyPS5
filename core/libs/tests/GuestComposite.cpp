#include "prx/libSceComposite/include/Api.hpp"
#include <cstdint>
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

constexpr int kUnsupported = static_cast<int>(0x80d40006);

}

int main() {
    Require(sceCompositorCreateIndirectRenderTarget() == kUnsupported);
    Require(sceCompositorDeleteIndirectRenderTarget() == kUnsupported);
    Require(sceCompositorSetIndirectCanvasCommand() == 0);
    Require(sceCompositorSetIndirectCompositionCommand() == 0);
    Require(sceCompositorSetIndirectRenderTargetConfigCommand() == 0);

    const std::uint32_t virtualHandles[] = {
        0x0000ff00u,
        0x0000ff01u,
        0x0000ffffu,
        0x1234ff00u,
        0x1234ff78u,
        0x8000ff00u,
        0xffffff00u,
        0xffffffffu
    };
    for (const auto handle : virtualHandles) {
        Require(sceCompositorIsVirtualCanvasHandle(handle));
    }

    const std::uint32_t regularHandles[] = {
        0x00000000u,
        0x000000ffu,
        0x00000100u,
        0x00007f00u,
        0x0000fe00u,
        0x0000feffu,
        0x12345678u,
        0x12340078u,
        0x8000fe00u
    };
    for (const auto handle : regularHandles) {
        Require(!sceCompositorIsVirtualCanvasHandle(handle));
    }

    return 0;
}
