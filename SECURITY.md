# Security Policy

## Scope

XEITECH is a non-commercial, open-source project created at HackUPC 2026.
Under the EU Cyber Resilience Act (CRA), non-commercial open-source software
developed without a commercial intent is currently out of scope for the full
manufacturer obligations. We include this security policy anyway because we
believe responsible disclosure is the right thing to do regardless of
regulatory requirements.

---

## Reporting a vulnerability

**Please do not open a public GitHub issue for security vulnerabilities.**

Doing so exposes the vulnerability to all users before a fix is available.
Instead, contact the maintainers directly by email:

- Ignacio Garbayo — iggarbayo@gmail.com
- Anton Gomez — antongomez03@gmail.com
- Yago Flagueras — yagofc03@gmail.com
- Carlos Cao — caolopezcarlos@gmail.com

You can email one or all of them. Use the subject line:
`[XEITECH SECURITY] <short description>`.

If you want to encrypt your report, ask us for a PGP public key in a
preliminary email.

---

## What to include in your report

- A clear description of the vulnerability and the potential impact.
- Steps to reproduce (a minimal example or proof-of-concept if possible).
- The version or commit hash where you observed the issue.
- Any mitigating factors you are aware of.

The more detail you provide, the faster we can triage and fix the issue.

---

## Response timeline

| Event | Target |
|---|---|
| Acknowledgement | Within **72 hours** of your first email |
| Initial triage (severity assessment) | Within **14 days** |
| Fix or workaround published | Within **90 days** for critical issues |
| Public disclosure | Coordinated with you after a fix is available |

If we cannot meet a deadline, we will let you know proactively. We are
students maintaining this in spare time, so timelines may slip — but we
will communicate openly.

---

## Known limitations

The following limitations are known and accepted given the project's context.
They are not treated as vulnerabilities:

- **No authentication or authorisation.** The API has no access controls.
  Anyone who can reach port 8000 can launch, read, or cancel any simulation.
  XEITECH is designed for local and trusted-network use only.

- **Simulation results are non-binding.** The output of the simulator is for
  demonstration and research purposes. It should not be used to make real
  warehouse operational decisions without independent validation.

- **Not hardened for internet exposure.** The Docker Compose setup binds to
  `0.0.0.0` by default. Do not expose the backend port to the public internet
  without adding an authentication layer and proper network controls.

- **Input validation is minimal.** CSV files and API parameters are lightly
  validated. Malformed input may cause the C++ engine to crash (with a logged
  exception), but the backend process will continue serving other requests.

---

## Disclosure policy

We follow **coordinated disclosure**: we ask that you give us a reasonable
time to fix the issue before publishing details publicly. Once a fix is
released, we are happy to credit you in the CHANGELOG and in any public
disclosure, if you wish.

We will never take legal action against a researcher who acts in good faith
and follows this policy.
