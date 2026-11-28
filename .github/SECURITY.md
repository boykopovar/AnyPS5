# Security policy

## Supported versions

Fixes go to `main` and ship in the next release. Only the latest release and `main` are supported.

## Reporting a vulnerability

Report it privately through GitHub: the Security tab of this repository, then "Report a vulnerability" ([private vulnerability reporting](https://docs.github.com/en/code-security/security-advisories/guidance-on-reporting-and-writing-information-about-vulnerabilities/privately-reporting-a-security-vulnerability)). Do not open a public issue, discussion or pull request for it.

Give the AnyPS5 commit or release, the host and its OS, what an attacker controls and the steps to reproduce. Attach no game dumps, firmware, keys or other copyrighted files: build the input from homebrew, or describe it.

## Scope

A converted title is an ordinary program that runs with your account's permissions and none of the console's sandbox, as [USAGE](../docs/user/USAGE.md#input-and-conversion) states. What a title does once it runs is not a vulnerability in AnyPS5.

In scope:

- The relinker reading an executable or module: memory corruption, or files written outside the outputs it was asked for.
- Host libraries that reach the network for a title and skip a check the title asked for, such as TLS certificate verification.
- The build and the workflows: a dependency fetched without verification, or a pull request that can run code with the repository's token or secrets.

Out of scope:

- Anything a converted title can do with your account's permissions.
- The fixed load addresses of converted executables and their missing address space layout randomization: the default `0x400000` and `--windows-fixed-base`, `--linux-fixed-base` and `--macos-fixed-base`.
- Crashes caused by guest code.
- Bugs that need a modified AnyPS5 build.
