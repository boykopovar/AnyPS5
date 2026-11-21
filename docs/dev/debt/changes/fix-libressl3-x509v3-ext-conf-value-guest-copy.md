# libSceLibreSsl3

## add Functional

- [X509V3_EXT_conf_nid](../../../core/libs/prx/libSceLibreSsl3/src/X509V3ExtConfNid.cpp) (libSceLibreSsl3) copies its value string in with `CopyGuestString_nid_no_patch` (64 KiB, then up to 1 MiB) before parsing it, so an unreadable value returns null, as a null one does. This is a host-safety fallback: the console's result for an inaccessible value is not measured. Closes when: compared with the console.
