# libSceNKWebKit

## add Functional

- [ISOBox allocation operators](../../../core/libs/prx/libSceNKWebKit/src/WtfMemory.cpp) (libSceNKWebKit) follow the pinned WebKit 11.x [`WTF_MAKE_FAST_ALLOCATED` contract](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/FastMalloc.h#L287-L309) for `fastMalloc`/`fastFree`; placement overloads return the supplied storage. Console ABI remains unverified.
