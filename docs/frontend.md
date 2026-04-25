# Frontend

## Stack

- **Framework:** Next.js 14 (App Router, single route `/`)
- **Language:** TypeScript 5, React 18
- **Styling:** Inline `React.CSSProperties` everywhere — no Tailwind, no CSS modules, no styled-components
- **State:** `useState` hooks only; no global store
- **Build / dev server:** `next dev` / `next build`

## Layout

The app is a single page (`app/page.tsx`). Three fixed regions:

```
┌─────────────────────────────────────────────────┐  ← NavBar  (h=48px, z=300)
│ [sidebar] │ [panel drawer]  │  (empty / 3-D)    │
│  w=48px   │   w≈280px       │                   │
│  z=250    │   z=200         │                   │
│           │                 │                   │
└─────────────────────────────────────────────────┘
```

- **NavBar** — fixed top bar. Shows brand name "SILOS" (centered) and a live shuttle count with a pulsing green dot on the right.
- **Sidebar** — fixed left rail with three vertical labels (rotated 90 °). Clicking a label opens its panel; clicking again closes it.
- **Panel drawer** — slides in from the left when a section is active (`display: block/none` toggle). Hosts `PanelSistema`, `PanelConfiguracion`, or `PanelParametros`.

## File structure

```
frontend/
├── app/
│   ├── layout.tsx        # Root layout — imports globals.css, sets <title>
│   ├── page.tsx          # Single page, wires NavBar + Sidebar + panels
│   └── globals.css       # Reset, body font, scrollbar hide, keyframes
├── components/
│   ├── NavBar.tsx
│   ├── Sidebar.tsx
│   ├── PanelSistema.tsx
│   ├── PanelConfiguracion.tsx
│   ├── PanelParametros.tsx
│   └── ui/
│       ├── StatRow.tsx   # label / value row
│       ├── Hr.tsx        # thin horizontal divider
│       ├── CfgInput.tsx  # text/password input with bottom-border
│       ├── CfgSlider.tsx # range slider with live value display
│       ├── CfgToggle.tsx # label + toggle row
│       └── Toggle.tsx    # pill toggle (28×14 px)
├── lib/
│   ├── tokens.ts         # typography scale (T), font family (FF), DEFAULT_CONFIG
│   └── mock.ts           # generates random shuttle/robot data for dev
└── mockups/
    └── prelim-design.html  # standalone HTML mockup (Three.js, not used in prod)
```

## Design system

### Typography

All type is set in `'Helvetica Neue', Helvetica, Arial, sans-serif` (constant `FF` from `lib/tokens.ts`). No web fonts are loaded.

The token map `T` defines these named styles:

| Token     | Size | Weight | Letter-spacing | Transform  | Color  |
|-----------|------|--------|----------------|------------|--------|
| `brand`   | 15px | 500    | 0.26 em        | uppercase  | #000   |
| `nav`     | 11px | 400    | 0.12 em        | uppercase  | #aaa   |
| `section` | 9px  | 500    | 0.18 em        | uppercase  | #000   |
| `label`   | 9px  | 400    | 0.16 em        | uppercase  | #aaa   |
| `value`   | 11px | 500    | 0.04 em        | —          | #000   |
| `micro`   | 9px  | 400    | 0.12 em        | uppercase  | #ccc   |

Raw sizes used inline (inputs, demand table, etc.): **9 px** and **11 px** only.

### Color palette

| Role                   | Value     |
|------------------------|-----------|
| Background             | `#ffffff` |
| Surface (alt)          | `#f8f8f8` |
| Primary text           | `#000000` |
| Secondary text         | `#666`    |
| Muted text             | `#888`, `#999`, `#aaa`, `#bbb`, `#ccc` |
| Border (strong)        | `#e8e8e8` |
| Border (soft)          | `#e0e0e0`, `#ebebeb`, `#f4f4f4` |
| Accent — active/ok     | `#22c55e` (green) |
| Disabled / off-state   | `#e0e0e0` |
| White (on dark)        | `#ffffff` |

There are no other named colors. The palette is intentionally near-monochrome with a single green accent.

### Spacing

- Panel padding: `28px` top/bottom, `24px` left/right
- Section gap (between groups): `24px`
- Element gap (within a group): `14–16 px`
- Sidebar item gap: `36px`
- NavBar height / Sidebar width: **48 px** both

### Borders

Everything uses `1px solid` lines. Panels have `borderBottom` on the NavBar and `borderRight` on the Sidebar. Inside panels the `<Hr />` component renders `borderTop: '1px solid #ebebeb'`. Inputs use `borderBottom` only (no box).

### Interactive elements

| Element         | Appearance                                              |
|-----------------|---------------------------------------------------------|
| Primary button  | Full-width, `background: #000`, white text, no border, 9 px uppercase label |
| Button feedback | Background transitions to `#22c55e` for 2 s on success |
| Secondary button| Transparent, `border: 1px solid #e0e0e0`, muted uppercase label |
| Text input      | Transparent background, `borderBottom: 1px solid #e0e0e0` |
| Range slider    | Native `<input type="range">`, `accentColor: #000`     |
| Toggle          | 28×14 px pill; `#000` when on, `#e0e0e0` when off; 10×10 px white knob slides with CSS transition |
| Sidebar item    | Active: `color #000` + left border `1px solid #000`. Inactive: `color #bbb`, transparent border |
| Preset radio    | 6×6 px circle dot; filled `#000` when selected, outlined `#ccc` otherwise |

### Animations

```css
/* pulsing live indicator */
@keyframes pulse {
  0%, 100% { opacity: 1; }
  50%       { opacity: 0.3; }
}
/* duration: 2.4 s infinite */

/* panel/content entrance */
@keyframes fadeIn {
  from { opacity: 0; transform: translateY(4px); }
  to   { opacity: 1; transform: translateY(0); }
}
/* duration: 0.4 s ease */
```

State transitions (toggle, button color, sidebar highlight): `0.2–0.3 s` linear or ease.

---

## PanelParametros

`components/PanelParametros.tsx` — the simulation input generator. Produces a JSON payload for the backend.

### Data model

```ts
interface Params {
  num_boxes:                 number      // total boxes to simulate
  num_destinations:          number      // number of unique destinations
  seed:                      number      // RNG seed for reproducibility
  mean_inter_arrival_ticks:  number      // mean gap between box arrivals
  std_inter_arrival_ticks:   number      // std-dev of the arrival gap
  demand_profile:            DemandRow[] // time-based demand multipliers
  weights:                   Weight[]    // per-destination probability weights
}

interface DemandRow {
  from_tick:        number
  to_tick:          number
  rate_multiplier:  number   // 1.0 = baseline, >1 = surge
}

interface Weight {
  name:   string   // destination identifier (e.g. "zara_es")
  value:  number   // relative weight
}
```

### Presets

Two built-in presets load a full `Params` object on click. Editing any field switches the selector to "Personalizado" automatically.

| Preset      | `num_boxes` | `num_destinations` | Description                       |
|-------------|-------------|--------------------|-----------------------------------|
| Básica      | 100         | 5                  | Minimal load, 5 Inditex brands    |
| Avanzada    | 500         | 23                 | Full Inditex brand/country matrix |
| Personalizado | —         | —                  | Free-form, set by manual editing  |

Both presets share the same default demand profile: flat multiplier 1.0 from tick 0–3600, surge ×5.0 from tick 3600–3960, then back to 1.0.

### Sections

**Presets** — three rows with a radio-style dot indicator. "Personalizado" is not clickable; it activates automatically.

**Parámetros generales** — five numeric inputs in a responsive CSS grid (`auto-fill, minmax(180px, 1fr)`):
- Número de cajas
- Destinos
- Semilla
- Media de llegada (ticks)
- Desviación de llegada (ticks)

**Perfil de demanda** — dynamic table. Each row has three numeric fields (`from_tick`, `to_tick`, `rate_multiplier`) and a `×` delete button. "+ Añadir tramo" appends a new row with defaults `(0, 99999, 1.0)`.

**Pesos de destino** — dynamic two-column grid (`auto-fill, minmax(280px, 1fr)`). Each entry has a text input for the destination ID and a numeric input for its weight. "+ Añadir destino" appends a blank entry. A micro label in the section header shows the current count.

**Apply button** — full-width black button. On click it logs the serialized payload to the console (weights are serialized as a `Record<string, number>` object instead of an array) and flips to `✓ Aplicado` / green for 2 s. The actual `POST` to the backend is a TODO.

### State flow

```
applyPreset(p)          → replaces entire params with preset clone, sets preset = p
patch(key, value)       → partial update to params, sets preset = 'personalizado'
patchDemand(i, field)   → calls patch('demand_profile', updatedArray)
patchWeight(i, field)   → calls patch('weights', updatedArray)
addDemandRow / addWeight           → appends default entry via patch
removeDemandRow(i) / removeWeight(i) → filters out index via patch
handleApply             → sets applied=true for 2 s, logs JSON payload
```
