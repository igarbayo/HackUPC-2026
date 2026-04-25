'use client'

import Link from 'next/link'
import { useParams } from 'next/navigation'
import { useState } from 'react'
import { FF, T } from '@/lib/tokens'

type Tab = 'warehouse' | 'aisle' | 'shuttle'

const TABS: { id: Tab; label: string }[] = [
  { id: 'warehouse', label: 'Warehouse' },
  { id: 'aisle',     label: 'Aisle'     },
  { id: 'shuttle',   label: 'Shuttle'   },
]

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

export default function SimulationVisualizePage() {
  const { id } = useParams<{ id: string }>()
  const [activeTab, setActiveTab] = useState<Tab>('warehouse')

  return (
    <div style={{ fontFamily: FF, display: 'flex', flexDirection: 'column', height: '100%' }}>

      {/* ── Top bar: back + id ── */}
      <div style={{
        padding: '14px 24px 0',
        borderBottom: '1px solid #e8e8e8',
      }}>
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
        </div>

        {/* ── Tab bar ── */}
        <div style={{ display: 'flex', marginTop: 14, gap: 0 }}>
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
      <div style={{ flex: 1, position: 'relative', padding: '28px 24px' }}>

        {/* ── Metrics box ── */}
        <div style={{
          position: 'absolute', top: 20, right: 24,
          border: '1px solid #e8e8e8',
          padding: '14px 18px',
          display: 'flex', flexDirection: 'column', gap: 12,
          minWidth: 200,
        }}>
          <Metric label="% Full pallets"                   value={null} unit="%" />
          <Metric label="Throughput"                       value={null} />
          <Metric label="% Occupation dispatched pallets"  value={null} unit="%" />
        </div>

      </div>

    </div>
  )
}
