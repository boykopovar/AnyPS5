# Testing

Tests use the small framework in [core/testing](../../core/testing/include/Testing/Test.hpp). It needs only the C++20 standard library, so the [relinker-only build](BUILD.md#relinker-only) can use it.

## Writing a test

A test file registers named cases at namespace scope. The framework supplies `main`.

```cpp
#include <Testing/Test.hpp>

namespace {

using Testing::Case;
using Testing::RequireEqual;

const Case alignedValue{"AlignUp_AlignedValue_IsUnchanged", [] {
    RequireEqual(Io::AlignUp(32u, 16u), 32u, "aligned value");
}};

} // namespace
```

- Name each case `Unit_Scenario_ExpectedResult`. The name is the documentation, since code comments are not allowed.
- Check one behaviour per case: arrange, act, then assert.
- Cases share no mutable state. Build fixtures in each case, and clean up in destructors so a failing case still cleans up. An expensive fixture that doesn't change, such as the Vulkan device from `SharedVulkanTestDevice()`, may be shared.
- Don't add helpers like `Require` or `Check` to a test file. Use the framework's assertions:

| Assertion | Use |
|---|---|
| `Require(condition, message)` | A condition must hold. |
| `RequireEqual(actual, expected, message)` | Two values must be equal. The failure prints both. |
| `RequireThrows<TError>(operation, message)` | The operation must throw `TError`. Returns the error. |
| `RequireThrowsWithMessage<TError>(operation, text, message)` | Same, and `what()` must equal `text`. |
| `Fail(message)` | Fails the case. |
| `Skip(reason)` | The environment can't run the case, for example there is no Vulkan device. Prints `skipped, <reason>`. |
| `TemporaryDirectory` | A temporary directory, removed in its destructor. |
| `RequireArgument(index, name)` | A command-line argument, such as the relinker path given to end-to-end tests. |

A failing assertion stops only its own case. The other cases still run, and the summary line lists passed, failed and skipped cases. If every case that ran was skipped, the executable exits with 77 and ctest reports it as skipped.

Run one executable's cases selectively:

```
tests/nid_unit_tests --list
tests/nid_unit_tests --case PatchNids_Exports_ReplacesNamesWithNids
```

A test that has to start itself as a child process keeps its own `main`, ends it with `return Testing::Run(argc, argv);`, and is registered with `CUSTOM_MAIN`.

## Layout and labels

Tests live next to the code they test, in a folder named after their label:

| Label | Folder | What it covers |
|---|---|---|
| `unit` | `tests/unit` | Logic in the same process only. No files, sockets, sleeps, real clocks, child processes or GPU. Finishes in well under 30 s. |
| `integration` | `tests/integration` | Real operating system resources, or the system libraries used in the same process. |
| `gpu` | `tests/gpu`, `libSceAgcDriver/tests/execution` | Needs a Vulkan device. |
| `e2e` | `tests/e2e` | Runs the built relinker or patcher on generated files. |
| `tooling` | `tools/tests` | Repository scripts. |

```
ctest --test-dir build -L unit
ctest --test-dir build -L "unit|integration"
ctest --test-dir build -LE gpu
```

## Registration

Register C++ tests with `anyps5_add_test`, and tests of the system libraries with `anyps5_add_library_test`, which also adds the library include path, the Windows unwind fix-up and the `PATH` entry for the library. Register Python tests with `anyps5_add_python_test`. Every test needs a label. Unit tests get a 30 s timeout; other tests use the one given to ctest (CI passes `--timeout 120`) unless they set `TIMEOUT`.

```cmake
anyps5_add_test(buffer_bounds_tests
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/io/tests/unit/BufferBoundsTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/io/src/BufferUtils.cpp
    INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/io/include
)
```

The ctest name is the target name without `_tests`.
