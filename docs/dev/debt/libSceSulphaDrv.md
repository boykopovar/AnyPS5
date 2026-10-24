# libSceSulphaDrv technical debt

### Inventory exports declared elsewhere

sceSulphaGetClientInfo, sceSulphaGetConnectedClientCount, sceSulphaGetDefaultConfig, sceSulphaGetNeededMemory, sceSulphaInit, sceSulphaMessage, sceSulphaSetBookmark and sceSulphaShutdown are the complete PS5 3.20 export table of this module per the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceSulphaDrv.c), but all eight are currently declared by #7416 in libSceSulpha, so this module keeps its `APS5_DUMMY_FUN` skeleton to respect the duplicate-export rule. Declare them here (with no parameters, matching the `1b5004d77` unknown-prototype shape) if that PR's module attribution is rejected.
