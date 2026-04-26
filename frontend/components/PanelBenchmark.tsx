'use client'

import { FF, T } from '@/lib/tokens'
import Hr from '@/components/ui/Hr'

// ── Static data ────────────────────────────────────────────────────────────

const SETUP = [
  { label: 'Arrival rate',          value: '1 000 boxes / hour' },
  { label: 'Robots',                value: '1 or 2 (varied)' },
  { label: 'Aisles',                value: '4' },
  { label: 'Shuttles per aisle',    value: '8' },
  { label: 'Slots per shuttle',     value: '60  (2 sides × 2 slots per side per depth layer)' },
  { label: 'Pallet capacity (C)',   value: '12 boxes' },
  { label: 'Initial occupancy',     value: '~11.76 % of total capacity (pre-populated)' },
]

const HEURISTICS = [
  { name: 'largest_stock',   strategy: 'Always target the family with the most boxes currently stored in the silo' },
  { name: 'random',          strategy: 'Choose a destination family uniformly at random' },
  { name: 'stock_proximity', strategy: 'Weight families by stock volume and average proximity to the robot' },
  { name: 'nearest',         strategy: 'Always target the family whose boxes are closest to the output conveyor' },
  { name: 'coop',            strategy: '(2-robot only) Robots coordinate to avoid targeting the same family simultaneously' },
]

const METRICS = [
  {
    label:   'avg ticks / filled pallet',
    formula: 'avg_ticks_per_filled_pal = T / F',
    note:    'Lower is better. Most important metric: captures both processing speed and how many complete pallets are produced.',
  },
  {
    label:   'avg pct_filled',
    formula: 'avg_pct_filled = (1/|P|) × Σ 1[bᵢ = C]',
    note:    'Fraction of dispatched pallets that are completely full. Values near 1.0 indicate the robot almost never dispatches an incomplete pallet.',
  },
  {
    label:   'avg avg_cap',
    formula: 'avg_avg_cap = (1/|P|) × Σ (bᵢ / C)',
    note:    'Mean fill ratio across all dispatched pallets. When avg pct_filled < 1.0 this reveals whether incomplete pallets are nearly full or wastefully sparse.',
  },
]

interface BenchmarkRow {
  rateHr:      number
  robots:      number
  heuristic:   string
  ticksPerPal: number
  pctFilled:   number
  avgCap:      number
  movesK:      number
}

const RESULTS: BenchmarkRow[] = [
  { rateHr: 1000, robots: 1, heuristic: 'largest_stock',   ticksPerPal:  72.8, pctFilled: 1.000, avgCap: 1.000, movesK: 69.4 },
  { rateHr: 1000, robots: 1, heuristic: 'random',          ticksPerPal:  84.6, pctFilled: 0.954, avgCap: 0.990, movesK: 66.3 },
  { rateHr: 1000, robots: 1, heuristic: 'stock_proximity', ticksPerPal: 127.4, pctFilled: 0.980, avgCap: 0.997, movesK: 52.7 },
  { rateHr: 1000, robots: 1, heuristic: 'nearest',         ticksPerPal: 151.2, pctFilled: 0.933, avgCap: 0.977, movesK: 52.4 },
  { rateHr: 1000, robots: 2, heuristic: 'largest_stock',   ticksPerPal:  50.5, pctFilled: 1.000, avgCap: 1.000, movesK: 76.3 },
  { rateHr: 1000, robots: 2, heuristic: 'random',          ticksPerPal:  83.4, pctFilled: 0.956, avgCap: 0.993, movesK: 67.4 },
  { rateHr: 1000, robots: 2, heuristic: 'coop',            ticksPerPal:  96.0, pctFilled: 0.995, avgCap: 0.998, movesK: 59.7 },
  { rateHr: 1000, robots: 2, heuristic: 'stock_proximity', ticksPerPal: 107.4, pctFilled: 0.995, avgCap: 1.000, movesK: 57.2 },
  { rateHr: 1000, robots: 2, heuristic: 'nearest',         ticksPerPal: 135.5, pctFilled: 0.919, avgCap: 0.982, movesK: 56.4 },
]

// best = min ticksPerPal, max pctFilled, max avgCap, min movesK (per column over all rows)
const bestTicksPerPal = Math.min(...RESULTS.map(r => r.ticksPerPal))
const bestPctFilled   = Math.max(...RESULTS.map(r => r.pctFilled))
const bestAvgCap      = Math.max(...RESULTS.map(r => r.avgCap))
const bestMovesK      = Math.min(...RESULTS.map(r => r.movesK))

// ── Sub-components ─────────────────────────────────────────────────────────

function SectionTitle({ children }: { children: React.ReactNode }) {
  return (
    <div style={{ ...T.section, color: '#000', marginBottom: 16 }}>
      {children}
    </div>
  )
}

function Kv({ label, value }: { label: string; value: string }) {
  return (
    <div style={{ marginBottom: 10 }}>
      <div style={{ ...T.label, marginBottom: 3 }}>{label}</div>
      <div style={{ fontSize: 11, color: '#000', letterSpacing: '0.04em', fontFamily: FF }}>{value}</div>
    </div>
  )
}

function Formula({ label, formula, note }: { label: string; formula: string; note: string }) {
  return (
    <div style={{ flex: 1, minWidth: 0 }}>
      <div style={{ ...T.label, marginBottom: 6 }}>{label}</div>
      <div style={{
        fontFamily: 'monospace', fontSize: 10, background: '#f8f8f8',
        padding: '6px 10px', marginBottom: 8, letterSpacing: '0.02em',
        color: '#000', border: '1px solid #ebebeb',
        overflowX: 'auto', whiteSpace: 'nowrap',
      }}>
        {formula}
      </div>
      <div style={{ fontSize: 10, color: '#666', fontFamily: FF, lineHeight: 1.5, letterSpacing: '0.02em' }}>
        {note}
      </div>
    </div>
  )
}

function Finding({ children }: { children: React.ReactNode }) {
  return (
    <div style={{ marginBottom: 10, fontSize: 11, color: '#000', fontFamily: FF, lineHeight: 1.6, letterSpacing: '0.02em' }}>
      {children}
    </div>
  )
}

function Code({ children }: { children: React.ReactNode }) {
  return (
    <span style={{ fontFamily: 'monospace', fontSize: 10, background: '#f0f0f0', padding: '1px 5px', color: '#000' }}>
      {children}
    </span>
  )
}

function NumCell({ value, best, fmt }: { value: number; best: boolean; fmt: (v: number) => string }) {
  return (
    <div style={{
      fontVariantNumeric: 'tabular-nums',
      fontFamily: FF,
      fontSize: 11,
      letterSpacing: '0.04em',
      color: best ? '#000' : '#555',
      fontWeight: best ? 600 : 400,
    }}>
      {fmt(value)}
    </div>
  )
}

function GroupLabel({ label }: { label: string }) {
  return (
    <div style={{
      gridColumn: '1 / -1',
      display: 'flex', alignItems: 'center', gap: 10,
      padding: '10px 0 6px',
    }}>
      <div style={{ ...T.micro, color: '#777' }}>{label}</div>
      <div style={{ flex: 1, height: 1, background: '#ebebeb' }} />
    </div>
  )
}

// ── Main component ─────────────────────────────────────────────────────────

const COL = '52px 136px 1fr 1fr 1fr 1fr'

export default function PanelBenchmark() {
  const rows1 = RESULTS.filter(r => r.robots === 1)
  const rows2 = RESULTS.filter(r => r.robots === 2)

  return (
    <div style={{ padding: '28px 24px', fontFamily: FF, maxWidth: 960 }}>

      {/* ── Header ── */}
      <div style={{ marginBottom: 4 }}>
        <div style={{ ...T.section, fontSize: 11, color: '#000' }}>Benchmark Results</div>
        <div style={{ ...T.micro, marginTop: 4 }}>heuristic evaluation · 1 000 boxes / h</div>
      </div>

      <Hr my={20} />

      {/* ── Setup + Heuristics ── */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '0 48px', marginBottom: 8 }}>

        {/* Left: Simulation Setup */}
        <div>
          <SectionTitle>Simulation Setup</SectionTitle>
          {SETUP.map(({ label, value }) => (
            <Kv key={label} label={label} value={value} />
          ))}
          <div style={{ fontSize: 10, color: '#555', lineHeight: 1.6, letterSpacing: '0.02em', marginTop: 12 }}>
            The robot is the component responsible for dispatching pallets. At each decision
            step it observes aggregated statistics about the silo&rsquo;s internal state and applies
            a heuristic to select which destination family to fill next.
          </div>
        </div>

        {/* Right: Heuristics */}
        <div>
          <SectionTitle>Heuristics</SectionTitle>
          {HEURISTICS.map(({ name, strategy }, i) => (
            <div
              key={name}
              style={{
                display: 'grid', gridTemplateColumns: '116px 1fr', gap: 12,
                padding: '8px 0',
                borderTop: i === 0 ? '1px solid #ebebeb' : undefined,
                borderBottom: '1px solid #ebebeb',
                alignItems: 'start',
              }}
            >
              <Code>{name}</Code>
              <div style={{ fontSize: 10, color: '#555', fontFamily: FF, lineHeight: 1.5, letterSpacing: '0.02em' }}>
                {strategy}
              </div>
            </div>
          ))}
        </div>
      </div>

      <Hr my={24} />

      {/* ── Metrics ── */}
      <SectionTitle>Metrics</SectionTitle>
      <div style={{ ...T.micro, color: '#777', marginBottom: 14 }}>
        P = set of dispatched pallets · C = 12 (pallet capacity) · b&#x1D456; = boxes on pallet i · T = total elapsed ticks · F = fully-filled pallets
      </div>
      <div style={{ display: 'flex', gap: 24 }}>
        {METRICS.map(m => (
          <Formula key={m.label} label={m.label} formula={m.formula} note={m.note} />
        ))}
      </div>

      <Hr my={24} />

      {/* ── Key Findings ── */}
      <SectionTitle>Key Findings</SectionTitle>
      <div style={{
        borderLeft: '2px solid #000', paddingLeft: 16, marginBottom: 16,
      }}>
        <Finding>
          <Code>largest_stock</Code>{' '}consistently achieves the best throughput and perfect pallet fill in
          both the 1-robot and 2-robot configurations. Its simplicity — always committing to the family
          with the largest available stock — minimises pallet switches and keeps fill levels high without
          sacrificing speed.
        </Finding>
        <Finding>
          Proximity-aware heuristics (<Code>nearest</Code>, <Code>stock_proximity</Code>, <Code>coop</Code>) trade
          throughput for significantly fewer shuttle moves — roughly <strong>25 % fewer moves</strong> compared
          to <Code>largest_stock</Code>. Fewer moves means less mechanical wear and lower energy consumption,
          which can matter in cost-sensitive or energy-constrained deployments.
        </Finding>
        <Finding>
          Adding a second robot with <Code>largest_stock</Code> reduces <Code>avg ticks/filled_pal</Code> from{' '}
          <strong>72.8 → 50.5</strong> (≈ 31 % speedup) while maintaining perfect fill.
        </Finding>
      </div>

      <div style={{ fontSize: 10, color: '#555', fontFamily: FF, lineHeight: 1.6, letterSpacing: '0.02em', marginBottom: 4 }}>
        <strong>If maximising throughput and pallet completeness is the priority, <Code>largest_stock</Code> (+ 2 robots) is the
        clear choice.</strong> However, if the pallet dispatch rate is not a bottleneck and energy efficiency matters,{' '}
        <Code>stock_proximity</Code> (1 robot) or <Code>coop</Code> (2 robots) are worth considering: they save ~25 % of
        shuttle moves at the cost of slower throughput, while still keeping pallets nearly full.
      </div>

      <Hr my={24} />

      {/* ── Results Table ── */}
      <SectionTitle>Results</SectionTitle>

      {/* Table header */}
      <div style={{
        display: 'grid', gridTemplateColumns: COL, gap: '0 16px',
        paddingBottom: 8, borderBottom: '1px solid #000',
      }}>
        {['Robots', 'Heuristic', 'avg ticks / filled pal', 'avg pct_filled', 'avg avg_cap', 'avg moves / 10³'].map(h => (
          <div key={h} style={{ ...T.label }}>{h}</div>
        ))}
      </div>

      {/* 1-robot group */}
      <div style={{ display: 'grid', gridTemplateColumns: COL, gap: '0 16px' }}>
        <GroupLabel label="1 robot" />
        {rows1.map(r => (
          <ResultRow key={r.heuristic} row={r}
            bestTicksPerPal={bestTicksPerPal} bestPctFilled={bestPctFilled}
            bestAvgCap={bestAvgCap} bestMovesK={bestMovesK}
          />
        ))}
        <GroupLabel label="2 robots" />
        {rows2.map(r => (
          <ResultRow key={r.heuristic} row={r}
            bestTicksPerPal={bestTicksPerPal} bestPctFilled={bestPctFilled}
            bestAvgCap={bestAvgCap} bestMovesK={bestMovesK}
          />
        ))}
      </div>

    </div>
  )
}

function ResultRow({
  row, bestTicksPerPal, bestPctFilled, bestAvgCap, bestMovesK,
}: {
  row: BenchmarkRow
  bestTicksPerPal: number
  bestPctFilled: number
  bestAvgCap: number
  bestMovesK: number
}) {
  const isWinner = row.heuristic === 'largest_stock'
  return (
    <div
      style={{
        display: 'contents',
      }}
    >
      {/* robots */}
      <div style={{
        padding: '9px 0', borderBottom: '1px solid #f4f4f4',
        fontSize: 11, color: '#000', letterSpacing: '0.04em', fontFamily: FF,
        background: isWinner ? '#fafafa' : undefined,
      }}>
        {row.robots}
      </div>
      {/* heuristic */}
      <div style={{
        padding: '9px 0', borderBottom: '1px solid #f4f4f4',
        fontFamily: 'monospace', fontSize: 10, color: '#000',
        background: isWinner ? '#fafafa' : undefined,
      }}>
        {row.heuristic}
      </div>
      {/* ticksPerPal */}
      <div style={{ padding: '9px 0', borderBottom: '1px solid #f4f4f4', background: isWinner ? '#fafafa' : undefined }}>
        <NumCell value={row.ticksPerPal} best={row.ticksPerPal === bestTicksPerPal} fmt={v => v.toFixed(1)} />
      </div>
      {/* pctFilled */}
      <div style={{ padding: '9px 0', borderBottom: '1px solid #f4f4f4', background: isWinner ? '#fafafa' : undefined }}>
        <NumCell value={row.pctFilled} best={row.pctFilled === bestPctFilled} fmt={v => v.toFixed(3)} />
      </div>
      {/* avgCap */}
      <div style={{ padding: '9px 0', borderBottom: '1px solid #f4f4f4', background: isWinner ? '#fafafa' : undefined }}>
        <NumCell value={row.avgCap} best={row.avgCap === bestAvgCap} fmt={v => v.toFixed(3)} />
      </div>
      {/* movesK */}
      <div style={{ padding: '9px 0', borderBottom: '1px solid #f4f4f4', background: isWinner ? '#fafafa' : undefined }}>
        <NumCell value={row.movesK} best={row.movesK === bestMovesK} fmt={v => v.toFixed(1)} />
      </div>
    </div>
  )
}
