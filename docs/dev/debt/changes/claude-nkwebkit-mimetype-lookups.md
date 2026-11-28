# libSceNKWebKit

## add Functional

- [MIME type lookups by extension and path](../../../core/libs/prx/libSceNKWebKit/src/Mime.cpp) (libSceNKWebKit) - `systemPreviewMIMETypes` throws: before WebKit@c650d41f8945 it returns a `HashSet` reference, after it a `FixedVector`, and the 3.20 module's ABI is unverified. Not compared with a console module: the 3.20 `mimeTypeForExtension`, `mimeTypeForPath`, `mediaMIMETypeForExtension`, `isJavaAppletMIMEType` and `isSystemPreviewMIMEType` follow [MIMETypeRegistry.cpp](https://github.com/WebKit/WebKit/blob/e12a212a531aeddc11259e8515671cf5188e038d/Source/WebCore/platform/MIMETypeRegistry.cpp#L274-L689) of May 2021 but use the pinned 11.x PlayStation table, which adds `mjs`, `mp4` and `webp`. A null extension gives `mediaMIMETypeForExtension` the empty string; upstream dereferences it. Closes when: compared with a console module.
