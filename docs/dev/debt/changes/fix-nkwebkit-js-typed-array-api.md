# libSceNKWebKit

## add Functional

- [JSObjectMakeTypedArrayWithArrayBuffer](../../../core/libs/prx/libSceNKWebKit/src/JscObject.cpp) (libSceNKWebKit) - not checked on a console. The typed array exports follow [JSTypedArray.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/API/JSTypedArray.cpp#L152-L375). Gaps: a `SharedArrayBuffer` is not an array buffer for `JSObjectGetArrayBufferBytesPtr`, `JSObjectGetArrayBufferByteLength` and the two `JSObjectMakeTypedArrayWithArrayBuffer` exports, where `JSArrayBuffer` covers both; a NULL object gives 0 or NULL where WebKit reads it, except in `JSObjectGetArrayBufferByteLength`, which throws as its `RELEASE_ASSERT` does; the range errors of a bad offset or length carry the QuickJS messages; a resizable buffer, which only the engine here has, makes `JSObjectMakeTypedArrayWithArrayBuffer` throw. Closes when: shared buffers are recognised and the messages match.
