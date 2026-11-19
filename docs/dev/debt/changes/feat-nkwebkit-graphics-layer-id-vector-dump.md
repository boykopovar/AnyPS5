# libSceNKWebKit

## add Functional

- [WebCore PlatformLayerID vector dump](../../../core/libs/prx/libSceNKWebKit/src/GraphicsLayerIdVectorDump.cpp) (libSceNKWebKit) - not compared with a console module: the device's WebKit revision is unverified. `operator<<(TextStream&, const Vector<uint64_t>&)` follows WebKit 73a35b43 [GraphicsLayer.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/graphics/GraphicsLayer.cpp#L1033-L1042), writing each layer ID through the `TextStream` 64-bit unsigned export, separated by single spaces, with no group or indent. A null stream, vector or non-empty vector buffer throws. Closes when: compared with a console module.

## change 2159 of these

- 2158 of these [older callable exports](../../../core/libs/prx/libSceNKWebKit/Export.cpp) are named after the symbols the [verified NID name database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) gives for their NIDs, and the NID of each name matches. Their prototypes and behavior stay unverified, and each still throws through `NotImplemented_nid_no_patch`.
