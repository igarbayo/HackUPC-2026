# Governance

This document describes how XEITECH is managed: who makes decisions, how
decisions are made, and how new contributors can grow into maintainers.

---

## Project origin

XEITECH was built at HackUPC 2026 by four students as the Inditex challenge
submission. It is maintained voluntarily, outside of any academic or
professional obligation.

---

## Roles

### Maintainer

Maintainers have merge access to the repository and are responsible for
reviewing contributions, triaging issues, and setting the project's direction.
All four founding members are maintainers with equal standing:

| Name | Email |
|---|---|
| Ignacio Garbayo | iggarbayo@gmail.com |
| Anton Gomez | antongomez03@gmail.com |
| Yago Flagueras | yagofc03@gmail.com |
| Carlos Cao | caolopezcarlos@gmail.com |

### Contributor

Anyone who has had a pull request merged into the project. Contributors do not
have merge access but their views carry weight in discussions.

---

## Decision-making

### Day-to-day decisions

Any maintainer may act alone on routine matters: reviewing and merging pull
requests, closing stale issues, updating documentation, releasing patch
versions, and other changes that do not alter the project's overall direction.

### Significant decisions

The following require agreement from **at least two maintainers** before
proceeding:

- Adding or removing a dependency.
- Changing a public API or WebSocket protocol.
- Releasing a minor or major version.
- Modifying the governance, security, or licensing policy.
- Merging a pull request that introduces a breaking change.

Discussion happens in a GitHub issue or pull request, or asynchronously by
email. If a decision cannot be reached by consensus, a simple majority vote
among maintainers decides. In case of a tie, the change is postponed until
consensus is reached.

### Conflict resolution

1. The parties involved discuss the issue asynchronously (issue, PR, or email).
2. A 72-hour waiting period is observed to allow all maintainers to weigh in.
3. If no consensus, a simple majority vote of all four maintainers decides.
4. The outcome and reasoning are documented in the relevant issue or PR.

---

## Adding a new maintainer

New maintainers are added by **unanimous vote** of the current maintainers.
Candidates are typically long-term contributors who have demonstrated
consistent, high-quality contributions and good judgement.

There is no formal timeline or quota — maintainers are added when the project
genuinely needs more capacity.

---

## Communication

All significant decisions are discussed in public GitHub issues or pull
requests so that contributors can follow along and participate. Private
communication (email) is reserved for security matters and code-of-conduct
reports.
