# libSceNKWebKit

## add Functional

- [Frame, Document and FrameView forwarders](../../../core/libs/prx/libSceNKWebKit/src/FrameForwardExports.cpp) (libSceNKWebKit) - `Frame::pageID`, `Frame::frameID`, `Document::pageID` and `FrameView::tiledBacking`. The offsets are the module's own, not confirmed by any header here: `Frame` `m_loader` 0xa0, `Document` `m_frame` 0x188 then 0x8, and the `FrameView` chain 0x128, 0xc0, 0x578, 0xa0, 0xe8 (`libSceNKWebKit.sprx` SHA-256 `3af992253665387feeb5df90ff27dff683e3aa270cc4d92a0535b46ab18fa20d`). A null object or pointer where the module would fault throws instead. Each export is the module's load chain in assembly, tail-jumping to the already implemented `FrameLoader::pageID`, `FrameLoader::frameID` or `RenderLayerBacking::tiledBacking`, as in WebKit e12a212a53 (May 2021); the 11.x `Document::pageID` has a null check the module lacks.
