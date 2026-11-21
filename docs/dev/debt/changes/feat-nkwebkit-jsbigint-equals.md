# libSceNKWebKit

## add Functional

- [JSC::JSBigInt::equals and JSC::JSBigInt::equalsToInt32](../../../core/libs/prx/libSceNKWebKit/src/JscBigInt.cpp) (libSceNKWebKit) - not compared with a console module. They follow [JSBigInt.cpp](https://github.com/WebKit/WebKit/blob/e12a212a531a/Source/JavaScriptCore/runtime/JSBigInt.cpp#L1363-L1377) and [its equalsToInt32](https://github.com/WebKit/WebKit/blob/e12a212a531a/Source/JavaScriptCore/runtime/JSBigInt.cpp#L2575-L2580) and read the cell layout both [e12a212a531a](https://github.com/WebKit/WebKit/blob/e12a212a531a/Source/JavaScriptCore/runtime/JSBigInt.h#L618-L621) and [the PlayStation source](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/runtime/JSBigInt.h#L624-L627) use: length at 8, sign at 16 and a digit pointer at 24. The digit pointer is read uncaged because `GIGACAGE_ENABLED` is 0 outside Darwin and Linux. Closes when: checked against a console module.

## remove `_ZN3JSC8JSBigInt13equalsToInt32Ei`

## remove `_ZN3JSC8JSBigInt6equalsEPS0_S1_`
