# libkernel_sys

## remove `t+rTx5NW+qU`

## add Functional

- [`__sys_socketclose`](../../../core/libs/prx/libkernel_sys/AdditionalExports.cpp) (libkernel_sys) closes a descriptor via `close_nid_postfix`, matching console `sys_socketclose`. Sets caller guest errno on failure. Unverified on a retail console. Closes when: compared with a retail PS5.

# libkernel_web

## add Functional

- [`__sys_socketclose`](../../../core/libs/prx/libkernel_web/Export.cpp) (libkernel_web) forwards to `__sys_socketclose` in `libkernel_sys`. Unverified on a retail console. Closes when: compared with the console.
