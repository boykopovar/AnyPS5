# relinker

## add Functional

- [Windows output staging](../../../core/relinker/io/src/WindowsFileOutput.hpp) leaves sibling `.anyps5-` files or recovery directories after a process crash or forced termination. Stale files are not swept automatically because a directory can hold the only remaining previous output. Replacement requires destination write access and directory create/delete access; unsupported replacement volumes fail instead of truncating an existing regular file. Replacement preserves the DACL and selected file metadata, but can change ownership. Encrypted files, alternate streams, devices and unsupported reparse points use the stream writer and do not receive staged-write protection. Network-share and power-loss recovery have not been verified.
