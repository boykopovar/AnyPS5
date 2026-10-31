# libwebrtc technical debt

### Unknown function info

- [412 exports (60 without a known name, bound by NID)](../../../core/libs/prx/libwebrtc/Export.cpp) (libwebrtc) - every function export of `libwebrtc.sprx` from the firmware 13.60 export table, names from aerolib.csv only where `tools/nid_names.py` `compute_nid` reproduces the NID. Signatures, parameter layouts and behaviour are unknown; every export throws through `NotImplemented_nid_no_patch`
- [31 data exports](../../../core/libs/prx/libwebrtc/Export.cpp) (libwebrtc) - the module also exports 31 data objects (+qcl2IzxTzg, -ix-knPYqWA, 00mefEErs+Y, 5FM0YOa3NWc, 67nsSkXmJ7E, 7I2xDnqfDV4, 8Dzy4JayJSU, CVCDyAzGrhI, HpbtuVdvO-w, PGIpx6NnFLE, Q+xnDp4KI-s, QXDv9OpZNvM, aOOjLmup+u4, g5USRJNh3bU, hvpHGXjlpbk, iU6+qODNSjU, jpkYoOv34AM, lHLJlq3NEqE, lQJvoyxHov4, nUaORqH85DQ, o6LS+08kOUs, oh8srl3AcNE, oslHzys1vV0, sp5NqvqDdHw, t66Gw3OIpSo, tr8vseK8-ro, uSis0q+bv6k, uTz5pEKL+hM, uxkudCJIYJA, whHm70jlRXw, yW0sV3sCdPA); they are not defined, so imports of them stay unresolved until their layout is known
