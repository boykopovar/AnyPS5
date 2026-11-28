#include "prx/libSceAgcDriver/Graphics/include/FmaskTexture.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureSwizzleEquations.hpp"
#include "IntermediateRepresentation/IrMetadata/BufferFormat.hpp"
#include <bit>
#include <limits>

namespace AgcDriver::Graphics {
namespace {

struct Plane {
    GuestMemory::UnchangedQuery query;
    std::vector<std::byte> bytes;
};

void settle(Plane& plane) {
    const auto [address, bytes, generation] = plane.query;
    Require(address != 0u && bytes != 0u && address <= std::numeric_limits<std::uint64_t>::max() - bytes, "standalone FMASK plane range overflows");
    RequireNoMultisampleTarget(address, bytes, "a standalone FMASK read");
    if (auto* recorder = Recorder::Active()) recorder->SyncThrough(address, bytes);
    StorageTexture::FlushPending(address, bytes);
    GuestMemory::FlushGpuWrites(address, bytes);
    Require(GuestMemory::Accessible(reinterpret_cast<const void*>(address), bytes), "standalone FMASK requires accessible planes");
    GuestMemory::MaterializeKnownZero(address, bytes);
}

bool holds(std::array<Plane, 2>& planes) {
    std::vector<GuestMemory::UnchangedQuery> watched;
    for (auto& plane : planes) {
        GuestMemory::CollectWritesUncached(plane.query.address, plane.query.bytes);
        if (plane.query.generation != 0u) watched.push_back(plane.query);
        else if (GuestMemory::CompareMapped(plane.query.address, plane.bytes) != GuestMemory::Compare::Equal) return false;
    }
    return watched.empty() || GuestMemory::UnchangedSinceAll(watched);
}

GuestTextureResource snapshotResource(const GuestTextureResource& resource) {
    Require(resource.format == static_cast<std::uint32_t>(ShaderRecompiler::IrBufferFormat::Format8UInt) && resource.fmaskCmaskAddress != 0u, "standalone FMASK requires four samples and four fragments");
    Require(resource.dimension == TextureDimension::k2D && resource.baseArray == 0u && resource.depthOrLastArray == 0u, "standalone FMASK arrays are not implemented");
    Require(resource.tileMode == TextureTileMode::kZ64KBX && (resource.pipeBankXor & ~0xcf00u) == 0u, "standalone FMASK swizzle or pipe/bank XOR is not implemented");
    Require(resource.width != 0u && resource.width <= 16384u && resource.height != 0u && resource.height <= 16384u, "invalid standalone FMASK extent");
    Require(resource.mipCount == 1u && resource.allocatedMipCount == 1u && resource.baseLevel == 0u && resource.lastLevel == 0u && resource.minLod == 0u, "standalone FMASK mip fields are not implemented");
    Require(resource.dccAddress == 0u && resource.dccPipeAligned && !resource.iterate256, "standalone FMASK CMASK layout is not implemented");
    Require((resource.baseAddress & 65535u) == 0u && (resource.fmaskCmaskAddress & 4095u) == 0u, "standalone FMASK plane alignment is not implemented");
    const bool identity = resource.dstSelX == 4u && resource.dstSelY == 5u && resource.dstSelZ == 6u && resource.dstSelW == 7u;
    for (const auto selector : {resource.dstSelX, resource.dstSelY, resource.dstSelZ, resource.dstSelW}) Require(identity || selector == 0u || selector == 1u || selector == 4u, "standalone FMASK selects an unverified channel");
    auto snapshot = resource;
    snapshot.tileMode = TextureTileMode::kLinear;
    snapshot.pipeBankXor = 0u;
    snapshot.fmaskCmaskAddress = 0u;
    snapshot.dccAlphaOnMsb = false;
    snapshot.dccPipeAligned = false;
    return snapshot;
}

std::vector<std::byte> decode(const GuestTextureResource& resource, const GuestTextureResource& snapshot, const std::array<Plane, 2>& planes) {
    const auto raw = DescribeSurface(resource);
    const auto linear = DescribeSurface(snapshot);
    const CmaskLayout metadata(resource.width, resource.height);
    const auto* equation = FindTextureSwizzleEquation(24u, 1u);
    Require(equation != nullptr, "standalone FMASK swizzle equation is unavailable");
    const auto& mip = raw.mips.front();
    const auto& outputMip = linear.mips.front();
    const auto block = ThinBlockLayout(TextureTileMode::kZ64KBX, 1u);
    std::vector<std::byte> output(linear.guestBytes);
    for (std::uint32_t y = 0u; y < resource.height; ++y) {
        for (std::uint32_t x = 0u; x < resource.width; ++x) {
            const auto nibble = metadata.Nibble(x / 8u, y / 8u) ^ ((resource.pipeBankXor & 0xf00u) << 1u);
            Require(nibble / 2u < planes[1].bytes.size(), "standalone FMASK CMASK address exceeds its snapshot");
            const auto code = (std::to_integer<std::uint32_t>(planes[1].bytes[nibble / 2u]) >> ((nibble & 1u) * 4u)) & 15u;
            Require(code == 0xcu || code == 0xeu, "standalone FMASK CMASK compression code is not implemented");
            std::byte value{0};
            if (code == 0xeu) {
                std::uint32_t offset = resource.pipeBankXor;
                const auto coordinate = ((x + (mip.tail ? mip.tailX : 0u)) & 0xfffu) | (((y + (mip.tail ? mip.tailY : 0u)) & 0xfffu) << 12u);
                for (std::uint32_t bit = 0u; bit < equation->bits.size(); ++bit) offset ^= (static_cast<std::uint32_t>(std::popcount(coordinate & equation->bits[bit])) & 1u) << bit;
                const auto tile = mip.tail ? 0u : static_cast<std::uint64_t>(y / block[2]) * mip.blocksPerRow + x / block[1];
                const auto address = mip.tiledOffset + tile * block[0] + offset;
                Require(address < planes[0].bytes.size(), "standalone FMASK address exceeds its snapshot");
                value = planes[0].bytes[address];
            }
            const auto destination = outputMip.tiledOffset + static_cast<std::uint64_t>(y) * outputMip.pitchBytes + x;
            Require(destination < output.size(), "standalone FMASK linear address exceeds its snapshot");
            output[destination] = value;
        }
    }
    return output;
}

}

std::shared_ptr<Texture> StandaloneFmaskTexture_nid_no_patch(const Context& context, const GuestTextureResource& resource, VkComponentMapping components) {
    const auto snapshot = snapshotResource(resource);
    auto planes = std::make_shared<std::array<Plane, 2>>();
    (*planes)[0].query = {resource.baseAddress, static_cast<std::size_t>(SurfaceGuestBytes(resource)), 0u};
    (*planes)[1].query = {resource.fmaskCmaskAddress, CmaskLayout(resource.width, resource.height).Bytes(), 0u};
    for (auto& plane : *planes) settle(plane);
    const auto& fmask = (*planes)[0].query;
    const auto& cmask = (*planes)[1].query;
    Require(fmask.address + fmask.bytes <= cmask.address || cmask.address + cmask.bytes <= fmask.address, "standalone FMASK planes overlap");
    for (auto& plane : *planes) {
        plane.query.generation = GuestMemory::CollectWritesUncached(plane.query.address, plane.query.bytes);
        plane.bytes.resize(plane.query.bytes);
        GuestMemory::Read(plane.query.address, plane.bytes);
    }
    Require(holds(*planes), "guest writes raced standalone FMASK import");
    const auto bytes = decode(resource, snapshot, *planes);
    auto texture = std::make_shared<Texture>(context, *context.detiler, snapshot, components, bytes);
    Require(holds(*planes), "guest writes raced standalone FMASK import");
    texture->MarkRefreshedPerUse();
    texture->SetReuseLookup([context, resource, components, planes, weak = std::weak_ptr<Texture>(texture)] {
        if (auto native = MultisampleTargetFmaskTexture_nid_no_patch(context, resource, components, 4u)) return native;
        for (auto& plane : *planes) settle(plane);
        return holds(*planes) ? weak.lock() : nullptr;
    });
    return texture;
}

}
