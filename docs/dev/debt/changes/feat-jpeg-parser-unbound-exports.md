# libSceJpegParser

## add Unknown function info

- [947b0wgy+oo, EU-X+AriMlg and mQgXX9lB5f8](../../../core/libs/prx/libSceJpegParser/Thumbnail.cpp) (libSceJpegParser) - firmware 13.42 exports that nothing declared, mapped in the #21873 reading (disassembly, not measured on a console). `EU-X+AriMlg` returns `0x809D4054` without reading an argument. `947b0wgy+oo` is the two-argument call on a path whose body (`0x8a00`) writes metadata, and `mQgXX9lB5f8` is `jpegParserCreateThumbnail2`, which the reading gives as GIF's shape without reading it in full. Both throw: the metadata writer is not implemented here, and the thumbnail call needs libSceImageUtil's `SaveJpegFromFile` (`oOjUpbj5GyI`), which is not implemented either.
