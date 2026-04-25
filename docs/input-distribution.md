# Input Distribution

Boxes arrive at staggered ticks following a normal distribution of inter-arrival times. All arrival ticks are pre-computed in `InputBelt::generate()` and stored in each `Box`; the `Aisle` only admits a box once `currentTick >= box.arrivalTick`.

## Parameters (`generator_config.json`)

| Field | Default | Meaning |
|---|---|---|
| `mean_inter_arrival_ticks` | `3.6` | Mean ticks between consecutive boxes (≈ 1000 boxes/hour) |
| `std_inter_arrival_ticks` | `0.072` | Std dev of inter-arrival time in ticks |

Inter-arrival samples are drawn from `N(μ, σ)` and clamped to ≥ 0.

## Demand profile

An optional piecewise rate multiplier. A `rate_multiplier` of `k` in a tick window means the arrival rate is `k×` higher (inter-arrival mean and std are divided by `k`).

```json
"demand_profile": [
  { "from_tick":    0, "to_tick":  3600, "rate_multiplier": 1.0 },
  { "from_tick": 3600, "to_tick":  3960, "rate_multiplier": 5.0 },
  { "from_tick": 3960, "to_tick": 99999, "rate_multiplier": 1.0 }
]
```

Omit `demand_profile` (or leave it empty) for a constant rate throughout the simulation.
