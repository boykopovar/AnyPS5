#include "prx/libSceVdecCore.native/include/Api.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int VdecCoreErrorInvalidArgument = static_cast<int>(0x80C00001);
constexpr int VdecCoreErrorInvalidAddress = static_cast<int>(0x80C00007);
constexpr int VdecCoreErrorMaxMapCount = static_cast<int>(0x80C00015);
constexpr std::uint32_t MaxMappedMemoryCalls = 16;
constexpr std::uintptr_t SvpInputAlignmentMask = 0x3f;

bool CopyGuestIn(void* dst, const void* src, std::size_t size) {
    const int saved = errno;
    const bool ok = GuestCopyIn_nid_no_patch(dst, src, size);
    errno = saved;
    return ok;
}

bool CopyGuestOut(void* dst, const void* src, std::size_t size) {
    const int saved = errno;
    const bool ok = GuestCopyOut_nid_no_patch(dst, src, size);
    errno = saved;
    return ok;
}

struct MapState {
    std::mutex mutex;
    std::unordered_map<void*, std::uint32_t> counts;
};

MapState& State() {
    static MapState state;
    return state;
}

int CreateDecoderVariant(const VdecCoreConfigInfo* config, const VdecCoreMemoryInfo* memory, void** decoder) {
    if (config == nullptr || decoder == nullptr) return VdecCoreErrorInvalidArgument;
    void* created = nullptr;
    const int status = sceVdecCoreCreateDecoder(config, memory, nullptr, &created);
    if (status != 0) return status;
    if (!CopyGuestOut(decoder, &created, sizeof(created))) {
        sceVdecCoreDeleteDecoder(created);
        return VdecCoreErrorInvalidArgument;
    }
    auto& state = State();
    std::lock_guard lock(state.mutex);
    state.counts[created] = 0;
    return 0;
}

}

extern "C" {

int APS5_VABI sceVdecCoreCreateDecoderBid(std::int32_t, const VdecCoreConfigInfo* config, const VdecCoreMemoryInfo* memory, void** decoder) {
    return CreateDecoderVariant(config, memory, decoder);
}

int APS5_VABI sceVdecCoreCreateDecoderSvp(std::int32_t, const VdecCoreConfigInfo* config, const VdecCoreMemoryInfo* memory, void** decoder) {
    return CreateDecoderVariant(config, memory, decoder);
}

int APS5_VABI sceVdecCoreMapMemory(void* decoder, void* address, std::uint64_t size) {
    if (decoder == nullptr || address == nullptr || size == 0) return VdecCoreErrorInvalidArgument;
    auto& state = State();
    std::lock_guard lock(state.mutex);
    const auto found = state.counts.find(decoder);
    if (found != state.counts.end() && found->second >= MaxMappedMemoryCalls) {
        return VdecCoreErrorMaxMapCount;
    }
    const VdecCoreMemoryBlock block{address, size, 256, 0};
    const int status = sceVdecCoreMapMemoryBlock(decoder, &block, 1);
    if (status == 0) {
        ++state.counts[decoder];
    }
    return status;
}

int APS5_VABI sceVdecCoreSetDecodeInputSvp(void* decoder, const VdecCoreSvpInputMeta* meta, const VdecCoreSvpInputBuffer* svpBuffer, std::uint32_t* status) {
    if (decoder == nullptr || meta == nullptr || svpBuffer == nullptr) return VdecCoreErrorInvalidArgument;
    VdecCoreSvpInputMeta localMeta{};
    if (!CopyGuestIn(&localMeta, meta, sizeof(localMeta))) return VdecCoreErrorInvalidArgument;
    VdecCoreSvpInputBuffer localBuffer{};
    if (!CopyGuestIn(&localBuffer, svpBuffer, sizeof(localBuffer))) return VdecCoreErrorInvalidArgument;
    if ((reinterpret_cast<std::uintptr_t>(localBuffer.auData) & SvpInputAlignmentMask) != 0) {
        return VdecCoreErrorInvalidAddress;
    }
    const VdecCoreInputData input{
        localBuffer.auData,
        localBuffer.auSize,
        localMeta.pts,
        localMeta.dts,
        localMeta.userData,
        localMeta.flags,
        localMeta.streamId,
    };
    return sceVdecCoreSetDecodeInput(decoder, &input, nullptr, status);
}

}
