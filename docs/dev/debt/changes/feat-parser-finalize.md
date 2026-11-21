# libSceJpegParser

## change [_ZN23sceMetadataReaderWriter10jpegParser18jpegParserFinalizeEv,

- [_ZN23sceMetadataReaderWriter10jpegParser20jpegParserInitializeEv, _ZN23sceMetadataReaderWriter10jpegParser22jpegParserReadMetadataERKSsRKNS_8MetadataERS3_, _ZN23sceMetadataReaderWriter10jpegParser23jpegParserWriteMetadataERKSsRKNS_8MetadataE, _ZN23sceMetadataReaderWriter10jpegParser26jpegParserExtractThumbnailERKSsRNS_9ThumbnailE](../../../core/libs/prx/libSceJpegParser/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceJpegParser.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.

## add Functional

- [_ZN23sceMetadataReaderWriter10jpegParser18jpegParserFinalizeEv](../../../core/libs/prx/libSceJpegParser/Export.cpp) returns 0, following a [reading of firmware 13.42's libSceJpegParser.sprx](https://github.com/boykopovar/AnyPS5/issues/21873#issuecomment-6946580685), not measured on a console: it reads no argument and always returns 0. With no context it does nothing else. The module's Initialize (`v3aOHSEXfjo`) is not bound here, so no context exists; binding it needs Finalize's teardown of the context and the allocator table it copies.

# libSceGvMp4Parser

## change [_ZN23sceMetadataReaderWriter11gvMp4Parser19gvMp4ParserFinalizeEv,

- [_ZN23sceMetadataReaderWriter11gvMp4Parser21gvMp4ParserInitializeEv, _ZN23sceMetadataReaderWriter11gvMp4Parser23gvMp4ParserReadMetadataERKSsRKNS_8MetadataERS3_, _ZN23sceMetadataReaderWriter11gvMp4Parser24gvMp4ParserWriteMetadataERKSsRKNS_8MetadataE, _ZN23sceMetadataReaderWriter11gvMp4Parser27gvMp4ParserExtractThumbnailERKSsRNS_9ThumbnailE](../../../core/libs/prx/libSceGvMp4Parser/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceGvMp4Parser.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.

## add Functional

- [_ZN23sceMetadataReaderWriter11gvMp4Parser19gvMp4ParserFinalizeEv](../../../core/libs/prx/libSceGvMp4Parser/Export.cpp) returns 0, following a [reading of firmware 13.42's libSceGvMp4Parser.sprx](https://github.com/boykopovar/AnyPS5/issues/21873#issuecomment-6946580685), not measured on a console: it reads no argument and always returns 0. With no context it only zeroes its 0x58-byte allocator table. The module's Initialize (`sSswLa3vxMA`) is not bound here, so no context exists; binding it needs Finalize's teardown of the context and the allocator table it copies.
