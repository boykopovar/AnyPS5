#include "GraphicsTests.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include <cstring>
#include <string>
#include <string_view>

namespace {

using namespace AgcDriver::Graphics;

struct Push {
    std::uint32_t srcBase;
    std::uint32_t dstBase;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t pitchBytes;
    std::uint32_t blocksPerRow;
    std::uint32_t tail;
    std::uint32_t tailX;
    std::uint32_t tailY;
    std::uint32_t elementBytes;
    std::uint32_t slice;
    std::uint32_t rangeBegin;
    std::uint32_t rangeEnd;
    std::uint32_t tiledBase;
    std::uint32_t linearBase;
    std::uint32_t columnBegin;
    std::uint32_t rowBegin;
    std::uint32_t pipeBankXor;
};

Push decodePush(const std::vector<std::byte>& bytes) {
    Push push{};
    Require(bytes.size() == sizeof(Push), "unexpected texture detiling push constant size");
    std::memcpy(&push, bytes.data(), sizeof(Push));
    return push;
}

TileMipLayout makeLayout(std::uint32_t width, std::uint32_t height, std::uint32_t pitchBytes, std::uint32_t blocksPerRow, std::uint64_t tiledSize, std::uint64_t linearSize) {
    TileMipLayout layout{};
    layout.width = width;
    layout.height = height;
    layout.pitchBytes = pitchBytes;
    layout.blocksPerRow = blocksPerRow;
    layout.tiledSize = tiledSize;
    layout.linearSize = linearSize;
    layout.tail = false;
    layout.tailX = 0;
    layout.tailY = 0;
    return layout;
}

template<typename TAction>
void reject(TAction action, std::string_view reason) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        Require(std::string_view(error.what()).find(reason) != std::string_view::npos, std::string("unexpected texture detiler test error: ") + error.what());
        return;
    }
    throw std::runtime_error(std::string("expected texture detiler rejection: ") + std::string(reason));
}

}

void RunTextureDetilerTests(const Context& context, const TextureDetilerTestAccess& access) {
    TextureDetiler detiler(context);
    const auto commands = reinterpret_cast<VkCommandBuffer>(static_cast<std::uintptr_t>(1));
    const auto source = access.makeBuffer(4096);
    const auto destination = access.makeBuffer(4096);

    const auto layout = makeLayout(20, 12, 96, 5, 64, 48);
    detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 20, destination, 40, layout, 0);
    auto capture = access.lastDispatch();
    Require(capture.groupsX == 3 && capture.groupsY == 2 && capture.groupsZ == 1, "dispatch group counts were computed incorrectly");
    auto push = decodePush(capture.pushConstants);
    Require(push.srcBase == 4 && push.dstBase == 8, "push constant buffer bases must account for storage buffer offset alignment");
    Require(push.width == 20 && push.height == 12, "push constant dimensions changed");
    Require(push.pitchBytes == 96 && push.blocksPerRow == 5, "push constant row layout changed");
    Require(push.tail == 0 && push.tailX == 0 && push.tailY == 0, "push constant tail fields must reflect a non-tail mip");
    Require(push.elementBytes == 4, "push constant element size changed");
    Require(push.rangeBegin == 0 && push.rangeEnd == 0xffffffffu && push.tiledBase == 0 && push.linearBase == 0, "a whole-mip dispatch must move every element out of whole buffers");
    Require(push.columnBegin == 0 && push.rowBegin == 0, "a whole-mip dispatch starts at the mip's origin");
    Require(capture.sourceBuffer == source && capture.destinationBuffer == destination, "dispatch bound the wrong source or destination buffer");
    Require(capture.sourceOffset == 16 && capture.sourceRange == 68, "source descriptor offset or range computed incorrectly");
    Require(capture.destinationOffset == 32 && capture.destinationRange == 56, "destination descriptor offset or range computed incorrectly");

    const auto pipelinesAfterFirst = access.pipelineCount();
    Require(pipelinesAfterFirst == 1, "the first dispatch must create exactly one compute pipeline");

    // A window inside the mip: tiled bytes [16, 48) of a mip whose tiled buffer starts at the
    // mip's byte 16 and whose linear buffer holds one 96-byte row from linear byte 96; the grid
    // covers only the window's rectangle (columns 8-16 of row 1: one workgroup) and the buffer
    // ranges only the window's bytes, in both directions.
    {
        DetileWindow window{16, 48, 16, 96, 96};
        window.columnBegin = 8;
        window.columnEnd = 16;
        window.rowBegin = 1;
        window.rowEnd = 2;
        detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 20, destination, 40, layout, false, 0, false, window);
        capture = access.lastDispatch();
        Require(capture.groupsX == 1 && capture.groupsY == 1 && capture.groupsZ == 1, "a windowed detile must dispatch only the window's workgroups");
        push = decodePush(capture.pushConstants);
        Require(push.rangeBegin == 16 && push.rangeEnd == 48 && push.tiledBase == 16 && push.linearBase == 96, "windowed detile push constants must carry the window");
        Require(push.columnBegin == 8 && push.rowBegin == 1, "windowed detile push constants must carry the grid origin");
        Require(push.width == 20 && push.height == 12, "a window must not change the mip's dimensions");
        Require(capture.sourceOffset == 16 && capture.sourceRange == 4 + 32, "a windowed detile reads the window's tiled bytes only");
        Require(capture.destinationOffset == 32 && capture.destinationRange == 8 + 96, "a windowed detile writes the window's linear rows only");

        detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 20, destination, 40, layout, true, 0, false, window);
        capture = access.lastDispatch();
        Require(capture.groupsX == 1 && capture.groupsY == 1 && capture.groupsZ == 1, "a windowed retile must dispatch only the window's workgroups");
        push = decodePush(capture.pushConstants);
        Require(push.rangeBegin == 16 && push.rangeEnd == 48 && push.tiledBase == 16 && push.linearBase == 96, "windowed retile push constants must carry the window");
        Require(push.columnBegin == 8 && push.rowBegin == 1, "windowed retile push constants must carry the grid origin");
        Require(capture.sourceOffset == 16 && capture.sourceRange == 4 + 96, "a windowed retile reads the window's linear rows only");
        Require(capture.destinationOffset == 32 && capture.destinationRange == 8 + 32, "a windowed retile writes the window's tiled bytes only");
        Require(access.pipelineCount() == pipelinesAfterFirst + 1, "a retile pipeline is separate from the detile pipeline");

        // The unit-shadow form of the same window (StorageTexture::uploadWindows): the tiled bytes
        // come from a sub-buffer holding the window from its first byte (a slab at offset 1000),
        // so the tiled base is the range's begin and the descriptor covers the window's bytes
        // only; the push constants and the grid are the import form's.
        detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 1000, destination, 40, layout, false, 0, false, window);
        capture = access.lastDispatch();
        Require(capture.groupsX == 1 && capture.groupsY == 1 && capture.groupsZ == 1, "a slab-sourced detile must dispatch only the window's workgroups");
        push = decodePush(capture.pushConstants);
        Require(push.rangeBegin == 16 && push.rangeEnd == 48 && push.tiledBase == 16 && push.linearBase == 96, "slab-sourced detile push constants must carry the window");
        Require(push.columnBegin == 8 && push.rowBegin == 1, "slab-sourced detile push constants must carry the grid origin");
        Require(push.srcBase == 8 && capture.sourceOffset == 992 && capture.sourceRange == 8 + 32, "a slab-sourced detile reads the window's tiled bytes at the slab offset");
        Require(capture.destinationOffset == 32 && capture.destinationRange == 8 + 96, "a slab-sourced detile writes the same linear rows as the import form");

        // Whole block rows: the columns default to the mip's width, the rows to the window's.
        DetileWindow rows{0, 64, 0, 0, 48};
        rows.rowBegin = 8;
        detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 0, destination, 0, layout, false, 0, false, rows);
        capture = access.lastDispatch();
        Require(capture.groupsX == 3 && capture.groupsY == 1, "a row window must dispatch whole rows from its first row");
        reject([&] {
            DetileWindow outside{0, 64, 0, 0, 0};
            outside.rowBegin = 12;
            detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 0, destination, 0, layout, false, 0, false, outside);
        }, "window lies outside the mip");
    }
    const auto pipelinesAfterWindow = access.pipelineCount();

    detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 0, destination, 0, makeLayout(8, 8, 32, 2, 32, 32), 0);
    Require(access.pipelineCount() == pipelinesAfterWindow, "dispatching with the same tile mode and element size must reuse the cached pipeline");

    detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 8, source, 0, destination, 0, makeLayout(8, 8, 32, 2, 32, 32), 0);
    Require(access.pipelineCount() == pipelinesAfterWindow + 1, "a different element size must create a new compute pipeline");

    detiler.Dispatch(commands, TextureTileMode::kLinear, 4, source, 0, destination, 0, makeLayout(8, 8, 32, 2, 32, 32), 0);
    Require(access.pipelineCount() == pipelinesAfterWindow + 2, "a different tile mode must create a new compute pipeline");

    detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 0, destination, 0, makeLayout(8, 8, 32, 2, 32, 32), 0);
    Require(access.pipelineCount() == pipelinesAfterWindow + 2, "reusing an earlier tile mode and element size must not create another pipeline");

    detiler.Dispatch(commands, TextureTileMode::RenderTarget64KB, 4, source, 0, destination, 0, layout, false, 13);
    capture = access.lastDispatch();
    Require(decodePush(capture.pushConstants).slice == 13, "render target detiling must preserve the absolute array layer for XOR addressing");
    Require(access.pipelineCount() == pipelinesAfterWindow + 3, "render target detiling must use a separate pipeline");
    const auto specialization = access.lastSpecialization();
    Require(specialization[0] == 4 && specialization[1] == 65536 && specialization[2] == 2, "render target detiling must select its own swizzle family");

    detiler.Dispatch(commands, TextureTileMode::kD4KBX, 4, source, 0, destination, 0, layout, false, 0);
    const auto equationSpecialization = access.lastSpecialization();
    Require(decodePush(access.lastDispatch().pushConstants).pipeBankXor == 0u, "a dispatch without a window passed a pipe/bank XOR");
    detiler.Dispatch(commands, TextureTileMode::kD4KBX, 4, source, 0, destination, 0, layout, false, 0, false, {.pipeBankXor = 0xa00u});
    Require(decodePush(access.lastDispatch().pushConstants).pipeBankXor == 0xa00u, "the window's pipe/bank XOR did not reach the detiling shader");
    Require(equationSpecialization[0] == 4 && equationSpecialization[1] == 4096 && equationSpecialization[2] == 2, "SW_4KB_D_X detiling must select the equation family over 4 KiB blocks");

    reject([&] { detiler.Dispatch(VK_NULL_HANDLE, TextureTileMode::kStandard4KB, 4, source, 0, destination, 0, layout, 0); }, "active command buffer");
    reject([&] { detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, VK_NULL_HANDLE, 0, destination, 0, layout, 0); }, "source and destination buffers");
    reject([&] { detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 0, VK_NULL_HANDLE, 0, layout, 0); }, "source and destination buffers");
    reject([&] { detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 0, destination, 0, makeLayout(0, 12, 96, 5, 64, 48), 0); }, "non-empty mip layout");
    reject([&] { detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 0, destination, 0, makeLayout(20, 0, 96, 5, 64, 48), 0); }, "non-empty mip layout");
    reject([&] { detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 0, destination, 0, makeLayout(20, 12, 96, 5, 0, 48), 0); }, "non-empty mip layout");
    reject([&] { detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 0, destination, 0, makeLayout(20, 12, 96, 5, 64, 0), 0); }, "non-empty mip layout");
    reject([&] { detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 3, source, 0, destination, 0, layout, 0); }, "unsupported element size");
    reject([&] { detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 20, destination, 40, makeLayout(20, 12, 96, 5, 1024, 48), 0); }, "buffer range exceeds device limits");
    reject([&] { detiler.Dispatch(commands, TextureTileMode::kStandard4KB, 4, source, 0, destination, 0, layout, false, 0, false, DetileWindow{48, 48, 0, 0, 0}); }, "window is empty");
}
