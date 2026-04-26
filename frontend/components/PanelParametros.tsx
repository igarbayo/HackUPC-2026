'use client'

import { useState, useRef } from 'react'
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

type PresetKey = 'basic' | 'advanced'
type Preset = PresetKey | 'custom'

interface CustomScalars {
  num_boxes: string
  num_destinations: string
  seed: string
  mean_inter_arrival_ticks: string
  std_inter_arrival_ticks: string
  ticks_per_second: string
  // warehouse structure
  num_aisles: string
  num_sides: string
  num_slots: string
  num_y: string
  num_robots: string
}

const STRUCTURE_DEFAULTS = {
  num_aisles: 1,
  num_sides:  2,
  num_slots:  10,
  num_y:      2,
  num_robots: 1,
} as const

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

const PRESETS_MAP: Record<PresetKey, Params> = { basic: PRESET_BASIC, advanced: PRESET_ADVANCED }

const EMPTY_SCALARS: CustomScalars = {
  num_boxes: '', num_destinations: '', seed: '',
  mean_inter_arrival_ticks: '', std_inter_arrival_ticks: '', ticks_per_second: '',
  num_aisles: '', num_sides: '', num_slots: '', num_y: '', num_robots: '',
}

// ── Inditex master destinations list ─────────────────────
// Ordered by market importance; used to auto-populate weights in custom mode.

const INDITEX_DESTINATIONS: Weight[] = [
  { name: 'zara_es',            value: 8.0 },
  { name: 'zara_fr',            value: 6.0 },
  { name: 'zara_de',            value: 5.0 },
  { name: 'zara_uk',            value: 5.0 },
  { name: 'zara_us',            value: 4.0 },
  { name: 'zara_it',            value: 3.5 },
  { name: 'bershka_es',         value: 3.5 },
  { name: 'zara_cn',            value: 3.0 },
  { name: 'stradivarius_es',    value: 3.0 },
  { name: 'zara_pt',            value: 2.5 },
  { name: 'pull_bear_es',       value: 2.5 },
  { name: 'bershka_fr',         value: 2.5 },
  { name: 'zara_pl',            value: 2.0 },
  { name: 'zara_mx',            value: 2.0 },
  { name: 'bershka_de',         value: 2.0 },
  { name: 'bershka_uk',         value: 2.0 },
  { name: 'stradivarius_fr',    value: 2.0 },
  { name: 'massimo_dutti_es',   value: 1.5 },
  { name: 'stradivarius_de',    value: 1.5 },
  { name: 'pull_bear_fr',       value: 1.5 },
  { name: 'bershka_it',         value: 1.5 },
  { name: 'zara_tr',            value: 1.2 },
  { name: 'zara_home_es',       value: 1.2 },
  { name: 'stradivarius_it',    value: 1.2 },
  { name: 'zara_jp',            value: 1.0 },
  { name: 'lefties_es',         value: 1.0 },
  { name: 'oysho_es',           value: 1.0 },
  { name: 'massimo_dutti_fr',   value: 1.0 },
  { name: 'pull_bear_de',       value: 1.0 },
  { name: 'pull_bear_uk',       value: 1.0 },
  { name: 'bershka_pt',         value: 1.0 },
  { name: 'zara_ro',            value: 0.9 },
  { name: 'zara_br',            value: 0.9 },
  { name: 'stradivarius_pt',    value: 0.9 },
  { name: 'massimo_dutti_uk',   value: 0.8 },
  { name: 'massimo_dutti_de',   value: 0.8 },
  { name: 'zara_home_fr',       value: 0.8 },
  { name: 'zara_kr',            value: 0.8 },
  { name: 'oysho_fr',           value: 0.7 },
  { name: 'lefties_pt',         value: 0.7 },
  { name: 'bershka_ro',         value: 0.7 },
  { name: 'massimo_dutti_it',   value: 0.7 },
  { name: 'zara_ar',            value: 0.6 },
  { name: 'pull_bear_pt',       value: 0.6 },
  { name: 'zara_home_de',       value: 0.6 },
  { name: 'zara_home_uk',       value: 0.6 },
  { name: 'lefties_mx',         value: 0.5 },
  { name: 'oysho_pt',           value: 0.5 },
  { name: 'oysho_it',           value: 0.5 },
  { name: 'zara_home_it',       value: 0.5 },
]

function syncWeightsToCount(count: number, current: Weight[]): Weight[] {
  const n = Math.max(1, Math.min(count, INDITEX_DESTINATIONS.length))
  if (n <= current.length) return current.slice(0, n)
  const existing = new Set(current.map(w => w.name))
  const additions = INDITEX_DESTINATIONS
    .filter(d => !existing.has(d.name))
    .slice(0, n - current.length)
  return [...current, ...additions]
}

// ── Sub-components ────────────────────────────────────────

const inputBase: React.CSSProperties = {
  border: 'none', borderBottom: '1px solid #e0e0e0',
  padding: '4px 0', fontSize: 11, fontFamily: FF,
  outline: 'none', letterSpacing: '0.02em', background: 'transparent',
  color: '#000', width: '100%',
}

const disabledStyle: React.CSSProperties = {
  color: '#777', borderBottomColor: '#f0f0f0', cursor: 'default',
}

function NumInput({
  value, placeholder, onChange, step, min, max, disabled,
}: {
  value: string
  placeholder?: string
  onChange: (v: string) => void
  step?: number
  min?: number
  max?: number
  disabled?: boolean
}) {
  return (
    <input
      type="number"
      value={value}
      placeholder={placeholder}
      step={step}
      min={min}
      max={max}
      disabled={disabled}
      onChange={e => onChange(e.target.value)}
      style={{ ...inputBase, ...(disabled ? disabledStyle : {}) }}
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
  const { startStream } = useSimulationStream()
  const [preset, setPreset]             = useState<Preset>('basic')
  const [lastPresetKey, setLastPresetKey] = useState<PresetKey>('basic')

  const [customScalars, setCustomScalars] = useState<CustomScalars>({ ...EMPTY_SCALARS })
  const [customProfile, setCustomProfile] = useState<DemandRow[]>([...DEMAND_DEFAULT])
  const [customWeights, setCustomWeights] = useState<Weight[]>([...PRESET_BASIC.weights])

  const [status, setStatus] = useState<'idle' | 'loading' | 'ok' | 'error'>('idle')
  const [csvId, setCsvId]           = useState<string | null>(null)
  const [csvFileName, setCsvFileName] = useState<string | null>(null)
  const fileInputRef = useRef<HTMLInputElement>(null)

  const isCustom = preset === 'custom'
  const base = PRESETS_MAP[lastPresetKey]

  const csvLocked = csvId !== null
  const CSV_TOPO_FIELDS = new Set(['num_aisles', 'num_sides', 'num_slots', 'num_y'])

  function parseCsvClientSide(text: string) {
    const lines = text.trim().split('\n').slice(1) // skip header
    const boxes: { aisle: number; side: number; x: number; y: number; z: number; id: string; destination: string }[] = []
    let maxAisle = 0, maxSide = 0, maxX = 0, maxY = 0
    for (const line of lines) {
      const [posicion, etiqueta] = line.split(',').map(s => s.trim())
      if (!posicion || posicion.length !== 11) continue
      const aisle = parseInt(posicion.slice(0, 2), 10)
      const side  = parseInt(posicion.slice(2, 4), 10)
      const x     = parseInt(posicion.slice(4, 7), 10)
      const y     = parseInt(posicion.slice(7, 9), 10)
      const z     = parseInt(posicion.slice(9, 11), 10)
      maxAisle = Math.max(maxAisle, aisle)
      maxSide  = Math.max(maxSide, side)
      maxX     = Math.max(maxX, x)
      maxY     = Math.max(maxY, y)
      if (etiqueta && etiqueta.length >= 15) {
        boxes.push({ aisle, side, x, y, z, id: etiqueta, destination: etiqueta.slice(7, 15) })
      }
    }
    const topology = { num_aisles: maxAisle, num_sides: maxSide, num_slots: maxX, num_y: maxY }
    const byAisle: Record<number, typeof boxes> = {}
    for (const b of boxes) { (byAisle[b.aisle] ??= []).push(b) }
    const aisleStats = Object.fromEntries(
      Object.entries(byAisle).map(([a, bs]) => [
        `aisle_${a}`,
        { total_boxes: bs.length, sample: bs.slice(0, 3) },
      ])
    )
    console.group(`[SILOS] CSV parsed — ${boxes.length} pre-placed boxes`)
    console.log('Topology inferred:', topology)
    console.log('Boxes per aisle:', aisleStats)
    console.log('Full box list (first 10):', boxes.slice(0, 10))
    console.groupEnd()
    return topology
  }

  async function handleCsvUpload(e: React.ChangeEvent<HTMLInputElement>) {
    const file = e.target.files?.[0]
    if (!file) return
    e.target.value = ''
    const text = await file.text()
    parseCsvClientSide(text)
    const formData = new FormData()
    formData.append('file', file)
    try {
      const res = await fetch(`${API_URL}/simulations/parse-csv`, { method: 'POST', body: formData })
      if (!res.ok) throw new Error()
      const data = await res.json()
      setCsvId(data.csv_id)
      setCsvFileName(file.name)
      setCustomScalars(s => ({
        ...s,
        num_aisles: String(data.num_aisles),
        num_sides:  String(data.num_sides),
        num_slots:  String(data.num_slots),
        num_y:      String(data.num_y),
      }))
    } catch {
      setCsvId(null)
      setCsvFileName(null)
    }
  }

  function clearCsv() {
    setCsvId(null)
    setCsvFileName(null)
    setCustomScalars(s => ({ ...s, num_aisles: '', num_sides: '', num_slots: '', num_y: '' }))
  }

  function selectPreset(p: Preset) {
    if (p === 'basic' || p === 'advanced') {
      setPreset(p)
      setLastPresetKey(p)
    } else {
      setCustomScalars({ ...EMPTY_SCALARS })
      const b = PRESETS_MAP[lastPresetKey]
      setCustomProfile(b.demand_profile.map(r => ({ ...r })))
      setCustomWeights(INDITEX_DESTINATIONS.slice(0, b.num_destinations).map(w => ({ ...w })))
      setPreset('custom')
    }
  }

  // ── Weight quick-set actions ──────────────────────────────

  function handleUniform() {
    setCustomWeights(prev => prev.map(w => ({ ...w, value: 1.0 })))
  }
  function handleRandom() {
    setCustomWeights(prev => prev.map(w => ({
      ...w, value: Number((0.3 + Math.random() * 7.7).toFixed(1)),
    })))
  }
  function handleDefaultWeights() {
    const n = customScalars.num_destinations
      ? Number(customScalars.num_destinations)
      : base.num_destinations
    setCustomWeights(INDITEX_DESTINATIONS.slice(0, Math.max(1, n)).map(w => ({ ...w })))
  }

  // ── Demand profile helpers (custom only) ──────────────────

  function patchDemand(i: number, field: keyof DemandRow, val: string) {
    setCustomProfile(prev => prev.map((r, idx) =>
      idx === i ? { ...r, [field]: val === '' ? 0 : Number(val) } : r
    ))
  }
  function addDemandRow() {
    setCustomProfile(prev => [...prev, { from_tick: 0, to_tick: 99999, rate_multiplier: 1.0 }])
  }
  function removeDemandRow(i: number) {
    setCustomProfile(prev => prev.filter((_, idx) => idx !== i))
  }

  // ── Weight helpers (custom only) ──────────────────────────

  function patchWeightName(i: number, val: string) {
    setCustomWeights(prev => prev.map((w, idx) => idx === i ? { ...w, name: val } : w))
  }
  function patchWeightValue(i: number, val: string) {
    setCustomWeights(prev => prev.map((w, idx) =>
      idx === i ? { ...w, value: val === '' ? 0 : Number(val) } : w
    ))
  }
  function addWeight() {
    setCustomWeights(prev => [...prev, { name: '', value: 1.0 }])
  }
  function removeWeight(i: number) {
    setCustomWeights(prev => prev.filter((_, idx) => idx !== i))
  }

  // ── Resolve final params for submission ───────────────────

  function resolveStructure() {
    const s = customScalars
    const D = STRUCTURE_DEFAULTS
    return {
      num_aisles: s.num_aisles ? Number(s.num_aisles) : D.num_aisles,
      num_sides:  s.num_sides  ? Number(s.num_sides)  : D.num_sides,
      num_slots:  s.num_slots  ? Number(s.num_slots)  : D.num_slots,
      num_y:      s.num_y      ? Number(s.num_y)      : D.num_y,
      num_robots: s.num_robots ? Number(s.num_robots) : D.num_robots,
    }
  }

  function resolveParams(): Params & { ticks_per_second: number } {
    if (!isCustom) {
      return { ...PRESETS_MAP[preset as PresetKey], ticks_per_second: 1 }
    }
    const s = customScalars
    return {
      num_boxes:                s.num_boxes                ? Number(s.num_boxes)                : base.num_boxes,
      num_destinations:         s.num_destinations         ? Number(s.num_destinations)         : base.num_destinations,
      seed:                     s.seed                     ? Number(s.seed)                     : base.seed,
      mean_inter_arrival_ticks: s.mean_inter_arrival_ticks ? Number(s.mean_inter_arrival_ticks) : base.mean_inter_arrival_ticks,
      std_inter_arrival_ticks:  s.std_inter_arrival_ticks  ? Number(s.std_inter_arrival_ticks)  : base.std_inter_arrival_ticks,
      demand_profile: customProfile,
      weights: customWeights,
      ticks_per_second: s.ticks_per_second ? Number(s.ticks_per_second) : 1,
    }
  }

  async function handleApply() {
    setStatus('loading')
    const resolved = resolveParams()
    const structure = isCustom ? resolveStructure() : STRUCTURE_DEFAULTS
    const { ticks_per_second, ...genParams } = resolved
    try {
      const res = await fetch(`${API_URL}/simulations`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          ...structure,
          ...(csvId ? { csv_id: csvId } : {}),
          generator: {
            ...genParams,
            weights: Object.fromEntries(genParams.weights.map(w => [w.name, w.value])),
          },
        }),
      })
      if (!res.ok) throw new Error(await res.text())
      const data = await res.json()
      localStorage.setItem(`silos_tps_${data.sim_id}`, String(ticks_per_second))
      startStream(data.sim_id, ticks_per_second)
      setStatus('ok')
      setTimeout(() => setStatus('idle'), 2000)
    } catch {
      setStatus('error')
      setTimeout(() => setStatus('idle'), 2000)
    }
  }

  // ── Helpers for rendering scalar inputs ───────────────────

  function structNum(
    field: 'num_aisles' | 'num_sides' | 'num_slots' | 'num_y' | 'num_robots',
    min: number,
    max: number,
  ) {
    const def = STRUCTURE_DEFAULTS[field]
    const locked = csvLocked && CSV_TOPO_FIELDS.has(field)
    return (
      <NumInput
        value={isCustom ? customScalars[field] : String(def)}
        placeholder={isCustom && !locked ? String(def) : undefined}
        step={1}
        min={min}
        max={max}
        disabled={!isCustom || locked}
        onChange={v => setCustomScalars(s => ({ ...s, [field]: v }))}
      />
    )
  }

  function scalarNum(
    field: 'num_boxes' | 'num_destinations' | 'seed' | 'mean_inter_arrival_ticks' | 'std_inter_arrival_ticks',
    presetValue: number,
    step?: number,
    min?: number,
  ) {
    return (
      <NumInput
        value={isCustom ? customScalars[field] : String(presetValue)}
        placeholder={isCustom ? String(presetValue) : undefined}
        step={step}
        min={min}
        disabled={!isCustom}
        onChange={v => setCustomScalars(s => ({ ...s, [field]: v }))}
      />
    )
  }

  // Display source for demand/weights when not in custom mode
  const displayProfile = isCustom ? customProfile : base.demand_profile
  const displayWeights = isCustom ? customWeights : base.weights

  const PRESETS_UI: { id: Preset; label: string; sub: string }[] = [
    { id: 'basic',    label: 'Basic',    sub: '100 boxes · 5 destinations'  },
    { id: 'advanced', label: 'Advanced', sub: '500 boxes · 23 destinations' },
    { id: 'custom',   label: 'Custom',   sub: 'Manual configuration'        },
  ]

  return (
    <div style={{ padding: '28px 24px', fontFamily: FF }}>
      <div style={{ ...T.section, marginBottom: 24, color: '#000' }}>Parameters</div>

      {/* ── Preset selector ── */}
      <div style={{ ...T.label, marginBottom: 12 }}>Presets</div>
      {PRESETS_UI.map(({ id, label, sub }) => (
        <div
          key={id}
          onClick={() => selectPreset(id)}
          style={{
            display: 'flex', alignItems: 'center', gap: 10,
            padding: '9px 0', borderBottom: '1px solid #f4f4f4',
            cursor: 'pointer',
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
            <span style={{ fontSize: 9, color: '#666', letterSpacing: '0.06em', marginLeft: 8 }}>
              {sub}
            </span>
          </div>
        </div>
      ))}

      <Hr my={24} />

      {/* ── Warehouse structure ── */}
      <div style={{ display: 'flex', alignItems: 'baseline', justifyContent: 'space-between', marginBottom: 16 }}>
        <div style={{ ...T.label, color: '#000' }}>Warehouse structure</div>
        {isCustom && (
          csvFileName ? (
            <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
              <span style={{ fontSize: 9, color: '#22c55e', letterSpacing: '0.08em', fontFamily: FF }}>
                ✓ {csvFileName}
              </span>
              <span
                onClick={clearCsv}
                style={{ fontSize: 13, color: '#aaa', cursor: 'pointer', lineHeight: 1, userSelect: 'none' }}
              >×</span>
            </div>
          ) : (
            <button
              onClick={() => fileInputRef.current?.click()}
              style={{
                padding: '4px 10px',
                border: '1px solid #e0e0e0', background: 'transparent',
                fontSize: 9, letterSpacing: '0.12em', textTransform: 'uppercase',
                fontFamily: FF, color: '#555', cursor: 'pointer',
              }}
            >
              Upload CSV
            </button>
          )
        )}
        <input ref={fileInputRef} type="file" accept=".csv" style={{ display: 'none' }} onChange={handleCsvUpload} />
      </div>
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(150px, 1fr))', gap: '16px 32px', marginBottom: 4 }}>
        <Field label="Aisles (1–4)">{structNum('num_aisles', 1, 4)}</Field>
        <Field label="Sides (1–2)">{structNum('num_sides',  1, 2)}</Field>
        <Field label="Slots / X (1–60)">{structNum('num_slots',  1, 60)}</Field>
        <Field label="Levels / Y (1–8)">{structNum('num_y',     1, 8)}</Field>
        <Field label="Robots (1–2)">{structNum('num_robots', 1, 2)}</Field>
      </div>

      <Hr my={24} />

      {/* ── General parameters ── */}
      <div style={{ ...T.label, color: '#000', marginBottom: 16 }}>General parameters</div>
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(180px, 1fr))', gap: '16px 32px', marginBottom: 4 }}>
        <Field label="Number of boxes">
          {scalarNum('num_boxes', base.num_boxes, 1, 1)}
        </Field>
        <Field label="Destinations">
          <NumInput
            value={isCustom ? customScalars.num_destinations : String(base.num_destinations)}
            placeholder={isCustom ? String(base.num_destinations) : undefined}
            step={1}
            min={1}
            disabled={!isCustom}
            onChange={v => {
              setCustomScalars(s => ({ ...s, num_destinations: v }))
              const n = v === '' ? base.num_destinations : Number(v)
              if (!isNaN(n) && n > 0) setCustomWeights(prev => syncWeightsToCount(n, prev))
            }}
          />
        </Field>
        <Field label="Seed">
          {scalarNum('seed', base.seed, 1, 0)}
        </Field>
        <Field label="Mean arrival (ticks)">
          {scalarNum('mean_inter_arrival_ticks', base.mean_inter_arrival_ticks, 0.1, 0.1)}
        </Field>
        <Field label="Arrival std dev (ticks)">
          {scalarNum('std_inter_arrival_ticks', base.std_inter_arrival_ticks, 0.001, 0)}
        </Field>
        <Field label="Playback speed (ticks/s)">
          <NumInput
            value={isCustom ? customScalars.ticks_per_second : '1'}
            placeholder={isCustom ? '1' : undefined}
            step={0.5}
            min={0.5}
            disabled={!isCustom}
            onChange={v => setCustomScalars(s => ({ ...s, ticks_per_second: v }))}
          />
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

      {displayProfile.map((row, i) => (
        <div key={i} style={{
          display: 'grid', gridTemplateColumns: '1fr 1fr 1fr 24px',
          gap: 12, marginBottom: 10, alignItems: 'end',
        }}>
          <NumInput
            value={String(row.from_tick)}
            min={0}
            disabled={!isCustom}
            onChange={v => patchDemand(i, 'from_tick', v)}
          />
          <NumInput
            value={String(row.to_tick)}
            min={0}
            disabled={!isCustom}
            onChange={v => patchDemand(i, 'to_tick', v)}
          />
          <NumInput
            value={String(row.rate_multiplier)}
            step={0.1}
            min={0}
            disabled={!isCustom}
            onChange={v => patchDemand(i, 'rate_multiplier', v)}
          />
          {isCustom ? (
            <span
              onClick={() => removeDemandRow(i)}
              style={{ fontSize: 14, color: '#888', cursor: 'pointer', lineHeight: 1, paddingBottom: 4, userSelect: 'none' }}
            >×</span>
          ) : <span />}
        </div>
      ))}

      {isCustom && (
        <button
          onClick={addDemandRow}
          style={{
            marginTop: 4, padding: '6px 14px',
            border: '1px solid #e0e0e0', background: 'transparent',
            fontSize: 9, letterSpacing: '0.14em', textTransform: 'uppercase',
            fontFamily: FF, color: '#555', cursor: 'pointer',
          }}
        >
          + Add segment
        </button>
      )}

      <Hr my={24} />

      {/* ── Destination weights ── */}
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'baseline', marginBottom: isCustom ? 10 : 16 }}>
        <div style={{ ...T.label, color: '#000' }}>Destination weights</div>
        <span style={{ ...T.micro }}>{displayWeights.length} destinations</span>
      </div>

      {isCustom && (
        <div style={{ display: 'flex', gap: 6, marginBottom: 16 }}>
          {([
            { label: 'Uniform', onClick: handleUniform },
            { label: 'Random',  onClick: handleRandom  },
            { label: 'Default', onClick: handleDefaultWeights },
          ] as const).map(({ label, onClick }) => (
            <button
              key={label}
              onClick={onClick}
              style={{
                padding: '4px 10px',
                border: '1px solid #e0e0e0', background: 'transparent',
                fontSize: 9, letterSpacing: '0.12em', textTransform: 'uppercase',
                fontFamily: FF, color: '#555', cursor: 'pointer',
              }}
            >
              {label}
            </button>
          ))}
        </div>
      )}

      <div style={{
        display: 'grid',
        gridTemplateColumns: 'repeat(auto-fill, minmax(280px, 1fr))',
        gap: '0 32px',
      }}>
        {displayWeights.map((w, i) => (
          <div key={i} style={{
            display: 'grid', gridTemplateColumns: '1fr 72px 20px',
            gap: 10, marginBottom: 10, alignItems: 'end',
          }}>
            <div>
              {i === 0 && <div style={{ ...T.label, marginBottom: 5 }}>Destination</div>}
              <input
                type="text"
                value={w.name}
                onChange={e => patchWeightName(i, e.target.value)}
                placeholder="destination_id"
                disabled={!isCustom}
                style={{ ...inputBase, ...(isCustom ? {} : disabledStyle) }}
              />
            </div>
            <div>
              {i === 0 && <div style={{ ...T.label, marginBottom: 5 }}>Weight</div>}
              <NumInput
                value={String(w.value)}
                step={0.1}
                min={0}
                disabled={!isCustom}
                onChange={v => patchWeightValue(i, v)}
              />
            </div>
            {isCustom ? (
              <span
                onClick={() => removeWeight(i)}
                style={{ fontSize: 14, color: '#888', cursor: 'pointer', lineHeight: 1, paddingBottom: 4, userSelect: 'none' }}
              >×</span>
            ) : <span />}
          </div>
        ))}
      </div>

      {isCustom && (
        <button
          onClick={addWeight}
          style={{
            marginTop: 4, padding: '6px 14px',
            border: '1px solid #e0e0e0', background: 'transparent',
            fontSize: 9, letterSpacing: '0.14em', textTransform: 'uppercase',
            fontFamily: FF, color: '#555', cursor: 'pointer',
          }}
        >
          + Add destination
        </button>
      )}

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
      <div style={T.micro}>v1.0 · XEITECH Input Generator</div>
    </div>
  )
}
