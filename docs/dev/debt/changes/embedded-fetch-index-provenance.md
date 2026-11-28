# shader

## add Functional

- [Embedded vertex-fetch lifting](../../../core/shader/recompiler/Translation/src/EmbeddedVertexFetch.cpp) has no copy/phi or lane-sensitive provenance proof. Its straight-line proof follows entry vertex/instance indices, at most one admitted entry-user-data offset add per dimension and the unmodified index select. Prefix control flow, relative writes, partial EXEC and unmodelled status writes use ordinary translation. Valid forms outside this proof may lose fixed-function fetches or GPU indirect eligibility; performance and gfx1013 results are unmeasured. Closes when: broader reaching-definition proofs have positive and fallback regression coverage.
- [Buffer status results](../../../core/shader/recompiler/RdnaDecoder/src/RdnaMemoryOpDecoder.cpp) are not represented as extra destinations, and buffer translation does not write the trailing status VGPR. The embedded-fetch analyzer rejects prefixes with the MUBUF/MTBUF status bit set; this restriction does not implement runtime status writes. Supported encodings and PS5 status behavior remain unverified. Closes when: status encodings, decoded write extents and runtime results have hardware validation.
