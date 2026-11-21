# libSceAjmi

## change Direct-console operation

- Direct-console operation and other firmware versions remain unverified for [`sceAjmiBatchJobInlineBuffer` and `sceAjmiBatchJobRunBufferRa`](../../../core/libs/prx/libSceAjmi/Export.cpp). Their signatures and forwarding follow the [13.42 symbol-table report](https://github.com/boykopovar/AnyPS5/pull/15495): each shares its entry address with the implemented `sceAjm*` export, at 0xb30 and 0xc50 in `libSceAjmi.sprx` (SHA-256 `ff0dc985c1f7a6b25a766904e3a535b55e44d0d4b8872329d2bd6c019491df0b`). The same-module routines were measured under an emulator, not on a console, as cited above. The inline builder now uses checked guest reads and writes for the batch header, input bytes and optional output pointer; a failed guest copy throws `std::runtime_error` because the console behavior for invalid pointers is unknown. Insufficient batch capacity remains unchecked by the inline builder, and the run builder's guest pointer checks remain missing.
