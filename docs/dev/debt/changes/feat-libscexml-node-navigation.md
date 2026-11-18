# libSceXml

## change [The `String`, `SimpleData`, `SerializeParameter`, `NodeId` and `Node` value operations and the handle constructors]

- [The `String`, `SimpleData`, `SerializeParameter`, `NodeId` and `Node` value operations and the handle constructors](../../../core/libs/prx/libSceXml/Export.cpp) (libSceXml) - follow `libSceXml.sprx` of firmware 13.60, read from disassembly, not measured on a console. `NodeId::operator void*` returns null for every `NodeId`, and the `NodeId` constructor and copies leave its padding bytes untouched. The handle constructors only clear the implementation pointer. Exports that read the objects `initialize` builds still throw, because nothing creates those objects yet: the `Sax::Parser` setters, and `Node` `getOwnerDocument`, `getNodeName` and `getNodeValue`, whose return types are not established. `Node::getAttributes` and `Node::getChildNodes`, in both forms, are a bare return in the module, and the deleting `MemAllocator` destructor is `ud2`, so they still throw too.

## add Functional

- [Node navigation](../../../core/libs/prx/libSceXml/src/Node.cpp) (libSceXml) - `Node::getFirstChild`, `getLastChild`, `getNextSibling` and `getParentNode` return a `NodeId` through a hidden result pointer and give the same node as `Document::getFirstChild`, `getLastChild`, `getSibling` and `getParent` for the node's document and position, or a null `NodeId` when the node has no document. Read from a decrypted PS4 `libSceXml.sprx`, where each getter calls the same lookup as its `Document` counterpart; not compared with a PS5 module or measured on a console. Reading a node after its builder parsed again or terminated throws, as the other node reads do.
