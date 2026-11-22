# libSceNKWebKit

## add Functional

- [EmptyFrameLoaderClient::shouldUseCredentialStorage](../../../core/libs/prx/libSceNKWebKit/src/EmptyClients.cpp) (libSceNKWebKit) - the PS5 ABI is unverified, and this 3.20-only symbol takes an `unsigned long` identifier, which the pinned 11.x revision replaced with `ResourceLoaderIdentifier`. It returns false and ignores the document and identifier, as WebKit [EmptyFrameLoaderClient.h:59](https://github.com/WebKit/WebKit/blob/72c72d5a8583a1cc18a0181ef16c577d862892f5/Source/WebCore/loader/EmptyFrameLoaderClient.h#L59) (2020, the signature this export mangles) and 11.x [EmptyClients.cpp:674-677](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/loader/EmptyClients.cpp#L674-L677) do.
