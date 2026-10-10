# Progress reporting

[`tools/progress.py`](../../tools/progress.py) generates `progress.json`, HTML, SVG maps and badges. The [Progress workflow](../../.github/workflows/progress.yml) publishes them after pushes to `main`. The [Progress report workflow](../../.github/workflows/progress-report.yml) compares the base and head trees of pull requests.

## Library counts

The scanner reads `APS5_VABI` function definitions under `core/libs/prx/<library>/`, excluding `tests` subdirectories and their descendants. Test callbacks do not contribute to library counts. A definition is pending when its body calls `NotImplemented_nid_no_patch`, directly or through a recognized local stub wrapper. Other definitions are counted as implemented. These counts describe declared functions, not every firmware export, verified ABI compatibility or playable games. Silent behavior and unverified assumptions remain in [Technical debt](TechnicalDebt.md).

A library with no scanned definitions can include a sibling library's source builder. The scanner recognizes literal `include(${CMAKE_CURRENT_SOURCE_DIR}/../<library>/<file>.cmake)` references, including quoted paths. It does not evaluate arbitrary CMake expressions. It also recognizes a top-level `.cpp` that includes another library's source with `#include "prx/<library>/<file>.cpp"`.

For these wrappers, the JSON group adds `shared_sources` with the source library's directory name. Its own counts and name lists remain empty. The HTML table links to the source library instead of showing `0/0`; the source library's map tooltip lists its wrappers. Global totals count the definitions at the source library once.

A library with no scanned definitions and no shared sources, such as one whose `Export.cpp` only has `APS5_DUMMY_FUN` because its functions are implemented in another library, is listed as having no exports instead of `0/0`. It is left out of the map.

## Comparisons

Comparisons identify functions by library and name. When a head group becomes a shared-source wrapper, its previous definitions are compared under the source library's identity. Removing a duplicate implementation is reported as sharing sources, without reporting the retained functions as removed.

Real removals and transitions back to stubs remain visible. Percentages retain the original snapshot totals, so eliminating duplicate accounting can change a percentage without changing available functionality. Older JSON snapshots without `shared_sources` are accepted.

## Verification

```sh
python3 tools/tests/test_progress.py
python3 tools/progress.py /tmp/anyps5-progress
```

The regressions check that adding test callbacks leaves counts and comparisons unchanged, including when a callback shares a pending function's name or a wrapper uses shared sources. They also create a source library and a copied wrapper, move the wrapper to a shared builder, check output labels and totals, then verify real regressions and removals are still reported. With `BUILD_TESTING=ON` and Python available, CTest registers them as `progress_report`.
