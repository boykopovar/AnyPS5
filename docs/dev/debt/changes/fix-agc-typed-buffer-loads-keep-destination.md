# shader

## change [Buffer descriptor type]

- [Buffer descriptor type](../../../core/shader/recompiler/Optimization/src/ResourceMaterializer.cpp) - PS5 multi-dword load behavior is unmeasured. On CFI-1115B firmware 12.20, TYPE 2/3 single-dword vector loads keep the destination, stores write nothing, TYPE 0 FORMAT 0 loads return 0, and TYPE 1 accesses fault. RX 6650 XT (gfx1032) raw loads of 1-4 dwords preserve every destination through TYPE 2/3 in wave32/64 with mapped valid-format descriptors; TYPE 0 reads the buffer. Read-only raw multi-dword loads use that behavior. Such loads recompile to preserve their destinations with a null V#. Still refused for TYPE 2/3: D16, formatted multi-component loads, tbuffer, atomics, FORMAT above 77, GCN, run-time V#s, mixed read/write multi-dword buffers and multi-dword/multi-component stores. Closes when: remaining paths and multi-dword loads are measured on PS5.
