# shader

## add Functional

- The adjust values an `_a` sample ORs into bits 12 to 29 of dword 3 of its S# are dropped, for an [S# read from a table record](../../../core/shader/recompiler/Optimization/src/ResourceTracker.cpp) as for one read from the SRT: the record is the read under that OR. What those bits change on the console is not measured and nothing emulates it. PPSA38500's mesh shaders OR in a `s_quadmask_b64` of a compare shifted left by 12. A table S# handle that an `_a` sample planned is also used, without the OR, by any plain sample of the same handle.
