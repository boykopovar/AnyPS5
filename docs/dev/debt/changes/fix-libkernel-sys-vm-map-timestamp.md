# libkernel_sys

## change [get_vm_map_timestamp]

- [get_vm_map_timestamp](../../../core/libs/prx/libkernel_sys/Export.Discovery.cpp) (libkernel_sys) returns the low 32 bits of a dedicated guest VM-map generation. It advances when a tracked mapping or protection changes, including image registration, unmapping and explicit page-state invalidation, but not when a registry mutation scope only inspects mappings. Previously it reused the broader allocation-registry generation, which advances at the end of every mutation scope and therefore changed on read-only operations. Firmware 13.60 `libkernel_sys.sprx` routes this export to syscall 624. The console timestamp arithmetic is not hardware-validated, and host mappings made outside the tracker can still be missed.
