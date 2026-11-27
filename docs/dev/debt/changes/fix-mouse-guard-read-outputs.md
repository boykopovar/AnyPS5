# libSceMouse

## add Functional

- [sceMouseRead and sceMouseReadAbsolutePosition](../../../core/libs/prx/libSceMouse/src/mouse_impl.cpp) (libSceMouse) - reject unwritable or invalid guest output buffers with `MOUSE_ERROR_INVALID_ARG`, preserving `errno`, queued samples and cursor state instead of writing partial records; connected `sceMouseReadAbsolutePosition` records leave `padding` and `reserved` untouched. This is a host-safety choice not measured on a console.
