#!/usr/bin/env python3
"""
Read benchmark JSON from stdin or a file and print a comparison table.

Usage:
    ./build/bench | python3 benchmark/compare.py
    python3 benchmark/compare.py results.json
"""

import json
import sys
from collections import defaultdict


# ── per-run table columns ────────────────────────────────────────────────────

RUN_COLUMNS = [
    ("rate_boxes_per_hour",      "rate/hr",          ".0f"),
    ("pre_populated",            "pre_pop",          "b"),
    ("num_robots",               "robots",           "d"),
    ("heuristic",                "heuristic",        "s"),
    ("seed",                     "seed",             "d"),
    ("total_pallets_sent",       "pallets",          "d"),
    ("filled_pallets",           "filled",           "d"),
    ("total_boxes_sent",         "boxes",            "d"),
    ("pct_pallets_filled",       "pct_filled",       ".3f"),
    ("avg_pallet_capacity",      "avg_cap",          ".3f"),
    ("ticks_per_filled_pallet",  "ticks/filled_pal", ".1f"),
    ("finish_tick",              "finish_tick",      "d"),
    ("occupation_pct",           "occ_%",            ".1f"),
]

# ── aggregate table columns ──────────────────────────────────────────────────

AGG_METRICS = [
    ("pct_pallets_filled",      "avg pct_filled",       ".3f"),
    ("avg_pallet_capacity",     "avg avg_cap",           ".3f"),
    ("ticks_per_filled_pallet", "avg ticks/filled_pal",  ".1f"),
]


# ── helpers ──────────────────────────────────────────────────────────────────

def load(source) -> list:
    data = json.loads(source.read())
    return data if isinstance(data, list) else [data]


def fmt(val, spec):
    if spec == "s":
        return str(val)
    if spec == "b":
        return "yes" if val else "no"
    if spec in (".1f", ".3f") and val == 0.0:
        return "--"       # sentinel: metric undefined (e.g. no filled pallets)
    return format(val, spec)


def print_table(headers, rows, specs):
    widths = [max(len(h), max((len(r[i]) for r in rows), default=0))
              for i, h in enumerate(headers)]
    sep  = "+-" + "-+-".join("-" * w for w in widths) + "-+"
    head = "| " + " | ".join(h.ljust(w) for h, w in zip(headers, widths)) + " |"
    print(sep)
    print(head)
    print(sep)
    for row in rows:
        print("| " + " | ".join(c.rjust(w) for c, w in zip(row, widths)) + " |")
    print(sep)


# ── main ─────────────────────────────────────────────────────────────────────

def main():
    if len(sys.argv) > 1:
        with open(sys.argv[1]) as f:
            rows = load(f)
    else:
        rows = load(sys.stdin)

    if not rows:
        print("No results found.", file=sys.stderr)
        sys.exit(1)

    # ── per-run table ────────────────────────────────────────────────────────
    headers = [c[1] for c in RUN_COLUMNS]
    keys    = [c[0] for c in RUN_COLUMNS]
    specs   = [c[2] for c in RUN_COLUMNS]
    table   = [[fmt(row.get(k, ""), s) for k, s in zip(keys, specs)] for row in rows]
    print_table(headers, table, specs)

    # ── aggregate by (rate, pre_populated, num_robots, heuristic) across seeds ─
    groups = defaultdict(list)
    for row in rows:
        groups[(row["rate_boxes_per_hour"], row["pre_populated"],
                row["num_robots"], row["heuristic"])].append(row)

    # Build aggregate rows, then split by rate
    agg_by_rate = defaultdict(list)
    for (rate, pre_pop, num_robots, heuristic), group in groups.items():
        row_vals = [fmt(rate, ".0f"), fmt(num_robots, "d"), heuristic]
        sort_key = None
        for key, _, spec in AGG_METRICS:
            vals = [r[key] for r in group]
            avg  = sum(vals) / len(vals)
            row_vals.append(fmt(avg, spec))
            if key == "ticks_per_filled_pallet":
                sort_key = avg  # 0.0 = undefined; sort those last
        agg_by_rate[rate].append((pre_pop, num_robots, sort_key, row_vals))

    agg_headers = ["rate/hr", "robots", "heuristic"] + [c[1] for c in AGG_METRICS]

    print()
    for rate in sorted(agg_by_rate.keys()):
        entries = agg_by_rate[rate]
        # Sort by pre_populated, then num_robots, then ticks_per_filled_pallet ascending
        entries.sort(key=lambda x: (x[0], x[1], x[2] == 0.0, x[2]))
        agg_rows = [e[3] for e in entries]
        print(f"Averages across seeds — rate {rate:.0f} boxes/hr:")
        print_table(agg_headers, agg_rows, [])
        print()


if __name__ == "__main__":
    main()
