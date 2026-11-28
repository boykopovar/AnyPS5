# libSceJpegParser

## add Unknown function info

- [947b0wgy+oo, EU-X+AriMlg and mQgXX9lB5f8](../../../core/libs/prx/libSceJpegParser/Thumbnail.cpp) (libSceJpegParser) - firmware 13.42 exports that nothing declared, mapped in the #21873 reading (disassembly, not measured on a console). `EU-X+AriMlg` returns `0x809D4054` without reading an argument. `947b0wgy+oo` is the two-argument call on a path whose body (`0x8a00`) writes metadata, and `mQgXX9lB5f8` is `jpegParserCreateThumbnail2`, which hands its paths and options to libSceImageUtil's `oOjUpbj5GyI` and writes `0x20001` on success, as GIF's does (part 10). Both throw: the metadata writer is not implemented here, and `oOjUpbj5GyI` needs the console's JPEG decoder and encoder.
