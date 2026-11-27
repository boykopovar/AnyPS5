# libSceAc3Enc

## change [sceAc3EncCreateEncoder]

- [sceAc3EncCreateEncoder](../../../core/libs/prx/libSceAc3Enc/Export.cpp) (libSceAc3Enc) - `int32_t (int32_t* handle, const void* p, int32_t* status)` makes firmware 13.42's argument checks ([reading](https://github.com/boykopovar/AnyPS5/issues/21863#issuecomment-6940149738), disassembly only, not run on a console) and throws past them, since nothing here encodes: `0x80E00002` for a null `handle` or `p`, then `0x80E00003` unless the 32-bit `p+0x04` (channels) is 6, `0x80E00004` unless `p+0x08` (rate) is 48000, `0x80E00005` unless `p+0x0c` (bitrate) is 640000, `0x80E00006` unless `p+0x10` (mode) is 0 or 1, and `0x80E00007` unless `p+0x14` is 0. The console's first check, `0x80E00008` while an encoder exists, can't fire here; the AJM instance it creates after the checks is not implemented.

## add Unknown function info

- [sceAc3EncDeleteEncoder, sceAc3EncEncode, sceAc3EncEncodeAsync, sceAc3EncFlush, sceAc3EncIsEncoding and sceAc3EncWaitEncode](../../../core/libs/prx/libSceAc3Enc/Export.cpp) (libSceAc3Enc) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAc3Enc.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified

# libSceDtsEnc

## change [sceDtsEncCreateEncoder]

- [sceDtsEncCreateEncoder](../../../core/libs/prx/libSceDtsEnc/Export.cpp) (libSceDtsEnc) - `int32_t (int32_t* handle, const void* p, int32_t* status)` makes firmware 13.42's argument checks ([reading](https://github.com/boykopovar/AnyPS5/issues/21863#issuecomment-6940149738), disassembly only, not run on a console) and throws past them, since nothing here encodes: `0x80FA0002` for a null `handle` or `p`, then `0x80FA0003` unless the 32-bit `p+0x04` (channels) is 6, `0x80FA0004` unless `p+0x08` (rate) is 48000, `0x80FA0006` unless `p+0x0c` (mode) is 0 or 1, and `0x80FA0007` unless `p+0x10` is 0. The console's first check, `0x80FA0008` while an encoder exists, can't fire here; the AJM instance it creates after the checks is not implemented.

## add Unknown function info

- [sceDtsEncDeleteEncoder, sceDtsEncEncode, sceDtsEncEncodeAsync, sceDtsEncFlush, sceDtsEncIsEncoding and sceDtsEncWaitEncode](../../../core/libs/prx/libSceDtsEnc/Export.cpp) (libSceDtsEnc) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceDtsEnc.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified
