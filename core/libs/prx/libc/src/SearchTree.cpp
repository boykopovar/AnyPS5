#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <cerrno>
#include <stdexcept>

namespace {

struct Node {
    const void* key;
    Node* left;
    Node* right;
    int height;
};

using Compare = int (APS5_VABI *)(const void*, const void*);
using Action = void (APS5_VABI *)(const void*, int, int);

int HeightOf(const Node* node) { return node ? node->height : 0; }

void Update(Node* node) {
    node->height = 1 + std::max(HeightOf(node->left), HeightOf(node->right));
}

Node* RotateRight(Node* node) {
    Node* pivot = node->left;
    node->left = pivot->right;
    pivot->right = node;
    Update(node);
    Update(pivot);
    return pivot;
}

Node* RotateLeft(Node* node) {
    Node* pivot = node->right;
    node->right = pivot->left;
    pivot->left = node;
    Update(node);
    Update(pivot);
    return pivot;
}

Node* Balance(Node* node) {
    Update(node);
    const int balance = HeightOf(node->left) - HeightOf(node->right);
    if (balance > 1) {
        if (HeightOf(node->left->left) < HeightOf(node->left->right)) node->left = RotateLeft(node->left);
        return RotateRight(node);
    }
    if (balance < -1) {
        if (HeightOf(node->right->right) < HeightOf(node->right->left)) node->right = RotateRight(node->right);
        return RotateLeft(node);
    }
    return node;
}

Node* Insert(Node* node, const void* key, Compare compare, Node*& found, bool& failed) {
    if (node == nullptr) {
        auto* created = static_cast<Node*>(malloc_nid_postfix(sizeof(Node)));
        if (created == nullptr) {
            failed = true;
            return nullptr;
        }
        *created = Node{key, nullptr, nullptr, 1};
        found = created;
        return created;
    }
    const int order = compare(key, node->key);
    if (order == 0) {
        found = node;
        return node;
    }
    if (order < 0) {
        Node* child = Insert(node->left, key, compare, found, failed);
        if (failed) return node;
        node->left = child;
    } else {
        Node* child = Insert(node->right, key, compare, found, failed);
        if (failed) return node;
        node->right = child;
    }
    return Balance(node);
}

Node* RemoveMinimum(Node* node, Node*& minimum) {
    if (node->left == nullptr) {
        minimum = node;
        return node->right;
    }
    node->left = RemoveMinimum(node->left, minimum);
    return Balance(node);
}

Node* Remove(Node* node, Node* parent, const void* key, Compare compare, Node*& parentOfRemoved, bool& found) {
    if (node == nullptr) return nullptr;
    const int order = compare(key, node->key);
    if (order < 0) {
        node->left = Remove(node->left, node, key, compare, parentOfRemoved, found);
        return found ? Balance(node) : node;
    }
    if (order > 0) {
        node->right = Remove(node->right, node, key, compare, parentOfRemoved, found);
        return found ? Balance(node) : node;
    }
    found = true;
    parentOfRemoved = parent;
    if (node->left == nullptr || node->right == nullptr) {
        Node* replacement = node->left != nullptr ? node->left : node->right;
        free_nid_postfix(node);
        return replacement;
    }
    Node* minimum = nullptr;
    Node* right = RemoveMinimum(node->right, minimum);
    minimum->left = node->left;
    minimum->right = right;
    free_nid_postfix(node);
    return Balance(minimum);
}

void Walk(const Node* node, int depth, Action action) {
    if (node->left == nullptr && node->right == nullptr) {
        action(&node->key, 3, depth);
        return;
    }
    action(&node->key, 0, depth);
    if (node->left != nullptr) Walk(node->left, depth + 1, action);
    action(&node->key, 1, depth);
    if (node->right != nullptr) Walk(node->right, depth + 1, action);
    action(&node->key, 2, depth);
}

Node* ReadRoot(void* const* rootp) {
    Node* root = nullptr;
    if (!GuestCopyIn_nid_no_patch(&root, rootp, sizeof(root))) throw std::invalid_argument("tree search: unreadable root");
    return root;
}

}

void* APS5_VABI tsearch_nid_postfix(const void* key, void** rootp, int (APS5_VABI *compare)(const void*, const void*)) {
    if (!rootp) return nullptr;
    if (!key || !compare) throw std::invalid_argument("tsearch: invalid arguments");
    if (!GuestWritable_nid_no_patch(rootp, sizeof(void*))) throw std::invalid_argument("tsearch: unwritable root");
    Node* root = ReadRoot(rootp);
    Node* found = nullptr;
    bool failed = false;
    Node* updated = Insert(root, key, compare, found, failed);
    if (failed) {
        errno = 12;
        return nullptr;
    }
    if (updated != root && !GuestCopyOut_nid_no_patch(rootp, &updated, sizeof(updated))) throw std::invalid_argument("tsearch: unwritable root");
    return &found->key;
}

void* APS5_VABI tfind_nid_postfix(const void* key, void* const* rootp, int (APS5_VABI *compare)(const void*, const void*)) {
    if (!rootp) return nullptr;
    if (!key || !compare) throw std::invalid_argument("tfind: invalid arguments");
    Node* node = ReadRoot(rootp);
    while (node != nullptr) {
        const int order = compare(key, node->key);
        if (order == 0) return &node->key;
        node = order < 0 ? node->left : node->right;
    }
    return nullptr;
}

void* APS5_VABI tdelete_nid_postfix(const void* key, void** rootp, int (APS5_VABI *compare)(const void*, const void*)) {
    if (!rootp) return nullptr;
    if (!key || !compare) throw std::invalid_argument("tdelete: invalid arguments");
    if (!GuestWritable_nid_no_patch(rootp, sizeof(void*))) throw std::invalid_argument("tdelete: unwritable root");
    Node* root = ReadRoot(rootp);
    Node* parent = nullptr;
    bool found = false;
    Node* updated = Remove(root, nullptr, key, compare, parent, found);
    if (!found) return nullptr;
    if (!GuestCopyOut_nid_no_patch(rootp, &updated, sizeof(updated))) throw std::invalid_argument("tdelete: unwritable root");
    return parent != nullptr ? &parent->key : static_cast<void*>(rootp);
}

void APS5_VABI twalk_nid_postfix(const void* root, void (APS5_VABI *action)(const void*, int, int)) {
    if (!action) return;
    if (root == nullptr) return;
    Walk(static_cast<const Node*>(root), 0, action);
}
