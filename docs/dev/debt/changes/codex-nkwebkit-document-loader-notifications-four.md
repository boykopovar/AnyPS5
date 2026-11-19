# libSceNKWebKit

## add Functional

- [EmptyFrameLoaderClient document-loader notifications](../../../core/libs/prx/libSceNKWebKit/src/EmptyClients.cpp) (libSceNKWebKit) - four wrappers follow the empty `void` bodies in pinned [EmptyClients.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/loader/EmptyClients.cpp#L670-L975) for `assignIdentifierToInitialRequest`, `updateCachedDocumentLoader`, `dispatchUnableToImplementPolicy` and `setTitle`. Their exported NIDs remain unchanged; the physical console ABI is unverified.

## remove `_ZN7WebCore22EmptyFrameLoaderClient26updateCachedDocumentLoaderERNS_14DocumentLoaderE`

## remove `_ZN7WebCore22EmptyFrameLoaderClient31dispatchUnableToImplementPolicyERKNS_13ResourceErrorE`

## remove `_ZN7WebCore22EmptyFrameLoaderClient8setTitleERKNS_19StringWithDirectionERKN3WTF3URLE`
