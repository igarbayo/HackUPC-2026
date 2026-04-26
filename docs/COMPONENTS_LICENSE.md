# Components License

This document explains the license chosen for XEITECH, the reasoning behind
that choice, and the license status of every dependency used in the project.

---

## Project license: MIT

XEITECH is released under the **MIT License** (SPDX: `MIT`).
The full text is in [LICENSE](../LICENSE) and a copy is in [LICENSES/MIT.txt](../LICENSES/MIT.txt).

### Why MIT?

The MIT license is the most appropriate choice for this project for the
following reasons:

**Greenfield codebase.** XEITECH was written from scratch at HackUPC 2026.
There is no inherited code, no prior proprietary base, and no obligation to
adopt a specific license for compatibility reasons. We were free to choose the
most suitable license for our goals.

**Permissive intent.** We want anyone — individuals, universities, companies,
or other hackathon participants — to be able to read, run, modify, and
redistribute the code with minimal friction. A copyleft license (GPL, AGPL)
would impose obligations on downstream users that are unnecessary for a
demonstration project of this nature.

**No viral-copyleft dependencies.** All dependencies we use are either MIT or
BSD-licensed (see table below). None of them are GPL or AGPL, so there is no
obligation to adopt a copyleft license for compatibility reasons.

**No network service obligation.** We considered AGPL v3, which would require
any party running XEITECH as a network service to publish their modifications.
However, given that this is a hackathon project without any known commercial
deployments, the added friction of AGPL outweighs its benefits.

**Standard for open-source projects of this scale.** MIT is widely understood,
OSI-approved, and imposes no conditions beyond preserving copyright and license
notices. It maximises the chance that the code is genuinely useful to others.

### No warranties

As stated in the MIT License text:

> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
> IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
> FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.

This means that the XEITECH maintainers provide no guarantee that the software
is correct, suitable for any particular purpose, or free of defects.
The software is provided in good faith for educational and demonstration use.

---

## Dependency audit

The table below lists every dependency used in the project, its license, and
whether that license is compatible with MIT.

### Python backend (`backend/python/requirements.txt`)

| Package | License | SPDX | Compatible with MIT |
|---|---|---|---|
| FastAPI | MIT | `MIT` | Yes |
| Uvicorn | BSD 3-Clause | `BSD-3-Clause` | Yes |
| Pydantic | MIT | `MIT` | Yes |
| python-dotenv | BSD 3-Clause | `BSD-3-Clause` | Yes |
| httpx | BSD 3-Clause | `BSD-3-Clause` | Yes |
| python-multipart | Apache 2.0 | `Apache-2.0` | Yes |
| pytest | MIT | `MIT` | Yes |
| pytest-anyio | MIT | `MIT` | Yes |

### C++ engine (`backend/cpp/`)

| Component | License | SPDX | Compatible with MIT |
|---|---|---|---|
| pybind11 | BSD 3-Clause | `BSD-3-Clause` | Yes |
| C++ standard library (libstdc++ / libc++) | GPL v3 with runtime exception | `GPL-3.0-only WITH GCC-exception-3.1` | Yes (runtime exception removes copyleft obligation for linked binaries) |

### Frontend (`frontend/`)

| Package | License | SPDX | Compatible with MIT |
|---|---|---|---|
| Next.js | MIT | `MIT` | Yes |
| React | MIT | `MIT` | Yes |
| react-dom | MIT | `MIT` | Yes |
| Three.js | MIT | `MIT` | Yes |
| TypeScript | Apache 2.0 | `Apache-2.0` | Yes |
| @types/node | MIT | `MIT` | Yes |
| @types/react | MIT | `MIT` | Yes |
| @types/three | MIT | `MIT` | Yes |

### Conclusion

No dependency is licensed under GPL, AGPL, LGPL (without exception), or any
other license that would impose conditions incompatible with MIT. The MIT
license for XEITECH is fully compatible with the entire dependency tree.

---

## REUSE compliance

XEITECH follows the REUSE specification v3.3 for license metadata:

- The full MIT license text is stored in `LICENSES/MIT.txt` using the SPDX
  identifier as the file name.
- New source files should include SPDX headers:
  ```
  // SPDX-License-Identifier: MIT
  // SPDX-FileCopyrightText: 2026 XEITECH Team
  ```
- Files that cannot carry headers (binary assets, generated files) are covered
  by the `.reuse/dep5` declaration.
