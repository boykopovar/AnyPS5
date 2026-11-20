#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/PositionAnchorExports.hpp"

#include <cerrno>
#include <stdexcept>
#include <string>

namespace {

using NKWebKit::WebCoreAbi::Node;
using NKWebKit::WebCoreAbi::NodeRefCountIncrement;
using NKWebKit::WebCoreAbi::Position;
using NKWebKit::WebCoreAbi::PositionAnchorTypeMask;
using NKWebKit::WebCoreAbi::PositionIsAfterAnchor;
using NKWebKit::WebCoreAbi::PositionIsAfterChildren;
using NKWebKit::WebCoreAbi::PositionIsBeforeAnchor;
using NKWebKit::WebCoreAbi::PositionIsBeforeChildren;
using NKWebKit::WebCoreAbi::PositionIsOffsetInAnchor;
using NKWebKit::WebCoreAbi::PositionUnrelatedBitsMask;

template<typename T>
T ReadGuest(const T* pointer, const char* function) {
    if (!pointer) throw std::invalid_argument(std::string(function) + ": null object");
    T value{};
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(&value, pointer, sizeof(T));
    errno = savedErrno;
    if (!ok) throw std::invalid_argument(std::string(function) + ": unreadable object");
    return value;
}

template<typename T>
void WriteGuest(T* pointer, const T& value, const char* function) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(pointer, &value, sizeof(T));
    errno = savedErrno;
    if (!ok) throw std::invalid_argument(std::string(function) + ": unwritable object");
}

void Construct(Position* self, Node* anchorNode, std::uint32_t offset, std::uint32_t anchorType, const char* function) {
    Position position = ReadGuest(self, function);
    position.anchorNode = anchorNode;
    position.offset = offset;
    position.anchorTypeAndFlags = static_cast<std::uint8_t>((position.anchorTypeAndFlags & PositionUnrelatedBitsMask) | (anchorType & PositionAnchorTypeMask));
    if (anchorNode) {
        std::uint32_t* refCount = &anchorNode->refCountAndParentBit;
        WriteGuest(refCount, ReadGuest(refCount, function) + NodeRefCountIncrement, function);
    }
    WriteGuest(self, position, function);
}

}

extern "C" {

void APS5_VABI _ZN7WebCore8PositionC1EPNS_4NodeENS0_10AnchorTypeE(Position* self, Node* anchorNode, std::uint32_t anchorType) {
    Construct(self, anchorNode, 0, anchorType, __func__);
}

void APS5_VABI _ZN7WebCore8PositionC2EPNS_4NodeENS0_10AnchorTypeE(Position* self, Node* anchorNode, std::uint32_t anchorType) {
    Construct(self, anchorNode, 0, anchorType, __func__);
}

void APS5_VABI _ZN7WebCore8PositionC1EPNS_4NodeEjNS0_10AnchorTypeE(Position* self, Node* anchorNode, std::uint32_t offset, std::uint32_t anchorType) {
    Construct(self, anchorNode, offset, anchorType, __func__);
}

void APS5_VABI _ZN7WebCore8PositionC2EPNS_4NodeEjNS0_10AnchorTypeE(Position* self, Node* anchorNode, std::uint32_t offset, std::uint32_t anchorType) {
    Construct(self, anchorNode, offset, anchorType, __func__);
}

Node* APS5_VABI _ZNK7WebCore8Position13containerNodeEv(const Position* self) {
    const Position position = ReadGuest(self, __func__);
    if (!position.anchorNode) return nullptr;
    switch (position.anchorTypeAndFlags & PositionAnchorTypeMask) {
    case PositionIsBeforeChildren:
    case PositionIsAfterChildren:
    case PositionIsOffsetInAnchor:
        return position.anchorNode;
    case PositionIsBeforeAnchor:
    case PositionIsAfterAnchor:
        return ReadGuest(position.anchorNode, __func__).parentNode;
    }
    return nullptr;
}

}
