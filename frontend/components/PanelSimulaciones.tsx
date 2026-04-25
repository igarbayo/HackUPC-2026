'use client'

import { useState, useEffect, useCallback } from 'react'
import Link from 'next/link'
import { FF, T } from '@/lib/tokens'
import Hr from './ui/Hr'

const API_URL = process.env.NEXT_PUBLIC_API_URL ?? 'http://localhost:8000'

interface GeneratorParams {
  num_boxes: number
  num_destinations: number
  seed: number
  mean_inter_arrival_ticks: number
  std_inter_arrival_ticks: number
  weights: Record<string, number>
  demand_profile: { from_tick: number; to_tick: number; rate_multiplier: number }[]
}

interface SimulationParams {
  num_slots: number
  num_y: number
  num_sides: number
  max_ticks: number
  generator: GeneratorParams | null
}

interface SimulationRecord {
  sim_id: string
  status: 'running' | 'done' | 'error'
  created_at: string
  finished_at: string | null
  params: SimulationParams
}

function displayStatus(sim: SimulationRecord): SimulationRecord['status'] {
  if (sim.status !== 'done') return sim.status
  // Backend says done, but the WS stream may still be running
  const hasTps     = localStorage.getItem(`silos_tps_${sim.sim_id}`) !== null
  const streamDone = localStorage.getItem(`silos_stream_done_${sim.sim_id}`) === 'true'
  if (hasTps && !streamDone) return 'running'
  return 'done'
}

function statusColor(s: SimulationRecord['status']) {
  if (s === 'done')    return '#22c55e'
  if (s === 'error')   return '#ef4444'
  return '#f59e0b'
}

function fmt(iso: string) {
  return new Date(iso).toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit', second: '2-digit' })
}

function SimRow({ sim }: { sim: SimulationRecord }) {
  const [open, setOpen] = useState(false)
  const g = sim.params.generator
  const status = displayStatus(sim)

  return (
    <div style={{ borderBottom: '1px solid #f4f4f4' }}>
      <div
        onClick={() => setOpen(o => !o)}
        style={{
          display: 'flex', alignItems: 'center', gap: 10,
          padding: '10px 0', cursor: 'pointer', userSelect: 'none',
        }}
      >
        <div style={{
          width: 6, height: 6, borderRadius: '50%', flexShrink: 0,
          background: statusColor(status),
        }} />
        <div style={{ flex: 1, minWidth: 0 }}>
          <div style={{ fontSize: 10, color: '#000', letterSpacing: '0.04em', fontFamily: FF,
            overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
            {sim.sim_id.slice(0, 8).toUpperCase()}
          </div>
          <div style={{ ...T.micro, color: '#666', marginTop: 2 }}>{fmt(sim.created_at)}</div>
        </div>
        <div style={{ ...T.label, color: statusColor(status) }}>{status}</div>
        <Link
          href={`/simulations/${sim.sim_id}`}
          onClick={e => e.stopPropagation()}
          style={{
            fontSize: 9, letterSpacing: '0.14em', textTransform: 'uppercase',
            fontFamily: FF, color: '#fff', textDecoration: 'none',
            background: '#000', border: '1px solid #000', padding: '3px 8px',
          }}
        >
          Visualize
        </Link>
        <div style={{ fontSize: 9, color: '#777', fontFamily: FF }}>{open ? '▲' : '▼'}</div>
      </div>

      {open && (
        <div style={{ paddingBottom: 14, paddingLeft: 16, display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px 24px' }}>
          <Kv label="Slots"    value={sim.params.num_slots} />
          <Kv label="Levels"   value={sim.params.num_y} />
          <Kv label="Sides"    value={sim.params.num_sides} />
          <Kv label="Max ticks" value={sim.params.max_ticks} />
          {g && <>
            <Kv label="Boxes"       value={g.num_boxes} />
            <Kv label="Destinations" value={g.num_destinations} />
            <Kv label="Seed"        value={g.seed} />
            <Kv label="Mean arr."   value={g.mean_inter_arrival_ticks} />
          </>}
          {sim.finished_at && <Kv label="End" value={fmt(sim.finished_at)} span />}
        </div>
      )}
    </div>
  )
}

function Kv({ label, value, span }: { label: string; value: string | number; span?: boolean }) {
  return (
    <div style={span ? { gridColumn: '1 / -1' } : {}}>
      <div style={{ ...T.label, marginBottom: 3 }}>{label}</div>
      <div style={{ fontSize: 11, color: '#000', letterSpacing: '0.04em', fontFamily: FF }}>{value}</div>
    </div>
  )
}

export default function PanelSimulaciones() {
  const [sims, setSims]     = useState<SimulationRecord[]>([])
  const [loading, setLoading] = useState(false)
  const [error, setError]   = useState(false)

  const fetchSims = useCallback(async () => {
    setLoading(true)
    setError(false)
    try {
      const res = await fetch(`${API_URL}/simulations`)
      if (!res.ok) throw new Error()
      setSims(await res.json())
    } catch {
      setError(true)
    } finally {
      setLoading(false)
    }
  }, [])

  useEffect(() => { fetchSims() }, [fetchSims])

  return (
    <div style={{ padding: '28px 24px', fontFamily: FF }}>
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'baseline', marginBottom: 24 }}>
        <div style={{ ...T.section, color: '#000' }}>Simulations</div>
        <button
          onClick={fetchSims}
          style={{
            background: 'transparent', border: 'none', cursor: 'pointer',
            fontSize: 9, letterSpacing: '0.14em', textTransform: 'uppercase',
            fontFamily: FF, color: loading ? '#777' : '#555',
          }}
          disabled={loading}
        >
          {loading ? 'Loading...' : '↻ Refresh'}
        </button>
      </div>

      {error && (
        <div style={{ ...T.label, color: '#ef4444', marginBottom: 16 }}>
          Error connecting to backend
        </div>
      )}

      {!error && sims.length === 0 && !loading && (
        <div style={{ ...T.micro, color: '#777', marginTop: 8 }}>No simulations</div>
      )}

      {sims.map(sim => <SimRow key={sim.sim_id} sim={sim} />)}

      <Hr my={20} />
      <div style={{ display: 'flex', gap: 16 }}>
        <Kv label="Total"   value={sims.length} />
        <Kv label="Running" value={sims.filter(s => s.status === 'running').length} />
        <Kv label="Done"    value={sims.filter(s => s.status === 'done').length} />
        <Kv label="Error"   value={sims.filter(s => s.status === 'error').length} />
      </div>
    </div>
  )
}
