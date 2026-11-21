# libScePngDec

## change [scePngDecDecode]

- [scePngDecDecode](../../../core/libs/prx/libScePngDec/Export.cpp): the input bit-depth limit and 16-to-8-bit conversion follow [Prosper's PS5 3.20 module analysis](https://github.com/mattias800/prosper/blob/550886e3677e4a2ef81ad1603bf313f6a9696de2/prosper/src/hle/util/hle_pngdec.cpp#L412-L414), and the large-dimension image-info requirement and its validation order follow [the same analysis](https://github.com/mattias800/prosper/blob/550886e3677e4a2ef81ad1603bf313f6a9696de2/prosper/src/hle/util/hle_pngdec.cpp#L385-L396). The supplied 13.42 module accepts row filter type 7 and leaves the rows decoded before an error when the IDAT data ends early. The host decodes nothing and returns `0x80690012` in those cases. Decoder scratch-memory behavior is not matched to the supplied module.

## change [scePngDecDecodeWithInputControl]

- [scePngDecDecodeWithInputControl](../../../core/libs/prx/libScePngDec/Export.cpp) (libScePngDec): native refill boundaries and callback call counts remain unmatched, and read-ahead at IEND is unmeasured. The host reads 16-byte header blocks and chunk-bounded body blocks of up to 1 KiB. ABI offsets and validation order come from firmware 11.00 disassembly, not console measurements. Decoding without a prior parse and concurrent use of a busy stream throw. Malformed data returns `0x80690010` and negative callback results propagate; the native `0x80690011` and `0x80690020` parse failures remain unmatched.

## add Functional

- [Indexed PNG output](../../../core/libs/prx/libScePngDec/Export.cpp) uses the buffer layout and size validation from [Prosper's PS5 3.20 module analysis](https://github.com/mattias800/prosper/blob/550886e3677e4a2ef81ad1603bf313f6a9696de2/prosper/src/hle/util/hle_pngdec.cpp#L266-L291). The host writes at most `1 << bitDepth` palette entries, preserves unused palette bytes and copies raw indices even when they exceed the palette size. This follows [13.42 module runs](https://github.com/boykopovar/AnyPS5/pull/12903#issuecomment-6530850638) for direct decode. Indexed callback output and Adam7 output are covered by host tests. Their console behavior and partial writes on indexed decode errors remain unmeasured.
