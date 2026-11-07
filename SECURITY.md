# Security Policy

## Supported Versions

Security fixes are applied to the latest released version and the `main` branch. Older versions are not actively maintained, so please reproduce any issue against the latest release before reporting it.

| Version         | Supported          |
| --------------- | ------------------ |
| Latest release  | :white_check_mark: |
| `main` branch   | :white_check_mark: |
| Older releases  | :x:                |

## Reporting a Vulnerability

Please do not report security vulnerabilities through public GitHub issues, discussions, or pull requests.

Instead, report them privately using one of these methods:

1. **GitHub Private Vulnerability Reporting (preferred):** Go to the [Security tab](https://github.com/boykopovar/AnyPS5/security) of this repository, click **Report a vulnerability**, and submit the details there.
2. **Email:** If you cannot use GitHub, contact the maintainer directly at the address listed on the repository owner's profile.

### What to Include

To help us triage and reproduce the issue quickly, please include as much of the following as you can:

- A description of the vulnerability and its potential impact
- The affected component, file, or version (commit hash or release tag)
- Step-by-step instructions to reproduce the issue, including any proof-of-concept code
- The environment you tested in (OS, device, firmware, or dependency versions where relevant)
- Any suggested mitigation or fix, if you have one

## Response Timeline

We aim to:

- **Acknowledge** your report within 72 hours
- **Confirm or dismiss** the vulnerability within 7 days, with an explanation either way
- **Provide a fix or mitigation plan** within 30 days for confirmed issues, depending on complexity
- **Publish an advisory** once a fix is available, crediting the reporter unless they prefer to remain anonymous

These timelines are targets rather than guarantees. If a fix will take longer, we will keep you informed.

## Coordinated Disclosure

We ask that you give us a reasonable opportunity to address the issue before any public disclosure. We will coordinate the disclosure timing with you. If a report is not acknowledged within a reasonable period, you may escalate it through GitHub's Security Advisories or the relevant CVE process.

## Scope

In scope:

- Vulnerabilities in code maintained in this repository
- Issues that could lead to remote code execution, privilege escalation, data exposure, or authentication bypass

Out of scope:

- Vulnerabilities in third-party dependencies (please report these upstream, and let us know so we can update)
- Issues requiring physical access to a device the user already controls
- Social engineering, phishing, or denial-of-service via unrealistic resource exhaustion
- Missing security headers or best-practice findings without a demonstrable impact
- Findings from automated scanners submitted without validation

## Safe Harbor

We will not pursue legal action against researchers who:

- Make a good-faith effort to avoid privacy violations, data destruction, and service disruption
- Only interact with accounts and systems they own or have explicit permission to test
- Report findings promptly and keep details confidential until a fix is released

## Recognition

We are grateful to everyone who helps keep this project secure. With your permission, we will credit reporters in the release notes and security advisory.
