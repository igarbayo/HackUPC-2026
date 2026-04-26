# InputBelt & Box Generator

## What it is

`InputBelt` is the entry point for boxes into the silo. It is a FIFO queue that sits at the silo
entrance (the input port). Shuttles pull boxes from it on demand when they become idle.

It also owns the **box generation** logic: given a set of parameters it produces the full sequence
of boxes that will flow through the simulation.

---

## Interface

```cpp
class InputBelt {
public:
    void               push(Box b);       // enqueue a box manually
    std::optional<Box> pop();             // dequeue front box (called by aisle on shuttle request)
    std::optional<Box> peek() const;      // inspect front without removing (look-ahead)
    bool               empty() const;
    std::size_t        size()  const;

    static InputBelt                   generate(const BoxGeneratorParams& p);
    static const std::vector<Family>&  availableDestinations();
};
```

`pop()` and `peek()` return `std::nullopt` when the queue is empty so callers never need to
check `empty()` first.

---

## BoxGeneratorParams

```cpp
struct BoxGeneratorParams {
    int                                num_boxes        = 100;
    int                                num_destinations = 20;
    std::unordered_map<Family, double> weights;   // empty = uniform
    uint64_t                           seed        = 42;

    static BoxGeneratorParams fromFile(const std::string& path);
};
```

| Field | Meaning | Default |
|---|---|---|
| `num_boxes` | Total boxes to generate | 100 |
| `num_destinations` | How many distinct Inditex destinations to use (clamped to ≤ 23) | 20 |
| `weights` | Per-destination probability weight; empty = uniform | uniform |
| `seed` | RNG seed for reproducibility | 42 |

### fromFile

Loads params from a JSON file. Any missing field keeps its default value, so the JSON can be
partial. Throws `std::runtime_error` if the file cannot be opened.

```cpp
BoxGeneratorParams p = BoxGeneratorParams::fromFile("backend/cpp/input/generator_config.json");
```

---

## JSON config file

Location: `backend/cpp/input/generator_config.json`

```json
{
  "num_boxes": 500,
  "num_destinations": 23,
  "seed": 42,
  "weights": {
    "zara_es": 8.0,
    "zara_fr": 6.0,
    ...
  }
}
```

All fields are optional. `weights` empty or absent means uniform distribution.
The format is intentionally identical to what the REST API will receive in `POST /simulations`,
so the Python layer can later construct `BoxGeneratorParams` directly from the request body
without touching the file.

The executable accepts an optional path argument; if omitted it reads the default file:

```bash
./build/silos                                        # uses backend/cpp/input/generator_config.json
./build/silos backend/cpp/input/generator_config.json
```

---

## Available destinations

23 Inditex destinations ordered by expected volume. `num_destinations = N` takes the first N.

| Family | Destination code |
|---|---|
| zara_es | 01100001 |
| zara_fr | 01100002 |
| zara_de | 01100003 |
| zara_uk | 01100004 |
| zara_it | 01100005 |
| zara_pt | 01100006 |
| bershka_es | 01200001 |
| bershka_fr | 01200002 |
| bershka_de | 01200003 |
| bershka_uk | 01200004 |
| stradivarius_es | 01300001 |
| stradivarius_fr | 01300002 |
| stradivarius_de | 01300003 |
| pull_bear_es | 01400001 |
| pull_bear_fr | 01400002 |
| pull_bear_de | 01400003 |
| massimo_dutti_es | 01500001 |
| massimo_dutti_fr | 01500002 |
| oysho_es | 01600001 |
| oysho_fr | 01600002 |
| zara_home_es | 01700001 |
| zara_home_fr | 01700002 |
| lefties_es | 01800001 |

The first 20 entries cover at minimum one country per brand across all 8 Inditex brands,
satisfying the challenge requirement of ≥ 20 destinations.

---

## Box ID format

Each box gets a 20-character barcode string following the challenge specification:

```
3010028  +  dest_code(8)  +  bulk(5)   =  20 chars
└source─┘   └─dest────────┘  └counter─┘
```

- **Source** `3010028`: fixed warehouse code for this silo.
- **Dest code**: 8-digit code from the table above (unique per destination).
- **Bulk**: per-destination sequential counter, zero-padded to 5 digits (00001, 00002, …).

Example: `30100280110000100003` = silo → zara_es → 3rd box for that destination.

`BoxId` is `std::string` (not `uint64_t`) because 20-digit codes starting with `3…` overflow
`uint64_t` (max ≈ 1.84 × 10¹⁹).

---

## Generation algorithm

`InputBelt::generate(params)`:

1. Clamp `num_destinations` to `[1, 23]`.
2. Take the first `num_destinations` entries from the destination table.
3. Build a probability vector from `weights` (uniform if empty).
4. Seed `std::mt19937_64` with `params.seed`.
5. For each of `num_boxes` boxes: sample a destination, increment its bulk counter, build the
   20-char ID, push `Box(id, family, 0)` onto the queue.

`arrivalTick = 0` for all boxes — arrival timing is not modelled yet.

---

## Why Scheduler no longer pushes boxes

Previously `Scheduler::activate()` drained one box per tick from `InputBelt` and called
`aisle.input()` directly. This was replaced by a pull model:

- A shuttle that becomes idle requests a box from the aisle.
- The aisle calls `inputBelt.pop()`.

`Scheduler` still holds a reference to `InputBelt` (constructor signature unchanged) but no
longer drains it actively. The wiring between aisle and belt is Carlos's responsibility
(`aisle.input()` / shuttle request functions).

---

## Dependencies

- **nlohmann/json** (`include/json.hpp`): single-header JSON parser used by `fromFile`.
  No CMake changes required. Download:
  ```bash
  curl -sL https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp \
       -o backend/cpp/include/json.hpp
  ```
