# libScePngDec

## add Functional

- [scePngDecParseHeader and scePngDecDecode](../../../core/libs/prx/libScePngDec/Export.cpp) reject empty, non-triplet and over-768-byte `PLTE` chunks for all PNG color types, including RGB and RGBA; reject `PLTE` for grayscale and grayscale-alpha images, duplicate palettes and palettes after IDAT. A legal RGB or RGBA `PLTE` before IDAT remains accepted. These validations follow [PNG Third Edition, PLTE](https://www.w3.org/TR/png/#11PLTE); the emulator returns the existing `0x80690010` invalid-data status for violations, but the exact native PS5 error precedence for malformed palettes has not been measured.
