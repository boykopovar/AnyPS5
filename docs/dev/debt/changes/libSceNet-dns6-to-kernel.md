# libSceNet

## change [sceNetSetDnsInfoToKernel, sceNetSetDns6InfoToKernel]

- [sceNetSetDnsInfoToKernel, sceNetSetDns6InfoToKernel](../../../core/libs/prx/libSceNet/Export.cpp) (libSceNet) - both forward to `sceNetSetDnsInfo` and `sceNetSetDns6Info`, updating process-local DNS configuration overrides; this forwarding is inferred from the IPv4 sibling and unmeasured on console hardware, and the kernel DNS state is not modelled.
