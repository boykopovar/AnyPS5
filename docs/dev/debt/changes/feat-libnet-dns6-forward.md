# libSceNet

## change [sceNetSetDnsInfoToKernel, sceNetSetDns6InfoToKernel]

- [sceNetSetDnsInfoToKernel, sceNetSetDns6InfoToKernel](../../../core/libs/prx/libSceNet/Export.cpp) (libSceNet) - `sceNetSetDnsInfoToKernel` forwards to `sceNetSetDnsInfo`, updating process-local DNS configuration overrides; `sceNetSetDns6InfoToKernel` now forwards to `sceNetSetDns6Info` the same way, updating process-local IPv6 DNS overrides. Both forwardings are inferred from the pattern already in this file and are unmeasured on console hardware.
