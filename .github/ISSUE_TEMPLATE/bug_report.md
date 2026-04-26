---
name: Bug report
about: Something is broken or behaving unexpectedly
labels: bug
---

## Description

<!-- A clear and concise description of what the bug is. -->

Example: *The simulation hangs indefinitely when `num_aisles` is set to 0.*

## Steps to reproduce

1. ...
2. ...
3. ...

<!-- If you can reproduce it with a curl command or a specific CSV file,
     please include it. -->

```bash
# Example:
curl -X POST http://localhost:8000/simulations \
  -H "Content-Type: application/json" \
  -d '{"num_aisles": 0, "num_shuttles": 1}'
```

## Expected behaviour

<!-- What did you expect to happen? -->

## Actual behaviour

<!-- What actually happened? Include the full error message or log output. -->

```
# Paste logs here
```

## Environment

| Field | Value |
|---|---|
| OS | e.g. Ubuntu 22.04 |
| Compiler | e.g. GCC 13 |
| Python version | e.g. 3.11 |
| Node.js version | e.g. 20 |
| Docker version (if using Docker) | e.g. 26 |
| Commit or version | e.g. `f7477f9` |

## Additional context

<!-- Screenshots, related issues, or anything else that helps. -->
