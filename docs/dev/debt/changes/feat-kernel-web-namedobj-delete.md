# libkernel_web

## add Functional

- [`__sys_namedobj_delete`](../../../core/libs/prx/libkernel_web/Export.cpp) is unverified on a console. It assumes the prototype `int (std::uint32_t id, std::uint32_t type)` and the behavior of the libkernel_sys export of the same name and forwards to it, so ids made through either library are deleted from libkernel_sys's id table, and the errno results and limits in the [libkernel_sys debt](libkernel_sys.md) apply. Closes when: compared with the console.
