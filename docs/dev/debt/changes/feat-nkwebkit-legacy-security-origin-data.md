# libSceNKWebKit

## add Functional

- [SecurityOriginData::fromDatabaseIdentifier (3.20)](../../../core/libs/prx/libSceNKWebKit/src/LegacySecurityOriginTuple.cpp) (libSceNKWebKit) - not compared with a console module. Only in the 3.20 inventory; it follows the 2020 [SecurityOriginData.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/page/SecurityOriginData.cpp), with the port read as `String::toInt` reads it. The 3.20 `SecurityOriginData` is two strings and a `WTF::Optional<uint16_t>` (a flag followed by the value); the result is written through the hidden result pointer, and an absent result has only its flag written. Both are derived from the pinned headers, not measured. A port of zero is stored as a disengaged port with a zero value, and null arguments throw `std::invalid_argument` where the source dereferences them. Closes when: compared with a console module.
