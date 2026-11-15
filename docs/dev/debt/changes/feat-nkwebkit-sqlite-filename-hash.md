# libSceNKWebKit

## add Unknown function info

- [WebCore::SQLiteFileSystem::computeHashForFileName](../../../core/libs/prx/libSceNKWebKit/src/SQLiteFileSystem.cpp) (libSceNKWebKit) - not compared with a console module. It follows [SQLiteFileSystem.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/sql/SQLiteFileSystem.cpp#L148-L165): the lenient UTF-8 of the name goes through `PAL::CryptoDigest` SHA-256 and the result is 64 uppercase hex characters. The pinned header takes a `StringView`, but the console export takes `const WTF::String&`. The UTF-8 bytes are the same either way, and a null String hashes as empty input. Closes when: compared with a console module.
