# libkernel

## change [rtprio_thread

- [rtprio_thread](../../../core/libs/prx/libkernel/Pthread/Posix/Thread.cpp) (libkernel) - a set the firmware 13.60 kernel lets a title through throws, as applying a priority is not modelled: real-time (2, 10) 256 to 767 (a per-process check at kernel-13.60 @ 0xffffffff9e890811 may still refuse it), idle 960, and type 11 615 to 895 (`rtp_to_pri` @ 0xffffffff9e890790). Every other set returns `EINVAL`, taking a title to lack the system credentials that open type 3 and real-time 896 to 959 (@ 0xffffffff9e8906b6, 0xffffffff9e8907ed); no path returns `EPERM`. Function 2 returns `ESRCH` as a PS5 payload measured, though 13.60 gives an uncredentialed caller `EINVAL` (@ 0xffffffff9e890596); function 5 `EINVAL`. `RTP_LOOKUP` reports the policy's type (10 or 2) and priority, 3 with 0 without real-time attributes (unmeasured). A missing thread gives `ESRCH` before a null `rtprio` gives `EFAULT`

## add Functional

- [pthread_setconcurrency, pthread_getconcurrency](../../../core/libs/prx/libkernel/Pthread/Posix/Thread.cpp) (libkernel) - `pthread_setconcurrency` stores 0 or more and returns `EINVAL` (not errno) below 0, as libthr does; `pthread_getconcurrency` returns it, a hint. Morrowind (`mshivam019/morrowind-ps5`) imports them and `rtprio_thread`
