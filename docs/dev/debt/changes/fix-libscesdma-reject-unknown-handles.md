# libSceSdma

## change a null handle, and a copy or fill command on a closed handle

- [libSceSdma](../../../core/libs/prx/libSceSdma/Export.cpp): a null handle, a handle that is not the address of an engine slot `sceSdmaOpen` returns (engines 12 to 19), and a copy or fill command on a closed handle, return `0x80990005` (`SdmaErrorInvalidArgument`, the code sceSdmaOpen already uses for bad arguments). That value is not measured on a console for these cases; the library header documents no invalid-handle code.
