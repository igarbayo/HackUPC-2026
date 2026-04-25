import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../build"))

import scheduler_cpp as cpp
from collections import Counter


def make_params(num_boxes=100, num_destinations=20, weights=None, seed=42):
    p = cpp.BoxGeneratorParams()
    p.num_boxes        = num_boxes
    p.num_destinations = num_destinations
    p.seed             = seed
    if weights:
        p.weights = weights
    return p


def test_count():
    boxes = cpp.generate_boxes(make_params(num_boxes=50))
    assert len(boxes) == 50, f"Expected 50, got {len(boxes)}"


def test_unique_ids():
    boxes = cpp.generate_boxes(make_params(num_boxes=200))
    ids = [b.id for b in boxes]
    assert len(ids) == len(set(ids)), "Box IDs are not unique"


def test_id_format_20_chars():
    for b in cpp.generate_boxes(make_params(num_boxes=10)):
        assert len(b.id) == 20, f"id '{b.id}' is not 20 chars"
        assert b.id.isdigit(), f"id '{b.id}' contains non-digits"


def test_id_source_prefix():
    for b in cpp.generate_boxes(make_params(num_boxes=10)):
        assert b.id[:7] == "3010028", f"wrong source prefix: {b.id[:7]}"


def test_num_destinations_exact():
    boxes = cpp.generate_boxes(make_params(num_destinations=5))
    assert len(set(b.family for b in boxes)) == 5


def test_all_families_valid():
    valid = set(cpp.available_destinations())
    for b in cpp.generate_boxes(make_params()):
        assert b.family in valid, f"Unknown family: {b.family}"


def test_available_destinations_minimum_20():
    dests = cpp.available_destinations()
    assert len(dests) >= 20, f"Expected >= 20, got {len(dests)}"


def test_uniform_distribution():
    boxes = cpp.generate_boxes(make_params(num_boxes=2000, num_destinations=4))
    counts = Counter(b.family for b in boxes)
    for fam, cnt in counts.items():
        assert 425 <= cnt <= 575, f"{fam}: {cnt} boxes (expected ~500)"


def test_weighted_distribution():
    dests = cpp.available_destinations()
    weights = {dests[0]: 3.0, dests[1]: 1.0}
    params = make_params(num_boxes=2000, num_destinations=2)
    params.weights = weights
    boxes = cpp.generate_boxes(params)
    counts = Counter(b.family for b in boxes)
    ratio = counts[dests[0]] / counts[dests[1]]
    assert 2.5 <= ratio <= 3.5, f"Expected ratio ~3, got {ratio:.2f}"


def test_reproducible():
    b1 = cpp.generate_boxes(make_params(seed=7))
    b2 = cpp.generate_boxes(make_params(seed=7))
    assert [(b.id, b.family) for b in b1] == [(b.id, b.family) for b in b2]


def test_different_seeds_differ():
    b1 = cpp.generate_boxes(make_params(seed=1))
    b2 = cpp.generate_boxes(make_params(seed=2))
    assert [b.family for b in b1] != [b.family for b in b2]


def test_arrival_tick_zero():
    boxes = cpp.generate_boxes(make_params())
    assert all(b.arrival_tick == 0 for b in boxes), "All arrival_ticks must be 0"


if __name__ == "__main__":
    tests = [
        test_count,
        test_unique_ids,
        test_id_format_20_chars,
        test_id_source_prefix,
        test_num_destinations_exact,
        test_all_families_valid,
        test_available_destinations_minimum_20,
        test_uniform_distribution,
        test_weighted_distribution,
        test_reproducible,
        test_different_seeds_differ,
        test_arrival_tick_zero,
    ]
    passed = failed = 0
    for t in tests:
        try:
            t()
            print(f"  PASS  {t.__name__}")
            passed += 1
        except Exception as e:
            print(f"  FAIL  {t.__name__}: {e}")
            failed += 1
    print(f"\n{passed} passed, {failed} failed")
