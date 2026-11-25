# ulobjmgr

## remove registers no object

## remove Additional callable exports

## remove | `Unknown` | `HZ9Q2c+4BU4` |

## remove | `Unknown` | `SweJO7t3pkk` |

## change [_sceUlobjmgrRegisterObject]
- [_sceUlobjmgrRegisterObject](../../../core/libs/prx/ulobjmgr/Export.cpp) (ulobjmgr) - the `0x3ffc`-slot (`0..0x3ffb`) LIFO freelist, `ENOMEM` (`12`) with `0xffffffffu` output on exhaustion, `EINVAL` (`22`) bound in `_sceUlobjmgrUnregisterObject`, and global slot behavior in `SweJO7t3pkk` / `HZ9Q2c+4BU4` match `ulobjmgr.sprx` disassembly; parameter names and `EFAULT` (`14`) on unwritable `id` remain unmeasured on hardware
