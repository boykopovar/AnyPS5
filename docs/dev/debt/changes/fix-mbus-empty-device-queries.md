# libSceMbus

## change [sceMbusGetDeviceInfoByCondition_]

- [sceMbusGetDeviceInfoByCondition_](../../../core/libs/prx/libSceMbus/PendingExports.cpp): its first parameter points to a 256-byte query condition; the second is a 32-bit scalar. Two optional 32-bit outputs and the sized 256-byte record array are confirmed, while condition fields and output meanings remain unresolved. No device is listed, so both outputs are written as 0 and the record array is left untouched. A null or unreadable condition, a record size other than 256, a negative capacity, a positive capacity with a null record pointer, or an unwritable output returns `0x809B0080`. The null-condition and unwritable-output results are a host-safety fallback; console behaviour for those pointers has not been measured.

## change [sceMbusGetDeviceInfoByConditionForDeviceService]

- [sceMbusGetDeviceInfoByConditionForDeviceService](../../../core/libs/prx/libSceMbus/PendingExports.cpp): assembly and its DeviceService caller confirm five register arguments followed by a 256-byte condition passed by value on the stack. The condition keeps opaque storage because its service-side fields remain unresolved; byte 228 contains the 16-bit device class. Output records occupy 112 bytes. The two optional outputs are 32-bit values whose meanings remain unresolved. No device is listed, so both outputs are written as 0 and the record array is left untouched. A negative capacity, a positive capacity with a null record pointer, or an unwritable output returns `0x809B0080`. The unwritable-output result is a host-safety fallback; console behaviour for that pointer has not been measured.
