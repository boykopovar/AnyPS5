# libSceXml technical debt

### Known export names; ABI prototypes unverified

- 93 of the 150 `sce::Xml` exports in [Export.cpp](../../../core/libs/prx/libSceXml/Export.cpp) are stubs. All 150 are in the PS5 3.20 `libSceXml` export inventory from [PS5-3.20_Libs](https://github.com/DNNDHH/PS5-3.20_Libs) and are named after the mangled symbol the [verified NID name database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) gives for their NID, and the NID of that name matches. The prototypes and behavior of the 93 stubs are not established; they use the existing no-parameter import-resolution convention and throw through `NotImplemented_nid_no_patch`. The declarations do not claim an ABI or behavior.

### Value types and handle constructors

- [The `String`, `SimpleData`, `SerializeParameter`, `NodeId` and `Node` value operations and the handle constructors](../../../core/libs/prx/libSceXml/Export.cpp) (libSceXml) - follow `libSceXml.sprx` of firmware 13.60, read from disassembly, not measured on a console. `NodeId::operator void*` returns null for every `NodeId`, and the `NodeId` constructor and copies leave its padding bytes untouched. The handle constructors only clear the implementation pointer. Exports that read the objects `initialize` builds still throw, because nothing creates those objects yet: `AttributeList` `getLength` and `isAvailable`, `Document::resetStatus`, the `Sax::Parser` setters, `Node(NodeId)`, and `Node` `getNodeType`, `hasChildNodes`, `hasAttributes` and `getOwnerDocument`. `Node::getAttributes` and `getChildNodes` are a bare return in the module, and the deleting `MemAllocator` destructor is `ud2`, so they still throw too.

### Functional

- [Initializer::initialize](../../../core/libs/prx/libSceXml/Export.cpp) (libSceXml) allocates the 16-byte allocator wrapper and the 200-byte engine block that `libSceXml.sprx` of firmware 13.42 allocates, through the same allocator and in the same order, but fills them with zeros where the module stores its internal vtable pointer and its own state; no export reads them yet. Two module globals are not kept either: a flag set when the allocator changes, and a status that `initialize` records; only internal code reads them. Run under an emulator, not measured on a console.
- [DocumentBuilder](../../../core/libs/prx/libSceXml/src/DocumentBuilder.cpp) and [its parser](../../../core/libs/prx/libSceXml/src/DomParser.cpp) (libSceXml) - follow `libSceXml.sprx` of firmware 13.42 run under an emulator, not measured on a console. The parser builds the module's document byte stream and its text, element-name and attribute-name tables; on 9,187 firmware parses (every setter combination, both values of `parse`'s last-chunk flag, three random sets of malformed input) the result and all four buffers are identical. The first point below lists module bugs this keeps on purpose; the rest is not modelled:
  - Kept on purpose: names are looked up in a 211-bucket hash that matches a new name to any stored name it is a prefix of, siblings after an element that holds a reference lose their links, an element with an attribute block of 0xf0 bytes or more loses the links of its children, a character reference from U+10000 up whose low 16 bits are below 0x80 is stored as that low byte alone, a numeric reference is accumulated in 32 bits and wraps (`&#4294967361;` gives `A`), and an element with exactly one attribute stores that attribute's name offset in one byte.
  - `initialize` and `terminate` make the module's 15 allocations and frees in its order, but `parse` takes its working memory from the host heap: the module's per-parse engine reset, its per-element and per-attribute blocks and its buffer growth all go through the title's allocator, so a title that counts allocations or runs out of memory during `parse` sees a difference.
  - A name whose hash comes out negative (possible only for a name with a byte of 0x80 or more) makes the module read and write the bucket array at a negative index, in front of the array: what happens depends on the allocator's layout. This keeps a separate bucket for each negative hash, which matches every emulator run.
  - Each chunk is parsed again with the chunks before it, which gives the module's result for every split the emulator showed but costs time on documents fed in many chunks. A chunk after a stall inside a tag (a duplicate attribute, or a reference in an attribute value that does not resolve) throws, because the module resumes those differently depending on the split. A chunk also throws when a setting differs from the one its document started with. A builder that has thrown keeps throwing until it is terminated.
  - A failure of allocation 2 to 9 in `initialize` throws (the module faults at 2 and 9 and keeps a partial engine at 3 to 8), and so does `parse` after a failure of allocation 10 to 15.
  - A text entry or table offset of 16 MiB or more, an element-name offset above 0xffff, an attribute block above 0xffff bytes and a sibling link that would land past the end of the node stream throw, because the module's result was not measured.
  - Terminating the `Initializer` before the `DocumentBuilder` makes the module fault in `terminate`; here the builder still frees its blocks.
  - `getDocument` and the `Document` and `Node` reads that would show the parsed document are not implemented yet.

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
