# Architecture Decisions

This document records the significant technical decisions made during the
development of XEITECH. It is meant to be honest and human: it explains what
we tried, what broke, why we changed course, and why we settled on the
current approach. It is not a polished design document — it is the truth about
how the project was built under hackathon conditions.

Entries follow the Architecture Decision Record (ADR) format:
**Title · Status · Context · Decision · Consequences**.

Maintainers should add a new entry for every significant decision going forward.

---

## ADR-001 — [Title]

**Status:** Proposed / Accepted / Deprecated / Superseded by ADR-XXX

**Context:**
What situation or problem prompted this decision? What constraints existed?
What had we already tried that did not work?

**Decision:**
What did we decide to do? State it clearly and unambiguously.

**Consequences:**
What became easier or harder as a result? Any known trade-offs or risks?

---

## How to add a new ADR

Copy the template above, increment the number, and fill in the four fields.
Add it at the bottom of this file. Do not modify past decisions — if a
decision is reversed, add a new ADR that supersedes the old one and update
the old entry's status to `Superseded by ADR-XXX`.

Keep the language plain and honest. Future contributors (including your
future self) will thank you for explaining the *why*, not just the *what*.
