# libc

## change [__std_free_crt](../../../core/libs/prx/libc/src/FileAndHeap.cpp)

- [__std_free_crt](../../../core/libs/prx/libc/src/FileAndHeap.cpp) (libc, NID `y-r+TFZgeUU`) is unverified on a console. Its `void*` argument and forwarding to `free` follow [Microsoft STL](https://github.com/microsoft/STL/blob/b6f01e438b68ee497f9c08313e49d12bb2f0d8e0/stl/src/tzdb.cpp#L651-L653), where it releases what `__std_calloc_crt` allocates; the export name and NID are listed in [ps5rs](https://github.com/claimore22/ps5rs/blob/5f252a658ff470bcb2b6cd15205fd8ca64ae4477/data/stubs/by_library/libc.txt). The `libc.prx` bundled with PPSA04264 v01.011.001 (J03093718) and PPSA26344 v02.002.000 (J03547016), read from disassembly, calls its own `free` export and returns, and that export calls through a pointer at 0x194ef8 in both builds. Where the pointer is filled, and so whether it reaches the application allocator, is not read.
