# libSceMbus

## add Unknown function info

- [sceMbusGetDeviceInfo_, sceMbusGetDeviceInfoByBusId_ and sceMbusGetDeviceDescription](../../../core/libs/prx/libSceMbus/PendingExports.cpp) (libSceMbus) - no device service is connected, so a well-formed call returns `0x809B00FF`, the same code `sceMbusAcquireControlList` returns, and does not write the record. The info calls require 256 bytes; a null record or any other size returns `0x809B0080`. Description has no size argument; a null record returns `0x809B0080`. Identifiers and the description selector are not interpreted. Record fields remain unresolved. The null-record result is a host-safety fallback; console behaviour for a null record has not been measured.
