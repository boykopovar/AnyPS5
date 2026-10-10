#include <Testing/Test.hpp>
#include "GraphicsTests.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;
using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

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
};

Push decodePush(const std::vector<std::byte>& bytes) {
    Push push{};
    RequireEqual(bytes.size(), sizeof(Push), "unexpected texture detiling push constant size");
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

const VkCommandBuffer Commands = reinterpret_cast<VkCommandBuffer>(static_cast<std::uintptr_t>(1));
const TileMipLayout MipLayout = makeLayout(20, 12, 96, 5, 64, 48);

struct DetilerFixture {
    MockVulkanSession session;
    Context context = DetilerMockContext();
    TextureDetilerTestAccess access = DetilerMockAccess();
    TextureDetiler detiler{context};
    VkBuffer source = access.makeBuffer(4096);
    VkBuffer destination = access.makeBuffer(4096);
};

DetileWindow mipWindow() {
    DetileWindow window{16, 48, 16, 96, 96};
    window.columnBegin = 8;
    window.columnEnd = 16;
    window.rowBegin = 1;
    window.rowEnd = 2;
    return window;
}

const Case wholeMip{"TextureDetiler_WholeMipDispatch_CoversTheMipFromAlignedBufferOffsets", [] {
    DetilerFixture fixture;
    fixture.detiler.Dispatch(Commands, TextureTileMode::kStandard4KB, 4, fixture.source, 20, fixture.destination, 40, MipLayout, 0);
    const auto capture = fixture.access.lastDispatch();
    Require(capture.groupsX == 3 && capture.groupsY == 2 && capture.groupsZ == 1, "dispatch group counts were computed incorrectly");
    const auto push = decodePush(capture.pushConstants);
    Require(push.srcBase == 4 && push.dstBase == 8, "push constant buffer bases must account for storage buffer offset alignment");
    Require(push.width == 20 && push.height == 12, "push constant dimensions changed");
    Require(push.pitchBytes == 96 && push.blocksPerRow == 5, "push constant row layout changed");
    Require(push.tail == 0 && push.tailX == 0 && push.tailY == 0, "push constant tail fields must reflect a non-tail mip");
    RequireEqual(push.elementBytes, 4u, "push constant element size changed");
    Require(push.rangeBegin == 0 && push.rangeEnd == 0xffffffffu && push.tiledBase == 0 && push.linearBase == 0, "a whole-mip dispatch must move every element out of whole buffers");
    Require(push.columnBegin == 0 && push.rowBegin == 0, "a whole-mip dispatch starts at the mip's origin");
    Require(capture.sourceBuffer == fixture.source && capture.destinationBuffer == fixture.destination, "dispatch bound the wrong source or destination buffer");
    Require(capture.sourceOffset == 16 && capture.sourceRange == 68, "source descriptor offset or range computed incorrectly");
    Require(capture.destinationOffset == 32 && capture.destinationRange == 56, "destination descriptor offset or range computed incorrectly");
    RequireEqual(fixture.access.pipelineCount(), 1u, "the first dispatch must create exactly one compute pipeline");
}};

const Case windowedDetile{"TextureDetiler_WindowedDetile_DispatchesAndBindsOnlyTheWindow", [] {
    DetilerFixture fixture;
    fixture.detiler.Dispatch(Commands, TextureTileMode::kStandard4KB, 4, fixture.source, 20, fixture.destination, 40, MipLayout, false, 0, false, mipWindow());
    const auto capture = fixture.access.lastDispatch();
    Require(capture.groupsX == 1 && capture.groupsY == 1 && capture.groupsZ == 1, "a windowed detile must dispatch only the window's workgroups");
    const auto push = decodePush(capture.pushConstants);
    Require(push.rangeBegin == 16 && push.rangeEnd == 48 && push.tiledBase == 16 && push.linearBase == 96, "windowed detile push constants must carry the window");
    Require(push.columnBegin == 8 && push.rowBegin == 1, "windowed detile push constants must carry the grid origin");
    Require(push.width == 20 && push.height == 12, "a window must not change the mip's dimensions");
    Require(capture.sourceOffset == 16 && capture.sourceRange == 4 + 32, "a windowed detile reads the window's tiled bytes only");
    Require(capture.destinationOffset == 32 && capture.destinationRange == 8 + 96, "a windowed detile writes the window's linear rows only");
}};

const Case windowedRetile{"TextureDetiler_WindowedRetile_UsesItsOwnPipelineAndBindsOnlyTheWindow", [] {
    DetilerFixture fixture;
    fixture.detiler.Dispatch(Commands, TextureTileMode::kStandard4KB, 4, fixture.source, 20, fixture.destination, 40, MipLayout, false, 0, false, mipWindow());
    const auto detilePipelines = fixture.access.pipelineCount();
    fixture.detiler.Dispatch(Commands, TextureTileMode::kStandard4KB, 4, fixture.source, 20, fixture.destination, 40, MipLayout, true, 0, false, mipWindow());
    const auto capture = fixture.access.lastDispatch();
    Require(capture.groupsX == 1 && capture.groupsY == 1 && capture.groupsZ == 1, "a windowed retile must dispatch only the window's workgroups");
    const auto push = decodePush(capture.pushConstants);
    Require(push.rangeBegin == 16 && push.rangeEnd == 48 && push.tiledBase == 16 && push.linearBase == 96, "windowed retile push constants must carry the window");
    Require(push.columnBegin == 8 && push.rowBegin == 1, "windowed retile push constants must carry the grid origin");
    Require(capture.sourceOffset == 16 && capture.sourceRange == 4 + 96, "a windowed retile reads the window's linear rows only");
    Require(capture.destinationOffset == 32 && capture.destinationRange == 8 + 32, "a windowed retile writes the window's tiled bytes only");
    RequireEqual(fixture.access.pipelineCount(), detilePipelines + 1, "a retile pipeline is separate from the detile pipeline");
}};

const Case slabSourcedDetile{"TextureDetiler_SlabSourcedWindow_ReadsTheWindowAtTheSlabOffset", [] {
    DetilerFixture fixture;
    fixture.detiler.Dispatch(Commands, TextureTileMode::kStandard4KB, 4, fixture.source, 1000, fixture.destination, 40, MipLayout, false, 0, false, mipWindow());
    const auto capture = fixture.access.lastDispatch();
    Require(capture.groupsX == 1 && capture.groupsY == 1 && capture.groupsZ == 1, "a slab-sourced detile must dispatch only the window's workgroups");
    const auto push = decodePush(capture.pushConstants);
    Require(push.rangeBegin == 16 && push.rangeEnd == 48 && push.tiledBase == 16 && push.linearBase == 96, "slab-sourced detile push constants must carry the window");
    Require(push.columnBegin == 8 && push.rowBegin == 1, "slab-sourced detile push constants must carry the grid origin");
    Require(push.srcBase == 8 && capture.sourceOffset == 992 && capture.sourceRange == 8 + 32, "a slab-sourced detile reads the window's tiled bytes at the slab offset");
    Require(capture.destinationOffset == 32 && capture.destinationRange == 8 + 96, "a slab-sourced detile writes the same linear rows as the import form");
}};

const Case rowWindow{"TextureDetiler_RowWindow_DispatchesWholeRowsFromItsFirstRow", [] {
    DetilerFixture fixture;
    DetileWindow rows{0, 64, 0, 0, 48};
    rows.rowBegin = 8;
    fixture.detiler.Dispatch(Commands, TextureTileMode::kStandard4KB, 4, fixture.source, 0, fixture.destination, 0, MipLayout, false, 0, false, rows);
    const auto capture = fixture.access.lastDispatch();
    Require(capture.groupsX == 3 && capture.groupsY == 1, "a row window must dispatch whole rows from its first row");
    RequireRejection([&] {
        DetileWindow outside{0, 64, 0, 0, 0};
        outside.rowBegin = 12;
        fixture.detiler.Dispatch(Commands, TextureTileMode::kStandard4KB, 4, fixture.source, 0, fixture.destination, 0, MipLayout, false, 0, false, outside);
    }, "window lies outside the mip");
}};

const Case pipelineCache{"TextureDetiler_PipelineCache_CreatesOnePipelinePerTileModeAndElementSize", [] {
    DetilerFixture fixture;
    const auto small = makeLayout(8, 8, 32, 2, 32, 32);
    fixture.detiler.Dispatch(Commands, TextureTileMode::kStandard4KB, 4, fixture.source, 0, fixture.destination, 0, MipLayout, 0);
    const auto first = fixture.access.pipelineCount();
    fixture.detiler.Dispatch(Commands, TextureTileMode::kStandard4KB, 4, fixture.source, 0, fixture.destination, 0, small, 0);
    RequireEqual(fixture.access.pipelineCount(), first, "dispatching with the same tile mode and element size must reuse the cached pipeline");
    fixture.detiler.Dispatch(Commands, TextureTileMode::kStandard4KB, 8, fixture.source, 0, fixture.destination, 0, small, 0);
    RequireEqual(fixture.access.pipelineCount(), first + 1, "a different element size must create a new compute pipeline");
    fixture.detiler.Dispatch(Commands, TextureTileMode::kLinear, 4, fixture.source, 0, fixture.destination, 0, small, 0);
    RequireEqual(fixture.access.pipelineCount(), first + 2, "a different tile mode must create a new compute pipeline");
    fixture.detiler.Dispatch(Commands, TextureTileMode::kStandard4KB, 4, fixture.source, 0, fixture.destination, 0, small, 0);
    RequireEqual(fixture.access.pipelineCount(), first + 2, "reusing an earlier tile mode and element size must not create another pipeline");
    fixture.detiler.Dispatch(Commands, TextureTileMode::RenderTarget64KB, 4, fixture.source, 0, fixture.destination, 0, MipLayout, false, 13);
    RequireEqual(decodePush(fixture.access.lastDispatch().pushConstants).slice, 13u, "render target detiling must preserve the absolute array layer for XOR addressing");
    RequireEqual(fixture.access.pipelineCount(), first + 3, "render target detiling must use a separate pipeline");
}};

const Case swizzleFamilies{"TextureDetiler_XorTileModes_SelectTheirSwizzleFamily", [] {
    DetilerFixture fixture;
    fixture.detiler.Dispatch(Commands, TextureTileMode::RenderTarget64KB, 4, fixture.source, 0, fixture.destination, 0, MipLayout, false, 13);
    const auto specialization = fixture.access.lastSpecialization();
    Require(specialization[0] == 4 && specialization[1] == 65536 && specialization[2] == 2, "render target detiling must select its own swizzle family");
    fixture.detiler.Dispatch(Commands, TextureTileMode::kD4KBX, 4, fixture.source, 0, fixture.destination, 0, MipLayout, false, 0);
    const auto equationSpecialization = fixture.access.lastSpecialization();
    Require(equationSpecialization[0] == 4 && equationSpecialization[1] == 4096 && equationSpecialization[2] == 2, "SW_4KB_D_X detiling must select the equation family over 4 KiB blocks");
}};

const Case invalidDispatches{"TextureDetiler_InvalidDispatch_IsRejected", [] {
    DetilerFixture fixture;
    auto& detiler = fixture.detiler;
    const auto source = fixture.source;
    const auto destination = fixture.destination;
    RequireRejection([&] { detiler.Dispatch(VK_NULL_HANDLE, TextureTileMode::kStandard4KB, 4, source, 0, destination, 0, MipLayout, 0); }, "active command buffer");
    RequireRejection([&] { detiler.Dispatch(Commands, TextureTileMode::kStandard4KB, 4, VK_NULL_HANDLE, 0, destination, 0, MipLayout, 0); }, "source and destination buffers");
    RequireRejection([&] { detiler.Dispatch(Commands, TextureTileMode::kStandard4KB, 4, source, 0, VK_NULL_HANDLE, 0, MipLayout, 0); }, "source and destination buffers");
    RequireRejection([&] { detiler.Dispatch(Commands, TextureTileMode::kStandard4KB, 4, source, 0, destination, 0, makeLayout(0, 12, 96, 5, 64, 48), 0); }, "non-empty mip layout");
    RequireRejection([&] { detiler.Dispatch(Commands, TextureTileMode::kStandard4KB, 4, source, 0, destination, 0, makeLayout(20, 0, 96, 5, 64, 48), 0); }, "non-empty mip layout");
    RequireRejection([&] { detiler.Dispatch(Commands, TextureTileMode::kStandard4KB, 4, source, 0, destination, 0, makeLayout(20, 12, 96, 5, 0, 48), 0); }, "non-empty mip layout");
    RequireRejection([&] { detiler.Dispatch(Commands, TextureTileMode::kStandard4KB, 4, source, 0, destination, 0, makeLayout(20, 12, 96, 5, 64, 0), 0); }, "non-empty mip layout");
    RequireRejection([&] { detiler.Dispatch(Commands, TextureTileMode::kStandard4KB, 3, source, 0, destination, 0, MipLayout, 0); }, "unsupported element size");
    RequireRejection([&] { detiler.Dispatch(Commands, TextureTileMode::kStandard4KB, 4, source, 20, destination, 40, makeLayout(20, 12, 96, 5, 1024, 48), 0); }, "buffer range exceeds device limits");
    RequireRejection([&] { detiler.Dispatch(Commands, TextureTileMode::kStandard4KB, 4, source, 0, destination, 0, MipLayout, false, 0, false, DetileWindow{48, 48, 0, 0, 0}); }, "window is empty");
}};

} // namespace
