# libSceNKWebKit

## add Functional

- [WTF::URLHelpers::userVisibleURL](../../../core/libs/prx/libSceNKWebKit/src/UserVisibleURL.cpp) (libSceNKWebKit) - Throws for any URL containing `xn--` outside a percent escape: upstream's host decoding there (`mapHostNames`) needs ICU IDNA, which libSceIcu lacks, and the throw also covers `xn--` URLs with no host, which upstream returns unchanged. Lookalike and NFC answers use libSceIcu data, whose Unicode version is unverified against the console. Not compared with a console module. Ports the 11.x [URLHelpers.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/URLHelpers.cpp#L67-L967); the `xn--` check skips upstream's read before its buffer, and inputs over (2^31 - 2) / 3 bytes give a null String. Closes when: ICU IDNA is implemented and the result is compared with a console module.

## remove | `_ZN3WTF10URLHelpers14userVisibleURLERKNS_7CStringE` | `2lI1xw8MWoE` |
