# libSceAgcDriver

## add Functional

- [Mixed DCC key clear coverage](../../../core/libs/prx/libSceAgcDriver/Graphics/src/DccMetadata.cpp) (libSceAgcDriver) - a slice whose keys mix codes is read as stored because per-block GF(2) key order inside a slice is not modeled, and a multi-mip surface fills only when every slice holds the same clear code. Pipe-aligned 8-byte single-mip layers still use `guestBytes / 256` in `DccKeyCount` rather than 512x256 4096-key blocks; color targets with mixed keys, `ClearRegister`, `ClearSingle`, multiple clear codes, and non-32-bit-repeatable 64/128-bit clear values still read as stored. Per-layer 4-byte R64KBX extents and pipe-aligned mip-chain offsets match console measurements on firmware 13.00 (#21238); non-zero pipe-bank XOR, non-pipe-aligned arrays, and 3D surfaces remain unmeasured. Closes when: per-block GF(2) key order and 8-byte extents are modeled. Reached by PPSA03974 "Set V's appearance".
