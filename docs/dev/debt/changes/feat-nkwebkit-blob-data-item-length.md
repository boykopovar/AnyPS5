# libSceNKWebKit

## add Functional

- [WebCore::BlobDataItem::length](../../../core/libs/prx/libSceNKWebKit/src/BlobDataItem.cpp) (libSceNKWebKit) - not compared with a console module; the 40-byte `BlobDataItem` layout (`int` type, data and file pointers, `long long` offset and length) follows the pinned [BlobData.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/network/BlobData.h#L43-L96) and is unverified; the file size is read at offset 32 of the existing `BlobDataFileReference` model, without `FILE_REPLACEMENT`. A null item or a null file reference for an unknown file length throws `std::invalid_argument` where the module would crash, and an unreadable item or file reference faults as not implemented. Closes when: compared with a console module.

## remove | `_ZNK7WebCore12BlobDataItem6lengthEv` | `wAWyYd8-0Bk` |
