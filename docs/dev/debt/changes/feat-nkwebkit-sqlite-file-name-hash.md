# libSceNKWebKit

## add Functional

- [WebCore SQLiteFileSystem file name hash](../../../core/libs/prx/libSceNKWebKit/src/SQLiteFileSystem.cpp) (libSceNKWebKit) - not compared with a console module: the device's WebKit revision is unverified. `SQLiteFileSystem::computeHashForFileName(const String&)` follows [SQLiteFileSystem.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/sql/SQLiteFileSystem.cpp#L128-L144); the 11.x source takes a `StringView`, which the export name does not match. The name is converted with lenient `String::utf8`, hashed with the `PAL::CryptoDigest` SHA-256 export, and returned as 64 uppercase hex digits in an 8-bit `String`. A null `String` hashes as empty input. A null reference throws. Closes when: compared with a console module.
