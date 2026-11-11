# Performance regression bisector

`tools/performance_bisect.py` searches the first-parent history between a known-good and a known-bad revision for the first revision whose repeated title-run median FPS crosses a configured slowdown threshold. It uses the existing `tools/title_run.py` workflow and writes a machine-readable JSON report.

## What the result means

The bisector is useful only when the supplied endpoints are comparable and the performance change is monotonic across the searched history. It checks the endpoint regression before searching, keeps the title, game content version, operating system, CPU, GPU, and driver identity fixed, and stops if repeated samples exceed the configured spread. The result identifies the first measured commit crossing the threshold; it does not prove that a single change in that commit caused the regression.

The search follows first-parent commits and assumes one monotonic regression between the endpoints. It may miss a regression that later reverted, multiple performance changes, or a regression that falls below the threshold between sampled commits. Review the reported bracket and commits manually before attributing a cause.

The tool needs an extracted game package, a working AnyPS5 build recipe, and a machine able to run the title. The repository tests use synthetic measurements; they do not benchmark a real title or validate a local GPU/driver setup.
