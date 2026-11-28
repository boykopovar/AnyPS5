# libkernel

## add Unknown function info

- [APR read failures](../../../core/libs/prx/libkernel/Apr/src/Apr.cpp) (libkernel) - a read command that cannot complete no longer throws: an unknown file id or a file that is gone fails with `ENOENT` (`0x80020002`), an unwritable destination with `EFAULT` (`0x8002000E`), and an unreadable file, an unseekable offset or a read that comes back short before the end of the file with `EIO` (`0x80020005`). The rest of the buffer still runs and the first failure is the buffer's status, which `sceKernelAprWaitCommandBuffer` returns; the `result` of `sceKernelAprSubmitCommandBufferAndGetResult` is written before the buffer runs, as the submit entry says, and stays 0. Only the last 4096 failures are kept. The codes and the continuation after a failed read are not measured on a console; other malformed commands and every AMM failure still throw

## change APR reads past the end of a file

- [APR reads past the end of a file](../../../core/libs/prx/libkernel/Apr/src/Apr.cpp) (libkernel) - a read that reaches the current end of the file on disk copies the bytes the file has and fills the rest of the destination with zeros; a read that comes back short before that end fails with `EIO`. PPSA18605 resolves its settings file once, rewrites it one byte shorter and reads it again with the size that resolve returned. The console's behaviour for a read past the end of a file is unverified: it may zero-fill, leave the destination untouched or fail the command
