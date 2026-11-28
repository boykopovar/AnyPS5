# libc

## add Unknown function info

- [uuid_compare, uuid_create_nil, uuid_equal, uuid_from_string, uuid_hash, uuid_is_nil, uuid_to_string and the uuid_enc/uuid_dec byte-order pairs](../../../core/libs/prx/libc/src/Uuid.cpp) (libc) - follow FreeBSD 11.4's [`lib/libc/uuid`](https://github.com/freebsd/freebsd-src/tree/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/uuid) at the pinned commit: comparison, parsing and formatting use the DCE layout, and `uuid_to_string` returns a string from the guest heap that the caller frees. `uuid_create` throws through `NotImplemented_nid_no_patch`: FreeBSD fills the node address from the `uuidgen(2)` system call, and the console's equivalent and node address are not known.
