# libSceJsc

## add Functional

- [WTF::Persistence::Coder<SHA1::Digest>::encode](../../../core/libs/prx/libSceJsc/src/WtfSha1.cpp) (libSceJsc) - not compared with a console module; the console WTF version and 4216-byte `Encoder` layout (`buffer`, `capacity`, `size`, `inlineBuffer[4096]`, `SHA1` at 4112) are unverified. Following [PersistentCoders.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WTF/wtf/persistence/PersistentCoders.cpp#L143-L146) and [PersistentEncoder.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WTF/wtf/persistence/PersistentEncoder.cpp#L41-L61), it hashes salt 101 and the 20-byte digest into `sha1` and appends the digest. `libSceNKWebKit` registers `fastMalloc`/`fastFree` at load for `NKWebKit::AllocationHeader` compatibility; growing past `capacity` unregistered, null pointers, `buffer == nullptr`, `cursor >= 64`, `size > capacity` or `unsigned` overflow throw. Closes when: compared with a console module.

## remove | `_ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6encodeERNS0_7EncoderERKS3_` | `jiOeDsjiDkY` |
