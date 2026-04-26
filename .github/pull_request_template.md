## Summary

<!-- One or two sentences describing what this PR does and why. -->

## Type of change

- [ ] `feat` — new feature
- [ ] `fix` — bug fix
- [ ] `refactor` — code change with no behaviour change
- [ ] `perf` — performance improvement
- [ ] `docs` — documentation only
- [ ] `test` — tests added or corrected
- [ ] `build` — build system or dependency change
- [ ] `chore` — everything else

## Motivation and context

<!-- Why is this change needed? What problem does it solve? Link to a related
     issue if one exists: "Closes #123" -->

## Testing done

<!-- Describe what you tested and how. Include commands if relevant. -->

```bash
# Example:
./backend/cpp/run_tests.sh
pytest backend/python/
```

## Definition of done checklist

- [ ] C++ build passes without warnings (`cmake --build build`)
- [ ] All C++ tests pass (`./backend/cpp/run_tests.sh`)
- [ ] All Python tests pass (`pytest`)
- [ ] `CHANGELOG.md` updated under `[Unreleased]` with a human-readable entry
- [ ] New source files include SPDX license header
- [ ] PR description explains the motivation (not just the file changes)

## Notes for reviewers

<!-- Anything the reviewer should pay special attention to, or context that
     is not obvious from the diff. -->
