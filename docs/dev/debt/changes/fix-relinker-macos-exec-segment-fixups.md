# relinker

## add Functional

- In macOS output, a guest module initializer that reads a pointer of the executable in a writable and executable page sees it unrelocated: the [macOS image writer](../../../core/relinker/elfpatcher/src/macos/MacOsImage.cpp) maps segments with such pages without write access for dyld and applies their rebases and binds in each image's own `__mod_init_func` stub, which dyld runs for the executable after the initializers of the dylibs it loads. While the stub writes, every page of those segments is read-write, not executable; a thread already running code there would fault.

## remove macOS output still writes pointer fixups into executable segments
