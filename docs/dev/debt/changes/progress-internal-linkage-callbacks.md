# libfreetype

## add Functional

- [CompressedStreams](../../../core/libs/prx/libfreetype/src/CompressedStreams.cpp) (libfreetype) - the internal `OutputRead` and `OutputClose` stream callbacks throw when the decompressed stream hit a source fault or a read the state cannot serve. Closes when: those paths return the FreeType error a console module returns.

# libSceFontGs

## add Functional

- [GnmBackendUnported](../../../core/libs/prx/libSceFontGs/Export.cpp) (libSceFontGs) - the internal backend callback behind `sceFontSelectGraphicsGnm` throws `sceFontSelectGraphicsGnm_backend_<opcode>` for every opcode, so no Gnm draw backend call completes. Closes when: the Gnm backend opcodes are implemented.

# libSceFreeTypeOt

## add Functional

- [StreamIo](../../../core/libs/prx/libSceFreeTypeOt/src/System.cpp) (libSceFreeTypeOt) - the internal stream I/O callback throws as `FT_Stream_Open` for an offset above `std::streamoff` max. Closes when: that offset returns the FreeType read error instead.

# libSceLibreSsl

## add Functional

- [BIO ssl and memory callbacks](../../../core/libs/prx/libSceLibreSsl/src/Bio.cpp) (libSceLibreSsl) - not measured on a PS5. The internal `readSsl`, `writeSsl` and `callbackControlSsl` callbacks always throw. `putsSsl`, `controlSsl` and `destroySsl` throw when an SSL is attached, when a guest string or out-pointer cannot be copied, or for a non-null control pointer. `writeMemory` and `putsMemory` throw on size overflow or an uncopyable guest string. Closes when: BIO_f_ssl reads, writes and controls an attached SSL, and those copy failures set the BIO error.
- [EVP_PKEY RSA method callbacks](../../../core/libs/prx/libSceLibreSsl/src/DigestVerify.cpp) (libSceLibreSsl) - not measured on a PS5. `UnavailableSign`, `UnavailableVerifyRecover`, `UnavailableEncrypt`, `UnavailableDecrypt`, `UnavailableKeyGeneration` and `UnavailableControlString` throw for key contexts this library created. `RsaVerify` throws for RSA-PSS and X9.31 padding. `RsaContextCopy` and `RsaContextCleanup` throw for a context with a keygen public exponent. Closes when: those operations and paddings are implemented.
- [RSA default method callbacks](../../../core/libs/prx/libSceLibreSsl/src/Rsa.cpp) (libSceLibreSsl) - not measured on a PS5. `DefaultModExp` and `DefaultBignumModExp` throw as `RSA_eay_mod_exp` and `BN_mod_exp_mont_ct`, and `DefaultFinish` throws when it would free a Montgomery context. Closes when: Montgomery modular exponentiation is implemented.
- [X509_STORE callbacks](../../../core/libs/prx/libSceLibreSsl/src/X509Store.cpp) (libSceLibreSsl) - not measured on a PS5. `CompareObjects` throws for object comparison and `LoadFile` throws for loading certificates and CRLs from a file. Closes when: the store compares objects and loads files.

# libSceLibreSsl3

## add Functional

- [EVP digest callbacks](../../../core/libs/prx/libSceLibreSsl3/src/EvpDigest.cpp) (libSceLibreSsl3) - not measured on a PS5. `Sha1Init`, `Sha1Update`, `Sha1Final` and the SHA-224 equivalents throw when the context or its digest data is null. Closes when: a null context returns the LibreSSL result instead.
- [EVP_PKEY RSA callbacks](../../../core/libs/prx/libSceLibreSsl3/src/PkeyContext.cpp) (libSceLibreSsl3) - not measured on a PS5. `RsaSign` and `PkeyRsaVerify` throw for X9.31 padding, `RsaPssInitialize` for keys with PSS restrictions, and `RsaKeyControl` for a key without a native RSA key or with PSS restrictions. `PkeyRsaContextCopy`, `RsaContextClear`, `RsaEncryption`, `RsaDecryption` and `PkeyRsaControl` reach unimplemented exports. Closes when: X9.31 and PSS are implemented.
- [X509_STORE_CTX callbacks](../../../core/libs/prx/libSceLibreSsl3/src/X509StoreContext.cpp) (libSceLibreSsl3) - not measured on a PS5. `InternalVerify`, `GetIssuer` and `CheckIssued` always throw. Closes when: chain verification is implemented.

# libSceNKWebKit

## add Functional

- [JSON writeJSON slots](../../../core/libs/prx/libSceNKWebKit/src/WtfJson.cpp) (libSceNKWebKit) - the `ValueWriteJSON` and `ArrayWriteJSON` vtable slots throw because `writeJSON` serialization is not implemented; `ObjectWriteJSON` in [WtfJsonObject.cpp](../../../core/libs/prx/libSceNKWebKit/src/WtfJsonObject.cpp) does the same. Closes when: JSON serialization is implemented.
- [TrackPrivateBase destructors](../../../core/libs/prx/libSceNKWebKit/src/TrackPrivateBase.cpp) (libSceNKWebKit) - the vtable `CompleteDestructor` and `DeletingDestructor` forward to the `TrackPrivateBase` D1 and D0 exports, which are not implemented. Closes when: those destructors are implemented.

# libSceNpCppWebApi

## add Functional

- [Matches model destructors](../../../core/libs/prx/libSceNpCppWebApi/Export.cpp) (libSceNpCppWebApi) - `DestroyVectorBody` and the `Destroy*` functions for the matches request and model objects release intrusive members and throw if a released member has no deleter, so a title-provided member without one is unsupported. Closes when: such members are released through their real deleter.
