---
name: find-task
description: Find one contribution task in this repository that is doable and that no open upstream pull request already covers. Runs the whole search in an isolated subagent and returns only a short brief. Use when the user asks what to work on next or wants a task to contribute.
argument-hint: "[focus or constraint, e.g. libkernel, shaders, a title you can run]"
context: fork
agent: general-purpose
---

Pick one task in this repository that the user can implement and get merged upstream, and hand back a short brief. The caller has deliberately seen nothing of the search and should not need to: everything it needs to start coding goes in the brief, everything else stays with you.

Focus or constraint from the user, if any: $ARGUMENTS

## Situation

- Upstream is very active (dozens of open pull requests, some authors on long streaks in one area), so the main risk is duplicating or conflicting with someone's open work. The maintainers ask contributors to check this before starting.
- The checkout is usually a fork and may be behind upstream. Judge everything against `refs/pr/base` (upstream `main`, fetched by the script below), not the working tree: `git show refs/pr/base:<path>`, `git grep <pattern> refs/pr/base -- <dir>`. Open pull request heads are at `refs/pr/<number>`.
- Unless the arguments name a title the user can run, assume they can run none. `docs/user/COMPATIBILITY.md` lists the titles known to work. A task whose result can be proven with a `ctest` unit test is worth much more than one that needs a title.
- Read `CONTRIBUTING.md` first. Two rules decide feasibility: a function either does exactly what it is supposed to or throws, and it implements the general behaviour, not what one title needs. So a task is only doable when the correct behaviour can be established, not guessed.

## Inventory

`.claude/skills/find-task/candidates.py` does the mechanical part: it resolves the upstream repository from the `upstream` or `origin` remote, lists the open pull requests, fetches them, and prints what is still a stub on upstream `main` minus what an open pull request already implements. Run it with Python 3 (`python3`, or `python` on Windows). It caches for 30 minutes; `--refresh` forces a new fetch. It reads the GitHub API with `GH_TOKEN` or the `gh` login when there is one, anonymously otherwise.

- no argument: stubs per system library and unimplemented shader instructions per encoding, with the free count and the pull requests touching each library, plus the host platform and whether the build toolchain is installed
- `--lib NAME`: free and claimed stubs of one library with `path:line`, and the pull requests touching it
- `--shaders [ENCODING]`: free and claimed shader instructions
- `--prs [FILTER]`: open pull requests with the areas they touch, filtered by area or text
- `--check NAME...`: every open pull request whose diff or title mentions the name
- `--touches PATH...`: open pull requests changing a file or directory, including the test file and `CMakeLists.txt` the task would edit
- `--issues`: open upstream issues

The script only knows stubs and shader opcodes, and a stub in its list is not necessarily implementable: read it before trusting it. Other sources are the "Functional" and "Unknown function info" sections of `docs/dev/TechnicalDebt.md` and the issues. For those, overlap has to be judged from `--prs`, `--check` and `--touches`.

Open pull requests do not show a series that was just merged. `git log --since=14.days --format="%an: %s" refs/pr/base -- <dir>` shows who is currently active in an area.

## What makes a task a good pick

- **Behaviour is knowable.** It follows from a public standard (POSIX, C/C++ library, AMD's public RDNA ISA description for shader instructions), from an already implemented sibling in the same library, or from a precise entry in the technical debt list. Reject anything whose signature or semantics are unknown, and libraries that front a console service or UI with no meaningful host equivalent.
- **Provable without a title.** There is an existing test file under `core/libs/tests`, the relinker tests or the shader tests whose pattern the new test can follow. Tests are registered per platform: one that the CMake files do not register on the host the script reports proves nothing on this machine, so check the guards. If only a title can prove it, say so and say which title; pick it only when nothing unit-testable is left in the requested area.
- **Free.** Not in the script's claimed list, `--check` finds no pull request mentioning it, and no open pull request title describes the same topic. Prefer files no open pull request touches. Stay away from an area where one author has many consecutive open or just-merged pull requests unless the pick is clearly disjoint from that series: the next one in the series will likely take it.
- **Small.** One topic, one pull request, reviewable in one sitting. A group of sibling functions sharing one mechanism counts as one topic.
- **Real.** Confirm on `refs/pr/base` that the stub or gap still exists and read the surrounding code and the nearest test, so the brief names real files and a real approach.

Before returning, try to break your own pick: run `--check` on every function or instruction it involves, and reread the relevant `CONTRIBUTING.md` rules against it. If it fails, pick again rather than returning it with caveats.

Do not write code, create branches or change the working tree. The only side effects allowed are the `refs/pr/*` refs and the cache the script writes.

## What to return

Return only this brief. No account of the search, no list of rejected candidates, no pull request inventory.

Write the content in the language of the arguments, or in the response language the session is configured for; in English when neither tells. The field labels, the title, identifiers and paths stay as they are below and in the repository.

```
## Task: <Conventional Commits title, as it would be used for the pull request>

**What**: 2 to 3 sentences on the behaviour to implement.
**Where**: files and lines on upstream main (path:line), plus the test file to extend or create.
**Behaviour source**: where the specification comes from (standard, sibling function, TechnicalDebt entry...).
**Verification**: the ctest test to write, or "needs the title <name>" with what to observe in it.
**Free**: why no open pull request covers it; the closest ones (#N) and why they do not overlap.
**Size**: estimate (files, order of magnitude in lines).
**Risks**: what remains uncertain, or "none identified".
**Base**: upstream main SHA used; how far the local checkout is behind; host and toolchain state.

Alternatives: at most two, one line each (title + why it ranks lower).
```

If nothing meets the bar in the requested area, say so in two lines and give the best pick outside it in the same format.
