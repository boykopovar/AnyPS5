# libkernel_sys

## change [pthread_get_specificarray_np]

- [pthread_get_specificarray_np](../../../core/libs/prx/libkernel_sys/Export.Discovery.cpp) (libkernel_sys) - returns EINVAL (22) for non-positive total, key below 1 or key range exceeding 512, EFAULT (14) for null or unwritable output, and otherwise copies thread-specific data atomically via a checked guest copy while preserving errno. These argument checks, fault handling and the 512-key bound are an unverified host model; console runtime behaviour was not independently measured.
