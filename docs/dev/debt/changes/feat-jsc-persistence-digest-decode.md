# libSceJsc

## add Functional

- [WTF::Persistence::Coder<SHA1::Digest>::decode](../../../core/libs/prx/libSceJsc/src/WtfSha1.cpp) (libSceJsc) - not compared with a console module; the console WTF version, the 128-byte `Decoder` layout (buffer, position and end pointers, `SHA1` at 24) and the 21-byte `Optional<SHA1::Digest>` (flag at byte 0, returned indirectly) are unverified. It follows pinned [PersistentDecoder.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WTF/wtf/persistence/PersistentDecoder.cpp#L45-L60): with 20 bytes left it copies them, advances and adds salt 101 and the digest to the checksum; otherwise it returns an empty value. libSceJsc exports no `Decoder` constructor. A null decoder, a checksum cursor of 64 or more and a position past the end throw where WebKit asserts or overreads. Closes when: compared with a console module.

## remove | `_ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6decodeERNS0_7DecoderE` | `3U2pDdbhjag` |
