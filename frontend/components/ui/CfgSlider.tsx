'use client'

import { T } from '@/lib/tokens'

interface CfgSliderProps {
  label: string
  value: number
  onChange: (v: number) => void
  min: number
  max: number
  step?: number
  unit?: string
}

export default function CfgSlider({ label, value, onChange, min, max, step = 1, unit = '' }: CfgSliderProps) {
  return (
    <div style={{ marginBottom: 14 }}>
      <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: 6 }}>
        <span style={T.label}>{label}</span>
        <span style={{ fontSize: 10, color: '#000', letterSpacing: '0.04em' }}>{value}{unit}</span>
      </div>
      <input
        type="range"
        min={min}
        max={max}
        step={step}
        value={value}
        onChange={e => onChange(Number(e.target.value))}
        style={{ width: '100%', accentColor: '#000', height: 1 } as React.CSSProperties}
      />
    </div>
  )
}
