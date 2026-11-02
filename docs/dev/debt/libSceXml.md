# libSceXml technical debt

### Known export names; ABI prototypes unverified

- 107 of the 150 `sce::Xml` exports in [Export.cpp](../../../core/libs/prx/libSceXml/Export.cpp) are present in the PS5 3.20 `libSceXml` export inventory from [PS5-3.20_Libs](https://github.com/DNNDHH/PS5-3.20_Libs). Each is named after the mangled symbol the [verified NID name database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) gives for its NID, and the NID of that name matches. Their prototypes and behavior are not established; these 107 use the existing no-parameter import-resolution convention and throw through `NotImplemented_nid_no_patch`. The declarations do not claim an ABI or behavior.

### Value types and handle constructors

- [The `String`, `SimpleData`, `SerializeParameter`, `NodeId` and `Node` value operations and the handle constructors](../../../core/libs/prx/libSceXml/Export.cpp) (libSceXml) - follow `libSceXml.sprx` of firmware 13.60, read from disassembly, not measured on a console. `NodeId::operator void*` returns null for every `NodeId`, and the `NodeId` constructor and copies leave its padding bytes untouched. The handle constructors only clear the implementation pointer. Exports that read the objects `initialize` builds still throw, because nothing creates those objects yet: `AttributeList` `getLength` and `isAvailable`, `Document::resetStatus`, the `DocumentBuilder` and `Sax::Parser` setters, `Node(NodeId)`, and `Node` `getNodeType`, `hasChildNodes`, `hasAttributes` and `getOwnerDocument`. `Node::getAttributes` and `getChildNodes` are a bare return in the module, and the deleting `MemAllocator` destructor is `ud2`, so they still throw too.
