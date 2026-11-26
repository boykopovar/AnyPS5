# relinker

## change [wide-store decoder]

- `--to-rosetta` no longer rejects an unaligned 256-bit VMOVUPS, VMOVUPD or VMOVDQU store whose effective address uses 32-bit addressing through a base or index ([wide-store decoder](../../../core/relinker/codegen/src/x86/WideStoreOperands.cpp)): it materializes the effective address into a 64-bit register inside the masked-store spill scaffold with a truncating 32-bit-addressed LEA, and stores the two 128-bit halves from that register with `VEXTRACTF128`/`VEXTRACTI128` at offsets 0 and 16, so the upper half never adds to a displacement that could wrap around 4 GiB. EIP-relative and 32-bit absolute addressing keep the split, and a 32-bit absolute store whose upper-half address crosses the address boundary still throws. No affected title has been verified against Rosetta execution of this case.
