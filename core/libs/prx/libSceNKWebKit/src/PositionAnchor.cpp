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

// The module stores the fields one by one in this order: the anchor node, the node's reference word (when
// the node is not null), the offset, then the flags byte (read, low nibble replaced, written). Each store is
// its own guest write, so a fault at `self` leaves the node's reference count untouched, and a fault part-way
// leaves the earlier stores in place as in the module.
void Construct(Position* self, Node* anchorNode, std::uint32_t offset, std::uint32_t anchorType, const char* function) {
    if (!self) throw std::invalid_argument(std::string(function) + ": null object");
    WriteGuest(&self->anchorNode, anchorNode, function);
    if (anchorNode) {
        std::uint32_t* refCount = &anchorNode->refCountAndParentBit;
        WriteGuest(refCount, ReadGuest(refCount, function) + NodeRefCountIncrement, function);
    }
    WriteGuest(&self->offset, offset, function);
    const std::uint8_t flags = ReadGuest(&self->anchorTypeAndFlags, function);
    WriteGuest(&self->anchorTypeAndFlags, static_cast<std::uint8_t>((flags & PositionUnrelatedBitsMask) | (anchorType & PositionAnchorTypeMask)), function);
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
