#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/ElementNavigationExports.hpp"

#include <cerrno>
#include <stdexcept>
#include <string>

namespace {

using NKWebKit::WebCoreAbi::ContainerNode;
using NKWebKit::WebCoreAbi::Node;
using NKWebKit::WebCoreAbi::NodeFlagIsElementNode;

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

bool IsElement(const Node& node) {
    return (node.nodeFlags & NodeFlagIsElementNode) != 0;
}

Node* FirstElementFrom(Node* start, Node* Node::* link, const char* function) {
    for (Node* node = start; node;) {
        const Node copy = ReadGuest(node, function);
        if (IsElement(copy)) return node;
        node = copy.*link;
    }
    return nullptr;
}

}

extern "C" {

Node* APS5_VABI _ZNK7WebCore13ContainerNode17firstElementChildEv(const ContainerNode* self) {
    return FirstElementFrom(ReadGuest(self, __func__).firstChild, &Node::next, __func__);
}

Node* APS5_VABI _ZNK7WebCore13ContainerNode16lastElementChildEv(const ContainerNode* self) {
    return FirstElementFrom(ReadGuest(self, __func__).lastChild, &Node::previous, __func__);
}

std::uint32_t APS5_VABI _ZNK7WebCore13ContainerNode17childElementCountEv(const ContainerNode* self) {
    std::uint32_t count = 0;
    for (Node* node = ReadGuest(self, __func__).firstChild; node;) {
        const Node copy = ReadGuest(node, __func__);
        if (IsElement(copy)) ++count;
        node = copy.next;
    }
    return count;
}

Node* APS5_VABI _ZNK7WebCore13ContainerNode17traverseToChildAtEj(const ContainerNode* self, std::uint32_t index) {
    Node* child = ReadGuest(self, __func__).firstChild;
    for (; child && index > 0; --index) child = ReadGuest(child, __func__).next;
    return child;
}

Node* APS5_VABI _ZNK7WebCore4Node18nextElementSiblingEv(const Node* self) {
    return FirstElementFrom(ReadGuest(self, __func__).next, &Node::next, __func__);
}

Node* APS5_VABI _ZNK7WebCore4Node22previousElementSiblingEv(const Node* self) {
    return FirstElementFrom(ReadGuest(self, __func__).previous, &Node::previous, __func__);
}

}
