# libSceSaveData technical debt

### Functional

- [libSceSaveData](../../../core/libs/prx/libSceSaveData/CMakeLists.txt) is built from the libSceSaveData.native sources, so a title that imports save data from `libSceSaveData`, as PS4 titles do, gets the same functions. The separate implementation it had before kept save-data memory in `_sd/sce_sdmemory/<user>/memory.dat` and mount aliases in `_sd/_aliases`; neither is read, so memory saved through that library is not found
