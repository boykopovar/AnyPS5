### What

### Tested
<!-- OS, title or homebrew, what was checked. "Not tested" is a valid answer. A performance change: the summary lines of main and of this PR from the same machine and scene, with the traces attached, captured as the Performance report issue form describes. -->

### Checklist
<!-- Rules behind each item: https://github.com/boykopovar/AnyPS5/blob/main/CONTRIBUTING.md -->
- [ ] Based on current `main`; no other open PR implements the same functions
- [ ] One topic per PR; follow-ups go in a new PR
- [ ] No comments in code except [technical debt](https://github.com/boykopovar/AnyPS5/blob/main/docs/dev/CONVENTIONS.md)
- [ ] Unimplemented paths throw (`NotImplemented_nid_no_patch`); silent stubs are listed under Silent stubs in their library's [technical debt](https://github.com/boykopovar/AnyPS5/tree/main/docs/dev/debt) file
- [ ] No notes, investigation `.md` files or images added to the repository (attach them to this PR)
- [ ] New third-party code is a submodule built from source
- [ ] Generic behaviour, not specific to one title
- [ ] Depends on: <!-- #PR, or none -->
- [ ] Fixes: <!-- issue it closes as Fixes #N, or none -->
- [ ] Unblocks: <!-- title ID and Game bring-up discussion, e.g. PPSA01325 #3232, or none -->
- [ ] GPUs tested: <!-- for AGC, shader recompiler or VideoOut changes: vendor, model, driver and OS of each GPU the GPU tests ran on; otherwise n/a -->
- [ ] AI-assisted: <!-- yes / no -->
