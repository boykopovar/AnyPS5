# libSceNKWebKit

## add Functional

- [WebCore ad click attribution state](../../../core/libs/prx/libSceNKWebKit/src/AdClickAttribution.cpp) (libSceNKWebKit) - not compared with a console module: the device's WebKit revision is unverified. `markAsExpired`, `hasExpired`, `hasHigherPriorityThan`, `markConversionAsSent` and `wasConversionSent` follow [AdClickAttribution.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/loader/AdClickAttribution.cpp#L129-L210) of August 2020; the 11.x source renames the class `PrivateClickMeasurement`. The 64-byte layout puts the `WTF::Optional` flag before its value. An attribution expires one week after the ad click. Closes when: compared with a console module.
