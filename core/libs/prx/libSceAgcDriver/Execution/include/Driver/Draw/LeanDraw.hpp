#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAW_LEANDRAW_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAW_LEANDRAW_HPP

#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawCache.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawRegisterKey.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace AgcDriver::DriverDetail {

enum class LeanVerdict : std::uint8_t { Unknown, Normal, Nothing, Live, Fallback };

struct LeanShape : std::enable_shared_from_this<LeanShape> {
    std::shared_ptr<const DrawDecode> decode;
    DrawShaderSet shaderSet;
    std::uint64_t shape = 0;
    std::uint64_t finalShape = 0;
    std::vector<std::uint64_t> keySuffix;
    std::vector<std::uint64_t> aliasSuffix;
    std::uint32_t reg207 = 0;
    bool skipped = false;
    std::vector<DrawPipeline::Range> writes;
};

struct ShapeRecord {
    std::uint64_t shape = 0;
    std::uint64_t deviceSerial = 0;
    std::uint64_t epoch = 0;
    std::uint64_t lastUse = 0;
    std::uint64_t lastSeq = 0;
    std::array<LeanVerdict, 3> verdicts{};
    bool used = false;
    bool refused = false;
    std::shared_ptr<const LeanShape> built;
};

struct LeanDrawJob {
    const LeanShape* shape;
    std::uint64_t anchor;
    std::uint64_t serial;
    std::uint64_t epoch;
    std::uint64_t packetOffset;
    std::uint64_t drawKey;
    std::uint64_t shapeKey;
    std::uint64_t deviceSerial;
    std::uint32_t queue;
    std::uint32_t opcode;
    std::uint16_t recordFlags;
    Pm4::DrawParameters parameters;
    DrawUserWords words;
};

static_assert(std::is_trivially_copyable_v<LeanDrawJob> && sizeof(LeanDrawJob) <= DrawFinisher::LeanPayloadBytes && alignof(LeanDrawJob) <= 16, "a lean draw job must fit a finisher ring slot as plain bytes");

struct LeanAnchor {
    std::shared_ptr<const ShaderRegistry> registry;
    std::shared_ptr<VulkanDevice> device;
    std::uint64_t firstSeq = 0;
};

struct LeanDrawSink {
    bool prechecked = false;
    bool passed = false;
    DrawVerdict verdict = DrawVerdict::Drawn;
    bool keyed = false;
    std::uint64_t key = 0;
    std::uint64_t shape = 0;
    std::uint64_t identity = 0;
    bool aliasAsked = false;
    std::shared_ptr<const DrawRelocation> relocation;
    std::uint64_t alias = 0;
    bool handed = false;
    DrawFinisher::Pending held;
    std::uint32_t reg207 = 0;
    Pm4::DrawParameters parameters{};
    std::shared_ptr<const DrawDecode> decode;
    std::shared_ptr<DrawEntry> dataEntry;
    const VulkanDevice* device = nullptr;
};

class LeanDraws {
public:
    static constexpr std::size_t Ways = 4;
    static constexpr std::size_t Sets = 2048;
    static constexpr std::size_t Anchors = 128;

    ShapeRecord* Find(std::uint64_t shape, std::uint64_t deviceSerial);
    ShapeRecord& Place(std::uint64_t shape, std::uint64_t deviceSerial, std::uint64_t front);
    void Retire(ShapeRecord& record, std::uint64_t front);
    void Clear(std::uint64_t front);
    void Release(std::uint64_t front, std::uint64_t issued);
    void ReleaseDevice();
    void Reset();
    std::optional<std::uint64_t> AnchorFor(const std::shared_ptr<const ShaderRegistry>& heldRegistry, const std::shared_ptr<VulkanDevice>& heldDevice, std::uint64_t issued);
    const LeanAnchor& AnchorAt(std::uint64_t index) const { return anchors[static_cast<std::size_t>(index % Anchors)]; }

    std::uint64_t stamp = 0;
    std::uint64_t registryEpoch = 0;
    std::uint64_t uses = 0;
    std::shared_ptr<const ShaderRegistry> registry;
    std::shared_ptr<VulkanDevice> device;
    std::uint64_t deviceStores = ~std::uint64_t{0};
    std::uint64_t published = 0;
    LeanDrawSink* sink = nullptr;

private:
    std::vector<ShapeRecord> records;
    std::vector<std::pair<std::shared_ptr<const LeanShape>, std::uint64_t>> retired;
    std::array<LeanAnchor, Anchors> anchors{};
    std::uint64_t anchorHead = 0;
    std::uint64_t anchorTail = 0;
};

std::shared_ptr<DrawDecode> LeanRefreshedDecode(const DrawDecode& from, const DrawUserWords& words);
std::uint64_t LeanExactKey(const LeanShape& shape, const DrawUserWords& words);
std::uint64_t LeanIdentityKey(const LeanShape& shape, const DrawUserWords& words);
void LeanAliasKey(const LeanShape& shape, const DrawUserWords& words, const DrawRelocation* relocation, std::uint64_t deviceSerial, std::uint64_t& alias);

}

#endif
