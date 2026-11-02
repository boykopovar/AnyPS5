# libSceXml technical debt

### Known export names; ABI prototypes unverified

- 107 of the 150 `sce::Xml` exports in [Export.cpp](../../../core/libs/prx/libSceXml/Export.cpp) are present in the PS5 3.20 `libSceXml` export inventory from [PS5-3.20_Libs](https://github.com/DNNDHH/PS5-3.20_Libs). Each is named after the mangled symbol the [verified NID name database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) gives for its NID, and the NID of that name matches. Their prototypes and behavior are not established; these 107 use the existing no-parameter import-resolution convention and throw through `NotImplemented_nid_no_patch`. The declarations do not claim an ABI or behavior.

### Value types and handle constructors

- [The `String`, `SimpleData`, `SerializeParameter`, `NodeId` and `Node` value operations and the handle constructors](../../../core/libs/prx/libSceXml/Export.cpp) (libSceXml) - follow `libSceXml.sprx` of firmware 13.60, read from disassembly, not measured on a console. `NodeId::operator void*` returns null for every `NodeId`, and the `NodeId` constructor and copies leave its padding bytes untouched. The handle constructors only clear the implementation pointer. Exports that read the objects `initialize` builds still throw, because nothing creates those objects yet: `AttributeList` `getLength` and `isAvailable`, `Document::resetStatus`, the `DocumentBuilder` and `Sax::Parser` setters, `Node(NodeId)`, and `Node` `getNodeType`, `hasChildNodes`, `hasAttributes` and `getOwnerDocument`. `Node::getAttributes` and `getChildNodes` are a bare return in the module, and the deleting `MemAllocator` destructor is `ud2`, so they still throw too.

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceXml/Export.cpp) - 33 callable NIDs match the [public 3.20 catalogue](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceXml.c) and the [selected export metadata](https://gist.github.com/kyon-eth/d35bcd082a172a28ada2052647b0ff20/72a4f16fcb238e61dc8c6424dc26eee106e6c8ce) from two version-labelled images (SHA-256 `3d83607a2ed81110a37d677e5ba1936a01cb78c5847a2e9b582a4a981d313050`). The labels are not authenticated. The names match the pinned NID name database above and hash to the recorded NIDs. Prototypes and behavior are unverified; the declarations throw through `NotImplemented_nid_no_patch` and implement no runtime functionality.

| Name | NID |
| --- | --- |
| `_ZNK3sce3Xml3Dom8Document11getNodeTypeENS1_6NodeIdE` | `1JAZNsOKua0` |
| `_ZNK3sce3Xml3Dom8Document12getLastChildENS1_6NodeIdE` | `5JyhV10GpQE` |
| `_ZNK3sce3Xml3Dom8Document7getTextENS1_6NodeIdE` | `7MsYv4D1Ang` |
| `_ZNK3sce3Xml3Dom8Document13getChildNodesENS1_6NodeIdEPNS1_8NodeListE` | `AI6mDpNxPYk` |
| `_ZNK3sce3Xml3Dom8Document11getNodeNameENS1_6NodeIdE` | `GceM6WE2WS4` |
| `_ZNK3sce3Xml3Dom8Document20getElementsByTagNameENS1_6NodeIdEPKNS0_6StringEPNS1_8NodeListE` | `HGZ0DTuCvuQ` |
| `_ZNK3sce3Xml3Dom8Document13getEntityTypeENS1_6NodeIdE` | `I3ootbOM-00` |
| `_ZNK3sce3Xml4Attr7getNameEv` | `KgzBbi3DUDA` |
| `_ZNK3sce3Xml3Dom8NodeList8findItemENS1_6NodeIdE` | `N-2qBJYv6lw` |
| `_ZNK3sce3Xml3Dom8Document13getAttributesENS1_6NodeIdEPNS1_8NodeListE` | `NZMWDTSdPXM` |
| `_ZNK3sce3Xml3Dom8Document13getFirstChildENS1_6NodeIdE` | `PXmLYh0zF14` |
| `_ZNK3sce3Xml3Dom8NodeList4itemEj` | `Q-ryXoj4ez0` |
| `_ZNK3sce3Xml4Attr8getValueEv` | `QOmIaoXFNig` |
| `_ZNK3sce3Xml3Dom8Document11getAttrNameENS1_6NodeIdE` | `REcO7rLo0fA` |
| `_ZNK3sce3Xml3Dom8NodeListixEj` | `VdYoc9kGeyA` |
| `_ZNK3sce3Xml3Dom8Document9getStatusEv` | `WPyn5rEy2DE` |
| `_ZNK3sce3Xml3Dom8Document14getSkippedTextENS1_6NodeIdE` | `XlzFT8nnAMU` |
| `_ZNK3sce3Xml3Dom8NodeList8findItemEPKNS0_6StringE` | `Zmk8Mb9qNQ0` |
| `_ZNK3sce3Xml3Dom8Document9getEntityENS1_6NodeIdE` | `aZH+29bSsEk` |
| `_ZNK3sce3Xml3Dom8Document11getNextAttrENS1_6NodeIdE` | `b9b6PA5TKc0` |
| `_ZNK3sce3Xml3Dom8Document12getAttrValueENS1_6NodeIdE` | `fw4+tIZ-4t8` |
| `_ZNK3sce3Xml3Dom8Document12getFirstAttrENS1_6NodeIdE` | `jMWTTO6X-kw` |
| `_ZNK3sce3Xml3Dom8NodeList11isAvailableEv` | `jc7L3HSltT0` |
| `_ZNK3sce3Xml3Dom8Document7getRootEv` | `mODyaCpyDnM` |
| `_ZNK3sce3Xml3Dom8Document12getAttributeENS1_6NodeIdEPKNS0_6StringE` | `miVYGbk3LHs` |
| `_ZNK3sce3Xml3Dom8Document10getXmlMetaEv` | `muE9OA3xYv4` |
| `_ZNK3sce3Xml3Dom8Document11isAvailableEv` | `nfoWUQ1eDl8` |
| `_ZNK3sce3Xml3Dom8Document9getParentENS1_6NodeIdE` | `p0IeMemvZKo` |
| `_ZNK3sce3Xml3Dom8NodeList9getLengthEv` | `tZioKsu5pxs` |
| `_ZNK3sce3Xml3Dom8Document10isReadOnlyEv` | `uiPVQSxXlsc` |
| `_ZNK3sce3Xml3Dom8Document13hasChildNodesENS1_6NodeIdE` | `ulLP+f5h0rg` |
| `_ZNK3sce3Xml3Dom8Document13hasAttributesENS1_6NodeIdE` | `vWA1r9CiPgI` |
| `_ZNK3sce3Xml4Attr11isAvailableEv` | `wlgM8cLl1mQ` |
