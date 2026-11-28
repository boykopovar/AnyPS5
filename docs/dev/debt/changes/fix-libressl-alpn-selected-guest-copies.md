# libSceLibreSsl

## add Functional

- [SSL_get0_alpn_selected](../../../core/libs/prx/libSceLibreSsl/src/Ssl.cpp) (libSceLibreSsl) checks that both outputs are writable and then writes the selected protocol and its length with `GuestCopyOut_nid_no_patch`, so an output that can't be written throws `std::invalid_argument`, as a null one does, and neither output is changed. This is a host-safety fallback; the console's behaviour for inaccessible outputs is not measured.
