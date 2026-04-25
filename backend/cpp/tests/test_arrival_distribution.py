import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../build"))

import scheduler_cpp as cpp


def make_params(num_boxes=200, mean_ticks=3.6, std_ticks=0.072, seed=42):
    p = cpp.BoxGeneratorParams()
    p.num_boxes                 = num_boxes
    p.mean_inter_arrival_ticks  = mean_ticks
    p.std_inter_arrival_ticks   = std_ticks
    p.seed                      = seed
    return p


def test_arrival_rate_fields_exist():
    p = cpp.BoxGeneratorParams()
    _ = p.mean_inter_arrival_ticks
    _ = p.std_inter_arrival_ticks


def test_demand_period_class_exists():
    dp = cpp.DemandPeriod()
    _ = dp.from_tick
    _ = dp.to_tick
    _ = dp.rate_multiplier


def test_demand_profile_field_exists():
    p = cpp.BoxGeneratorParams()
    _ = p.demand_profile


def test_arrival_ticks_monotone():
    boxes = cpp.generate_boxes(make_params())
    ticks = [b.arrival_tick for b in boxes]
    assert ticks == sorted(ticks), "arrival_ticks must be non-decreasing"


def test_arrival_ticks_spread_out():
    boxes = cpp.generate_boxes(make_params())
    ticks = [b.arrival_tick for b in boxes]
    assert not all(t == 0 for t in ticks), "arrival_ticks should not all be 0"
    assert max(ticks) > 0, "at least one box should arrive after tick 0"


def test_mean_inter_arrival_approx():
    boxes = cpp.generate_boxes(make_params(num_boxes=5000, mean_ticks=3.6))
    ticks = [b.arrival_tick for b in boxes]
    inter_arrivals = [ticks[i+1] - ticks[i] for i in range(len(ticks) - 1)]
    mean_ia = sum(inter_arrivals) / len(inter_arrivals)
    assert abs(mean_ia - 3.6) < 0.5, f"mean inter-arrival {mean_ia:.3f} too far from 3.6"


def test_seed_reproducible():
    b1 = cpp.generate_boxes(make_params(seed=7))
    b2 = cpp.generate_boxes(make_params(seed=7))
    assert [b.arrival_tick for b in b1] == [b.arrival_tick for b in b2]


def test_no_negative_inter_arrivals():
    boxes = cpp.generate_boxes(make_params(num_boxes=1000))
    ticks = [b.arrival_tick for b in boxes]
    assert all(t >= 0 for t in ticks), "arrival_ticks must be non-negative"


def test_demand_profile_increases_density():
    """Boxes in a 5x-rate window should arrive ~5x more densely than outside."""
    num_boxes = 5000

    # Flat rate — count boxes arriving in ticks [500, 1000)
    flat = make_params(num_boxes=num_boxes, mean_ticks=3.6)
    flat.demand_profile = []
    flat_boxes = cpp.generate_boxes(flat)
    flat_in_window = sum(1 for b in flat_boxes if 500 <= b.arrival_tick < 1000)

    # 5x multiplier in [500, 1000)
    boosted = make_params(num_boxes=num_boxes, mean_ticks=3.6, seed=123)
    dp = cpp.DemandPeriod()
    dp.from_tick = 500
    dp.to_tick = 1000
    dp.rate_multiplier = 5.0
    boosted.demand_profile = [dp]
    boosted_boxes = cpp.generate_boxes(boosted)
    boosted_in_window = sum(1 for b in boosted_boxes if 500 <= b.arrival_tick < 1000)

    assert boosted_in_window > flat_in_window * 2, (
        f"expected more boxes in boosted window ({boosted_in_window}) vs flat ({flat_in_window})"
    )


def test_no_demand_profile_is_flat():
    """Without a demand profile, inter-arrival distribution should be consistent."""
    boxes = cpp.generate_boxes(make_params(num_boxes=2000, mean_ticks=3.6))
    ticks = [b.arrival_tick for b in boxes]
    mid = ticks[len(ticks) // 2]
    first_half  = sum(1 for t in ticks if t < mid)
    second_half = sum(1 for t in ticks if t >= mid)
    ratio = first_half / max(second_half, 1)
    assert 0.8 <= ratio <= 1.2, f"expected ~equal split, got {first_half}/{second_half}"


if __name__ == "__main__":
    tests = [
        test_arrival_rate_fields_exist,
        test_demand_period_class_exists,
        test_demand_profile_field_exists,
        test_arrival_ticks_monotone,
        test_arrival_ticks_spread_out,
        test_mean_inter_arrival_approx,
        test_seed_reproducible,
        test_no_negative_inter_arrivals,
        test_demand_profile_increases_density,
        test_no_demand_profile_is_flat,
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
