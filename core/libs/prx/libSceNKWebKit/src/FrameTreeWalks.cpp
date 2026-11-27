#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/FrameTreeExports.hpp"

#include <cerrno>
#include <stdexcept>
#include <string>

namespace {

using NKWebKit::WebCoreAbi::Frame;
using NKWebKit::WebCoreAbi::FrameTree;
using NKWebKit::WebCoreAbi::WeakPtrImpl;

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
T* WeakGet(const WeakPtrImpl* impl, const char* function) {
    return impl ? static_cast<T*>(ReadGuest(impl, function).pointer) : nullptr;
}

Frame* ParentOf(const Frame* frame, const char* function) {
    return WeakGet<Frame>(ReadGuest(frame, function).tree.parent, function);
}

}

extern "C" {

std::uint32_t APS5_VABI _ZNK7WebCore9FrameTree10childCountEv(const FrameTree* self) {
    std::uint32_t count = 0;
    for (const Frame* child = ReadGuest(self, __func__).firstChild; child; child = ReadGuest(child, __func__).tree.nextSibling) ++count;
    return count;
}

Frame* APS5_VABI _ZNK7WebCore9FrameTree3topEv(const FrameTree* self) {
    Frame* top = ReadGuest(self, __func__).thisFrame;
    for (Frame* parent = top; parent; parent = ParentOf(parent, __func__)) top = parent;
    return top;
}

Frame* APS5_VABI _ZNK7WebCore9FrameTree12traverseNextEPKNS_5FrameE(const FrameTree* self, const Frame* stayWithin) {
    const FrameTree tree = ReadGuest(self, __func__);
    if (tree.firstChild) return tree.firstChild;
    if (tree.thisFrame == stayWithin) return nullptr;
    if (tree.nextSibling) return tree.nextSibling;
    Frame* frame = tree.thisFrame;
    for (;;) {
        Frame* parent = ParentOf(frame, __func__);
        if (stayWithin && parent == stayWithin) return nullptr;
        frame = parent;
        if (!frame) return nullptr;
        if (Frame* sibling = ReadGuest(frame, __func__).tree.nextSibling) return sibling;
    }
}

bool APS5_VABI _ZNK7WebCore9FrameTree14isDescendantOfEPKNS_5FrameE(const FrameTree* self, const Frame* ancestor) {
    if (!ancestor) return false;
    const Frame* thisFrame = ReadGuest(self, __func__).thisFrame;
    if (WeakGet<void>(ReadGuest(thisFrame, __func__).page, __func__) != WeakGet<void>(ReadGuest(ancestor, __func__).page, __func__)) return false;
    for (const Frame* frame = thisFrame; frame; frame = ParentOf(frame, __func__)) {
        if (frame == ancestor) return true;
    }
    return false;
}

}
