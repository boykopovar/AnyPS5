#include "prx/libSceVideodec2/include/InventoryExports.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace {

constexpr int Videodec2ErrorStructureSize = static_cast<int>(0x811D0101u);
constexpr int Videodec2ErrorArgumentPointer = static_cast<int>(0x811D0102u);
constexpr int Videodec2ErrorDecoderHandle = static_cast<int>(0x811D0103u);
constexpr int Videodec2ErrorFrameBufferSize = static_cast<int>(0x811D0106u);
constexpr int Videodec2ErrorFrameBufferPointer = static_cast<int>(0x811D0107u);
constexpr int Videodec2ErrorFrameBufferAlignment = static_cast<int>(0x811D0108u);
constexpr int Videodec2ErrorSliceInput = static_cast<int>(0x811D01E0u);

constexpr std::uint64_t SliceDecoderMagic = 0xA824D9799010A455ull;
constexpr std::size_t DecoderMagicOffset = 0x68;
constexpr std::uint64_t SliceSyncResultSize = 0x18;

struct SliceInput {
    std::uint64_t thisSize;
    const std::uint8_t* auData;
    std::uint64_t auSize;
    std::uint64_t ptsData;
    std::uint64_t dtsData;
    std::uint64_t attachedData;
    std::uint32_t field30;
    std::uint32_t field34;
    std::uint8_t field38;
    std::uint8_t reserved39;
    std::uint8_t reserved3a;
    std::uint8_t reserved3b;
    std::uint32_t reserved3c;
};

static_assert(sizeof(SliceInput) == 0x40);
static_assert(sizeof(SceVideodec2::FrameBuffer) == 0x20);
static_assert(sizeof(SceVideodec2::OutputInfo) == 0x38);
static_assert(offsetof(SceVideodec2::OutputInfo, isValid) == 8);

template <typename TValue>
bool CopyInGuest(TValue& destination, const TValue* source) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&destination, source, sizeof(TValue));
    errno = savedErrno;
    return copied;
}

template <typename TValue>
bool CopyOutGuest(TValue* destination, const TValue& source) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, &source, sizeof(TValue));
    errno = savedErrno;
    return copied;
}

bool ValidateDecoder(const void* decoder) {
    if (!decoder) return false;
    const auto address = reinterpret_cast<std::uintptr_t>(decoder);
    if (address > std::numeric_limits<std::uintptr_t>::max() - DecoderMagicOffset) return false;
    std::uint64_t magic = 0;
    const auto* magicPtr = reinterpret_cast<const std::uint64_t*>(static_cast<const std::uint8_t*>(decoder) + DecoderMagicOffset);
    if (!CopyInGuest(magic, magicPtr)) return false;
    return magic == SliceDecoderMagic;
}

int ValidateSliceInputFields(const SliceInput& input) {
    if (!input.auData || input.auSize == 0) return Videodec2ErrorSliceInput;
    if (input.field30 == 0 || input.field34 == 0) return Videodec2ErrorSliceInput;
    if (input.reserved39 != 0 || input.reserved3a != 0 || input.reserved3b != 0 || input.reserved3c != 0) {
        return Videodec2ErrorSliceInput;
    }
    return 0;
}

}

extern "C" {

int APS5_VABI sceVideodec2DecodeFirstSlice(const void* decoder, const void* input, SceVideodec2::FrameBuffer* frameBuffer) {
    if (!ValidateDecoder(decoder)) return Videodec2ErrorDecoderHandle;
    if (!input || !frameBuffer) return Videodec2ErrorArgumentPointer;
    std::uint64_t inputSize = 0;
    if (!CopyInGuest(inputSize, static_cast<const std::uint64_t*>(input))) return Videodec2ErrorArgumentPointer;
    std::uint64_t frameSize = 0;
    if (!CopyInGuest(frameSize, reinterpret_cast<const std::uint64_t*>(frameBuffer))) return Videodec2ErrorArgumentPointer;
    if (inputSize != sizeof(SliceInput) || frameSize != sizeof(SceVideodec2::FrameBuffer)) return Videodec2ErrorStructureSize;
    SliceInput localInput{};
    if (!CopyInGuest(localInput, static_cast<const SliceInput*>(input))) return Videodec2ErrorArgumentPointer;
    SceVideodec2::FrameBuffer localFrame{};
    if (!CopyInGuest(localFrame, frameBuffer)) return Videodec2ErrorArgumentPointer;
    if (const int status = ValidateSliceInputFields(localInput); status != 0) return status;
    if (!localFrame.frameBuffer) return Videodec2ErrorFrameBufferPointer;
    if ((reinterpret_cast<std::uintptr_t>(localFrame.frameBuffer) & 0xffu) != 0) return Videodec2ErrorFrameBufferAlignment;
    if (localFrame.frameBufferSize == 0) return Videodec2ErrorFrameBufferSize;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideodec2DecodeNextSlice(const void* decoder, const void* input) {
    if (!ValidateDecoder(decoder)) return Videodec2ErrorDecoderHandle;
    if (!input) return Videodec2ErrorArgumentPointer;
    std::uint64_t inputSize = 0;
    if (!CopyInGuest(inputSize, static_cast<const std::uint64_t*>(input))) return Videodec2ErrorArgumentPointer;
    if (inputSize != sizeof(SliceInput)) return Videodec2ErrorStructureSize;
    SliceInput localInput{};
    if (!CopyInGuest(localInput, static_cast<const SliceInput*>(input))) return Videodec2ErrorArgumentPointer;
    if (const int status = ValidateSliceInputFields(localInput); status != 0) return status;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideodec2SyncAllSlice(const void* decoder, SceVideodec2::OutputInfo* output, const void* syncResult) {
    if (!ValidateDecoder(decoder)) return Videodec2ErrorDecoderHandle;
    if (!output) return Videodec2ErrorArgumentPointer;
    std::uint64_t outputSize = 0;
    if (!CopyInGuest(outputSize, reinterpret_cast<const std::uint64_t*>(output))) return Videodec2ErrorArgumentPointer;
    if (outputSize != sizeof(SceVideodec2::OutputInfo)) return Videodec2ErrorStructureSize;
    const std::uint8_t zero = 0;
    if (!CopyOutGuest(reinterpret_cast<std::uint8_t*>(output) + offsetof(SceVideodec2::OutputInfo, isValid), zero)) {
        return Videodec2ErrorArgumentPointer;
    }
    if (!syncResult) return Videodec2ErrorArgumentPointer;
    std::uint64_t syncResultSize = 0;
    if (!CopyInGuest(syncResultSize, static_cast<const std::uint64_t*>(syncResult))) return Videodec2ErrorArgumentPointer;
    if (syncResultSize != SliceSyncResultSize) return Videodec2ErrorStructureSize;
    NotImplemented_nid_no_patch(__func__);
}

}
