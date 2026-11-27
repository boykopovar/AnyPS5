# libSceXml

## add Functional

- [Sax::Parser::parse](../../../core/libs/prx/libSceXml/src/SaxParser.cpp) (libSceXml) - implements `<!DOCTYPE ...>` (`node(5)`), `setResolveEntity(true)` entity resolution, chunk-split buffering, and boundary/markup error status codes (`0x8085000b`, `0x8085000e`, `0x80850010`..`0x80850017`) per firmware 13.60 `libSceXml.sprx` disassembly, not measured on a console. Initialization 2nd/3rd allocation failure, null handler dereference, and pre-init `setDocumentHandler` guards retain throwing checks.
