# libkernel_sys

## change [sceKernelCreateUuidV1]

- [sceKernelCreateUuidV1](../../../core/libs/prx/libkernel_sys/Uuid.cpp) (libkernel_sys) - the prototype `int sceKernelCreateUuidV1(std::uint8_t* uuid)` and null check returning `SCE_KERNEL_ERROR_EINVAL` are modeled by analogy with libkernel's `sceKernelCreateUuidV4`; returns `SCE_KERNEL_ERROR_EFAULT` for unwritable memory as a host-safety fallback; a payload process on firmware 11.60 got version 1 at byte 6, variant `10` at byte 8 and the same node bytes in two calls; the rest of the byte order, a null argument and a title are unmeasured. Encodes RFC 4122 big-endian version 1 UUID bytes sharing uuidgen's node and clock state

## change [sceKernelCreateUuidV7]

- [sceKernelCreateUuidV7](../../../core/libs/prx/libkernel_sys/Uuid.cpp) (libkernel_sys) - the prototype `int sceKernelCreateUuidV7(std::uint8_t* uuid)`, the `SCE_KERNEL_ERROR_EINVAL` null check and the `SCE_KERNEL_ERROR_EFAULT` host-safety fallback copy `sceKernelCreateUuidV1`; a payload process on firmware 11.60 measured version 7 at byte 6, variant `10` at byte 8, the first 48 bits as big-endian milliseconds equal to `CLOCK_REALTIME`, five calls 5 ms apart non-decreasing with the other bytes different each time, and `EINVAL` for null; unwritable memory and a title are unmeasured. Writes the [RFC 9562](https://www.rfc-editor.org/rfc/rfc9562#section-5.7) big-endian layout: 48-bit Unix milliseconds from `CLOCK_REALTIME`, version 7, variant `10` and 74 random bits, without the optional monotonic counter, so two UUIDs of one millisecond need not sort in call order. Closes when: compared with the console.
