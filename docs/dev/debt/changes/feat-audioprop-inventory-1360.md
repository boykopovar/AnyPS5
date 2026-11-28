# libSceAudioPropagation

## add Functional

- [Inventory query validation gates](../../../core/libs/prx/libSceAudioPropagation/InventoryExports.cpp) - Verified firmware 13.60 signatures and handle/null gates for `TrQMW7ARZXY` (materials query), `fOTrQvqzABM` (portals query), `mdH4mANi9GI` (rooms query, allowing null count-only queries), and `o0gGIp1vd6A` (sources query, allowing null count-only queries). Live-system enumeration and `MaterialQueryInfo`/`AudioPathQueryInfo`/`PortalQueryInfo` remain `NotImplemented_nid_no_patch` until object registries are exposed from `Export.cpp`.
