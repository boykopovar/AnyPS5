/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2008 Apple Inc. All rights reserved.
 *
 * Derived from Source/WebCore/dom/TreeWalker.cpp@c55364d53f971f63a709976f168d932d558b8909
 * Derived from Source/WebCore/dom/Traversal.cpp@c55364d53f971f63a709976f168d932d558b8909
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 *
 * Derived from Source/WebCore/dom/Node.h@c55364d53f971f63a709976f168d932d558b8909
 * Derived from Source/WebCore/dom/NodeTraversal.h@c55364d53f971f63a709976f168d932d558b8909
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/include/NodeTraversalExports.hpp"
#include "prx/libSceNKWebKit/include/TreeWalkerExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using NKWebKit::WebCoreAbi::ContainerNode;
using NKWebKit::WebCoreAbi::ExceptionOrNode;
using NKWebKit::WebCoreAbi::LastChild;
using NKWebKit::WebCoreAbi::Node;
using NKWebKit::WebCoreAbi::NodeFilterAcceptResult;
using NKWebKit::WebCoreAbi::NodeFlagIsContainerNode;
using NKWebKit::WebCoreAbi::NodeVtable;
using NKWebKit::WebCoreAbi::TreeWalker;
using NKWebKit::WtfAbi::StringImpl;

constexpr std::uint32_t RefCountIncrement = 2;
constexpr std::uint16_t FilterAccept = 1;
constexpr std::uint16_t FilterReject = 2;
constexpr std::uint16_t FilterSkip = 3;
constexpr std::int32_t InvalidStateError = 8;
constexpr std::int32_t ExistingExceptionError = 36;
constexpr std::int32_t CallbackExceptionThrown = 1;

void RefNode(Node* node) {
    node->refCountAndParentBit += RefCountIncrement;
}

void DerefNode(Node* node) {
    const std::uint32_t updatedRefCount = node->refCountAndParentBit - RefCountIncrement;
    if (!updatedRefCount) NotImplemented_nid_no_patch("_ZN7WebCore4Node14removedLastRefEv");
    node->refCountAndParentBit = updatedRefCount;
}

class NodePtr {
    Node* node;

public:
    explicit NodePtr(Node* value) : node(value) {
        if (node) RefNode(node);
    }

    NodePtr(const NodePtr&) = delete;
    NodePtr& operator=(const NodePtr&) = delete;

    ~NodePtr() noexcept(false) {
        if (node) DerefNode(node);
    }

    void Assign(Node* value) {
        if (value) RefNode(value);
        Node* old = std::exchange(node, value);
        if (old) DerefNode(old);
    }

    Node* Get() const { return node; }
    Node* Release() { return std::exchange(node, nullptr); }
};

Node* ParentNode(const Node* node) {
    return node->parentNode;
}

Node* FirstChild(const Node* node) {
    if (!(node->nodeFlags & NodeFlagIsContainerNode)) return nullptr;
    return static_cast<const ContainerNode*>(node)->firstChild;
}

Node* NextSkippingChildren(const Node* current, const Node* stayWithin) {
    if (current == stayWithin) return nullptr;
    if (current->next) return current->next;
    return _ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeEPS2_(current, stayWithin);
}

std::int32_t NodeType(const Node* node) {
    const auto* vtable = static_cast<const NodeVtable*>(node->vtable);
    return vtable->nodeType(node);
}

StringImpl* CreateMessage(const char* text) {
    auto created = _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), static_cast<unsigned>(std::strlen(text)));
    return std::exchange(created.impl, nullptr);
}

struct FilterResult {
    bool hasException;
    std::int32_t code;
    StringImpl* message;
    std::uint16_t value;
};

FilterResult Value(std::uint16_t value) {
    return {false, 0, nullptr, value};
}

class ActiveScope {
    TreeWalker& walker;
    bool previous;

public:
    explicit ActiveScope(TreeWalker& value) : walker(value), previous(value.isActive) {
        walker.isActive = true;
    }

    ActiveScope(const ActiveScope&) = delete;
    ActiveScope& operator=(const ActiveScope&) = delete;

    ~ActiveScope() {
        walker.isActive = previous;
    }
};

FilterResult AcceptNode(TreeWalker& walker, Node* node) {
    if (walker.isActive) return {true, InvalidStateError, CreateMessage("Recursive filters are not allowed"), 0};

    const std::int32_t nodeType = NodeType(node);
    if (nodeType < 1 || nodeType > 32) throw std::invalid_argument("TreeWalker: unexpected node type");
    if (!((1u << (nodeType - 1)) & walker.whatToShow)) return Value(FilterSkip);

    if (!walker.filter) return Value(FilterAccept);

    NodeFilterAcceptResult callbackResult;
    {
        ActiveScope isActive(walker);
        callbackResult = walker.filter->vtable->acceptNode(walker.filter, node);
    }
    if (!callbackResult.hasValue && callbackResult.type == CallbackExceptionThrown) return {true, ExistingExceptionError, nullptr, 0};
    if (!callbackResult.hasValue) throw std::runtime_error("TreeWalker: node filter could not be invoked");
    return Value(callbackResult.value);
}

void SetNode(ExceptionOrNode& result, Node* node) {
    result.value = node;
    result.hasValue = true;
}

void SetException(ExceptionOrNode& result, const FilterResult& filterResult) {
    result.exception.code = filterResult.code;
    result.exception.message = filterResult.message;
    result.hasValue = false;
}

void RequireReleasable(const Node* old, const Node* replacement) {
    if (old != replacement && old->refCountAndParentBit - RefCountIncrement == 0) NotImplemented_nid_no_patch("_ZN7WebCore4Node14removedLastRefEv");
}

void ReplaceCurrent(TreeWalker& walker, NodePtr& node) {
    Node* old = walker.current;
    RequireReleasable(old, node.Get());
    walker.current = node.Release();
    DerefNode(old);
}

TreeWalker& Walker(TreeWalker* self, ExceptionOrNode* result, const char* function) {
    if (!self) throw std::invalid_argument(std::string(function) + ": null TreeWalker");
    if (!result) throw std::invalid_argument(std::string(function) + ": null result");
    return *self;
}

void ParentNodeImpl(TreeWalker& walker, ExceptionOrNode& result) {
    NodePtr node(walker.current);
    while (node.Get() != walker.root) {
        node.Assign(ParentNode(node.Get()));
        if (!node.Get()) return SetNode(result, nullptr);

        const FilterResult filterResult = AcceptNode(walker, node.Get());
        if (filterResult.hasException) return SetException(result, filterResult);

        if (filterResult.value == FilterAccept) {
            ReplaceCurrent(walker, node);
            return SetNode(result, walker.current);
        }
    }
    SetNode(result, nullptr);
}

template<bool First>
void ChildImpl(TreeWalker& walker, ExceptionOrNode& result) {
    auto childOf = [](const Node* node) { return First ? FirstChild(node) : LastChild(*node); };
    auto siblingOf = [](const Node* node) { return First ? node->next : node->previous; };

    for (NodePtr node(childOf(walker.current)); node.Get();) {
        const FilterResult filterResult = AcceptNode(walker, node.Get());
        if (filterResult.hasException) return SetException(result, filterResult);

        switch (filterResult.value) {
        case FilterAccept:
            ReplaceCurrent(walker, node);
            return SetNode(result, walker.current);
        case FilterSkip:
            if (childOf(node.Get())) {
                node.Assign(childOf(node.Get()));
                continue;
            }
            break;
        case FilterReject:
            break;
        }
        do {
            if (siblingOf(node.Get())) {
                node.Assign(siblingOf(node.Get()));
                break;
            }
            Node* parent = ParentNode(node.Get());
            if (!parent || parent == walker.root || parent == walker.current) return SetNode(result, nullptr);
            node.Assign(parent);
        } while (node.Get());
    }
    SetNode(result, nullptr);
}

template<bool Next>
void SiblingImpl(TreeWalker& walker, ExceptionOrNode& result) {
    auto siblingOf = [](const Node* node) { return Next ? node->next : node->previous; };
    auto childOf = [](const Node* node) { return Next ? FirstChild(node) : LastChild(*node); };

    NodePtr node(walker.current);
    if (node.Get() == walker.root) return SetNode(result, nullptr);

    while (true) {
        for (NodePtr sibling(siblingOf(node.Get())); sibling.Get();) {
            const FilterResult filterResult = AcceptNode(walker, sibling.Get());
            if (filterResult.hasException) return SetException(result, filterResult);

            if (filterResult.value == FilterAccept) {
                ReplaceCurrent(walker, sibling);
                return SetNode(result, walker.current);
            }
            node.Assign(sibling.Get());
            sibling.Assign(childOf(sibling.Get()));
            if (filterResult.value == FilterReject || !sibling.Get()) sibling.Assign(siblingOf(node.Get()));
        }
        node.Assign(ParentNode(node.Get()));
        if (!node.Get() || node.Get() == walker.root) return SetNode(result, nullptr);

        const FilterResult filterResult = AcceptNode(walker, node.Get());
        if (filterResult.hasException) return SetException(result, filterResult);

        if (filterResult.value == FilterAccept) return SetNode(result, nullptr);
    }
}

void PreviousNodeImpl(TreeWalker& walker, ExceptionOrNode& result) {
    NodePtr node(walker.current);
    while (node.Get() != walker.root) {
        while (Node* previousSibling = node.Get()->previous) {
            node.Assign(previousSibling);

            const FilterResult filterResult = AcceptNode(walker, node.Get());
            if (filterResult.hasException) return SetException(result, filterResult);

            std::uint16_t acceptNodeResult = filterResult.value;
            if (acceptNodeResult == FilterReject) continue;
            while (Node* lastChild = LastChild(*node.Get())) {
                node.Assign(lastChild);

                const FilterResult childResult = AcceptNode(walker, node.Get());
                if (childResult.hasException) return SetException(result, childResult);

                acceptNodeResult = childResult.value;
                if (acceptNodeResult == FilterReject) break;
            }
            if (acceptNodeResult == FilterAccept) {
                ReplaceCurrent(walker, node);
                return SetNode(result, walker.current);
            }
        }
        if (node.Get() == walker.root) return SetNode(result, nullptr);
        Node* parent = ParentNode(node.Get());
        if (!parent) return SetNode(result, nullptr);
        node.Assign(parent);

        const FilterResult filterResult = AcceptNode(walker, node.Get());
        if (filterResult.hasException) return SetException(result, filterResult);

        if (filterResult.value == FilterAccept) {
            ReplaceCurrent(walker, node);
            return SetNode(result, walker.current);
        }
    }
    SetNode(result, nullptr);
}

void NextNodeImpl(TreeWalker& walker, ExceptionOrNode& result) {
    NodePtr node(walker.current);
    while (true) {
        bool descend = false;
        while (Node* firstChild = FirstChild(node.Get())) {
            node.Assign(firstChild);

            const FilterResult filterResult = AcceptNode(walker, node.Get());
            if (filterResult.hasException) return SetException(result, filterResult);

            if (filterResult.value == FilterAccept) {
                ReplaceCurrent(walker, node);
                return SetNode(result, walker.current);
            }
            if (filterResult.value == FilterReject) break;
        }
        while (Node* nextSibling = NextSkippingChildren(node.Get(), walker.root)) {
            node.Assign(nextSibling);

            const FilterResult filterResult = AcceptNode(walker, node.Get());
            if (filterResult.hasException) return SetException(result, filterResult);

            if (filterResult.value == FilterAccept) {
                ReplaceCurrent(walker, node);
                return SetNode(result, walker.current);
            }
            if (filterResult.value == FilterSkip) {
                descend = true;
                break;
            }
        }
        if (!descend) return SetNode(result, nullptr);
    }
}

}

extern "C" {

void APS5_VABI _ZN7WebCore10TreeWalker14setCurrentNodeERNS_4NodeE(TreeWalker* self, Node* node) {
    if (!self) throw std::invalid_argument("TreeWalker::setCurrentNode: null TreeWalker");
    if (!node) throw std::invalid_argument("TreeWalker::setCurrentNode: null node");
    Node* old = self->current;
    RequireReleasable(old, node);
    RefNode(node);
    self->current = node;
    DerefNode(old);
}

ExceptionOrNode* APS5_VABI _ZN7WebCore10TreeWalker10parentNodeEv(ExceptionOrNode* result, TreeWalker* self) {
    TreeWalker& walker = Walker(self, result, "TreeWalker::parentNode");
    ParentNodeImpl(walker, *result);
    return result;
}

ExceptionOrNode* APS5_VABI _ZN7WebCore10TreeWalker10firstChildEv(ExceptionOrNode* result, TreeWalker* self) {
    TreeWalker& walker = Walker(self, result, "TreeWalker::firstChild");
    ChildImpl<true>(walker, *result);
    return result;
}

ExceptionOrNode* APS5_VABI _ZN7WebCore10TreeWalker9lastChildEv(ExceptionOrNode* result, TreeWalker* self) {
    TreeWalker& walker = Walker(self, result, "TreeWalker::lastChild");
    ChildImpl<false>(walker, *result);
    return result;
}

ExceptionOrNode* APS5_VABI _ZN7WebCore10TreeWalker15previousSiblingEv(ExceptionOrNode* result, TreeWalker* self) {
    TreeWalker& walker = Walker(self, result, "TreeWalker::previousSibling");
    SiblingImpl<false>(walker, *result);
    return result;
}

ExceptionOrNode* APS5_VABI _ZN7WebCore10TreeWalker11nextSiblingEv(ExceptionOrNode* result, TreeWalker* self) {
    TreeWalker& walker = Walker(self, result, "TreeWalker::nextSibling");
    SiblingImpl<true>(walker, *result);
    return result;
}

ExceptionOrNode* APS5_VABI _ZN7WebCore10TreeWalker12previousNodeEv(ExceptionOrNode* result, TreeWalker* self) {
    TreeWalker& walker = Walker(self, result, "TreeWalker::previousNode");
    PreviousNodeImpl(walker, *result);
    return result;
}

ExceptionOrNode* APS5_VABI _ZN7WebCore10TreeWalker8nextNodeEv(ExceptionOrNode* result, TreeWalker* self) {
    TreeWalker& walker = Walker(self, result, "TreeWalker::nextNode");
    NextNodeImpl(walker, *result);
    return result;
}

}
