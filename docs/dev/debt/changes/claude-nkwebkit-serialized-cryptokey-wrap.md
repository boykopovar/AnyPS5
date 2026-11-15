# libSceNKWebKit

## add Unknown function info

- [WebCore serialized crypto key wrapping](../../../core/libs/prx/libSceNKWebKit/src/SerializedCryptoKeyWrap.cpp) - not compared with a console module. `wrapSerializedCryptoKey` and `unwrapSerializedCryptoKey` follow the pinned [SerializedCryptoKeyWrapOpenSSL.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/crypto/openssl/SerializedCryptoKeyWrapOpenSSL.cpp#L48-L63), which the PlayStation build selects through platform/OpenSSL.cmake: the master key is ignored, the input bytes are copied into the output vector with capacity equal to their size as the pinned Vector.h copy constructor does, and the call returns true. The 16-byte `Vector<uint8_t>` layout is source-derived and pinned with `static_assert`.

## remove `_ZN7WebCore23wrapSerializedCryptoKeyERKN3WTF6VectorIhLm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEES6_RS4_`

## remove `_ZN7WebCore25unwrapSerializedCryptoKeyERKN3WTF6VectorIhLm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEES6_RS4_`
