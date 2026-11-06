# Thread sampling output

`APS5_SAMPLE_THREADS=<path>` enables the process sampler. It samples process threads every 2 ms and writes the legacy cumulative format every 20 seconds. `APS5_SAMPLE_THREADS_INTERVAL=1` changes only this process sampler when `APS5_SAMPLE_THREADS` is also set. Other values, or setting the interval variable alone, leave process sampler output in legacy mode.

`APS5_SAMPLE_WORKER=<path>` enables a separate sampler for the calling worker thread. It retains its legacy cumulative output and overwrite behavior, including when process interval mode is enabled. The interval option does not configure this worker sampler.

In process interval mode, each record is appended to the configured path. A record header includes steady-clock elapsed start and end milliseconds, cumulative and interval round counts, process ID, run key, and sequence. Each thread block includes its DWORD thread ID, creation FILETIME key, optional escaped name, sample counts, and all frames with a nonzero self or inclusive count. The creation key distinguishes different lifetimes that reuse a thread ID. Thread names are optional: lookup is dynamically resolved, query failures are ignored, returned storage is freed with `LocalFree`, UTF-8 bytes are escaped for output, and names are limited to 128 bytes (with an ellipsis when truncated). This is diagnostic metadata, not a stable OS-wide thread identity.

The `# complete` line closes a record. Consumers should ignore records without that line and, for duplicate process/run/sequence keys after a failed attempt, keep the latest complete record. A retry retains its sequence; a successful flush advances it. Counts and retired-thread intervals are cleared only after writes, flush, stream-error checks, and close all report success. An open or write failure therefore retains pending data for retry. Time bounds on a retry continue from the prior successful flush.

`ProcessSampler::writeInterval(FILE*)` accepts an optional stream for the accounting harness. A supplied stream transfers ownership to the function: it is flushed and closed on every path after acceptance, including reported write or flush failures. The production call omits the argument and opens the configured path in append mode.

The output is best-effort diagnostic sampling. It does not guarantee persistence after process termination or storage failure, and a successful C runtime close does not establish durable media storage. Thread names may be unavailable or truncated; creation-time lookup failure causes that handle to be skipped in interval mode.
