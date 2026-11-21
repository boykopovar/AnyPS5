#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/NodeTraversalExports.hpp"

#include <cerrno>
#include <stdexcept>
#include <string>

namespace {

using NKWebKit::WebCoreAbi::ContainerNode;
using NKWebKit::WebCoreAbi::Node;
using NKWebKit::WebCoreAbi::NodeFlagIsContainerNode;

template<typename T>
T ReadGuest(const T* pointer, const char* function) {
    if (!pointer) throw std::invalid_argument(std::string(function) + ": null node");
    T value{};
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(&value, pointer, sizeof(T));
    errno = savedErrno;
    if (!ok) throw std::invalid_argument(std::string(function) + ": unreadable node");
    return value;
}

Node* ReadLastChild(const Node* node, const char* function) {
    const Node copy = ReadGuest(node, function);
    if (!(copy.nodeFlags & NodeFlagIsContainerNode)) return nullptr;
    return ReadGuest(&static_cast<const ContainerNode*>(node)->lastChild, function);
}

}

extern "C" {

Node* APS5_VABI _ZN7WebCore13NodeTraversal13deepLastChildERNS_4NodeE(Node* node) {
    Node* lastChild = node;
    while (Node* next = ReadLastChild(lastChild, __func__)) lastChild = next;
    return lastChild;
}

Node* APS5_VABI _ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeE(const Node* current) {
    const Node currentCopy = ReadGuest(current, __func__);
    for (const Node* ancestor = currentCopy.parentNode; ancestor;) {
        const Node ancestorCopy = ReadGuest(ancestor, __func__);
        if (ancestorCopy.next) return ancestorCopy.next;
        ancestor = ancestorCopy.parentNode;
    }
    return nullptr;
}

Node* APS5_VABI _ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeEPS2_(const Node* current, const Node* stayWithin) {
    const Node currentCopy = ReadGuest(current, __func__);
    for (const Node* ancestor = currentCopy.parentNode; ancestor;) {
        if (ancestor == stayWithin) return nullptr;
        const Node ancestorCopy = ReadGuest(ancestor, __func__);
        if (ancestorCopy.next) return ancestorCopy.next;
        ancestor = ancestorCopy.parentNode;
    }
    return nullptr;
}

Node* APS5_VABI _ZN7WebCore13NodeTraversal4lastERKNS_13ContainerNodeE(const ContainerNode* current) {
    const ContainerNode copy = ReadGuest(current, __func__);
    Node* node = copy.lastChild;
    if (!node) return nullptr;
    while (Node* next = ReadLastChild(node, __func__)) node = next;
    return node;
}

}
