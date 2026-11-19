# libSceNKWebKit

## add Functional

- [WebCore ExceptionData isolated copy](../../../core/libs/prx/libSceNKWebKit/src/ExceptionData.cpp) (libSceNKWebKit) - not compared with a console module: the device's WebKit revision is unverified. `ExceptionData::isolatedCopy() const` follows [ExceptionData.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/dom/ExceptionData.cpp#L30-L37); the 11.x source has inline `const&`/`&&` overloads instead, which the export name does not match. The code is copied unchanged and the message goes through `String::isolatedCopy`. The struct is read as `{int code; String message}` (16 bytes) and returned through sret. A null `this` throws. Closes when: compared with a console module.
