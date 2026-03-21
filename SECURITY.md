# Security policy

[中文](SECURITY.zh.md) | English

## Reporting a vulnerability

**Please do not** file public GitHub issues for **undisclosed security vulnerabilities** (they would notify everyone before a fix is ready).

### Preferred: GitHub private reporting

If the repository has **[Private vulnerability reporting](https://docs.github.com/en/code-security/security-advisories/guidance-on-reporting-and-writing-information-about-vulnerabilities/privately-reporting-a-security-vulnerability)** enabled, use **Security → Report a vulnerability** on the GitHub repo page.

### Alternative: maintainer contact

If you cannot use private reporting, contact the maintainers through a **non-public** channel they have published (e.g. email on profile or an agreed contact). **Do not** put exploit details in public issues or social posts.

## What to include

- Affected **component** (app, native, proot, wayland, etc.) and **version / commit** if known.
- **Steps to reproduce** or a clear description of the impact.
- Whether you plan to **coordinate disclosure** after a fix.

## Scope

This policy covers **this repository** and shipped artifacts. Issues in **upstream** projects (Termux proot, KDE, Firefox, etc.) should be reported to those projects according to their policies; we can still fix **our integration** if needed.

## Response

Maintainers will aim to **acknowledge** serious reports in a reasonable time; timing depends on availability. Thank you for helping keep users safe.
