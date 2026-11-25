# Performance regression bisector

`tools/performance_bisect.py` searches the first-parent history between a known-good and a known-bad revision for the first revision whose repeated title-run median FPS crosses a configured slowdown threshold. It uses the existing `tools/title_run.py` workflow and writes a machine-readable JSON report.

## What the result means

The bisector is useful only when the supplied endpoints are comparable and the performance change is monotonic across the searched history. It checks the endpoint regression before searching, keeps the title, game content version, operating system, CPU, GPU, and driver identity fixed, and stops if repeated samples exceed the configured spread. The result identifies the first measured commit crossing the threshold; it does not prove that a single change in that commit caused the regression.

The search follows first-parent commits and assumes one monotonic regression between the endpoints. It may miss a regression that later reverted, multiple performance changes, or a regression that falls below the threshold between sampled commits. Review the reported bracket and commits manually before attributing a cause.

The tool needs an extracted game package, a working AnyPS5 build recipe, and a machine able to run the title. The repository tests use synthetic measurements; they do not benchmark a real title or validate a local GPU/driver setup.

## Prepare a recipe

Create a JSON recipe outside the checkout so it stays available while the bisector checks out older revisions. Build commands are argument arrays and run without a shell. They may use `{worktree}`, `{build_dir}`, `{result_dir}`, and `{commit}` placeholders. Use the same compiler, build type, and meaningful build options for every revision. The bisector leaves submodule setup to these commands. Build the patched runtime libraries with the relinker so the title runner can stage them.

```json
{
  "title_id": "PPSA00000",
  "games": ["/data/ps5/extracted-games"],
  "build_commands": [
    ["git", "submodule", "update", "--init", "--recursive"],
    ["cmake", "-S", "{worktree}", "-B", "{build_dir}", "-DCMAKE_BUILD_TYPE=Release"],
    ["cmake", "--build", "{build_dir}", "--target", "libs", "relinker", "--parallel", "4"]
  ],
  "seconds": 60,
  "repetitions": 5,
  "regression_pct": 5,
  "max_spread_pct": 3
}
```

Replace the example title ID, game directory, and build options with values for the local setup. Relative `games`, `titles`, and `content` paths are resolved from the recipe file directory. `games` must point to an existing directory containing an extracted game package. `build_commands` must configure and build the selected revision in `{build_dir}`; every revision starts with that directory cleared, so the bisector cannot accidentally reuse another revision's build products. It refuses to clear a build path that contains tracked files or resolves outside its temporary worktree.

Optional recipe fields are `build_dir` (default `build`), `titles` (default `tools/titles.json`), and `content` (directory for title data files referenced by the catalog). `seconds` defaults to 60 and must be at least 20; `repetitions` defaults to 3 and must be at least 3. `regression_pct` defaults to 5 and `max_spread_pct` to 3. Both percentages must be greater than 0 and less than 100.

Only use build commands from a recipe you trust: the tool does not invoke a shell, but it does execute the listed programs and arguments on your machine.

## Run a search

From the repository, run:

```sh
python3 tools/performance_bisect.py \
  --repo /path/to/AnyPS5 \
  --good <known-good-commit> \
  --bad <known-bad-commit> \
  --recipe /path/to/performance-recipe.json \
  --output /path/to/performance-bisect.json
```

The known-good revision must be an ancestor of the known-bad revision. The tool creates an isolated temporary Git worktree, builds and samples the endpoints, then uses binary search on first-parent commits. It builds each tested revision from a clean build directory and removes the temporary worktree and measurements when finished. Use `--temp-dir` to select an existing disk location with enough room for a worktree and a full build.

Each tested revision runs `title_run.py` repeatedly with one measured run per invocation. The median FPS is compared with the good endpoint. A search stops instead of guessing if the bad endpoint is not slow enough, a run fails to render, the machine/title/content identity changes, or sample spread exceeds `max_spread_pct`.

## Read the report

The command prints the first measured regression and its previous good commit. The optional JSON report records those commits, the supplied endpoints, the percentage threshold, the sample spread limit, the first-parent method, its monotonicity assumption, and every commit actually measured. `tested_commits` can be much smaller than `commits_between` because the search is logarithmic; it is not a claim that every commit was benchmarked.

The output path must not already exist. Reports are written atomically so a failed write does not leave a partial JSON file. Keep the report with the recipe and the machine setup details when sharing or comparing results.

## Resume an interrupted search

Add `--checkpoint /path/to/bisect-checkpoint.json` to the search command to save completed measurements. Use a new checkpoint path. A search replaces the checkpoint atomically after all samples for a revision pass the spread and identity checks. If an interruption leaves a revision partly measured, its measurements start over on resume.

Repeat the same command with `--resume` to reuse the checkpoint. The tool checks the endpoint commits, first-parent history, normalized recipe, title catalog and the contents of `performance_bisect.py` and `title_run.py`. It also builds the good endpoint and takes one fresh sample. The sample must match the saved machine, title and game content identity and keep the combined good-endpoint samples within `max_spread_pct`. The reported bracket uses the original good endpoint measurements. Keep the same game files, compiler, build settings and run conditions.

The checkpoint is a local file holding measured samples and configuration hashes. Use a separate path for `--output`, which is written after the search completes. The neighboring `.lock` file prevents concurrent searches from writing the same checkpoint. The lock is released when the process exits.
