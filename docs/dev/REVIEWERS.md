# Pull Request Review Guidelines

This document provides guidance for reviewing pull requests in the AnyPS5 project. It covers what to look for, common issues, and best practices based on experience reviewing dozens of PRs.

## General Principles

- **Be constructive**: Focus on improving the code, not criticizing the author
- **Check the checklist**: Every PR should have a completed checklist in its description
- **Verify testing**: Ensure tests are comprehensive and actually run
- **Look for overlap**: Check if other open PRs touch the same files or functionality

## What to Look For

### Code Quality
- Follows project conventions (see [CONVENTIONS.md](CONVENTIONS.md))
- No unnecessary comments in code (technical debt items go in TechnicalDebt.md)
- Proper error handling and validation
- Clean, readable implementation

### Testing
- New functionality has corresponding tests
- Tests cover edge cases and error conditions
- Test names are descriptive
- Tests actually verify behavior (not just compilation)

### Documentation
- TechnicalDebt.md updated if new limitations introduced
- USAGE.md updated for user-facing changes
- BUILD.md updated for build system changes

### Compatibility
- Works on both Linux and Windows (unless explicitly platform-specific)
- No breaking changes to existing APIs without good reason
- Backward compatibility considered

## Common Issues to Watch For

### Shader Recompiler PRs
- Verify instruction encodings against AMD documentation or LLVM sources
- Check that new instructions are properly registered in the progress counter
- Ensure Vulkan execution tests validate actual behavior, not just compilation
- Look for proper handling of edge cases (unaligned addresses, out-of-range values)

### Relinker PRs
- Verify ELF parsing handles malformed input gracefully
- Check for memory safety issues (buffer overflows, use-after-free)
- Ensure both Linux and Windows paths are tested
- Look for proper error messages that help debugging

### Library Implementation PRs
- Verify function signatures match console behavior or documented references
- Check that unimplemented paths throw rather than silently failing
- Ensure proper thread safety where needed
- Look for appropriate use of existing infrastructure (don't reinvent the wheel)

## Review Process

1. **Read the description**: Understand what the PR is trying to accomplish
2. **Check the diff**: Look at the actual code changes
3. **Run tests locally** (if possible): Verify they pass
4. **Provide feedback**: Be specific about issues and suggestions
5. **Approve or request changes**: Don't approve until all concerns are addressed

## AI-Assisted Development

Many PRs in this project use AI assistance (Claude Code, OpenAI Codex). This is fine, but reviewers should:
- Verify the AI didn't make assumptions that differ from console behavior
- Check that tests actually validate the implementation
- Ensure documentation accurately reflects what was implemented vs. assumed

## When to Approve

Approve a PR when:
- It addresses a real issue or adds valuable functionality
- Code quality is good and follows conventions
- Tests are comprehensive and pass
- Documentation is updated as needed
- No significant overlap with other open PRs (or overlap is noted)

## When to Request Changes

Request changes when:
- Tests are missing or incomplete
- Code doesn't follow project conventions
- Error handling is inadequate
- Documentation is outdated or incorrect
- The implementation makes unverified assumptions about console behavior