# libSceNetCtl technical debt

### Unknown function info

- [sceNetCtlGetInfo](../../../core/libs/prx/libSceNetCtl/Export.cpp) (libSceNetCtl) - answers from the host interface that holds an IPv4 address, preferring the one that carries the default route: its address, netmask, gateway, MTU and first two IPv4 DNS servers. The device is reported as wired, the link as connected, the IP configuration as DHCP and the HTTP proxy as off, whatever the host uses, and the Ethernet address is the one `sceNetGetMacAddress` reports. A gateway or DNS server the interface does not have is reported as `0.0.0.0`; what a console reports then is unknown. The DHCP host name (12), the PPPoE user name (13), the proxy server (20) and the Wi-Fi BSSID and SSID (5, 6) are empty strings, as that configuration sets none of them; the remaining Wi-Fi codes (7-10) and the HTTP proxy port (21) report 0
