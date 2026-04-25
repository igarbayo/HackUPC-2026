'use client'

import Link from 'next/link'
import { useParams } from 'next/navigation'
import { useState, useEffect } from 'react'
import dynamic from 'next/dynamic'
import { FF, T } from '@/lib/tokens'
import { useSimulationStream } from '@/lib/SimulationStreamContext'
import type { StreamStatus, InstructionState } from '@/lib/SimulationStreamContext'

type Tab = 'warehouse' | 'aisle' | 'shuttle' | 'simulation' | '3d'
const Warehouse3DView = dynamic(
  () => import('@/components/Warehouse3DView'),
  { ssr: false, loading: () => null },
)

type WsStatus = StreamStatus

interface SimParams {
  num_slots: number
  num_y: number
  num_sides: number
}

// ── Constants ──────────────────────────────────────────────────────────

const PALLET_CAPACITY = 12

const TABS: { id: Tab; label: string }[] = [
  { id: 'warehouse', label: 'Warehouse' },
  { id: 'aisle',     label: 'Aisle'     },
  { id: 'shuttle',   label: 'Shuttle'   },
  { id: 'simulation', label: 'Simulation' },
  { id: '3d',        label: '3D'        },
]

const STATUS_LABEL: Record<WsStatus, string> = {
  connecting: 'connecting',
  streaming:  'running',
  done:       'done',
  error:      'error',
}

const STATUS_COLOR: Record<WsStatus, string> = {
  connecting: '#f59e0b',
  streaming:  '#f59e0b',
  done:       '#aaa',
  error:      '#ef4444',
}


// ── Sub-components ─────────────────────────────────────────────────────

function Metric({ label, value, unit }: { label: string; value: number | null; unit?: string }) {
  return (
    <div>
      <div style={{ ...T.label, marginBottom: 4 }}>{label}</div>
      <div style={{ fontSize: 13, letterSpacing: '0.04em', color: '#000', fontFamily: FF }}>
        {value === null ? '—' : `${value}${unit ?? ''}`}
      </div>
    </div>
  )
}

// ── Main component ─────────────────────────────────────────────────────

const API_BASE = process.env.NEXT_PUBLIC_API_URL ?? 'http://localhost:8000'

export default function SimulationVisualizePage() {
  const { id }           = useParams<{ id: string }>()
  const [activeTab, setActiveTab] = useState<Tab>('warehouse')
  const [selectedAisle, setSelectedAisle] = useState(0)
  const [simParams, setSimParams] = useState<SimParams | null>(null)
  const { streams, setSpeed } = useSimulationStream()
  const stream = streams[id] ?? { status: 'connecting' as WsStatus, snapshot: null, totalTicks: null, speed: 1 }

  useEffect(() => {
    fetch(`${API_BASE}/simulations/${id}`)
      .then(r => r.json())
      .then((d: { params: SimParams }) => setSimParams(d.params))
      .catch(() => {})
  }, [id])

  function handleSpeedChange(v: number) {
    setSpeed(id, Math.max(0.1, v))
  }

  // ── Computed values ────────────────────────────────────────────────────
  const { status: wsStatus, snapshot, totalTicks, speed } = stream
  const pallets      = snapshot?.robot.pallets ?? []
  const shuttles     = snapshot?.aisles[selectedAisle]?.shuttles ?? []
  const busyShuttles = shuttles.filter(s => s.phase !== 'Idle').length
  const avgFill = pallets.length > 0
    ? Math.round(pallets.reduce((sum, p) => sum + p.placed_count, 0) / pallets.length / PALLET_CAPACITY * 100)
    : null


  // ── Tab content ────────────────────────────────────────────────────────
  function renderWarehouse() {
    if (!snapshot) return <div style={T.label}>Waiting for data...</div>
    if (pallets.length === 0) return <div style={T.micro}>No active pallets</div>
    return (
      <div>
        <div style={{
          display: 'grid', gridTemplateColumns: '32px 1fr 64px 120px',
          gap: 16, marginBottom: 8,
        }}>
          {['Slot', 'Family', 'Boxes', 'Fill'].map(h => (
            <div key={h} style={T.label}>{h}</div>
          ))}
        </div>
        {pallets.map(p => (
          <div key={p.slot} style={{
            display: 'grid', gridTemplateColumns: '32px 1fr 64px 120px',
            gap: 16, padding: '8px 0', borderBottom: '1px solid #f4f4f4',
            alignItems: 'center',
          }}>
            <div style={{ ...T.label }}>#{p.slot}</div>
            <div style={{ fontSize: 11, color: '#000', fontFamily: FF }}>{p.family}</div>
            <div style={{ fontSize: 11, color: '#000', fontFamily: FF }}>
              {p.placed_count}/{PALLET_CAPACITY}
            </div>
            <div style={{ height: 4, background: '#f0f0f0', borderRadius: 2 }}>
              <div style={{
                height: '100%', borderRadius: 2,
                background: p.placed_count >= PALLET_CAPACITY ? '#22c55e' : '#000',
                width: `${Math.round(p.placed_count / PALLET_CAPACITY * 100)}%`,
                transition: 'width 0.3s',
              }} />
            </div>
          </div>
        ))}
      </div>
    )
  }

  function renderAisle() {
    if (!snapshot) return <div style={T.label}>Waiting for data...</div>
    return (
      <div>
        <div style={{
          display: 'grid', gridTemplateColumns: '60px 120px 1fr',
          gap: 16, marginBottom: 8,
        }}>
          {['Level', 'Phase', 'Carrying'].map(h => (
            <div key={h} style={T.label}>{h}</div>
          ))}
        </div>
        {shuttles.map((s, i) => (
          <div key={i} style={{
            display: 'grid', gridTemplateColumns: '60px 120px 1fr',
            gap: 16, padding: '8px 0', borderBottom: '1px solid #f4f4f4',
            alignItems: 'center',
          }}>
            <div style={{ fontSize: 11, color: '#000', fontFamily: FF }}>Y={s.y_level}</div>
            <div style={{
              fontSize: 9, letterSpacing: '0.1em', textTransform: 'uppercase',
              color: s.phase === 'Idle' ? '#ccc' : '#000',
            }}>
              {s.phase}
            </div>
            <div style={{ fontSize: 11, color: s.is_carrying ? '#000' : '#ccc', fontFamily: FF }}>
              {s.is_carrying ? `${s.carried_box_id.slice(-6)} · ${s.carried_box_family}` : '—'}
            </div>
          </div>
        ))}
      </div>
    )
  }

  function renderShuttle() {
    if (!snapshot) return <div style={T.label}>Waiting for data...</div>
    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: 12 }}>
        {shuttles.map((s, i) => (
          <div key={i} style={{ border: '1px solid #e8e8e8', padding: '14px 18px' }}>
            <div style={{ ...T.label, color: '#000', marginBottom: 12 }}>
              Shuttle Y={s.y_level}
            </div>
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: 16 }}>
              <Metric label="Phase"      value={null} />
              <Metric label="Position X" value={s.position.x} />
              <Metric label="Position Z" value={s.position.z} />
            </div>
            <div style={{ marginTop: 8, ...T.label }}>
              {s.is_carrying
                ? `Carrying: ${s.carried_box_id} · ${s.carried_box_family}`
                : 'Not carrying'}
            </div>
          </div>
        ))}
      </div>
    )
  }

  function renderInstructionTable(instrs: InstructionState[], title: string) {
    return (
      <div>
        <div style={{ ...T.label, marginBottom: 10 }}>{title}</div>
        {instrs.length === 0 ? (
          <div style={{ ...T.micro, color: '#bbb' }}>No pending instructions</div>
        ) : (
          <>
            <div style={{
              display: 'grid', gridTemplateColumns: '36px 52px 52px 1fr 1fr 80px 52px',
              gap: 10, marginBottom: 6,
            }}>
              {['Seq', 'Tick', 'Kind', 'Family', 'Box ID', 'Target', 'Priority'].map(h => (
                <div key={h} style={T.label}>{h}</div>
              ))}
            </div>
            {instrs.map((instr, i) => (
              <div key={i} style={{
                display: 'grid', gridTemplateColumns: '36px 52px 52px 1fr 1fr 80px 52px',
                gap: 10, padding: '6px 0', borderBottom: '1px solid #f4f4f4',
                alignItems: 'center',
              }}>
                <div style={{ fontSize: 10, color: '#aaa', fontFamily: FF }}>{instr.seq}</div>
                <div style={{ fontSize: 10, color: '#aaa', fontFamily: FF }}>{instr.issued_at}</div>
                <div style={{
                  fontSize: 9, letterSpacing: '0.1em', textTransform: 'uppercase',
                  color: instr.kind === 'Input' ? '#000' : '#555',
                }}>
                  {instr.kind === 'Input' ? 'IN' : 'OUT'}
                </div>
                <div style={{ fontSize: 11, color: '#000', fontFamily: FF }}>{instr.family}</div>
                <div style={{ fontSize: 10, color: instr.box_id ? '#000' : '#ccc', fontFamily: FF }}>
                  {instr.box_id ? instr.box_id.slice(-8) : '—'}
                </div>
                <div style={{ fontSize: 10, color: '#555', fontFamily: FF }}>
                  x={instr.target.x},z={instr.target.z}
                </div>
                <div style={{ fontSize: 10, color: '#aaa', fontFamily: FF }}>{instr.priority}</div>
              </div>
            ))}
          </>
        )}
      </div>
    )
  }

  function renderSimulation() {
    if (!snapshot) return <div style={T.label}>Waiting for data...</div>
    const aisleList = snapshot.aisles
    const aisle     = aisleList[selectedAisle]
    const activeMissions = (aisle?.shuttles ?? []).filter(s => s.phase !== 'Idle')

    return (
      <div>
        {/* Aisle selector */}
        {aisleList.length > 1 && (
          <div style={{ display: 'flex', gap: 8, marginBottom: 20 }}>
            {aisleList.map((a, idx) => (
              <button
                key={idx}
                onClick={() => setSelectedAisle(idx)}
                style={{
                  background: 'none', border: 'none', cursor: 'pointer',
                  padding: '4px 12px',
                  fontSize: 9, letterSpacing: '0.12em', textTransform: 'uppercase',
                  fontFamily: FF,
                  color: selectedAisle === idx ? '#000' : '#bbb',
                  borderBottom: selectedAisle === idx ? '1px solid #000' : '1px solid transparent',
                }}
              >
                Aisle #{a.aisle_id}
              </button>
            ))}
          </div>
        )}

        {/* Two instruction queue tables */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 32, marginBottom: 32 }}>
          {renderInstructionTable(aisle?.pending_fifo ?? [], 'Arrival Order')}
          {renderInstructionTable(aisle?.pending_sorted ?? [], 'Priority Order')}
        </div>

        {/* Active missions */}
        <div style={{ ...T.label, marginBottom: 10 }}>Active Missions</div>
        {activeMissions.length === 0 ? (
          <div style={{ ...T.micro, color: '#bbb' }}>No active missions</div>
        ) : (
          <>
            <div style={{
              display: 'grid', gridTemplateColumns: '60px 140px 1fr 80px 80px',
              gap: 12, marginBottom: 6,
            }}>
              {['Shuttle', 'Phase', 'Box ID', 'Family', 'Pos (x,z)'].map(h => (
                <div key={h} style={T.label}>{h}</div>
              ))}
            </div>
            {activeMissions.map((s, i) => (
              <div key={i} style={{
                display: 'grid', gridTemplateColumns: '60px 140px 1fr 80px 80px',
                gap: 12, padding: '6px 0', borderBottom: '1px solid #f4f4f4',
                alignItems: 'center',
              }}>
                <div style={{ fontSize: 11, color: '#000', fontFamily: FF }}>Y={s.y_level}</div>
                <div style={{
                  fontSize: 9, letterSpacing: '0.1em', textTransform: 'uppercase', color: '#000',
                }}>
                  {s.phase}
                </div>
                <div style={{ fontSize: 10, color: s.is_carrying ? '#000' : '#ccc', fontFamily: FF }}>
                  {s.is_carrying ? s.carried_box_id.slice(-8) : '—'}
                </div>
                <div style={{ fontSize: 11, color: '#000', fontFamily: FF }}>
                  {s.is_carrying ? s.carried_box_family : '—'}
                </div>
                <div style={{ fontSize: 10, color: '#555', fontFamily: FF }}>
                  x={s.position.x},z={s.position.z}
                </div>
              </div>
            ))}
          </>
        )}
      </div>
    )
  }

  function render3D() { return null }

  const TAB_RENDER: Record<Tab, () => React.ReactNode> = {
    warehouse:  renderWarehouse,
    aisle:      renderAisle,
    shuttle:    renderShuttle,
    simulation: renderSimulation,
    '3d':      render3D
  }

  return (
    <div style={{ fontFamily: FF, display: 'flex', flexDirection: 'column', height: '100%' }}>

      {/* ── Top bar ── */}
      <div style={{ padding: '14px 24px 0', borderBottom: '1px solid #e8e8e8' }}>
        <Link
          href="/simulations"
          style={{
            display: 'inline-flex', alignItems: 'center', gap: 6,
            fontSize: 9, letterSpacing: '0.14em', textTransform: 'uppercase',
            color: '#aaa', textDecoration: 'none',
          }}
        >
          ← Return to all simulations
        </Link>

        <div style={{ display: 'flex', alignItems: 'baseline', gap: 12, marginTop: 10 }}>
          <div style={{ ...T.section, color: '#000' }}>
            {id.slice(0, 8).toUpperCase()}
          </div>
          <div style={{ fontSize: 11, color: '#aaa', fontFamily: FF }}>
            {snapshot ? `Tick ${snapshot.tick}` : '—'}
            {totalTicks !== null ? ` / ${totalTicks}` : ''}
          </div>
          <div style={{
            fontSize: 9, letterSpacing: '0.14em', textTransform: 'uppercase',
            color: STATUS_COLOR[wsStatus],
          }}>
            {STATUS_LABEL[wsStatus]}
          </div>
        </div>

        {/* ── Tab bar ── */}
        <div style={{ display: 'flex', marginTop: 14 }}>
          {TABS.map(tab => (
            <button
              key={tab.id}
              onClick={() => setActiveTab(tab.id)}
              style={{
                background: 'none', border: 'none', cursor: 'pointer',
                padding: '8px 20px 8px 0',
                fontSize: 9, letterSpacing: '0.16em', textTransform: 'uppercase',
                fontFamily: FF,
                color: activeTab === tab.id ? '#000' : '#bbb',
                borderBottom: activeTab === tab.id ? '1px solid #000' : '1px solid transparent',
                marginBottom: -1,
                transition: 'color 0.15s, border-color 0.15s',
              }}
            >
              {tab.label}
            </button>
          ))}
        </div>
      </div>

      {/* ── Content ── */}
      <div style={{ flex: 1, position: 'relative', padding: '28px 24px', overflowY: 'auto' }}>

        {/* ── Metrics + speed ── */}
        <div style={{
          position: 'absolute', top: 20, right: 24,
          border: '1px solid #e8e8e8',
          padding: '14px 18px',
          display: 'flex', flexDirection: 'column', gap: 12,
          minWidth: 200,
        }}>
          <Metric label="Current tick"   value={snapshot?.tick ?? null} />
          <Metric label="Avg pallet fill" value={avgFill} unit="%" />
          <Metric label="Shuttles busy"  value={busyShuttles > 0 ? busyShuttles : null} />

          <div style={{ borderTop: '1px solid #f0f0f0', paddingTop: 12 }}>
            <div style={{ ...T.label, marginBottom: 6 }}>Playback speed</div>
            <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
              <input
                type="number"
                value={speed}
                step={0.5}
                min={0.1}
                onChange={e => handleSpeedChange(Number(e.target.value))}
                style={{
                  border: 'none', borderBottom: '1px solid #e0e0e0',
                  padding: '4px 0', fontSize: 11, fontFamily: FF,
                  outline: 'none', background: 'transparent',
                  color: '#000', width: 52,
                }}
              />
              <div style={T.micro}>ticks/s</div>
            </div>
          </div>
        </div>

        {/* ── KPI panel ── */}
        {snapshot && (() => {
          const m = snapshot.metrics
          const boxesInProgress = pallets.reduce((s, p) => s + p.placed_count, 0)
          return (
            <div style={{
              position: 'absolute', bottom: 20, right: 24,
              border: '1px solid #e8e8e8',
              padding: '14px 18px',
              display: 'flex', flexDirection: 'column', gap: 12,
              minWidth: 200,
            }}>
              <Metric label="Active pallets"    value={pallets.length > 0 ? pallets.length : null} />
              <Metric label="Boxes in progress" value={boxesInProgress > 0 ? boxesInProgress : null} />
              <div style={{ borderTop: '1px solid #f0f0f0', paddingTop: 12, display: 'flex', flexDirection: 'column', gap: 12 }}>
                <Metric label="Pallets sent"      value={m.total_pallets_sent > 0 ? m.total_pallets_sent : null} />
                <Metric label="Full pallets"      value={m.full_pallets > 0 ? m.full_pallets : null} />
                <Metric label="Full pallet ratio" value={m.total_pallets_sent > 0 ? m.full_pallet_ratio : null} unit="%" />
                <Metric label="Total boxes sent"  value={m.total_boxes_sent > 0 ? m.total_boxes_sent : null} />
                <Metric label="Avg fill rate"     value={m.total_pallets_sent > 0 ? m.avg_fill_rate : null} unit="%" />
                <Metric label="Throughput"        value={m.total_pallets_sent > 0 ? Math.round(snapshot.tick / m.total_pallets_sent) : null} unit=" ticks/pallet" />
              </div>
            </div>
          )
        })()}

        {/* ── Tab content ── */}
        {activeTab !== '3d' && (
          <div style={{ maxWidth: 'calc(100% - 240px)' }}>
            {TAB_RENDER[activeTab]()}
          </div>
        )}

        {/* ── 3D view (absolute fill, leaves space for right panels) ── */}
        {activeTab === '3d' && (
          <div style={{ position: 'absolute', top: 0, left: 0, right: 240, bottom: 0 }}>
            {!simParams
              ? <div style={{ ...T.label, padding: 24 }}>Loading...</div>
              : <Warehouse3DView simId={id} params={simParams} snapshot={snapshot} />
            }
          </div>
        )}

      </div>
    </div>
  )
}
