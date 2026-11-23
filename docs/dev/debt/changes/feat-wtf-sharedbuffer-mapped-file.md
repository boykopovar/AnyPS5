# libSceNKWebKit

## add Unknown function info

- [WebCore::SharedBuffer mapped-file constructors](../../../core/libs/prx/libSceNKWebKit/src/SharedBuffer.cpp) (libSceNKWebKit) - the console layout for C1 (`VuC0CBsgzIU`) and C2 (`OwdCvNr3VXs`) is unverified. Both use the [2020 SharedBuffer layout](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/SharedBuffer.cpp) shared by the other buffer exports. The [pinned 11.x source](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/SharedBuffer.h#L263-L310) uses a different `FragmentedSharedBuffer` layout, including a contiguity flag and a different offset for the size. Closes when: the layout written by both NIDs is measured on a console module.

## remove | `_ZN7WebCore12SharedBufferC1EON3WTF14FileSystemImpl14MappedFileDataE` | `VuC0CBsgzIU` |

## remove | `_ZN7WebCore12SharedBufferC2EON3WTF14FileSystemImpl14MappedFileDataE` | `OwdCvNr3VXs` |
