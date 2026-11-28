# libSceTtsCoreG3Bc

## add Functional

- [Verified engine helper exports](../../../core/libs/prx/libSceTtsCoreG3Bc/Export.cpp) - In firmware 13.60 `libSceTtsCoreG3Bc.sprx`, these 6 exports match verified disassembly: `RByzVVv6e0A` null-checks `object` and tail-calls `vtable[1](object)`; `iW0DoaHZE5Y` stores the byte at `+0xe79`; `Sq5GSDo-Rhk` copies 16 bytes into `+0xe58`; `pacG6ffHAvU` returns `27` when `int32_t` at `+0xe6c` is `10` or `int(1.11f^(10-level) * 27.47f)`; `bapD6H6e7Yk` returns `0` for null `engine`, `0x5622` when voice context `+0x5f8` is inactive (`+0x48 == 0` or `+0x38 == -1`), or `uint32_t` sample rate at `+0x7e4`; `fF3Q3LdPUtk` zeroes 128 bytes, sets `+0x84 = 64`, and writes `1000000000 / sampleRate` (`62499` if `62500`) to `+0x80`. Engine mutex locking at `+0x8` is omitted until `1scTM6aHK5I` constructs initialized mutexes.
