# libSceNKWebKit

## add Unknown function info

- [SecurityOriginData](../../../core/libs/prx/libSceNKWebKit/src/SecurityOrigin.cpp) (libSceNKWebKit) - console symbols predate pinned [SecurityOriginData.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/page/SecurityOriginData.cpp#L40-L156): the exported `isolatedCopy` has no ref qualifier, 11.x `const &`. The 32-byte `isolatedCopy` result follows the pinned variant layout and is not compared with a console module; it is returned through the hidden result pointer, which the SysV x86-64 psABI prescribes for a class larger than 16 bytes. Closes when compared with a console.

## add Functional

- [WebCore::SecurityOriginData::fromFrame with a frame](../../../core/libs/prx/libSceNKWebKit/src/SecurityOrigin.cpp) (libSceNKWebKit) - a non-null `Frame*` throws `NotImplemented_nid_no_patch` because the console `LocalFrame::document` and `Document::securityOrigin` offsets are not modelled; a null frame returns the default-constructed value as [SecurityOriginData.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/page/SecurityOriginData.cpp#L61-L71) does. Closes when: those offsets are measured on a console.

