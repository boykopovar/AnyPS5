# libkernel

## change [setrlimit](../../../core/libs/prx/libkernel/System/src/Process.cpp)

- [setrlimit](../../../core/libs/prx/libkernel/System/src/Process.cpp) (libkernel) validates the resource and input in FreeBSD order, maps negative limits to infinity and lowers a soft limit above the hard limit. `RLIMIT_NOFILE` stores the normalized guest limits but clamps both to 12555, the measured per-process ceiling; descriptor allocation still follows the host and does not enforce a lowered guest soft limit. A PS5 payload process (firmware 12.70) started at {13952, 13952}, changed {1000, 13952} to {1000, 12555}, and changed {13952, 13952} back to {12555, 12555}. `RLIMIT_NPROC` likewise clamps to 1..1443. `RLIMIT_CORE` stores only a changed soft limit when the hard limit is unchanged. Raising only the hard limit of `RLIMIT_DATA` or `RLIMIT_STACK` succeeds without changing either value; changing other unmodelled limits still throws. Title-process limits are unmeasured.
