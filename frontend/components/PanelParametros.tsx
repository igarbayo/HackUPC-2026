'use client'

import { useState } from 'react'
import { FF, T } from '@/lib/tokens'
import Hr from './ui/Hr'
import { useSimulationStream } from '@/lib/SimulationStreamContext'

// ── Types ─────────────────────────────────────────────────

interface DemandRow {
  from_tick: number
  to_tick: number
  rate_multiplier: number
}

interface Weight {
  name: string
  value: number
}

interface Params {
  num_boxes: number
  num_destinations: number
  seed: number
  mean_inter_arrival_ticks: number
  std_inter_arrival_ticks: number
  demand_profile: DemandRow[]
  weights: Weight[]
}

type Preset = 'basic' | 'advanced' | 'custom'

// ── Presets ───────────────────────────────────────────────

const DEMAND_DEFAULT: DemandRow[] = [
  { from_tick: 0,    to_tick: 3600,  rate_multiplier: 1.0 },
  { from_tick: 3600, to_tick: 3960,  rate_multiplier: 5.0 },
  { from_tick: 3960, to_tick: 99999, rate_multiplier: 1.0 },
]

const PRESET_BASIC: Params = {
  num_boxes: 100,
  num_destinations: 5,
  seed: 42,
  mean_inter_arrival_ticks: 3.6,
  std_inter_arrival_ticks: 0.072,
  demand_profile: DEMAND_DEFAULT,
  weights: [
    { name: 'zara_es',         value: 8.0 },
    { name: 'zara_fr',         value: 6.0 },
    { name: 'bershka_es',      value: 3.5 },
    { name: 'stradivarius_es', value: 3.0 },
    { name: 'pull_bear_es',    value: 2.5 },
  ],
}

const PRESET_ADVANCED: Params = {
  num_boxes: 500,
  num_destinations: 23,
  seed: 42,
  mean_inter_arrival_ticks: 3.6,
  std_inter_arrival_ticks: 0.072,
  demand_profile: DEMAND_DEFAULT,
  weights: [
    { name: 'zara_es',          value: 8.0 },
    { name: 'zara_fr',          value: 6.0 },
    { name: 'zara_de',          value: 5.0 },
    { name: 'zara_uk',          value: 5.0 },
    { name: 'zara_it',          value: 3.5 },
    { name: 'zara_pt',          value: 2.5 },
    { name: 'bershka_es',       value: 3.5 },
    { name: 'bershka_fr',       value: 2.5 },
    { name: 'bershka_de',       value: 2.0 },
    { name: 'bershka_uk',       value: 2.0 },
    { name: 'stradivarius_es',  value: 3.0 },
    { name: 'stradivarius_fr',  value: 2.0 },
    { name: 'stradivarius_de',  value: 1.5 },
    { name: 'pull_bear_es',     value: 2.5 },
    { name: 'pull_bear_fr',     value: 1.5 },
    { name: 'pull_bear_de',     value: 1.0 },
    { name: 'massimo_dutti_es', value: 1.5 },
    { name: 'massimo_dutti_fr', value: 1.0 },
    { name: 'oysho_es',         value: 1.0 },
    { name: 'oysho_fr',         value: 0.7 },
    { name: 'zara_home_es',     value: 1.2 },
    { name: 'zara_home_fr',     value: 0.8 },
    { name: 'lefties_es',       value: 1.0 },
  ],
}

// ── Sub-components ────────────────────────────────────────

const inputBase: React.CSSProperties = {
  border: 'none', borderBottom: '1px solid #e0e0e0',
  padding: '4px 0', fontSize: 11, fontFamily: FF,
  outline: 'none', letterSpacing: '0.02em', background: 'transparent',
  color: '#000', width: '100%',
}

function NumInput({
  value, onChange, step = 1, min,
}: {
  value: number
  onChange: (v: number) => void
  step?: number
  min?: number
}) {
  return (
    <input
      type="number"
      value={value}
      step={step}
      min={min}
      onChange={e => onChange(Number(e.target.value))}
      style={inputBase}
    />
  )
}

function Field({ label, children }: { label: string; children: React.ReactNode }) {
  return (
    <div>
      <div style={{ ...T.label, marginBottom: 5 }}>{label}</div>
      {children}
    </div>
  )
}

// ── Main component ────────────────────────────────────────

const API_URL = process.env.NEXT_PUBLIC_API_URL ?? 'http://localhost:8000'

export default function PanelParametros() {
  const { startStream }             = useSimulationStream()
  const [preset, setPreset]         = useState<Preset>('basic')
  const [params, setParams]         = useState<Params>({ ...PRESET_BASIC, demand_profile: [...DEMAND_DEFAULT], weights: [...PRESET_BASIC.weights] })
  const [status, setStatus]         = useState<'idle' | 'loading' | 'ok' | 'error'>('idle')
  const [ticksPerSecond, setTicksPerSecond] = useState(1)

  function applyPreset(p: Preset) {
    if (p === 'basic')    setParams({ ...PRESET_BASIC,    demand_profile: [...DEMAND_DEFAULT], weights: [...PRESET_BASIC.weights] })
    if (p === 'advanced') setParams({ ...PRESET_ADVANCED, demand_profile: [...DEMAND_DEFAULT], weights: [...PRESET_ADVANCED.weights] })
    setPreset(p)
  }

  function patch<K extends keyof Params>(key: K, val: Params[K]) {
    setParams(p => ({ ...p, [key]: val }))
    setPreset('custom')
  }

  function patchDemand(i: number, field: keyof DemandRow, val: number) {
    const next = params.demand_profile.map((r, idx) => idx === i ? { ...r, [field]: val } : r)
    patch('demand_profile', next)
  }
  function addDemandRow() {
    patch('demand_profile', [...params.demand_profile, { from_tick: 0, to_tick: 99999, rate_multiplier: 1.0 }])
  }
  function removeDemandRow(i: number) {
    patch('demand_profile', params.demand_profile.filter((_, idx) => idx !== i))
  }

  function patchWeight(i: number, field: keyof Weight, val: string | number) {
    const next = params.weights.map((w, idx) => idx === i ? { ...w, [field]: val } : w)
    patch('weights', next)
  }
  function addWeight() {
    patch('weights', [...params.weights, { name: '', value: 1.0 }])
  }
  function removeWeight(i: number) {
    patch('weights', params.weights.filter((_, idx) => idx !== i))
  }

  async function handleApply() {
    setStatus('loading')
    try {
      const res = await fetch(`${API_URL}/simulations`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          generator: {
            ...params,
            weights: Object.fromEntries(params.weights.map(w => [w.name, w.value])),
          },
        }),
      })
      if (!res.ok) throw new Error(await res.text())
      const data = await res.json()
      localStorage.setItem(`silos_tps_${data.sim_id}`, String(ticksPerSecond))
      startStream(data.sim_id, ticksPerSecond)
      setStatus('ok')
      setTimeout(() => setStatus('idle'), 2000)
    } catch {
      setStatus('error')
      setTimeout(() => setStatus('idle'), 2000)
    }
  }

  const PRESETS: { id: Preset; label: string; sub: string }[] = [
    { id: 'basic',    label: 'Basic',    sub: '100 boxes · 5 destinations'  },
    { id: 'advanced', label: 'Advanced', sub: '500 boxes · 23 destinations' },
    { id: 'custom',   label: 'Custom',   sub: 'Manual configuration'        },
  ]

  return (
    <div style={{ padding: '28px 24px', fontFamily: FF }}>
      <div style={{ ...T.section, marginBottom: 24, color: '#000' }}>Parameters</div>

      {/* ── Preset selector ── */}
      <div style={{ ...T.label, marginBottom: 12 }}>Presets</div>
      {PRESETS.map(({ id, label, sub }) => (
        <div
          key={id}
          onClick={() => id !== 'custom' && applyPreset(id)}
          style={{
            display: 'flex', alignItems: 'center', gap: 10,
            padding: '9px 0', borderBottom: '1px solid #f4f4f4',
            cursor: id !== 'custom' ? 'pointer' : 'default',
          }}
        >
          <div style={{
            width: 6, height: 6, borderRadius: '50%',
            background: preset === id ? '#000' : 'transparent',
            border: '1px solid ' + (preset === id ? '#000' : '#ccc'),
            flexShrink: 0, transition: 'all 0.15s',
          }} />
          <div>
            <span style={{ fontSize: 11, color: preset === id ? '#000' : '#888', letterSpacing: '0.04em' }}>
              {label}
            </span>
            <span style={{ fontSize: 9, color: '#bbb', letterSpacing: '0.06em', marginLeft: 8 }}>
              {sub}
            </span>
          </div>
        </div>
      ))}

      <Hr my={24} />

      {/* ── General parameters ── */}
      <div style={{ ...T.label, color: '#000', marginBottom: 16 }}>General parameters</div>
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(180px, 1fr))', gap: '16px 32px', marginBottom: 4 }}>
        <Field label="Number of boxes">
          <NumInput value={params.num_boxes} min={1} onChange={v => patch('num_boxes', v)} />
        </Field>
        <Field label="Destinations">
          <NumInput value={params.num_destinations} min={1} onChange={v => patch('num_destinations', v)} />
        </Field>
        <Field label="Seed">
          <NumInput value={params.seed} min={0} onChange={v => patch('seed', v)} />
        </Field>
        <Field label="Mean arrival (ticks)">
          <NumInput value={params.mean_inter_arrival_ticks} step={0.1} min={0.1} onChange={v => patch('mean_inter_arrival_ticks', v)} />
        </Field>
        <Field label="Arrival std dev (ticks)">
          <NumInput value={params.std_inter_arrival_ticks} step={0.001} min={0} onChange={v => patch('std_inter_arrival_ticks', v)} />
        </Field>
        <Field label="Playback speed (ticks/s)">
          <NumInput value={ticksPerSecond} step={0.5} min={0.5} onChange={setTicksPerSecond} />
        </Field>
      </div>

      <Hr my={24} />

      {/* ── Demand profile ── */}
      <div style={{ ...T.label, color: '#000', marginBottom: 16 }}>Demand profile</div>

      <div style={{
        display: 'grid', gridTemplateColumns: '1fr 1fr 1fr 24px',
        gap: 12, marginBottom: 8,
      }}>
        {['From (tick)', 'To (tick)', 'Multiplier', ''].map(h => (
          <div key={h} style={{ ...T.label }}>{h}</div>
        ))}
      </div>

      {params.demand_profile.map((row, i) => (
        <div key={i} style={{
          display: 'grid', gridTemplateColumns: '1fr 1fr 1fr 24px',
          gap: 12, marginBottom: 10, alignItems: 'end',
        }}>
          <NumInput value={row.from_tick}       min={0} onChange={v => patchDemand(i, 'from_tick', v)} />
          <NumInput value={row.to_tick}         min={0} onChange={v => patchDemand(i, 'to_tick', v)} />
          <NumInput value={row.rate_multiplier} step={0.1} min={0} onChange={v => patchDemand(i, 'rate_multiplier', v)} />
          <span
            onClick={() => removeDemandRow(i)}
            style={{ fontSize: 14, color: '#ccc', cursor: 'pointer', lineHeight: 1, paddingBottom: 4, userSelect: 'none' }}
          >×</span>
        </div>
      ))}

      <button
        onClick={addDemandRow}
        style={{
          marginTop: 4, padding: '6px 14px',
          border: '1px solid #e0e0e0', background: 'transparent',
          fontSize: 9, letterSpacing: '0.14em', textTransform: 'uppercase',
          fontFamily: FF, color: '#888', cursor: 'pointer',
        }}
      >
        + Add segment
      </button>

      <Hr my={24} />

      {/* ── Destination weights ── */}
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'baseline', marginBottom: 16 }}>
        <div style={{ ...T.label, color: '#000' }}>Destination weights</div>
        <span style={{ ...T.micro }}>{params.weights.length} destinations</span>
      </div>

      <div style={{
        display: 'grid',
        gridTemplateColumns: 'repeat(auto-fill, minmax(280px, 1fr))',
        gap: '0 32px',
      }}>
        {params.weights.map((w, i) => (
          <div key={i} style={{
            display: 'grid', gridTemplateColumns: '1fr 72px 20px',
            gap: 10, marginBottom: 10, alignItems: 'end',
          }}>
            <div>
              {i === 0 && <div style={{ ...T.label, marginBottom: 5 }}>Destination</div>}
              <input
                type="text"
                value={w.name}
                onChange={e => patchWeight(i, 'name', e.target.value)}
                placeholder="destination_id"
                style={{ ...inputBase }}
              />
            </div>
            <div>
              {i === 0 && <div style={{ ...T.label, marginBottom: 5 }}>Weight</div>}
              <NumInput value={w.value} step={0.1} min={0} onChange={v => patchWeight(i, 'value', v)} />
            </div>
            <span
              onClick={() => removeWeight(i)}
              style={{ fontSize: 14, color: '#ccc', cursor: 'pointer', lineHeight: 1, paddingBottom: 4, userSelect: 'none' }}
            >×</span>
          </div>
        ))}
      </div>

      <button
        onClick={addWeight}
        style={{
          marginTop: 4, padding: '6px 14px',
          border: '1px solid #e0e0e0', background: 'transparent',
          fontSize: 9, letterSpacing: '0.14em', textTransform: 'uppercase',
          fontFamily: FF, color: '#888', cursor: 'pointer',
        }}
      >
        + Add destination
      </button>

      <Hr my={24} />

      {/* ── Apply ── */}
      <button
        onClick={handleApply}
        style={{
          width: '100%', padding: '10px 0',
          background: status === 'ok' ? '#22c55e' : status === 'error' ? '#ef4444' : '#000',
          color: '#fff',
          border: 'none', fontSize: 9, letterSpacing: '0.18em',
          textTransform: 'uppercase', fontFamily: FF,
          cursor: status === 'loading' ? 'not-allowed' : 'pointer',
          transition: 'background 0.3s',
        }}
        disabled={status === 'loading'}
      >
        {status === 'loading' ? 'Launching...' : status === 'ok' ? '✓ Simulation launched' : status === 'error' ? '✗ Error' : 'Apply configuration'}
      </button>

      <Hr my={20} />
      <div style={T.micro}>v1.0 · SILOS Input Generator</div>
    </div>
  )
}
