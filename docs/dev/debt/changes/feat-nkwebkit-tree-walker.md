# libSceNKWebKit

## add Unknown function info

- [WebCore::TreeWalker](../../../core/libs/prx/libSceNKWebKit/src/TreeWalker.cpp) (libSceNKWebKit) - not compared with a console module. `Node::nodeType` is called through vtable slot 20 and `NodeFilter::acceptNode` through slot 4, counted from [Node.h](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/dom/Node.h#L120) and [NodeFilter.h](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/dom/NodeFilter.h#L39) at WebKit `c55364d`. The 48-byte walker and the 24-byte `ExceptionOr<Node*>` follow [TreeWalker.h](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/dom/TreeWalker.h) and [Traversal.h](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/dom/Traversal.h). Releasing the last reference to a node throws, because `Node::removedLastRef` is a stub.

## remove `_ZN7WebCore10TreeWalker10firstChildEv`

## remove `_ZN7WebCore10TreeWalker10parentNodeEv`

## remove `_ZN7WebCore10TreeWalker11nextSiblingEv`

## remove `_ZN7WebCore10TreeWalker12previousNodeEv`

## remove `_ZN7WebCore10TreeWalker14setCurrentNodeERNS_4NodeE`

## remove `_ZN7WebCore10TreeWalker15previousSiblingEv`

## remove `_ZN7WebCore10TreeWalker8nextNodeEv`

## remove `_ZN7WebCore10TreeWalker9lastChildEv`
