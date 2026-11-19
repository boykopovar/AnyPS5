# libSceNKWebKit

## add Functional

- [WebCore::Path::isNull](../../../core/libs/prx/libSceNKWebKit/src/PathCairo.cpp) (libSceNKWebKit) - not compared with a console module: the device's WebKit revision is unverified. The 3.20 export follows the Cairo [PathCairo.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/graphics/cairo/PathCairo.cpp#L534-L537) and returns whether the Cairo context pointer, the first member in [Path.h](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/graphics/Path.h#L266-L270), is null. The pinned 11.x source stores a variant there and has no such member. Closes when: compared with a console module.

## change 2159 of these

- 2158 of these [older callable exports](../../../core/libs/prx/libSceNKWebKit/Export.cpp) are named after the symbols the [verified NID name database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) gives for their NIDs, and the NID of each name matches. Their prototypes and behavior stay unverified, and each still throws through `NotImplemented_nid_no_patch`.
