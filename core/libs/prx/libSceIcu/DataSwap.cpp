#include "prx/libSceIcu/include/DataSwap.hpp"
#include "SceTypes.hpp"
#include "dictionarydata.h"
#include "normalizer2impl.h"
#include "rbbidata.h"
#include "ucmndata.h"
#include "ucnv_io.h"
#include "ucol_swp.h"
#include "udataswp.h"
#include "uprops.h"
#include <cstdarg>
#include <cstddef>
#include <cstdint>

namespace {

struct GuestSwapper;

using GuestRead16 = std::uint16_t (APS5_VABI*)(std::uint16_t);
using GuestRead32 = std::uint32_t (APS5_VABI*)(std::uint32_t);
using GuestCompare = std::int32_t (APS5_VABI*)(const GuestSwapper*, const char*, std::int32_t, const char16_t*, std::int32_t);
using GuestWrite16 = void (APS5_VABI*)(std::uint16_t*, std::uint16_t);
using GuestWrite32 = void (APS5_VABI*)(std::uint32_t*, std::uint32_t);
using GuestSwap = std::int32_t (APS5_VABI*)(const GuestSwapper*, const void*, std::int32_t, void*, UErrorCode*);
using GuestPrint = void (APS5_VABI*)(void*, const char*, VaList*);

struct GuestSwapper {
    UBool inIsBigEndian;
    std::uint8_t inCharset;
    UBool outIsBigEndian;
    std::uint8_t outCharset;
    GuestRead16 readUInt16;
    GuestRead32 readUInt32;
    GuestCompare compareInvChars;
    GuestWrite16 writeUInt16;
    GuestWrite32 writeUInt32;
    GuestSwap swapArray16;
    GuestSwap swapArray32;
    GuestSwap swapArray64;
    GuestSwap swapInvChars;
    GuestPrint printError;
    void* printErrorContext;
};

static_assert(sizeof(GuestSwapper) == sizeof(UDataSwapper));
static_assert(offsetof(GuestSwapper, readUInt16) == offsetof(UDataSwapper, readUInt16));
static_assert(offsetof(GuestSwapper, swapInvChars) == offsetof(UDataSwapper, swapInvChars));
static_assert(offsetof(GuestSwapper, printErrorContext) == offsetof(UDataSwapper, printErrorContext));

thread_local const GuestSwapper* Guest = nullptr;

std::uint16_t U_CALLCONV ReadUInt16(std::uint16_t value) {
    return Guest->readUInt16(value);
}

std::uint32_t U_CALLCONV ReadUInt32(std::uint32_t value) {
    return Guest->readUInt32(value);
}

std::int32_t U_CALLCONV CompareInvChars(const UDataSwapper*, const char* outString, std::int32_t outLength,
    const UChar* localString, std::int32_t localLength) {
    return Guest->compareInvChars(Guest, outString, outLength, localString, localLength);
}

void U_CALLCONV WriteUInt16(std::uint16_t* destination, std::uint16_t value) {
    Guest->writeUInt16(destination, value);
}

void U_CALLCONV WriteUInt32(std::uint32_t* destination, std::uint32_t value) {
    Guest->writeUInt32(destination, value);
}

template<GuestSwap GuestSwapper::*Member>
std::int32_t U_CALLCONV Swap(const UDataSwapper*, const void* inData, std::int32_t length, void* outData, UErrorCode* status) {
    return (Guest->*Member)(Guest, inData, length, outData, status);
}

void U_CALLCONV PrintError(void* context, const char* format, va_list arguments) {
#ifdef _WIN32
    VaList list{48, 176, arguments, nullptr};
    Guest->printError(context, format, &list);
#else
    Guest->printError(context, format, reinterpret_cast<VaList*>(arguments));
#endif
}

class Session {
public:
    explicit Session(const UDataSwapper* swapper)
        : guest(reinterpret_cast<const GuestSwapper*>(swapper)), previous(Guest) {
        if (!guest) return;
        host.inIsBigEndian = guest->inIsBigEndian;
        host.inCharset = guest->inCharset;
        host.outIsBigEndian = guest->outIsBigEndian;
        host.outCharset = guest->outCharset;
        host.readUInt16 = guest->readUInt16 ? ReadUInt16 : nullptr;
        host.readUInt32 = guest->readUInt32 ? ReadUInt32 : nullptr;
        host.compareInvChars = guest->compareInvChars ? CompareInvChars : nullptr;
        host.writeUInt16 = guest->writeUInt16 ? WriteUInt16 : nullptr;
        host.writeUInt32 = guest->writeUInt32 ? WriteUInt32 : nullptr;
        host.swapArray16 = guest->swapArray16 ? Swap<&GuestSwapper::swapArray16> : nullptr;
        host.swapArray32 = guest->swapArray32 ? Swap<&GuestSwapper::swapArray32> : nullptr;
        host.swapArray64 = guest->swapArray64 ? Swap<&GuestSwapper::swapArray64> : nullptr;
        host.swapInvChars = guest->swapInvChars ? Swap<&GuestSwapper::swapInvChars> : nullptr;
        host.printError = guest->printError ? PrintError : nullptr;
        host.printErrorContext = guest->printErrorContext;
        Guest = guest;
    }

    ~Session() {
        Guest = previous;
    }

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    const UDataSwapper* Host() const {
        return guest ? &host : nullptr;
    }

private:
    const GuestSwapper* guest;
    const GuestSwapper* previous;
    UDataSwapper host{};
};

bool UnsupportedNormalizationVersion(const UDataSwapper* ds, const void* inData, std::int32_t length, const void* outData,
    const UErrorCode* status) {
    if (!status || U_FAILURE(*status) || !ds || !inData || length < -1 || (length > 0 && !outData)) return false;
    if (length >= 0 && length < static_cast<std::int32_t>(sizeof(DataHeader))) return false;
    const DataHeader* header = static_cast<const DataHeader*>(inData);
    if (header->dataHeader.magic1 != 0xda || header->dataHeader.magic2 != 0x27 || header->info.sizeofUChar != 2) return false;
    const UDataInfo& info = header->info;
    return info.dataFormat[0] == 0x4e && info.dataFormat[1] == 0x72 && info.dataFormat[2] == 0x6d &&
        info.dataFormat[3] == 0x32 && info.formatVersion[0] == 5;
}

std::int32_t SwapNormalization(const UDataSwapper* ds, const void* inData, std::int32_t length, void* outData,
    UErrorCode* status) {
    if (!UnsupportedNormalizationVersion(ds, inData, length, outData, status)) {
        return unorm2_swap(ds, inData, length, outData, status);
    }
    udata_swapDataHeader(ds, inData, length, outData, status);
    if (U_FAILURE(*status)) return 0;
    const UDataInfo& info = static_cast<const DataHeader*>(inData)->info;
    udata_printError(ds, "unorm2_swap(): data format %02x.%02x.%02x.%02x (format version %02x) is not recognized as Normalizer2 data\n",
        info.dataFormat[0], info.dataFormat[1], info.dataFormat[2], info.dataFormat[3], info.formatVersion[0]);
    *status = U_UNSUPPORTED_ERROR;
    return 0;
}

template<auto Function>
std::int32_t Run(const UDataSwapper* swapper, const void* inData, std::int32_t length, void* outData, std::int32_t* status) {
    const Session session(swapper);
    return Function(session.Host(), inData, length, outData, reinterpret_cast<UErrorCode*>(status));
}

}

extern "C" {

std::int32_t APS5_VABI ucol_swap_nid_postfix(const UDataSwapper* swapper, const void* inData, std::int32_t length,
    void* outData, std::int32_t* status) {
    return Run<ucol_swap>(swapper, inData, length, outData, status);
}

std::int32_t APS5_VABI ucol_swapInverseUCA_nid_postfix(const UDataSwapper* swapper, const void* inData, std::int32_t length,
    void* outData, std::int32_t* status) {
    return Run<ucol_swapInverseUCA>(swapper, inData, length, outData, status);
}

std::int8_t APS5_VABI ucol_looksLikeCollationBinary_nid_postfix(const UDataSwapper* swapper, const void* inData,
    std::int32_t length) {
    const Session session(swapper);
    return ucol_looksLikeCollationBinary(session.Host(), inData, length);
}

std::int32_t APS5_VABI unorm2_swap_nid_postfix(const UDataSwapper* swapper, const void* inData, std::int32_t length,
    void* outData, std::int32_t* status) {
    return Run<SwapNormalization>(swapper, inData, length, outData, status);
}

std::int32_t APS5_VABI ucnv_swapAliases_nid_postfix(const UDataSwapper* swapper, const void* inData, std::int32_t length,
    void* outData, std::int32_t* status) {
    return Run<ucnv_swapAliases>(swapper, inData, length, outData, status);
}

std::int32_t APS5_VABI uchar_swapNames_nid_postfix(const UDataSwapper* swapper, const void* inData, std::int32_t length,
    void* outData, std::int32_t* status) {
    return Run<uchar_swapNames>(swapper, inData, length, outData, status);
}

std::int32_t APS5_VABI ubrk_swap_nid_postfix(const UDataSwapper* swapper, const void* inData, std::int32_t length,
    void* outData, std::int32_t* status) {
    return Run<ubrk_swap>(swapper, inData, length, outData, status);
}

std::int32_t APS5_VABI udict_swap_nid_postfix(const UDataSwapper* swapper, const void* inData, std::int32_t length,
    void* outData, std::int32_t* status) {
    return Run<udict_swap>(swapper, inData, length, outData, status);
}

}
