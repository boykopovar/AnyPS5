# libSceGvMp4Parser technical debt

### Unknown function info

- [_ZN23sceMetadataReaderWriter11gvMp4Parser19gvMp4ParserFinalizeEv, _ZN23sceMetadataReaderWriter11gvMp4Parser21gvMp4ParserInitializeEv, _ZN23sceMetadataReaderWriter11gvMp4Parser23gvMp4ParserReadMetadataERKSsRKNS_8MetadataERS3_, _ZN23sceMetadataReaderWriter11gvMp4Parser24gvMp4ParserWriteMetadataERKSsRKNS_8MetadataE, _ZN23sceMetadataReaderWriter11gvMp4Parser27gvMp4ParserExtractThumbnailERKSsRNS_9ThumbnailE](../../../core/libs/prx/libSceGvMp4Parser/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceGvMp4Parser.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [_ZN23sceMetadataReaderWriter11gvMp4Parser21gvMp4ParserInitializeEv, _ZN23sceMetadataReaderWriter11gvMp4Parser23gvMp4ParserReadMetadataERKSsRKNS_8MetadataERS3_, _ZN23sceMetadataReaderWriter11gvMp4Parser24gvMp4ParserWriteMetadataERKSsRKNS_8MetadataE, _ZN23sceMetadataReaderWriter11gvMp4Parser27gvMp4ParserExtractThumbnailERKSsRNS_9ThumbnailE](../../../core/libs/prx/libSceGvMp4Parser/Export.cpp) - not exported by any firmware 13.42 module, so on 13.42 an import of these NIDs resolves to nothing in the firmware; the 3.20 declarations remain for titles built against older firmware. Measured as a payload process on a device: `dlsym` on the `dlopen` handle of `libSceGvMp4Parser.sprx` returns null for each of these names, while `gvMp4ParserFinalize` resolves.

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceGvMp4Parser/Export.cpp) - 16 NIDs have callable entries in a 3.20-labelled image and in a later 13.60-labelled image under a different module and library identity. Image labels are not independently authenticated. Names, parameter lists, return types, behavior and cross-version equivalence are unverified. The parameterless `int` declarations are placeholders and every entry throws.

- [Older callable NIDs](../../../core/libs/prx/libSceGvMp4Parser/Export.cpp): 3 unidentified exports have unverified names, signatures and behavior. These are throwing import-resolution placeholders; no functionality is implemented. No matching NIDs were found in the later module inventory.
