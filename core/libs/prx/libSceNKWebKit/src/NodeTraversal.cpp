#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/NodeTraversalExports.hpp"

using NKWebKit::WebCoreAbi::ContainerNode;
using NKWebKit::WebCoreAbi::LastChild;
using NKWebKit::WebCoreAbi::Node;

extern "C" {

Node* APS5_VABI _ZN7WebCore13NodeTraversal13deepLastChildERNS_4NodeE(Node* node) {
    Node* lastChild = node;
    while (Node* next = LastChild(*lastChild)) lastChild = next;
    return lastChild;
}

Node* APS5_VABI _ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeE(const Node* current) {
    for (const Node* ancestor = current->parentNode; ancestor; ancestor = ancestor->parentNode) {
        if (ancestor->next) return ancestor->next;
    }
    return nullptr;
}

Node* APS5_VABI _ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeEPS2_(const Node* current, const Node* stayWithin) {
    for (const Node* ancestor = current->parentNode; ancestor; ancestor = ancestor->parentNode) {
        if (ancestor == stayWithin) return nullptr;
        if (ancestor->next) return ancestor->next;
    }
    return nullptr;
}

Node* APS5_VABI _ZN7WebCore13NodeTraversal4lastERKNS_13ContainerNodeE(const ContainerNode* current) {
    Node* node = current->lastChild;
    if (!node) return nullptr;
    while (Node* next = LastChild(*node)) node = next;
    return node;
}

}
