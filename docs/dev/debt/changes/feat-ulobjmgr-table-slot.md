# ulobjmgr

## remove `HZ9Q2c+4BU4`

## remove `SweJO7t3pkk`

## remove [Additional callable exports]

## add Unknown function info

- [ulobjmgrUnknown_SweJO7t3pkk_nid_no_patch and ulobjmgrUnknown_HZ9Q2c_plus_4BU4_nid_no_patch](../../../core/libs/prx/ulobjmgr/Export.cpp) (ulobjmgr, NIDs `SweJO7t3pkk` and `HZ9Q2c+4BU4`) - names unknown; read from the disassembly of `ulobjmgr.sprx` (firmware 13.40), not measured on a console. The module keeps a 64-bit slot at offset 0x18 of its object table, which it maps as named flexible memory and publishes with `sceKernelSetProcessProperty`; `SweJO7t3pkk` stores its 64-bit argument there (`EINVAL` for 0, `EBUSY` while the slot is set) and `HZ9Q2c+4BU4` clears it (`ENOENT` while it is clear). The host keeps the slot outside guest memory, like the rest of the table (see Silent stubs), so nothing but these two calls reads the stored value, and the meaning of that value is unknown
