# libSceNKWebKit

## add Older callable exports

- [EmptyFrameLoaderClient legacy callbacks](../../../core/libs/prx/libSceNKWebKit/src/EmptyLoaderLegacyCallbacks.cpp) (libSceNKWebKit) - not compared with a console module; argument conventions are unverified, and none of the seven reads its arguments. Their signatures predate the pinned PS5 WebKit 11.x source, so they follow WebKit [Safari-611.1.1 EmptyFrameLoaderClient.h](https://github.com/WebKit/WebKit/blob/Safari-611.1.1/Source/WebCore/loader/EmptyFrameLoaderClient.h), the first tag checked that declares all of them. `committedLoad` and `dispatchDidCommitLoad` do nothing, `dispatchCreatePage` returns null, `generatedMIMETypeForURLScheme` returns the empty string, and `pageID`, `frameID` and `createJavaAppletWidget` return a disengaged 16-byte `WTF::Optional` or a null `RefPtr` in memory.
