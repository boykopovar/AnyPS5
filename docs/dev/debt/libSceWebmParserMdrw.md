# libSceWebmParserMdrw technical debt

### Unknown function info

- [Callable exports](../../../core/libs/prx/libSceWebmParserMdrw/Export.cpp) - five of the seven callable NIDs are the `sceMetadataReaderWriter::webmParser` functions whose mangled names hash to them: `wVIrSj9g--M` is `webmParserFinalize`, `gq8g9It0U-w` `webmParserInitialize`, `8XGTsRfNvNs` `webmParserReadMetadata`, `NZQ5869nvfY` `webmParserWriteMetadata` and `vL4eBeS8uNs` `webmParserExtractThumbnail`. Return types and the `Metadata` and `Thumbnail` layouts are unknown, so the parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`.

- [Older callable NIDs](../../../core/libs/prx/libSceWebmParserMdrw/Export.cpp): `AUzGCnIU8WM` and `xnVMgJstTL4` stay literal NID aliases. Their names, signatures and behavior are unverified. These are throwing import-resolution placeholders; no functionality is implemented. No matching NIDs were found in the later module inventory.
