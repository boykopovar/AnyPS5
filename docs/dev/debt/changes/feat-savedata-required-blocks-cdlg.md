# libSceSaveData

## remove | `sceSaveDataGetRequiredBlocksForCdlg` | `wJ6oiu+TqK8` |

## add Unknown function info

- [sceSaveDataGetRequiredBlocksForCdlg](../../../core/libs/prx/libSceSaveData/InventoryExports.cpp) (libSceSaveData) - takes `(uint64_t* requiredBlocks)` and follows firmware 13.42's export ([reading](https://github.com/boykopovar/AnyPS5/issues/21835#issuecomment-6932719164), disassembly only, not run on a console): a null pointer returns `0x809F8004`; otherwise it writes the module's stored block count and returns 0, with no other check. 13.42's `sceSaveDataMount2` stores that count, moving `required_blocks` out of its result, only on a no-space failure and only when a context field is at least `0x11500000`, which wasn't traced to its source. That store isn't modelled, so the count here is always 0, as on the console before such a mount. An unwritable output returning `0x809F8004` is a host-safety fallback. Closes when: the context field is traced.
