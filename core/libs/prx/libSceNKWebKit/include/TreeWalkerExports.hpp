#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/NodeAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NKWebKit::WebCoreAbi {

struct NodeFilter;

struct NodeFilterAcceptResult {
    union {
        std::uint16_t value;
        std::int32_t type;
    };
    bool hasValue;
};

struct NodeVtable {
    void* entries[20];
    std::int32_t (APS5_VABI* nodeType)(const Node* self);
};

struct NodeFilterVtable {
    void* entries[4];
    NodeFilterAcceptResult (APS5_VABI* acceptNode)(NodeFilter* self, Node* node);
};

struct NodeFilter {
    const NodeFilterVtable* vtable;
    void* scriptExecutionContext;
    std::uint32_t refCount;
};

struct TreeWalker {
    void* wrapper;
    std::uint32_t refCount;
    Node* root;
    NodeFilter* filter;
    std::uint32_t whatToShow;
    bool isActive;
    Node* current;
};

struct DomException {
    std::int32_t code;
    NKWebKit::WtfAbi::StringImpl* message;
};

struct ExceptionOrNode {
    union {
        Node* value;
        DomException exception;
    };
    bool hasValue;
};

static_assert(sizeof(NodeFilterAcceptResult) == 8);
static_assert(offsetof(NodeFilterAcceptResult, value) == 0);
static_assert(offsetof(NodeFilterAcceptResult, type) == 0);
static_assert(offsetof(NodeFilterAcceptResult, hasValue) == 4);
static_assert(std::is_trivially_copyable_v<NodeFilterAcceptResult>);
static_assert(offsetof(NodeVtable, nodeType) == 160);
static_assert(offsetof(NodeFilterVtable, acceptNode) == 32);
static_assert(sizeof(NodeFilter) == 24);
static_assert(offsetof(NodeFilter, refCount) == 16);
static_assert(sizeof(TreeWalker) == 48);
static_assert(offsetof(TreeWalker, refCount) == 8);
static_assert(offsetof(TreeWalker, root) == 16);
static_assert(offsetof(TreeWalker, filter) == 24);
static_assert(offsetof(TreeWalker, whatToShow) == 32);
static_assert(offsetof(TreeWalker, isActive) == 36);
static_assert(offsetof(TreeWalker, current) == 40);
static_assert(sizeof(DomException) == 16);
static_assert(offsetof(DomException, message) == 8);
static_assert(sizeof(ExceptionOrNode) == 24);
static_assert(offsetof(ExceptionOrNode, value) == 0);
static_assert(offsetof(ExceptionOrNode, exception) == 0);
static_assert(offsetof(ExceptionOrNode, hasValue) == 16);
static_assert(std::is_trivially_copyable_v<ExceptionOrNode>);

}

extern "C" {

void APS5_VABI _ZN7WebCore10TreeWalker14setCurrentNodeERNS_4NodeE(NKWebKit::WebCoreAbi::TreeWalker* self, NKWebKit::WebCoreAbi::Node* node);
NKWebKit::WebCoreAbi::ExceptionOrNode* APS5_VABI _ZN7WebCore10TreeWalker10parentNodeEv(NKWebKit::WebCoreAbi::ExceptionOrNode* result, NKWebKit::WebCoreAbi::TreeWalker* self);
NKWebKit::WebCoreAbi::ExceptionOrNode* APS5_VABI _ZN7WebCore10TreeWalker10firstChildEv(NKWebKit::WebCoreAbi::ExceptionOrNode* result, NKWebKit::WebCoreAbi::TreeWalker* self);
NKWebKit::WebCoreAbi::ExceptionOrNode* APS5_VABI _ZN7WebCore10TreeWalker9lastChildEv(NKWebKit::WebCoreAbi::ExceptionOrNode* result, NKWebKit::WebCoreAbi::TreeWalker* self);
NKWebKit::WebCoreAbi::ExceptionOrNode* APS5_VABI _ZN7WebCore10TreeWalker15previousSiblingEv(NKWebKit::WebCoreAbi::ExceptionOrNode* result, NKWebKit::WebCoreAbi::TreeWalker* self);
NKWebKit::WebCoreAbi::ExceptionOrNode* APS5_VABI _ZN7WebCore10TreeWalker11nextSiblingEv(NKWebKit::WebCoreAbi::ExceptionOrNode* result, NKWebKit::WebCoreAbi::TreeWalker* self);
NKWebKit::WebCoreAbi::ExceptionOrNode* APS5_VABI _ZN7WebCore10TreeWalker12previousNodeEv(NKWebKit::WebCoreAbi::ExceptionOrNode* result, NKWebKit::WebCoreAbi::TreeWalker* self);
NKWebKit::WebCoreAbi::ExceptionOrNode* APS5_VABI _ZN7WebCore10TreeWalker8nextNodeEv(NKWebKit::WebCoreAbi::ExceptionOrNode* result, NKWebKit::WebCoreAbi::TreeWalker* self);

}
