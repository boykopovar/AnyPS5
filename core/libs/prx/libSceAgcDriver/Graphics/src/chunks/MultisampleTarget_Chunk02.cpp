#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MapImportGate.hpp"
#include <shared_mutex>
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/FirstSampleCopy.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuardSpans.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/SampleOrder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTextureLayout.hpp"
#include "IntermediateRepresentation/IrMetadata/BufferFormat.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "prx/libc/include/PrimedSharedMutex.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <span>
#include <utility>
#include <vector>

namespace AgcDriver::Graphics {
namespace {
std::shared_ptr<MultisampleTarget> snapshotTarget(const Context& context, const ColorTarget& use) {
    TargetsLock lock;
    for (const auto& candidate : targets()) {
        if (!candidate->active->load(std::memory_order_acquire) || candidate->context.device != context.device || !sameTarget(candidate->target, use)) continue;
        const auto& target = candidate->target;
        Require(target.address == use.address && target.mip == use.mip && target.mipCount == use.mipCount && target.mipTail == use.mipTail && target.layers == use.layers && target.depth == use.depth && target.arraySlice == use.arraySlice && target.arrayLastSlice == use.arrayLastSlice && target.depthSlice == use.depthSlice && target.elementBytes == use.elementBytes && target.tileMode == use.tileMode && target.pipeBankXor == use.pipeBankXor && target.dccAddress == use.dccAddress && target.cmaskAddress == use.cmaskAddress, "multisample color readback subresource does not match the resident target");
        return candidate;
    }
    throw std::runtime_error("AGC graphics: multisample color sample readback requires an existing resident target");
}

}

void InstallResidentImageReadGuard_nid_no_patch() {
    static std::once_flag installed;
    std::call_once(installed, [] { GuestMemory::SetReadGuard(&guardReads); });
    UpdateResidentImageUnmapObserver_nid_no_patch();
}

void UpdateResidentImageUnmapObserver_nid_no_patch() {
    static std::mutex mutex;
    std::lock_guard lock(mutex);
    GuestAllocations::GuestAllocationsSetUnmapObserver_nid_postfix(anyGuarded().load(std::memory_order_acquire) || anySingleClears().load(std::memory_order_acquire) || HasResidentDepthGuards_nid_no_patch() || DepthPlanesStale() ? &retireUnmapped : nullptr);
}

std::optional<DccKeys> MultisampledCmaskKeys(std::uint8_t code) {
    const auto tile = code & 0xfu;
    if ((code >> 4u) != tile) return std::nullopt;
    if (tile == 0u) return DccKeys::Clear0000;
    if ((tile >> 2u) == 3u) return DccKeys::Uncompressed;
    return std::nullopt;
}

void MultisampledCmask::NoteFill(std::uint64_t fillAddress, std::size_t fillBytes, std::uint32_t pattern) {
    if (address == 0 || fillAddress >= address + bytes || address >= fillAddress + fillBytes) return;
    if (fillAddress > address || fillAddress + fillBytes < address + bytes) {
        char text[192];
        std::snprintf(text, sizeof(text), "a fill of 0x%llx+0x%zx partly covers the CMASK 0x%llx+0x%llx of a multisampled color target, leaving its tiles mixed", static_cast<unsigned long long>(fillAddress), fillBytes, static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes));
        refusal = text;
        return;
    }
    refusal.clear();
    const auto code = static_cast<std::uint8_t>(pattern);
    if (const auto filled = MultisampledCmaskKeys(code); filled && pattern == code * 0x01010101u) {
        keys = *filled;
    } else {
        char text[224];
        std::snprintf(text, sizeof(text), "a fill of the CMASK 0x%llx+0x%llx of a multisampled color target with 0x%08x leaves its tiles mixed (neither a register color clear 0 nor a color-expanded code such as 0xcc or 0xff)", static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes), pattern);
        refusal = text;
    }
}

void MultisampledCmask::SeedCode(std::optional<std::uint8_t> code) {
    if (const auto seeded = code ? MultisampledCmaskKeys(*code) : std::nullopt) {
        keys = *seeded;
        return;
    }
    char text[192];
    if (code) std::snprintf(text, sizeof(text), "the CMASK 0x%llx of a multisampled color target reads as code 0x%02x, which is not modeled", static_cast<unsigned long long>(address), *code);
    else std::snprintf(text, sizeof(text), "the CMASK 0x%llx of a multisampled color target reads as mixed, which is not modeled", static_cast<unsigned long long>(address));
    refusal = text;
}

void MultisampledCmask::CheckAddress(std::uint64_t cmaskAddress) const {
    if (cmaskAddress == address) return;
    char text[192];
    std::snprintf(text, sizeof(text), "a multisampled color target used CMASK 0x%llx after its first use with CMASK 0x%llx, which is not modeled", static_cast<unsigned long long>(cmaskAddress), static_cast<unsigned long long>(address));
    Require(false, std::string(text));
}

bool MultisampledCmask::TakeClear() {
    Require(refusal.empty(), refusal);
    if (keys != DccKeys::Clear0000) return false;
    keys = DccKeys::Uncompressed;
    return true;
}

std::uint64_t MultisampledColorBytes(const ColorTarget& target) {
    const auto blockLog2 = MultisampledBlockLog2(target.tileMode);
    const auto fragments = storedFragments(target);
    if (blockLog2 == 0 || !std::has_single_bit(target.elementBytes) || !std::has_single_bit(fragments)) return 0;
    const auto fragmentsLog2 = static_cast<std::uint32_t>(std::countr_zero(fragments));
    const auto elementsLog2 = blockLog2 - static_cast<std::uint32_t>(std::countr_zero(target.elementBytes)) - fragmentsLog2;
    const auto widthLog2 = (elementsLog2 + ((fragmentsLog2 & 1u) == 0 ? 1u : 0u)) / 2u;
    const std::uint64_t blockWidth = std::uint64_t{1} << widthLog2;
    const std::uint64_t blockHeight = std::uint64_t{1} << (elementsLog2 - widthLog2);
    return ((target.extent.width + blockWidth - 1) / blockWidth) * ((target.extent.height + blockHeight - 1) / blockHeight) << blockLog2;
}

std::size_t MultisampledDccBytes(const ColorTarget& target) {
    Require(target.samples == 2 || target.samples == 4 || target.samples == 8, "unsupported multisampled DCC fragment count");
    Require(target.tileMode == ColorTileMode::RenderTarget && target.dccPipeAligned, "multisampled DCC requires pipe-aligned SW_64KB_R_X");
    Require(std::has_single_bit(target.elementBytes) && target.elementBytes <= 16, "unsupported multisampled DCC element size");
    Require(target.extent.width != 0 && target.extent.width <= 16384 && target.extent.height != 0 && target.extent.height <= 16384, "invalid multisampled DCC extent");
    Require(target.mipCount == 1 && target.mip == 0 && !target.mipTail && target.depth == 1 && target.depthSlice == 0 && target.arrayLastSlice == 0 && target.arraySlice == 0 && target.layers == 1, "multisampled DCC array, volume or mip contents are not implemented");
    const auto bits = 19u - static_cast<std::uint32_t>(std::countr_zero(target.elementBytes));
    const std::uint64_t width = std::uint64_t{1} << ((bits + 1u) / 2u);
    const std::uint64_t height = std::uint64_t{1} << (bits / 2u);
    return static_cast<std::size_t>(((target.extent.width + width - 1) / width) * ((target.extent.height + height - 1) / height) * 4096u);
}

namespace {

std::optional<std::array<std::uint32_t, 4>> recordedSingleClear(const ColorTarget& target) {
    std::lock_guard lock(singleClearsMutex());
    for (const auto& clear : singleClears()) {
        if (clear.surface == target.surfaceAddress && clear.pipeBankXor == target.pipeBankXor && clear.width == target.extent.width && clear.height == target.extent.height && clear.samples == target.samples) return clear.color;
    }
    return std::nullopt;
}

VkClearColorValue singleClearColor(const ColorTarget& target) {
    const auto color = recordedSingleClear(target);
    Require(color.has_value(), "multisampled DCC comp-to-single keys without a recorded clear color are not implemented");
    const auto elementBytes = static_cast<std::size_t>(target.elementBytes);
    Require(elementBytes == 1 || elementBytes == 2 || elementBytes == 4 || elementBytes == 8 || elementBytes == 16, "multisampled DCC comp-to-single clears of this texel size are not implemented");
    std::array<std::uint32_t, 4> pattern{};
    for (std::size_t offset = 0; offset < sizeof(pattern); offset += elementBytes) std::memcpy(reinterpret_cast<std::byte*>(pattern.data()) + offset, color->data(), elementBytes);
    VkClearColorValue clear{};
    Require(ClearColorForTexel(target.format, target.elementBytes, pattern, clear), "multisampled DCC comp-to-single clear has no exact host value");
    return clear;
}

}

std::optional<MultisampledSingleClear> MatchMultisampledSingleClear(std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, const std::array<std::uint32_t, 3>& numThreads) {
    static constexpr std::array<std::uint32_t, 22> kernel{0xbe800303u, 0xbe830382u, 0xf40c0401u, 0xfa000000u, 0xd7460000u, 0x04010609u, 0xd7460001u, 0x0405060au, 0x7e060208u, 0x7e0c020bu, 0x7e0e0280u, 0xd5690004u, 0x02000100u, 0x7e000205u, 0xd5690005u, 0x02000901u, 0xd7000001u, 0x02000c80u, 0xbf8cc07fu, 0xf0201f38u, 0x00040004u, 0xbf810000u};
    if (userData.size() < 9 || numThreads != std::array<std::uint32_t, 3>{8u, 8u, 1u}) return std::nullopt;
    if (code.size() < kernel.size() || !std::equal(kernel.begin(), kernel.end(), code.begin())) return std::nullopt;
    if (userData[3] == 0 || userData[4] == 0) return std::nullopt;
    return MultisampledSingleClear{userData[2] | (std::uint64_t{2} << 32u), {userData[5], userData[6], userData[7], userData[8]}};
}

void NoteMultisampledSingleClear(const GuestTextureResource& resource, const std::array<std::uint32_t, 4>& color) {
    {
        std::lock_guard lock(singleClearsMutex());
        auto& list = singleClears();
        std::erase_if(list, [&](const RecordedSingleClear& clear) { return clear.surface == resource.baseAddress; });
        list.push_back({resource.baseAddress, resource.pipeBankXor, resource.width, resource.height, resource.samples, color});
        anySingleClears().store(true, std::memory_order_release);
    }
    UpdateResidentImageUnmapObserver_nid_no_patch();
}

bool TakeMultisampledSingleClear(std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, const std::array<std::uint32_t, 3>& numThreads) {
    const auto clear = MatchMultisampledSingleClear(code, userData, numThreads);
    if (!clear) return false;
    std::array<std::uint32_t, 8> words{};
    if (!GuestMemory::Accessible(reinterpret_cast<const void*>(clear->descriptor), sizeof(words))) return false;
    GuestMemory::Read(clear->descriptor, std::as_writable_bytes(std::span(words)));
    const auto resource = DecodeTextureResource(words);
    if (resource.samples < 2) return false;
    NoteMultisampledSingleClear(resource, clear->color);
    return true;
}

VkClearColorValue MultisampledDccClear(const ColorTarget& target, std::span<const std::byte> dcc, std::span<const std::byte> cmask) {
    Require(target.dccAddress != 0 && (target.dccControl & 0x3ff80u) == 0 && (target.pipeBankXor & ~0xcf00u) == 0, "multisampled DCC address, color transform, lossy mode or XOR is not implemented");
    Require(dcc.size() == MultisampledDccBytes(target), "multisampled DCC metadata snapshot size mismatch");
    Require(std::ranges::all_of(dcc, [&](std::byte key) { return key == dcc.front(); }), "mixed multisampled DCC metadata is not implemented");
    Require(target.cmaskAddress == 0 ? cmask.empty() && target.cmaskBytes == 0 : cmask.size() == target.cmaskBytes && target.cmaskBytes == CmaskBytes(target.extent.width, target.extent.height), "multisampled DCC CMASK snapshot size mismatch");
    if (!cmask.empty() && target.cmaskFastClear) {
        const auto code = std::to_integer<std::uint32_t>(cmask.front());
        const auto expanded = target.samples == 2 ? 0xddu : target.samples == 4 ? 0xeeu : 0xffu;
        Require(std::ranges::all_of(cmask, [&](std::byte key) { return key == cmask.front(); }) && (code == 0xccu || code == expanded), "multisampled DCC CMASK compression or register clear is not implemented");
    }
    if (dcc.front() == std::byte{0x10}) return singleClearColor(target);
    DccKeys keys;
    switch (std::to_integer<std::uint32_t>(dcc.front())) {
        case 0x20u: return fastClearColor(target);
        case 0x00u: keys = DccKeys::Clear0000; break;
        case 0x40u: keys = DccKeys::Clear0001; break;
        case 0x80u: keys = DccKeys::Clear1110; break;
        case 0xc0u: keys = DccKeys::Clear1111; break;
        default: throw std::runtime_error("initial multisampled DCC contents are not a supported uniform fast clear");
    }
    std::array<std::byte, 16> texels{};
    Require(FillDccClear(target.format, keys, target.dccAlphaOnMsb, texels), "multisampled DCC clear format is not implemented");
    std::array<std::uint32_t, 4> pattern{};
    std::memcpy(pattern.data(), texels.data(), texels.size());
    VkClearColorValue clear{};
    const bool repeated = std::ranges::all_of(pattern, [&](std::uint32_t word) { return word == pattern.front(); });
    Require(ClearColorForTexel(target.format, target.elementBytes, pattern, clear) || (repeated && MultisampledFillClearColor(target, pattern.front(), clear)), "multisampled DCC fast clear has no exact host value");
    return clear;
}

bool MultisampledFillClearColor(const ColorTarget& target, std::uint32_t fill, VkClearColorValue& clear) {
    const std::array<std::uint32_t, 4> pattern{fill, fill, fill, fill};
    VkFormat unorm = VK_FORMAT_UNDEFINED;
    switch (target.format) {
        case VK_FORMAT_R8G8B8A8_SRGB: unorm = VK_FORMAT_R8G8B8A8_UNORM; break;
        case VK_FORMAT_B8G8R8A8_SRGB: unorm = VK_FORMAT_B8G8R8A8_UNORM; break;
        case VK_FORMAT_A8B8G8R8_SRGB_PACK32: unorm = VK_FORMAT_A8B8G8R8_UNORM_PACK32; break;
        default: return ClearColorForTexel(target.format, target.elementBytes, pattern, clear);
    }
    if (!ClearColorForTexel(unorm, target.elementBytes, pattern, clear)) return false;
    return std::all_of(clear.float32, clear.float32 + 3, [](float value) { return value == 0.0f || value == 1.0f; });
}

MultisampledColorFill::MultisampledColorFill(const ColorTarget& target) : address(target.address), bytes(MultisampledColorBytes(target)) {}

void MultisampledColorFill::NoteFill(std::uint64_t fillAddress, std::size_t fillBytes, std::uint32_t pattern) {
    if (bytes == 0 ? fillAddress > address || address >= fillAddress + fillBytes : fillAddress >= address + bytes || address >= fillAddress + fillBytes) return;
    ++noted;
    char text[192];
    if (bytes == 0) {
        std::snprintf(text, sizeof(text), "a fill of 0x%llx+0x%zx covers the color memory at 0x%llx of a multisampled target whose footprint is not modeled", static_cast<unsigned long long>(fillAddress), fillBytes, static_cast<unsigned long long>(address));
        refusal = text;
        sameRun = 0;
        return;
    }
    if (fillAddress > address || fillAddress + fillBytes < address + bytes) {
        std::snprintf(text, sizeof(text), "a fill of 0x%llx+0x%zx partly covers the color memory 0x%llx+0x%llx of a multisampled target, leaving its samples mixed", static_cast<unsigned long long>(fillAddress), fillBytes, static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes));
        refusal = text;
        sameRun = 0;
        return;
    }
    refusal.clear();
    sameRun = filled == pattern ? sameRun + 1u : 1u;
    filled = pattern;
}

std::optional<std::uint32_t> MultisampledColorFill::TakeFill() {
    Require(refusal.empty(), refusal);
    sameRun = 0;
    return std::exchange(filled, std::nullopt);
}

void MultisampledColorFill::Discard() {
    refusal.clear();
    filled.reset();
    sameRun = 0;
}

bool TakeMultisampledClear(MultisampledCmask& cmask, MultisampledColorFill& colorFill, const ColorTarget& use, VkClearColorValue& clear) {
    if (cmask.TakeClear()) {
        colorFill.Discard();
        clear = fastClearColor(use);
        return true;
    }
    const auto fill = colorFill.TakeFill();
    if (!fill) return false;
    Require(MultisampledFillClearColor(use, *fill, clear), "a fill over the color memory of a multisampled target has no exact clear value in its format");
    return true;
}

void PrepareMultisampleTargetForDraw_nid_no_patch(const Context& context, const ColorTarget& target) {
    {
        TargetsLock lock;
        discardRetiredTargets();
        if (std::ranges::any_of(targets(), [&](const auto& candidate) { return candidate->context.device == context.device && candidate->Serves(target); })) return;
    }
    if (target.dccAddress != 0u || target.cmaskFastClear) return;
    Require(target.cmaskAddress == 0u, "initial multisample color contents with CMASK require a modeled metadata import");
    Require(target.layers == 1u && target.arraySlice == 0u && target.arrayLastSlice == 0u && target.depth == 1u && target.depthSlice == 0u && target.mipCount == 1u && target.mip == 0u && !target.mipTail, "initial multisample array, volume or mip contents are not implemented");
    Require(FindGuestColorTargetFormat(target.format, target.elementBytes).has_value(), "initial multisample color format or element size is unsupported");
    Require(target.elementBytes != 0u && target.elementBytes <= 16u && 16u % target.elementBytes == 0u, "initial multisample color texel size is not implemented");
    const auto bytes = MultisampledColorBytes(target);
    Require(bytes != 0u && bytes <= std::numeric_limits<std::size_t>::max() && target.address <= std::numeric_limits<std::uint64_t>::max() - bytes, "initial multisample color footprint is not implemented");
    const auto requireUnowned = [&] {
        Require(!ResidentDepthGuardOverlaps_nid_no_patch(target.address, static_cast<std::size_t>(bytes)), "initial multisample color contents overlap resident native depth");
        std::shared_lock guardLock(guardedMutex());
        Require(std::ranges::none_of(guarded(), [&](const GuardedRange& range) { return range.active->load(std::memory_order_acquire) && target.address < range.address + range.bytes && range.address < target.address + bytes; }), "initial multisample color contents overlap another native owner");
    };
    requireUnowned();
    Require(GuestMemory::Accessible(reinterpret_cast<const void*>(target.address), static_cast<std::size_t>(bytes)), "initial multisample color contents are not fully mapped");
    std::vector<std::byte> snapshot(static_cast<std::size_t>(bytes));
    GuestMemory::Read(target.address, snapshot);
    requireUnowned();
    Require(GuestMemory::CompareMapped(target.address, snapshot) == GuestMemory::Compare::Equal, "initial multisample color contents changed during capture");
    std::optional<VkClearColorValue> clear;
    if (std::equal(snapshot.begin() + target.elementBytes, snapshot.end(), snapshot.begin())) {
        std::array<std::uint32_t, 4> pattern{};
        auto texels = std::as_writable_bytes(std::span(pattern));
        for (std::size_t offset = 0u; offset < texels.size(); offset += target.elementBytes) std::memcpy(texels.data() + offset, snapshot.data(), target.elementBytes);
        VkClearColorValue value{};
        if (ClearColorForTexel(target.format, target.elementBytes, pattern, value)) clear = value;
    }
    std::shared_ptr<Buffer> upload;
    if (!clear) {
        Require(target.tileMode == ColorTileMode::RenderTarget, "nonuniform initial multisample color tiling is not implemented");
        const MultisampleTextureLayout layout(TextureTileMode::kR64KBX, target.elementBytes, target.samples, target.extent.width, target.extent.height, 1u, target.pipeBankXor);
        Require(layout.Bytes() == bytes && layout.LinearBytes() / target.elementBytes <= std::numeric_limits<std::uint32_t>::max(), "initial multisample color layout and footprint differ");
        upload = stageGuestSamples(context, layout, snapshot);
    }
    TargetsLock lock;
    requireUnowned();
    Require(GuestMemory::CompareMapped(target.address, snapshot) == GuestMemory::Compare::Equal, "initial multisample color contents changed during upload preparation");
    acquire(context, target, clear, std::move(upload), std::move(snapshot));
}

SampleOrderTarget MultisampleTargetOrderTarget(const Context& context, const ColorTarget& target) {
    const DccPublication publication(target);
    TargetsLock lock;
    auto& found = acquire(context, target);
    found.drawn = true;
    ++found.contents;
    return {found.image, found.OrderView(), found.orderFormat, target.extent, target.samples, SampleOrderMethod::Storage, found.shared_from_this()};
}

VkImageView MultisampleTargetView(const Context& context, const ColorTarget& target, VkImage* image) {
    const DccPublication publication(target);
    TargetsLock lock;
    auto& acquired = acquire(context, target);
    acquired.drawn = true;
    ++acquired.contents;
    if (image != nullptr) *image = acquired.image;
    return acquired.View(target.format);
}

VkImageView MultisampleTargetView(const Context& context, const ColorTarget& target, std::shared_ptr<void>& owner, VkImage* image) {
    const DccPublication publication(target);
    TargetsLock lock;
    auto& acquired = acquire(context, target);
    acquired.drawn = true;
    ++acquired.contents;
    owner = acquired.shared_from_this();
    if (image != nullptr) *image = acquired.image;
    return acquired.View(target.format);
}

void KeepMultisampleTarget_nid_no_patch(const std::shared_ptr<void>& owner) {
    std::static_pointer_cast<MultisampleTarget>(owner)->Keep();
}

namespace {

bool redBlueSwapped(VkFormat rendered, VkFormat sampled) {
    const auto pair = [&](VkFormat a, VkFormat b) { return (rendered == a && sampled == b) || (rendered == b && sampled == a); };
    return pair(VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM) || pair(VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB) || pair(VK_FORMAT_B8G8R8A8_SNORM, VK_FORMAT_R8G8B8A8_SNORM) || pair(VK_FORMAT_B8G8R8A8_UINT, VK_FORMAT_R8G8B8A8_UINT) || pair(VK_FORMAT_B8G8R8A8_SINT, VK_FORMAT_R8G8B8A8_SINT);
}

VkComponentMapping swapRedBlue(VkComponentMapping components) {
    const auto swapped = [](VkComponentSwizzle swizzle, VkComponentSwizzle identity) {
        if (swizzle == VK_COMPONENT_SWIZZLE_IDENTITY) swizzle = identity;
        if (swizzle == VK_COMPONENT_SWIZZLE_R) return VK_COMPONENT_SWIZZLE_B;
        if (swizzle == VK_COMPONENT_SWIZZLE_B) return VK_COMPONENT_SWIZZLE_R;
        return swizzle;
    };
    return {swapped(components.r, VK_COMPONENT_SWIZZLE_R), swapped(components.g, VK_COMPONENT_SWIZZLE_G), swapped(components.b, VK_COMPONENT_SWIZZLE_B), swapped(components.a, VK_COMPONENT_SWIZZLE_A)};
}

bool sameStorageForm(const Context& context, VkFormat rendered, VkFormat requested) {
    if (rendered == requested) return true;
    const auto storage = HostStorageFormat(context, rendered);
    return storage != VK_FORMAT_UNDEFINED && storage == HostStorageFormat(context, requested);
}

VkFormat sampledFormat(const Context& context, std::uint32_t guestFormat) {
    return IsRepackedTextureFormat(guestFormat) ? ResolveTextureFormat(guestFormat) : SampledTextureFormat(context, guestFormat);
}

}

std::shared_ptr<Texture> MultisampleTargetTexture(const Context& context, const GuestTextureResource& resource, VkComponentMapping components, VkImageViewType viewType) {
    TargetsLock lock;
    discardRetiredTargets();
    const auto format = sampledFormat(context, resource.format);
    const MultisampleTarget* other = nullptr;
    for (const auto& target : targets()) {
        if (!target->active->load(std::memory_order_acquire)) continue;
        if (target->context.device != context.device || target->target.surfaceAddress != resource.baseAddress) continue;
        const auto& rendered = target->target;
        if (rendered.eqaaFragments != 0) throw std::runtime_error(eqaaReadRefusal("sampling", rendered, resource));
        const bool sameShape = rendered.extent.width == resource.width && rendered.extent.height == resource.height && rendered.samples == resource.samples && rendered.mip == 0;
        if (sameShape && rendered.format == format) return target->Sampled(components, viewType);
        if (sameShape && redBlueSwapped(rendered.format, format)) return target->Sampled(swapRedBlue(components), viewType);
        if (sameShape && rendered.elementBytes == BytesPerElement(resource.format) && target->Reinterpretable(format)) return target->SampledAs(format, components, viewType);
        other = target.get();
    }
    if (other == nullptr) {
        Require(Recorder::Active() != nullptr, "a multisampled texture is only sampled by recorded draws and dispatches");
        return createFilled(context, resource, format, "sampling", false).Sampled(components, viewType);
    }
    char text[320];
    std::snprintf(text, sizeof(text), "AGC graphics: sampling multisampled color target 0x%llx (%ux%u, %u samples, vk format %d) as a %ux%u texture of %u samples and guest format %u (vk %d) is not implemented", static_cast<unsigned long long>(resource.baseAddress), other->target.extent.width, other->target.extent.height, other->target.samples, static_cast<int>(other->target.format), resource.width, resource.height, resource.samples, resource.format, static_cast<int>(format));
    throw std::runtime_error(text);
}

std::shared_ptr<Texture> MultisampleTargetFirstSample(const Context& context, const GuestTextureResource& resource, VkComponentMapping components) {
    TargetsLock lock;
    discardRetiredTargets();
    const auto format = sampledFormat(context, resource.format);
    const MultisampleTarget* other = nullptr;
    for (const auto& target : targets()) {
        if (!target->active->load(std::memory_order_acquire)) continue;
        if (target->context.device != context.device || target->target.surfaceAddress != resource.baseAddress) continue;
        const auto& rendered = target->target;
        const bool sameShape = rendered.extent.width == resource.width && rendered.extent.height == resource.height && rendered.samples == resource.samples && rendered.mip == 0;
        if (sameShape && rendered.format == format) return target->FirstSample(components);
        if (sameShape && redBlueSwapped(rendered.format, format)) return target->FirstSample(swapRedBlue(components));
        other = target.get();
    }
    if (other == nullptr) {
        Require(Recorder::Active() != nullptr, "a multisampled texture is only sampled by recorded draws and dispatches");
        return createFilled(context, resource, format, "sampling", false).FirstSample(components);
    }
    char text[320];
    std::snprintf(text, sizeof(text), "AGC graphics: reading sample 0 of multisampled color target 0x%llx (%ux%u, %u samples, vk format %d) as a %ux%u texture of %u samples and guest format %u (vk %d) is not implemented", static_cast<unsigned long long>(resource.baseAddress), other->target.extent.width, other->target.extent.height, other->target.samples, static_cast<int>(other->target.format), resource.width, resource.height, resource.samples, resource.format, static_cast<int>(format));
    throw std::runtime_error(text);
}

}
